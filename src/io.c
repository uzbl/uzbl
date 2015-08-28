#include "io.h"

#include "commands.h"
#include "events.h"
#include "setup.h"
#include "type.h"
#include "util.h"
#include "variables.h"
#include "uzbl-core.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "3p/async-queue-source/rb-async-queue-watch.h"

struct _UzblIO {
    /* Sockets to connect to as event managers. */
    GPtrArray *connect_sockets;
    /* Sockets to connect to as clients. */
    GPtrArray *client_sockets;

    /* The event buffer. */
    GMutex     event_buffer_lock;
    GPtrArray *event_buffer;

    /* Path to the main FIFO for client communication. */
    gchar *fifo_path;
    /* Path to the main socket for client communication. */
    gchar *socket_path;

    /* The command queue for sending I/O commands from sockets/FIFO/etc. to be
     * run in the main thread. */
    GAsyncQueue  *cmd_q;
    /* The I/O thread variables. */
    GMainContext *io_ctx;
    GMainLoop    *io_loop;
    GThread      *io_thread;
};

/* =========================== PUBLIC API =========================== */

static gboolean
flush_event_buffer (gpointer data);
static void
free_cmd_req (gpointer data);
static void
run_command (gpointer item, gpointer data);
static gpointer
run_io (gpointer data);

void
uzbl_io_init ()
{
    uzbl.io = g_malloc (sizeof (UzblIO));

    uzbl.io->connect_sockets = g_ptr_array_new ();
    uzbl.io->client_sockets = g_ptr_array_new ();

    g_mutex_init (&uzbl.io->event_buffer_lock);
    uzbl.io->event_buffer = g_ptr_array_new_with_free_func (g_free);
    g_timeout_add_seconds (10, flush_event_buffer, NULL);

    uzbl.io->fifo_path = NULL;
    uzbl.io->socket_path = NULL;

    uzbl.io->cmd_q = g_async_queue_new_full (free_cmd_req);

    uzbl_rb_async_queue_watch_new (uzbl.io->cmd_q,
        G_PRIORITY_HIGH, run_command,
        NULL, NULL, NULL);

    uzbl.io->io_thread = g_thread_new ("uzbl-io", run_io, NULL);
}

void
uzbl_io_free ()
{
    g_ptr_array_unref (uzbl.io->connect_sockets);
    g_ptr_array_unref (uzbl.io->client_sockets);

    if (uzbl.io->event_buffer) {
        g_mutex_lock (&uzbl.io->event_buffer_lock);
        g_ptr_array_unref (uzbl.io->event_buffer);
        uzbl.io->event_buffer = NULL;
        g_mutex_unlock (&uzbl.io->event_buffer_lock);
    }
    g_mutex_clear (&uzbl.io->event_buffer_lock);

    if (uzbl.io->fifo_path) {
        unlink (uzbl.io->fifo_path);
    }
    if (uzbl.io->socket_path) {
        unlink (uzbl.io->socket_path);
    }

    g_free (uzbl.io->fifo_path);
    g_free (uzbl.io->socket_path);

    g_async_queue_unref (uzbl.io->cmd_q);
    g_thread_unref (uzbl.io->io_thread);

    g_free (uzbl.io);
    uzbl.io = NULL;
}

typedef gboolean (*UzblIODataCallback)(GIOChannel *gio, GString* line, gpointer data);
typedef void (*UzblIODataErrorCallback)(GIOChannel *gio, gpointer data);
static void
add_buffered_cmd_source (GIOChannel *gio, const gchar *name, UzblIODataCallback callback,
        UzblIODataErrorCallback error_callback, gpointer data);
static gboolean
control_stdin (GIOChannel *gio, GString *line, gpointer data);

void
uzbl_io_init_stdin ()
{
    GIOChannel *chan = g_io_channel_unix_new (STDIN_FILENO);
    if (chan) {
        add_buffered_cmd_source (chan, "Uzbl stdin watcher",
            control_stdin, NULL, NULL);
    } else {
        g_error ("create_stdin: error while opening stdin\n");
    }
}

static gboolean
control_client_socket (GIOChannel *gio, GString *line, gpointer data);
static void
close_client_socket (GIOChannel *gio, gpointer data);
static void
replay_event_buffer (GIOChannel *channel);

