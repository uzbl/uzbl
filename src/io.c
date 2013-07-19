#include "io.h"

#include "commands.h"
#include "events.h"
#include "requests.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>

#include "3p/async-queue-source/rb-async-queue-watch.h"

/* =========================== PUBLIC API =========================== */

static void
free_cmd_req (gpointer data);
static void
run_command (gpointer item, gpointer data);
static gpointer
run_io (gpointer data);

void
uzbl_io_init ()
{
    uzbl.state.cmd_q = g_async_queue_new_full (free_cmd_req);

    uzbl_rb_async_queue_watch_new (uzbl.state.cmd_q,
        G_PRIORITY_DEFAULT, run_command,
        NULL, NULL, NULL);

    uzbl.state.io_thread = g_thread_new ("uzbl-io", run_io, NULL);
}

static void
add_cmd_source (GIOChannel *gio, const gchar *name, GIOFunc callback);
static gboolean
control_stdin (GIOChannel *gio, GIOCondition condition, gpointer data);

void
uzbl_io_init_stdin ()
{
    GIOChannel *chan = g_io_channel_unix_new (STDIN_FILENO);
    if (chan) {
        add_cmd_source (chan, "Uzbl stdin watcher", control_stdin);
    } else {
        g_error ("create_stdin: error while opening stdin\n");
    }
}

static gboolean
control_client_socket (GIOChannel *clientchan, GIOCondition, gpointer data);

