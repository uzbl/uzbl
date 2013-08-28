#include "events.h"

#include "type.h"
#include "util.h"
#include "uzbl-core.h"
#include "variables.h"

#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

const char *event_table[] = {
/* TODO: Add UZBL_ prefix to built-in events? */
#define event_string(evt) #evt

    UZBL_EVENTS (event_string)

#undef event_string
};

typedef GString UzblEvent;

/* =========================== PUBLIC API =========================== */

typedef void UzblSignalFunc (int sig);

static UzblSignalFunc *
setup_signal (int sig, UzblSignalFunc *shandler);
static void
clear_event_buffer (int sig);
static int
set_event_timeout (guint sec);

int
uzbl_events_init ()
{
    if (setup_signal (SIGALRM, clear_event_buffer) == SIG_ERR) {
        fprintf (stderr, "uzbl: error hooking %d: %s\n", SIGALRM, strerror (errno));
        return -1;
    }
    set_event_timeout (10);

    return 0;
}

static void
send_event_sockets (GPtrArray *sockets, GString *msg);

void
uzbl_events_replay_buffer ()
{
    guint i = 0;

    set_event_timeout (0);

    /* Replay buffered events. */
    while (i < uzbl.state.event_buffer->len) {
        GString *tmp = g_ptr_array_index (uzbl.state.event_buffer, i++);
        send_event_sockets (uzbl.comm.connect_chan, tmp);
    }

    g_ptr_array_free (uzbl.state.event_buffer, TRUE);
    uzbl.state.event_buffer = NULL;
}

static void
vuzbl_events_send (UzblEventType type, const gchar *custom_event, va_list vargs);