gboolean
uzbl_io_init_connect_socket (const gchar *socket_path)
{
    GIOChannel *chan = NULL;

    int sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un local;
    local.sun_family = AF_UNIX;
    strcpy (local.sun_path, socket_path);

    if (!connect (sockfd, (struct sockaddr *)&local, sizeof (local))) {
        chan = g_io_channel_unix_new (sockfd);
        if (chan) {
            g_io_channel_set_encoding (chan, NULL, NULL);
            add_buffered_cmd_source (chan, "Uzbl connect socket",
                control_client_socket, close_client_socket, uzbl.io->connect_sockets);
            g_ptr_array_add (uzbl.io->connect_sockets, chan);
        }
    } else {
        g_warning ("Error connecting to socket: %s\n", socket_path);
        return G_SOURCE_REMOVE;
    }

    replay_event_buffer (chan);

    return G_SOURCE_CONTINUE;
}

static void
buffer_event (const gchar *message);
static void
send_event_sockets (GPtrArray *sockets, const gchar *message);

void
uzbl_io_send (const gchar *message, gboolean connect_only)
{
    if (!message) {
        return;
    }

    if (!strchr (message, '\n')) {
        return;
    }

    buffer_event (message);

    if (uzbl_variables_get_int ("print_events")) {
        fprintf (stdout, "%s", message);
        fflush (stdout);
    }

    /* Write to all --connect-socket sockets. */
    send_event_sockets (uzbl.io->connect_sockets, message);

    if (!connect_only) {
        /* Write to all client sockets. */
        send_event_sockets (uzbl.io->client_sockets, message);
    }
}

typedef struct {
    gchar *cmd;
    const UzblCommand *info;
    GArray *argv;
    UzblIOCallback callback;
    gpointer data;
} UzblCommandData;

void
uzbl_io_schedule_command (const UzblCommand *cmd, GArray *argv, UzblIOCallback callback, gpointer data)
{
    if (!cmd || !argv) {
        uzbl_debug ("Invalid command scheduled");
        return;
    }

    UzblCommandData *cmd_data = g_malloc (sizeof (UzblCommandData));
    cmd_data->cmd = NULL;
    cmd_data->info = cmd;
    cmd_data->argv = argv;
    cmd_data->callback = callback;
    cmd_data->data = data;

    g_async_queue_push (uzbl.io->cmd_q, cmd_data);
}

typedef enum {
    UZBL_COMM_FIFO,
    UZBL_COMM_SOCKET
} UzblCommType;

static gchar *
build_stream_name (UzblCommType type, const gchar *dir);
static int
create_dir (const gchar *dir);

static gboolean
attach_fifo (const gchar *path);

gboolean
uzbl_io_init_fifo (const gchar *dir)
{
    if (create_dir (dir)) {
        return FALSE;
    }

    if (uzbl.io->fifo_path) {
        /* We're changing the fifo path, get rid of the old fifo if one exists. */
        if (unlink (uzbl.io->fifo_path)) {
            g_warning ("Fifo: Can't unlink old fifo at %s\n", uzbl.io->fifo_path);
            return FALSE;
        }
        g_free (uzbl.io->fifo_path);
        uzbl.io->fifo_path = NULL;
    }

    gchar *path = build_stream_name (UZBL_COMM_FIFO, dir);

    /* If something exists at that path, try to delete it. */
    if (file_exists (path) && unlink (path)) {
        g_warning ("Fifo: Can't unlink old fifo at %s\n", path);
    }

    gboolean ret = FALSE;

    if (!mkfifo (path, 0600)) {
      if (attach_fifo (path)) {
         ret = TRUE;
      } else {
         g_warning ("init_fifo: can't attach to %s: %s\n", path, strerror (errno));
      }
    } else {
        g_warning ("init_fifo: can't create %s: %s\n", path, strerror (errno));
    }

    g_free (path);
    return ret;
}

static gboolean
attach_socket (const gchar *path, struct sockaddr_un *local);

