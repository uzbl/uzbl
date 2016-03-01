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

static void
vuzbl_events_send (UzblEventType type, const gchar *custom_event, va_list vargs)
{
    const gchar *event_name = custom_event ? custom_event : event_table[type];
    GString *event = uzbl_comm_vformat ("EVENT", event_name, vargs);

    uzbl_io_send (event->str, FALSE);

    g_string_free (event, TRUE);
}