void
uzbl_events_send (UzblEventType type, const gchar *custom_event, ...)
{
    va_list vargs;
    va_list vacopy;

    va_start (vargs, custom_event);
    va_copy (vacopy, vargs);

    vuzbl_events_send (type, custom_event, vacopy);

    va_end (vacopy);
    va_end (vargs);
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

UzblSignalFunc *
setup_signal (int sig, UzblSignalFunc *shandler)
{
    struct sigaction new_handler;

    new_handler.sa_handler = shandler;
    sigemptyset (&new_handler.sa_mask);
    new_handler.sa_flags = 0;

    /* TODO: Save the old handler? */
    if (sigaction (sig, &new_handler, NULL)) {
        return SIG_ERR;
    }

    return NULL;
}

void
clear_event_buffer (int sig)
{
    UZBL_UNUSED (sig);

    if (uzbl.state.event_buffer) {
        g_ptr_array_free (uzbl.state.event_buffer, TRUE);
        uzbl.state.event_buffer = NULL;
    }
}

int
set_event_timeout (guint sec)
{
    struct itimerval t;
    memset (&t, 0, sizeof (t));

    t.it_value.tv_sec = sec;
    t.it_value.tv_usec = 0;

    return setitimer (ITIMER_REAL, &t, NULL);
}

void
send_event_sockets (GPtrArray *sockets, GString *msg)
{
    GError *error = NULL;
    GIOStatus ret;
    gsize len;
    guint i = 0;

    while (i < sockets->len) {
        GIOChannel *gio = g_ptr_array_index (sockets, i++);

        if (gio && gio->is_writeable && msg) {
            ret = g_io_channel_write_chars (gio,
                    msg->str, msg->len,
                    &len, &error);

            if (ret == G_IO_STATUS_ERROR) {
                g_warning ("Error sending event to socket: %s", error->message);
                g_clear_error (&error);
            } else {
                if (g_io_channel_flush (gio, &error) == G_IO_STATUS_ERROR) {
                    g_warning ("Error flushing: %s", error->message);
                    g_clear_error (&error);
                }
            }
        }
    }
}

static UzblEvent *
vformat_event (UzblEventType type, const gchar *custom_event, va_list vargs);
static void
send_formatted_event (UzblEvent *event);

void
vuzbl_events_send (UzblEventType type, const gchar *custom_event, va_list vargs)
{
    UzblEvent *event = vformat_event (type, custom_event, vargs);

    send_formatted_event (event);

    g_string_free (event, TRUE);
}

static GString *
append_escaped (GString *dest, const gchar *src);

UzblEvent *
vformat_event (UzblEventType type, const gchar *custom_event, va_list vargs)
{
    GString *event_message = g_string_sized_new (512);
    const gchar *event = custom_event ? custom_event : event_table[type];
    char *str;

    int next;
    g_string_printf (event_message, "EVENT [%s] %s", uzbl.state.instance_name, event);

    while ((next = va_arg (vargs, int))) {
        g_string_append_c (event_message, ' ');
        switch (next) {
            case TYPE_INT:
                g_string_append_printf (event_message, "%d", va_arg (vargs, int));
                break;
            case TYPE_ULL:
                g_string_append_printf (event_message, "%llu", va_arg (vargs, unsigned long long));
                break;
            case TYPE_STR:
                /* A string that needs to be escaped. */
                g_string_append_c (event_message, '\'');
                append_escaped (event_message, va_arg (vargs, char *));
                g_string_append_c (event_message, '\'');
                break;
            case TYPE_FORMATTEDSTR:
                /* A string has already been escaped. */
                g_string_append (event_message, va_arg (vargs, char *));
                break;
            case TYPE_STR_ARRAY:
            {
                GArray *a = va_arg (vargs, GArray *);
                const char *p;
                int i = 0;

                while ((p = argv_idx (a, i++))) {
                    if (i) {
                        g_string_append_c (event_message, ' ');
                    }
                    g_string_append_c (event_message, '\'');
                    append_escaped (event_message, p);
                    g_string_append_c (event_message, '\'');
                }
                break;
            }
            case TYPE_NAME:
                str = va_arg (vargs, char *);
                g_assert (uzbl_variables_is_valid (str));
                g_string_append (event_message, str);
                break;
            case TYPE_FLOAT:
            {
                /* Make sure the formatted double fits in the buffer. */
                if (event_message->allocated_len - event_message->len < G_ASCII_DTOSTR_BUF_SIZE) {
                    g_string_set_size (event_message, event_message->len + G_ASCII_DTOSTR_BUF_SIZE);
                }

                /* Format in C locale. */
                char *tmp = g_ascii_formatd (
                    event_message->str + event_message->len,
                    event_message->allocated_len - event_message->len,
                    "%.2f", va_arg (vargs, double)); /* ‘float’ is promoted to ‘double’ when passed through ‘...’ */
                event_message->len += strlen (tmp);
                break;
            }
        }
    }

    return (UzblEvent *)event_message;
}

static void
send_event_stdout (GString *msg);
static void
send_event_socket (GString *msg);

void
send_formatted_event (UzblEvent *event)
{
    if (!event) {
        return;
    }

    /* An event string is not supposed to contain newlines as it will be
     * interpreted as two events. */
    if (!strchr (event->str, '\n')) {
        g_string_append_c (event, '\n');

        if (uzbl.state.events_stdout) {
            send_event_stdout (event);
        }
        send_event_socket (event);
    }
}

GString *
append_escaped (GString *dest, const gchar *src)
{
    g_assert (dest);
    g_assert (src);

    /* Hint that we are going to append another string. */
    int oldlen = dest->len;
    g_string_set_size (dest, dest->len + strlen (src) * 2);
    g_string_truncate (dest, oldlen);

    /* Append src char by char with baddies escaped. */
    for (const gchar *p = src; *p; ++p) {
        switch (*p) {
            case '\\':
                g_string_append (dest, "\\\\");
                break;
            case '\'':
                g_string_append (dest, "\\'");
                break;
            case '\n':
                g_string_append (dest, "\\n");
                break;
            default:
                g_string_append_c (dest, *p);
                break;
        }
    }

    return dest;
}

void
send_event_stdout (GString *msg)
{
    fprintf (stdout, "%s", msg->str);
    fflush (stdout);
}

static void
free_event_string (gpointer data);

void
send_event_socket (GString *msg)
{
    if (uzbl.comm.connect_chan) {
        /* Write to all --connect-socket sockets. */
        send_event_sockets (uzbl.comm.connect_chan, msg);
        if (uzbl.state.event_buffer) {
            uzbl_events_replay_buffer ();
        }
    } else {
        /* Buffer events until a socket is set and connected or a timeout is
         * encountered. */
        if (!uzbl.state.event_buffer) {
            uzbl.state.event_buffer = g_ptr_array_new ();
            g_ptr_array_set_free_func (uzbl.state.event_buffer, free_event_string);
        }
        g_ptr_array_add (uzbl.state.event_buffer, (gpointer)g_string_new (msg->str));
    }

    /* Write to all client sockets. */
    if (msg && uzbl.comm.client_chan) {
        send_event_sockets (uzbl.comm.client_chan, msg);
    }
}

void
free_event_string (gpointer data)
{
    GString *str = (GString *)data;

    g_string_free (str, TRUE);
}