gboolean
uzbl_io_init_socket (const gchar *dir)
{
    if (create_dir (dir)) {
        return FALSE;
    }

    if (uzbl.io->socket_path) {
        /* Remove an existing socket should one exist. */
        if (unlink (uzbl.io->socket_path)) {
            g_warning ("init_socket: couldn't unlink socket at %s\n", uzbl.io->socket_path);
            return FALSE;
        }
        g_free (uzbl.io->socket_path);
        uzbl.io->socket_path = NULL;
    }

    if (*dir == ' ') {
        return FALSE;
    }

    gchar *path = build_stream_name (UZBL_COMM_SOCKET, dir);

    /* If something exists at that path, try to delete it. */
    if (file_exists (path) && unlink (path)) {
        g_warning ("Socket: Can't unlink old socket at %s\n", path);
    }

    struct sockaddr_un local;
    local.sun_family = AF_UNIX;
    strcpy (local.sun_path, path);
    gboolean ret = FALSE;

    if (attach_socket (path, &local)) {
        ret = TRUE;
    } else {
        g_warning ("init_socket: can't attach to %s: %s\n", path, strerror (errno));
    }

    g_free (path);
    return ret;
}

void
uzbl_io_flush_buffer ()
{
    flush_event_buffer (NULL);
}

void
uzbl_io_quit ()
{
    g_main_loop_quit (uzbl.io->io_loop);
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

static void
send_buffered_event (gpointer event, gpointer data);

gboolean
flush_event_buffer (gpointer data)
{
    UZBL_UNUSED (data);

    if (!uzbl.io->event_buffer) {
        return FALSE;
    }

    g_mutex_lock (&uzbl.io->event_buffer_lock);
    g_ptr_array_foreach (uzbl.io->event_buffer, send_buffered_event, NULL);
    g_ptr_array_free (uzbl.io->event_buffer, TRUE);
    uzbl.io->event_buffer = NULL;
    g_mutex_unlock (&uzbl.io->event_buffer_lock);

    return FALSE;
}

void
free_cmd_req (gpointer data)
{
    UzblCommandData *cmd = (UzblCommandData *)data;

    uzbl_commands_args_free (cmd->argv);
    g_free (cmd->cmd);
    g_free (cmd);
}

void
run_command (gpointer item, gpointer data)
{
    UZBL_UNUSED (data);

    UzblCommandData *cmd = (UzblCommandData *)item;

    GString *result = NULL;

    if (cmd->callback) {
        result = g_string_new ("");
    }

    if (cmd->cmd) {
        uzbl_commands_run (cmd->cmd, result);
    } else {
        uzbl_commands_run_parsed (cmd->info, cmd->argv, result);
    }

    if (cmd->callback) {
        cmd->callback (result, cmd->data);
        g_string_free (result, TRUE);
    }

    free_cmd_req (cmd);
}

gpointer
run_io (gpointer data)
{
    UZBL_UNUSED (data);

    uzbl.io->io_ctx = g_main_context_new ();

    uzbl.io->io_loop = g_main_loop_new (uzbl.io->io_ctx, FALSE);
    g_main_loop_run (uzbl.io->io_loop);
    g_main_loop_unref (uzbl.io->io_loop);
    uzbl.io->io_loop = NULL;

    g_main_context_unref (uzbl.io->io_ctx);
    uzbl.io->io_ctx = NULL;

    return NULL;
}

typedef struct {
    GString *buffer;
    UzblIODataCallback callback;
    UzblIODataErrorCallback error_callback;
    gpointer data;
} UzblIOBufferData;

static gboolean
line_buffer_io (GIOChannel *gio, GIOCondition condition, gpointer data);
static void
free_watcher_data (gpointer data);

void
add_buffered_cmd_source (GIOChannel *gio, const gchar *name, UzblIODataCallback callback,
        UzblIODataErrorCallback error_callback, gpointer data)
{
    GSource *source = g_io_create_watch (gio, G_IO_IN | G_IO_HUP);
    g_source_set_name (source, name);

    GIOFlags flags = g_io_channel_get_flags (gio);
    g_io_channel_set_flags (gio, flags | G_IO_FLAG_NONBLOCK, NULL);

    UzblIOBufferData *io_data = g_malloc (sizeof (UzblIOBufferData));
    io_data->buffer = g_string_new ("");
    io_data->callback = callback;
    io_data->error_callback = error_callback;
    io_data->data = data;

    /* g_io_create_watch uses GIOFunc as its callback function. */
    g_source_set_callback (source, (GSourceFunc)line_buffer_io, io_data, free_watcher_data);

    g_source_attach (source, uzbl.io->io_ctx);

    g_source_unref (source);
}

static void
schedule_io_input (gchar *line, UzblIOCallback callback, gpointer data);
static void
write_stdout (GString *result, gpointer data);

gboolean
control_stdin (GIOChannel *gio, GString *input, gpointer data)
{
    UZBL_UNUSED (gio);
    UZBL_UNUSED (data);

    gchar *ctl_line = g_strdup (input->str);
    schedule_io_input (ctl_line, write_stdout, NULL);

    return G_SOURCE_CONTINUE;
}

static void
write_socket (GString *result, gpointer data);

gboolean
control_client_socket (GIOChannel *gio, GString *input, gpointer data)
{
    UZBL_UNUSED (data);

    gchar *ctl_line = g_strdup (input->str);
    g_io_channel_ref (gio);
    schedule_io_input (ctl_line, write_socket, gio);

    return G_SOURCE_CONTINUE;
}

void
close_client_socket (GIOChannel *gio, gpointer data)
{
    GError *error = NULL;
    GIOStatus ret = g_io_channel_shutdown (gio, TRUE, &error);
    GPtrArray *socket_array = (GPtrArray *)data;

    if (socket_array) {
        g_ptr_array_remove_fast (socket_array, gio);
    }

    if (ret == G_IO_STATUS_ERROR) {
        g_warning ("Error shutting down client socket: %s", error->message);
        g_clear_error (&error);
    }
}

static void
send_buffered_event_to_socket (gpointer event, gpointer data);

void
replay_event_buffer (GIOChannel *channel)
{
    if (!uzbl.io->event_buffer) {
        return;
    }

    g_mutex_lock (&uzbl.io->event_buffer_lock);
    g_ptr_array_foreach (uzbl.io->event_buffer, send_buffered_event_to_socket, channel);
    g_mutex_unlock (&uzbl.io->event_buffer_lock);
}

void
buffer_event (const gchar *message)
{
    if (!uzbl.io->event_buffer) {
        return;
    }

    g_mutex_lock (&uzbl.io->event_buffer_lock);
    g_ptr_array_add (uzbl.io->event_buffer, g_strdup (message));
    g_mutex_unlock (&uzbl.io->event_buffer_lock);
}

static void
write_to_socket (GIOChannel *channel, const gchar *message);

void
send_event_sockets (GPtrArray *sockets, const gchar *message)
{
    g_ptr_array_foreach (sockets, (GFunc)write_to_socket, (gpointer)message);
}

gchar *
build_stream_name (UzblCommType type, const gchar *dir)
{
    gchar *str = NULL;
    gchar *type_str;

    switch (type) {
    case UZBL_COMM_FIFO:
        type_str = "fifo";
        break;
    case UZBL_COMM_SOCKET:
        type_str = "socket";
        break;
    default:
        g_error ("Unknown communication type: %d\n", type);
        return NULL;
    }

    str = g_strdup_printf ("%s/uzbl_%s_%s", dir, type_str, uzbl.state.instance_name);

    return str;
}

int
create_dir (const gchar *dir)
{
    gchar *work_path = g_strdup (dir);
    size_t len = strlen (work_path);
    gchar *p;

    if (!len) {
        return EXIT_FAILURE;
    }

    if (work_path[len - 1] == '/') {
        work_path[len - 1] = '\0';
    }

#define check_mkdir(dir, mode)                              \
    if (mkdir (work_path, 0700)) {                          \
        switch (errno) {                                    \
        case EEXIST:                                        \
            break;                                          \
        default:                                            \
            perror ("Failed to create socket or fifo dir"); \
            return EXIT_FAILURE;                            \
        }                                                   \
    }

    /* Start making the parent directories from the bottom. */
    for (p = work_path + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            check_mkdir (work_path, 0700);
            *p = '/';
        }
    }

    check_mkdir (work_path, 0700);
    g_free (work_path);

    return EXIT_SUCCESS;
}