void
uzbl_io_init_connect_socket ()
{
    int sockfd;
    int replay = 0;
    struct sockaddr_un local;
    GIOChannel *chan;
    gchar **name = NULL;

    if (!uzbl.comm.connect_chan) {
        uzbl.comm.connect_chan = g_ptr_array_new ();
    }

    name = uzbl.state.connect_socket_names;

    while (name && *name) {
        sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
        local.sun_family = AF_UNIX;
        strcpy (local.sun_path, *name);

        if (!connect (sockfd, (struct sockaddr *)&local, sizeof (local))) {
            if ((chan = g_io_channel_unix_new (sockfd))) {
                g_io_channel_set_encoding (chan, NULL, NULL);
                add_cmd_source (chan, "Uzbl connect socket", control_client_socket);
                g_ptr_array_add (uzbl.comm.connect_chan, chan);
                ++replay;
            }
        } else {
            g_warning ("Error connecting to socket: %s\n", *name);
        }
        ++name;
    }

    /* Replay buffered events. */
    if (replay && uzbl.state.event_buffer) {
        uzbl_events_replay_buffer ();
    }
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
attach_fifo (gchar *path);

gboolean
uzbl_io_init_fifo (const gchar *dir)
{
    if (create_dir (dir)) {
        return FALSE;
    }

    if (uzbl.comm.fifo_path) {
        /* We're changing the fifo path, get rid of the old fifo if one exists. */
        if (unlink (uzbl.comm.fifo_path)) {
            g_warning ("Fifo: Can't unlink old fifo at %s\n", uzbl.comm.fifo_path);
        }
        g_free (uzbl.comm.fifo_path);
        uzbl.comm.fifo_path = NULL;
    }

    gchar *path = build_stream_name (UZBL_COMM_FIFO, dir);

    /* If something exists at that path, try to delete it. */
    if (file_exists (path) && unlink (path)) {
        g_warning ("Fifo: Can't unlink old fifo at %s\n", path);
    }

    if (!mkfifo (path, 0666)) {
      if (attach_fifo (path)) {
         return TRUE;
      } else {
         g_warning ("init_fifo: can't attach to %s: %s\n", path, strerror (errno));
      }
    } else {
        g_warning ("init_fifo: can't create %s: %s\n", path, strerror (errno));
    }

    /* If we got this far, there was an error; clean up. */
    g_free (path);
    return FALSE;
}

static gboolean
attach_socket (const gchar *path, struct sockaddr_un *local);

gboolean
uzbl_io_init_socket (const gchar *dir)
{
    if (create_dir (dir)) {
        return FALSE;
    }

    if (uzbl.comm.socket_path) {
        /* Remove an existing socket should one exist. */
        if (unlink (uzbl.comm.socket_path)) {
            g_warning ("init_socket: couldn't unlink socket at %s\n", uzbl.comm.socket_path);
        }
        g_free (uzbl.comm.socket_path);
        uzbl.comm.socket_path = NULL;
    }

    if (*dir == ' ') {
        return FALSE;
    }

    gchar *path = build_stream_name (UZBL_COMM_SOCKET, dir);

    /* If something exists at that path, try to delete it. */
    if (file_exists (path) && unlink (path)) {
        g_warning ("Fifo: Can't unlink old fifo at %s\n", path);
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

/* ===================== HELPER IMPLEMENTATIONS ===================== */

typedef void (*UzblIOCallback)(GString *result, gpointer data);

typedef struct {
    gchar *cmd;
    UzblIOCallback callback;
    gpointer data;
} UzblCommandData;

void
run_command (gpointer item, gpointer data)
{
    UZBL_UNUSED (data);

    UzblCommandData *cmd = (UzblCommandData *)item;

    GString *result = NULL;

    if (cmd->callback) {
        result = g_string_new ("");
    }

    uzbl_commands_run (cmd->cmd, result);

    if (cmd->callback && *result->str) {
        cmd->callback (result, cmd->data);
        g_string_free (result, TRUE);
    }

    free_cmd_req (cmd);
}

gpointer
run_io (gpointer data)
{
    UZBL_UNUSED (data);

    uzbl.state.io_ctx = g_main_context_new ();

    uzbl.state.io_loop = g_main_loop_new (uzbl.state.io_ctx, FALSE);
    g_main_loop_run (uzbl.state.io_loop);
    g_main_loop_unref (uzbl.state.io_loop);

    g_main_context_unref (uzbl.state.io_ctx);
    uzbl.state.io_ctx = NULL;

    return NULL;
}

void
add_cmd_source (GIOChannel *gio, const gchar *name, GIOFunc callback)
{
    GSource *source = g_io_create_watch (gio, G_IO_IN | G_IO_HUP);
    g_source_set_name (source, name);

    /* Why does casting callback into a GSourceFunc work? GIOFunc takes 3
     * parameters while GSourceFunc takes 1. However, this is what is done in
     * g_io_add_watch, we just want to attach to a different context. */
    g_source_set_callback (source, (GSourceFunc)callback, NULL, NULL);

    g_source_attach (source, uzbl.state.io_ctx);
}

void
free_cmd_req (gpointer data)
{
    UzblCommandData *cmd = (UzblCommandData *)data;

    g_free (cmd->cmd);
    g_free (cmd);
}

void
run_queue_thread (GTask *task, gpointer object, gpointer data, GCancellable *cancellable)
{
    UZBL_UNUSED (task);
    UZBL_UNUSED (object);
    UZBL_UNUSED (data);
    UZBL_UNUSED (cancellable);
}

static void
handle_command (gchar *line, UzblIOCallback callback, gpointer data);
static void
write_stdout (GString *result, gpointer data);

gboolean
control_stdin (GIOChannel *gio, GIOCondition condition, gpointer data)
{
    UZBL_UNUSED (condition);
    UZBL_UNUSED (data);

    gchar *ctl_line = NULL;
    GIOStatus ret;

    ret = g_io_channel_read_line (gio, &ctl_line, NULL, NULL, NULL);
    if ((ret == G_IO_STATUS_ERROR) || (ret == G_IO_STATUS_EOF)) {
        return FALSE;
    }

    handle_command (ctl_line, write_stdout, NULL);

    return TRUE;
}

static gboolean
remove_socket_from_array (GIOChannel *chan);
static void
write_socket (GString *result, gpointer data);

gboolean
control_client_socket (GIOChannel *clientchan, GIOCondition condition, gpointer data)
{
    UZBL_UNUSED (condition);
    UZBL_UNUSED (data);

    char *ctl_line;
    GError *error = NULL;
    GIOStatus ret;
    gsize len;

    ret = g_io_channel_read_line (clientchan, &ctl_line, &len, NULL, &error);
    if (ret == G_IO_STATUS_ERROR) {
        g_warning ("Error reading: %s", error->message);
        g_clear_error (&error);
        ret = g_io_channel_shutdown (clientchan, TRUE, &error);
        remove_socket_from_array (clientchan);
        if (ret == G_IO_STATUS_ERROR) {
            g_warning ("Error closing: %s", error->message);
            g_clear_error (&error);
        }
        return FALSE;
    } else if (ret == G_IO_STATUS_EOF) {
        /* Shutdown and remove channel watch from main loop. */
        ret = g_io_channel_shutdown (clientchan, TRUE, &error);
        remove_socket_from_array (clientchan);
        if (ret == G_IO_STATUS_ERROR) {
            g_warning ("Error closing: %s", error->message);
            g_clear_error (&error);
        }
        return FALSE;
    }

    g_io_channel_ref (clientchan);
    handle_command (ctl_line, write_socket, clientchan);

    return TRUE;
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

#define check_mkdir(dir, mode)                                  \
    if (mkdir (work_path, 0700)) {                              \
        switch (errno) {                                        \
            case EEXIST:                                        \
                break;                                          \
            default:                                            \
                perror ("Failed to create socket or fifo dir"); \
                return EXIT_FAILURE;                            \
        }                                                       \
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
control_fifo (GIOChannel *gio, GIOCondition condition, gpointer data);

gboolean
attach_fifo (gchar *path)
{
    GError *error = NULL;
    /* We don't really need to write to the file, but if we open the file as
     * 'r' we will block here, waiting for a writer to open the file. */
    GIOChannel *chan = g_io_channel_new_file (path, "r+", &error);
    if (chan) {
        add_cmd_source (chan, "Uzbl main fifo", control_fifo);

        uzbl_events_send (FIFO_SET, NULL,
            TYPE_STR, path,
            NULL);
        uzbl.comm.fifo_path = path;
        /* TODO: Collect all environment settings into one place. */
        g_setenv ("UZBL_FIFO", uzbl.comm.fifo_path, TRUE);
        return TRUE;
    } else {
        g_warning ("attach_fifo: can't open: %s\n", error->message);
    }

    g_error_free (error);
    return FALSE;
}

static gboolean
control_socket (GIOChannel *chan, GIOCondition condition, gpointer data);

gboolean
attach_socket (const gchar *path, struct sockaddr_un *local)
{
    GIOChannel *chan = NULL;
    int sock = socket (AF_UNIX, SOCK_STREAM, 0);

    if (!bind (sock, (struct sockaddr *)local, sizeof (*local))) {
        uzbl_debug ("init_socket: opened in %s\n", path);

        if (listen (sock, 5)) {
            g_warning ("attach_socket: could not listen on %s: %s\n", path, strerror (errno));
        }

        if ((chan = g_io_channel_unix_new (sock))) {
            add_cmd_source (chan, "Uzbl main socket", control_socket);

            uzbl.comm.socket_path = g_strdup (path);
            uzbl_events_send (SOCKET_SET, NULL,
                TYPE_STR, uzbl.comm.socket_path,
                NULL);
            /* TODO: Collect all environment settings into one place. */
            g_setenv ("UZBL_SOCKET", uzbl.comm.socket_path, TRUE);
            return TRUE;
        }
    } else {
        g_warning ("attach_socket: could not bind to %s: %s\n", path, strerror (errno));
    }

    return FALSE;
}

void
handle_command (gchar *line, UzblIOCallback callback, gpointer data)
{
    if (!line) {
        return;
    }

    remove_trailing_newline (line);

    if (!strprefix (line, "REPLY-")) {
        g_mutex_lock (&uzbl.state.reply_buffer_lock);
        g_ptr_array_add (uzbl.state.reply_buffer, line);
        g_cond_broadcast (&uzbl.state.reply_buffer_cond);
        g_mutex_unlock (&uzbl.state.reply_buffer_lock);
    } else {
        UzblCommandData *cmd_data = g_malloc (sizeof (UzblCommandData));
        cmd_data->cmd = line;
        cmd_data->callback = callback;
        cmd_data->data = data;

        g_async_queue_push (uzbl.state.cmd_q, cmd_data);
    }
}

void
write_stdout (GString *result, gpointer data)
{
    UZBL_UNUSED (data);

    puts (result->str);
}

gboolean
remove_socket_from_array (GIOChannel *chan)
{
    gboolean ret = 0;

    ret = g_ptr_array_remove_fast (uzbl.comm.connect_chan, chan);
    if (!ret) {
        ret = g_ptr_array_remove_fast (uzbl.comm.client_chan, chan);
    }

    if (ret) {
        g_io_channel_unref (chan);
    }

    return ret;
}

void
write_socket (GString *result, gpointer data)
{
    GIOStatus ret;
    gsize len;
    GError *error = NULL;

    GIOChannel *gio = (GIOChannel *)data;

    g_string_append_c (result, '\n');
    ret = g_io_channel_write_chars (gio, result->str, result->len,
                                    &len, &error);
    if (ret == G_IO_STATUS_ERROR) {
        g_warning ("Error writing: %s", error->message);
        g_clear_error (&error);
    }
    if (g_io_channel_flush (gio, &error) == G_IO_STATUS_ERROR) {
        g_warning ("Error flushing: %s", error->message);
        g_clear_error (&error);
    }

    g_io_channel_unref (gio);
}

gboolean
control_fifo (GIOChannel *gio, GIOCondition condition, gpointer data)
{
    UZBL_UNUSED (data);

    gchar *ctl_line;
    GIOStatus ret;
    GError *err = NULL;

    if (condition & G_IO_HUP) {
        g_error ("Fifo: Read end of pipe died!\n");
    }

    if (!gio) {
        g_error ("Fifo: GIOChannel broke\n");
    }

    ret = g_io_channel_read_line (gio, &ctl_line, NULL, NULL, &err);
    if (ret == G_IO_STATUS_ERROR) {
        g_error ("Fifo: Error reading: %s\n", err->message);
        g_error_free (err);
    }

    handle_command (ctl_line, NULL, NULL);

    return TRUE;
}

gboolean
control_socket (GIOChannel *chan, GIOCondition condition, gpointer data)
{
    UZBL_UNUSED (condition);
    UZBL_UNUSED (data);

    struct sockaddr_un remote;

    unsigned int t = sizeof (remote);
    GIOChannel *iochan;
    int clientsock;

    clientsock = accept (g_io_channel_unix_get_fd (chan),
                         (struct sockaddr *)&remote, &t);

    if (!uzbl.comm.client_chan) {
        uzbl.comm.client_chan = g_ptr_array_new ();
    }

    if ((iochan = g_io_channel_unix_new (clientsock))) {
        g_io_channel_set_encoding (iochan, NULL, NULL);
        add_cmd_source (iochan, "Uzbl control socket", control_client_socket);
        g_ptr_array_add (uzbl.comm.client_chan, iochan);
    }

    return TRUE;
}
