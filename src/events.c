#include "events.h"

#include "comm.h"
#include "io.h"
#include "util.h"
#include "uzbl-core.h"

#include <string.h>

const char *event_table[] = {
/* TODO: Add UZBL_ prefix to built-in events? */
#define event_string(evt) #evt

    UZBL_EVENTS (event_string)

#undef event_string
};

/* =========================== PUBLIC API =========================== */

void
uzbl_events_init ()
{
}

void
uzbl_events_free ()
{
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

void
send_event_sockets (GPtrArray *sockets, GString *msg)
{
    GError *err = NULL;
    GIOStatus ret;
    gsize len;
    guint i = 0;

    if (!msg) {
        return;
    }

    while (i < sockets->len) {
        GIOChannel *gio = g_ptr_array_index (sockets, i++);

        if (!gio || !gio->is_writeable) {
            continue;
        }

        ret = g_io_channel_write_chars (gio,
            msg->str, msg->len,
            &len, &err);

        if (ret == G_IO_STATUS_ERROR) {
            g_warning ("Error sending event to socket: %s", err->message);
            g_clear_error (&err);
        } else if (g_io_channel_flush (gio, &err) == G_IO_STATUS_ERROR) {
            g_warning ("Error flushing: %s", err->message);
            g_clear_error (&err);
        }
    }
}

void
vuzbl_events_send (UzblEventType type, const gchar *custom_event, va_list vargs)
{
    const gchar *event_name = custom_event ? custom_event : event_table[type];
    GString *event = uzbl_comm_vformat ("EVENT", event_name, vargs);

    uzbl_io_send (event->str, FALSE);

    g_string_free (event, TRUE);
}