static gboolean
control_fifo (GIOChannel *gio, GString *input, gpointer data);

gboolean
attach_fifo (const gchar *path)
{
    GError *error = NULL;
    /* We don't really need to write to the file, but if we open the file as
     * 'r' we will block here, waiting for a writer to open the file. */
    GIOChannel *chan = g_io_channel_new_file (path, "r+", &error);
    if (chan) {
        add_buffered_cmd_source (chan, "Uzbl main fifo",
            control_fifo, NULL, NULL);

        g_io_channel_unref (chan);

        uzbl.io->fifo_path = g_strdup (path);
        uzbl_events_send (FIFO_SET, NULL,
            TYPE_STR, uzbl.io->fifo_path,
            NULL);
        /* TODO: Collect all environment settings into one place. */
        g_setenv ("UZBL_FIFO", uzbl.io->fifo_path, TRUE);

        return TRUE;
    } else {
        g_warning ("attach_fifo: can't open: %s\n", error->message);
    }

    g_error_free (error);
    return FALSE;
}

static void
add_cmd_source (GIOChannel *gio, const gchar *name, GIOFunc callback, gpointer data);
static gboolean
control_socket (GIOChannel *gio, GIOCondition condition, gpointer data);

gboolean
attach_socket (const gchar *path, struct sockaddr_un *local)
{
    GIOChannel *chan = NULL;
    int sock = socket (AF_UNIX, SOCK_STREAM, 0);

    if (!bind (sock, (struct sockaddr *)local, sizeof (*local))) {
        uzbl_debug ("init_socket: opened in %s\n", path);

        if (chmod (path, 0700)) {
            g_warning ("unable to change permissions for %s socket: %s\n", path, strerror (errno));
            close (sock);
            return FALSE;
        }

        if (listen (sock, 5)) {
            g_warning ("attach_socket: could not listen on %s: %s\n", path, strerror (errno));
        }

        if ((chan = g_io_channel_unix_new (sock))) {
            add_cmd_source (chan, "Uzbl main socket",
                control_socket, NULL);

            g_io_channel_unref (chan);

            uzbl.io->socket_path = g_strdup (path);
            uzbl_events_send (SOCKET_SET, NULL,
                TYPE_STR, uzbl.io->socket_path,
                NULL);
            /* TODO: Collect all environment settings into one place. */
            g_setenv ("UZBL_SOCKET", uzbl.io->socket_path, TRUE);
            return TRUE;
        }
    } else {
        g_warning ("attach_socket: could not bind to %s: %s\n", path, strerror (errno));
    }

    return FALSE;
}

