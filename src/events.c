/*
 ** Uzbl event routines
 ** (c) 2009 by Robert Manea
*/

#include "uzbl-core.h"
#include "events.h"

UzblCore uzbl;

/* Event id to name mapping
 * Event names must be in the same
 * order as in 'enum event_type'
 *
 * TODO: Add more useful events
*/
const char *event_table[LAST_EVENT] = {
     "LOAD_START"       ,
     "LOAD_COMMIT"      ,
     "LOAD_FINISH"      ,
     "LOAD_ERROR"       ,
     "REQUEST_STARTING" ,
     "KEY_PRESS"        ,
     "KEY_RELEASE"      ,
     "COMMAND_EXECUTED" ,
     "LINK_HOVER"       ,
     "TITLE_CHANGED"    ,
     "GEOMETRY_CHANGED" ,
     "WEBINSPECTOR"     ,
     "NEW_WINDOW"       ,
     "SELECTION_CHANGED",
     "VARIABLE_SET"     ,
     "FIFO_SET"         ,
     "SOCKET_SET"       ,
     "INSTANCE_START"   ,
     "INSTANCE_EXIT"    ,
     "LOAD_PROGRESS"    ,
     "LINK_UNHOVER"     ,
     "FORM_ACTIVE"      ,
     "ROOT_ACTIVE"      ,
     "FOCUS_LOST"       ,
     "FOCUS_GAINED"     ,
     "FILE_INCLUDED"    ,
     "PLUG_CREATED"     ,
     "COMMAND_ERROR"    ,
     "BUILTINS"         ,
     "PTR_MOVE"         ,
     "SCROLL_VERT"      ,
     "SCROLL_HORIZ"     ,
     "DOWNLOAD_STARTED" ,
     "DOWNLOAD_PROGRESS",
     "DOWNLOAD_COMPLETE",
     "ADD_COOKIE"       ,
     "DELETE_COOKIE"
};

void
event_buffer_timeout(guint sec) {
    struct itimerval t;
    memset(&t, 0, sizeof t);
    t.it_value.tv_sec = sec;
    t.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &t, NULL);
}

static void
send_event_sockets(GPtrArray *sockets, GString *msg) {
    GError *error = NULL;
    GIOStatus ret;
    gsize len;
    guint i=0;

    while(i < sockets->len) {
        GIOChannel *gio = g_ptr_array_index(sockets, i++);

        if(gio && gio->is_writeable && msg) {
            ret = g_io_channel_write_chars (gio,
                    msg->str, msg->len,
                    &len, &error);

            if (ret == G_IO_STATUS_ERROR) {
                g_warning ("Error sending event to socket: %s", error->message);
                g_clear_error (&error);
            } else {
                if (g_io_channel_flush(gio, &error) == G_IO_STATUS_ERROR) {
                    g_warning ("Error flushing: %s", error->message);
                    g_clear_error (&error);
                }
            }
        }
    }
}

static void
replay_buffered_events() {
    guint i = 0;

    event_buffer_timeout(0);

    /* replay buffered events */
    while(i < uzbl.state.event_buffer->len) {
        GString *tmp = g_ptr_array_index(uzbl.state.event_buffer, i++);
        send_event_sockets(uzbl.comm.connect_chan, tmp);
        g_string_free(tmp, TRUE);
    }

    g_ptr_array_free(uzbl.state.event_buffer, TRUE);
    uzbl.state.event_buffer = NULL;
}

void
send_event_socket(GString *msg) {
    /* write to all --connect-socket sockets */
    if(uzbl.comm.connect_chan) {
        send_event_sockets(uzbl.comm.connect_chan, msg);
        if(uzbl.state.event_buffer)
            replay_buffered_events();
    }
    /* buffer events until a socket is set and connected
    * or a timeout is encountered
    */
    else {
        if(!uzbl.state.event_buffer)
            uzbl.state.event_buffer = g_ptr_array_new();
        g_ptr_array_add(uzbl.state.event_buffer, (gpointer)g_string_new(msg->str));
    }

    /* write to all client sockets */
    if(msg && uzbl.comm.client_chan) {
        send_event_sockets(uzbl.comm.client_chan, msg);
    }
}

void
send_event_stdout(GString *msg) {
    printf("%s", msg->str);
    fflush(stdout);
}

/*
 * build event string and send over the supported interfaces
 * custom_event == NULL indicates an internal event
*/
void
send_event(int type, const gchar *details, const gchar *custom_event) {
    GString *event_message = g_string_new("");

    /* check for custom events */
    if(custom_event) {
        g_string_printf(event_message, "EVENT [%s] %s %s\n",
                uzbl.state.instance_name, custom_event, details);
    }
    /* check wether we support the internal event */
    else if(type < LAST_EVENT) {
        g_string_printf(event_message, "EVENT [%s] %s %s\n",
                uzbl.state.instance_name, event_table[type], details);
    }

    if(event_message->str) {
        if(uzbl.state.events_stdout)
            send_event_stdout(event_message);
        send_event_socket(event_message);

        g_string_free(event_message, TRUE);
    }
}

/* Transform gdk key events to our own events */
void
key_to_event(guint keyval, gint mode) {
    gchar ucs[7];
    gint ulen;
    guint32 ukval = gdk_keyval_to_unicode(keyval);

    /* check for printable unicode char */
    /* TODO: Pass the keyvals through a GtkIMContext so that
     *       we also get combining chars right
    */
    if(g_unichar_isgraph(ukval)) {
        ulen = g_unichar_to_utf8(ukval, ucs);
        ucs[ulen] = 0;

        send_event(mode == GDK_KEY_PRESS ? KEY_PRESS : KEY_RELEASE,
                ucs, NULL);
    }
    /* send keysym for non-printable chars */
    else {
        send_event(mode == GDK_KEY_PRESS ? KEY_PRESS : KEY_RELEASE,
                gdk_keyval_name(keyval), NULL);
    }

}
