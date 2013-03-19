#include "io.h"

#include "events.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"

#include <unistd.h>

static gboolean
control_stdin (GIOChannel *gio, GIOCondition condition);

void
uzbl_io_init_stdin ()
{
    GIOChannel *chan = g_io_channel_unix_new (STDIN_FILENO);
    if (chan) {
        if (!g_io_add_watch (chan, G_IO_IN | G_IO_HUP, (GIOFunc)control_stdin, NULL)) {
            g_error ("create_stdin: could not add watch\n");
        } else {
            uzbl_debug ("create_stdin: watch added successfully\n");
        }
    } else {
        g_error ("create_stdin: error while opening stdin\n");
    }
}

static gboolean
control_client_socket (GIOChannel *clientchan);

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
                g_io_add_watch (chan, G_IO_IN|G_IO_HUP,
                                (GIOFunc)control_client_socket, chan);
                g_ptr_array_add (uzbl.comm.connect_chan, (gpointer)chan);
                ++replay;
            }
        } else {
            g_warning("Error connecting to socket: %s\n", *name);
        }
        ++name;
    }

    /* Replay buffered events. */
    if (replay && uzbl.state.event_buffer) {
        uzbl_events_replay_buffer ();
    }
}

static gboolean
remove_socket_from_array (GIOChannel *chan);

gboolean
control_client_socket (GIOChannel *clientchan) {
    char *ctl_line;
    GString *result = g_string_new ("");
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

    if (ctl_line) {
        parse_cmd_line (ctl_line, result);
        g_string_append_c (result, '\n');
        ret = g_io_channel_write_chars (clientchan, result->str, result->len,
                                        &len, &error);
        if (ret == G_IO_STATUS_ERROR) {
            g_warning ("Error writing: %s", error->message);
            g_clear_error (&error);
        }
        if (g_io_channel_flush (clientchan, &error) == G_IO_STATUS_ERROR) {
            g_warning ("Error flushing: %s", error->message);
            g_clear_error (&error);
        }
    }

    g_string_free (result, TRUE);
    g_free (ctl_line);
    return TRUE;
}

gboolean
remove_socket_from_array (GIOChannel *chan)
{
    gboolean ret = 0;

    ret = g_ptr_array_remove_fast (uzbl.comm.connect_chan, chan);
    if (!ret) {
        ret = g_ptr_array_remove_fast (uzbl.comm.client_chan, chan);
    }

    if(ret) {
        g_io_channel_unref (chan);
    }

    return ret;
}

gboolean
control_stdin (GIOChannel *gio, GIOCondition condition)
{
    UZBL_UNUSED (condition);

    gchar *ctl_line = NULL;
    GIOStatus ret;

    ret = g_io_channel_read_line (gio, &ctl_line, NULL, NULL, NULL);
    if ((ret == G_IO_STATUS_ERROR) || (ret == G_IO_STATUS_EOF)) {
        return FALSE;
    }

    GString *result = g_string_new ("");

    parse_cmd_line (ctl_line, result);
    g_free (ctl_line);

    if (*result->str) {
        puts (result->str);
    }
    g_string_free (result, TRUE);

    return TRUE;
}

typedef enum {
    UZBL_COMM_FIFO,
    UZBL_COMM_SOCKET
} UzblCommType;

static gchar*
build_stream_name (UzblCommType type, const gchar* dir);
static gboolean
attach_fifo (gchar *path);

gboolean
uzbl_io_init_fifo (const gchar *dir)
{
    if (uzbl.comm.fifo_path) {
        /* We're changing the fifo path, get rid of the old fifo if one exists. */
        if (unlink (uzbl.comm.fifo_path)) {
            g_warning ("Fifo: Can't unlink old fifo at %s\n", uzbl.comm.fifo_path);
        }
        g_free (uzbl.comm.fifo_path);
        uzbl.comm.fifo_path = NULL;
    }

    gchar *path = build_stream_name (UZBL_COMM_FIFO, dir);

    /* if something exists at that path, try to delete it. */
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

gchar*
build_stream_name (UzblCommType type, const gchar* dir) {
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

static gboolean
control_fifo(GIOChannel *gio, GIOCondition condition);

gboolean
attach_fifo (gchar *path)
{
    GError *error = NULL;
    /* We don't really need to write to the file, but if we open the file as
     * 'r' we will block here, waiting for a writer to open the file. */
    GIOChannel *chan = g_io_channel_new_file (path, "r+", &error);
    if (chan) {
        if (g_io_add_watch (chan, G_IO_IN | G_IO_HUP, (GIOFunc)control_fifo, NULL)) {
            uzbl_debug ("attach_fifo: created successfully as %s\n", path);

            uzbl_events_send (FIFO_SET, NULL,
                TYPE_STR, path,
                NULL);
            uzbl.comm.fifo_path = path;
            /* TODO: Collect all environment settings into one place. */
            g_setenv ("UZBL_FIFO", uzbl.comm.fifo_path, TRUE);
            return TRUE;
        } else {
            g_warning ("attach_fifo: could not add watch on %s\n", path);
        }
    } else {
        g_warning ("attach_fifo: can't open: %s\n", error->message);
    }

    g_error_free (error);
    return FALSE;
}

gboolean
control_fifo(GIOChannel *gio, GIOCondition condition)
{
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

    parse_cmd_line (ctl_line, NULL);
    g_free (ctl_line);

    return TRUE;
}

static gboolean
attach_socket (gchar *path, struct sockaddr_un *local);

gboolean
uzbl_io_init_socket (const gchar *dir)
{
    if (uzbl.comm.socket_path) {
        /* remove an existing socket should one exist */
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

    if (attach_socket (path, &local)) {
        return TRUE;
    } else {
        g_warning("init_socket: can't attach to %s: %s\n", path, strerror (errno));
    }

    /* If we got this far, there was an error; clean up. */
    g_free (path);
    return FALSE;
}

static gboolean
control_socket (GIOChannel *chan);

static gboolean
attach_socket(gchar *path, struct sockaddr_un *local)
{
    GIOChannel *chan = NULL;
    int sock = socket (AF_UNIX, SOCK_STREAM, 0);

    if (!bind (sock, (struct sockaddr *)local, sizeof (*local))) {
        uzbl_debug ("init_socket: opened in %s\n", path);

        if (listen (sock, 5)) {
            g_warning ("attach_socket: could not listen on %s: %s\n", path, strerror (errno));
        }

        if ((chan = g_io_channel_unix_new (sock))) {
            g_io_add_watch (chan, G_IO_IN | G_IO_HUP, (GIOFunc)control_socket, chan);

            uzbl.comm.socket_path = path;
            uzbl_events_send (SOCKET_SET, NULL,
                TYPE_STR, path,
                NULL);
            /* TODO: Collect all environment settings into one place. */
            g_setenv("UZBL_SOCKET", uzbl.comm.socket_path, TRUE);
            return TRUE;
        }
    } else {
        g_warning ("attach_socket: could not bind to %s: %s\n", path, strerror (errno));
    }

    return FALSE;
}

gboolean
control_socket (GIOChannel *chan)
{
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
        g_io_add_watch (iochan, G_IO_IN|G_IO_HUP,
                        (GIOFunc)control_client_socket, iochan);
        g_ptr_array_add (uzbl.comm.client_chan, (gpointer)iochan);
    }
    return TRUE;
}