void
send_buffered_event (gpointer event, gpointer data)
{
    UZBL_UNUSED (data);

    const gchar *message = (const gchar *)event;

    send_event_sockets (uzbl.io->connect_sockets, message);
}

gboolean
line_buffer_io (GIOChannel *gio, GIOCondition condition, gpointer data)
{
    UZBL_UNUSED (condition);

    UzblIOBufferData *io_data = (UzblIOBufferData *)data;

    static const gsize BUFSZ = 256;
    gchar input[BUFSZ];
    gsize read;
    GError *error = NULL;
    gboolean eof = FALSE;

    do {
        GIOStatus status = g_io_channel_read_chars (gio, input, BUFSZ, &read, &error);

        if (status == G_IO_STATUS_ERROR) {
            g_warning ("Error buffering: %s", error->message);
            g_clear_error (&error);

            if (io_data->error_callback) {
                io_data->error_callback(gio, io_data->data);
            }

            return G_SOURCE_REMOVE;
        }

        if (status == G_IO_STATUS_EOF) {
            eof = TRUE;
            break;
        }

        g_string_append_len (io_data->buffer, input, read);
    } while (read);

    gboolean contin = G_SOURCE_CONTINUE;

    char *end;
    do {
        end = strchr (io_data->buffer->str, '\n');
        if (end) {
            GString *line = g_string_new_len (io_data->buffer->str, end - io_data->buffer->str);
            contin = io_data->callback (gio, line, io_data->data);
            g_string_free (line, TRUE);
            GString *sav = io_data->buffer;
            io_data->buffer = g_string_new (end + 1);
            g_string_free (sav, TRUE);
            if (contin == G_SOURCE_REMOVE) {
                return contin;
            }
        }
    } while (end);

    if (eof) {
        if (io_data->error_callback) {
            io_data->error_callback(gio, io_data->data);
        }

        return G_SOURCE_REMOVE;
    }

    return contin;
}

