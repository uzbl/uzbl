/* Uzbl event routines 
 * (c) 2009 by Robert Manea
*/

#include "uzbl-core.h"
#include "uzbl-events.h"

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
     "KEY_PRESS"        ,
     "KEY_RELEASE"      ,
     "DOWNLOAD_REQUEST" ,
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
     "LINK_UNHOVER"
};

void
event_buffer_timeout(guint sec) {
    struct itimerval t;
    memset(&t, 0, sizeof t);
    t.it_value.tv_sec = sec;
    t.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &t, NULL);
}


void
send_event_socket(GString *msg) {
    GError *error = NULL;
    GString *tmp;
    GIOStatus ret = 0;
    gsize len;
    guint i=0;

    if(uzbl.comm.socket_path &&
            uzbl.comm.clientchan  &&
            uzbl.comm.clientchan->is_writeable) {

        if(uzbl.state.event_buffer) {
            event_buffer_timeout(0);

            while(i < uzbl.state.event_buffer->len) {
                tmp = g_ptr_array_index(uzbl.state.event_buffer, i++);
                ret = g_io_channel_write_chars (uzbl.comm.clientchan,
                        tmp->str, tmp->len,
                        &len, &error);
                /* is g_ptr_array_free(uzbl.state.event_buffer, TRUE) enough? */
                //g_string_free(tmp, TRUE);
                if (ret == G_IO_STATUS_ERROR) {
                    g_warning ("Error sending event to socket: %s", error->message);
                }
                g_io_channel_flush(uzbl.comm.clientchan, &error);
            }
            g_ptr_array_free(uzbl.state.event_buffer, TRUE);
            uzbl.state.event_buffer = NULL;
        }
        if(msg) {
            while(!ret ||
                  ret == G_IO_STATUS_AGAIN) {
                ret = g_io_channel_write_chars (uzbl.comm.clientchan,
                        msg->str, msg->len,
                        &len, &error);
            }

            if (ret == G_IO_STATUS_ERROR) {
                g_warning ("Error sending event to socket: %s", error->message);
            }
            g_io_channel_flush(uzbl.comm.clientchan, &error);
        }
    }
    /* buffer events until a socket is set and connected
     * or a timeout is encountered
    */
    else {
        if(!uzbl.state.event_buffer)
            uzbl.state.event_buffer = g_ptr_array_new();
        g_ptr_array_add(uzbl.state.event_buffer, (gpointer)g_string_new(msg->str));
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
    gchar *buf, *p_val = NULL;

    /* expand shell vars */
    if(details) {
        buf = g_strdup(details);
        p_val = parseenv(g_strdup(buf ? g_strchug(buf) : " "));
        g_free(buf);
    }

    /* check for custom events */
    if(custom_event) {
        g_string_printf(event_message, "EVENT [%s] %s %s\n",
                uzbl.state.instance_name, custom_event, p_val);
    }
    /* check wether we support the internal event */
    else if(type < LAST_EVENT) {
        g_string_printf(event_message, "EVENT [%s] %s %s\n",
                uzbl.state.instance_name, event_table[type], p_val);
    }

    if(event_message->str) {
        /* TODO: a means to select the interface to which events are sent */
        send_event_stdout(event_message);
        send_event_socket(event_message);

        g_string_free(event_message, TRUE);
    }
    g_free(p_val);
}