void
free_watcher_data (gpointer data)
{
    UzblIOBufferData *io_data = (UzblIOBufferData *)data;

    g_string_free (io_data->buffer, TRUE);

    g_free (io_data);
}

void
schedule_io_input (gchar *line, UzblIOCallback callback, gpointer data)
{
    if (!line) {
        return;
    }

    remove_trailing_newline (line);

    if (!strprefix (line, "REPLY-")) {
        uzbl_requests_set_reply (line);

        g_free (line);
    } else {
        UzblCommandData *cmd_data = g_malloc (sizeof (UzblCommandData));
        cmd_data->cmd = line;
        cmd_data->info = NULL;
        cmd_data->argv = NULL;
        cmd_data->callback = callback;
        cmd_data->data = data;

        g_async_queue_push (uzbl.io->cmd_q, cmd_data);
    }
}

void
write_stdout (GString *result, gpointer data)
{
    UZBL_UNUSED (data);

    fprintf (stdout, "%s\n", result->str);
    fflush (stdout);
}

void
write_socket (GString *result, gpointer data)
{
    GIOChannel *gio = (GIOChannel *)data;

    g_string_append_c (result, '\n');
    write_to_socket (gio, result->str);

    g_io_channel_unref (gio);
}

void
send_buffered_event_to_socket (gpointer event, gpointer data)
{
    const gchar *message = (const gchar *)event;
    GIOChannel *channel = (GIOChannel *)data;

    write_to_socket (channel, message);
}

void
write_to_socket (GIOChannel *channel, const gchar *message)
{
    GIOStatus ret;
    GError *error = NULL;

    if (!channel->is_writeable) {
        return;
    }

    ret = g_io_channel_write_chars (channel, message, strlen (message),
                                    NULL, &error);
    if (ret == G_IO_STATUS_ERROR) {
        g_warning ("Error writing: %s", error->message);
        g_clear_error (&error);
    } else if (g_io_channel_flush (channel, &error) == G_IO_STATUS_ERROR) {
        g_warning ("Error flushing: %s", error->message);
        g_clear_error (&error);
    }
}

gboolean
control_fifo (GIOChannel *gio, GString *input, gpointer data)
{
    UZBL_UNUSED (gio);
    UZBL_UNUSED (data);

    gchar *ctl_line = g_strdup (input->str);
    schedule_io_input (ctl_line, NULL, NULL);

    return G_SOURCE_CONTINUE;
}

void
add_cmd_source (GIOChannel *gio, const gchar *name, GIOFunc callback, gpointer data)
{
    GSource *source = g_io_create_watch (gio, G_IO_IN | G_IO_HUP);
    g_source_set_name (source, name);

    /* g_io_create_watch uses GIOFunc as its callback function. */
    g_source_set_callback (source, (GSourceFunc)callback, data, NULL);

    g_source_attach (source, uzbl.io->io_ctx);

    g_source_unref (source);
}

gboolean
control_socket (GIOChannel *gio, GIOCondition condition, gpointer data)
{
    UZBL_UNUSED (condition);
    UZBL_UNUSED (data);

    struct sockaddr_un remote;

    unsigned int t = sizeof (remote);
    GIOChannel *iochan;
    int clientsock;

    clientsock = accept (g_io_channel_unix_get_fd (gio),
                         (struct sockaddr *)&remote, &t);

    if ((iochan = g_io_channel_unix_new (clientsock))) {
        g_io_channel_set_encoding (iochan, NULL, NULL);
        add_buffered_cmd_source (iochan, "Uzbl control socket",
            control_client_socket, close_client_socket, uzbl.io->client_sockets);
        g_ptr_array_add (uzbl.io->client_sockets, iochan);
    }

    return G_SOURCE_CONTINUE;
}
