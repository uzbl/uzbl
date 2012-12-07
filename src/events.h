/*
 ** Uzbl event routines
 ** (c) 2009 by Robert Manea
*/

#ifndef __EVENTS__
#define __EVENTS__

#include <glib.h>
#include <stdarg.h>

/* Event system */
enum event_type {
    LOAD_START, LOAD_COMMIT, LOAD_FINISH, LOAD_ERROR,
    REQUEST_QUEUED, REQUEST_STARTING, REQUEST_FINISHED,
    KEY_PRESS, KEY_RELEASE, MOD_PRESS, MOD_RELEASE,
    COMMAND_EXECUTED,
    LINK_HOVER, TITLE_CHANGED, GEOMETRY_CHANGED,
    WEBINSPECTOR, NEW_WINDOW, CLOSE_WINDOW, SELECTION_CHANGED,
    VARIABLE_SET, FIFO_SET, SOCKET_SET,
    INSTANCE_START, INSTANCE_EXIT, LOAD_PROGRESS,
    LINK_UNHOVER, FORM_ACTIVE, ROOT_ACTIVE,
    FOCUS_LOST, FOCUS_GAINED, FILE_INCLUDED,
    PLUG_CREATED, COMMAND_ERROR, BUILTINS,
    SCROLL_VERT, SCROLL_HORIZ,
    DOWNLOAD_STARTED, DOWNLOAD_PROGRESS, DOWNLOAD_COMPLETE,
    ADD_COOKIE, DELETE_COOKIE,
    FOCUS_ELEMENT, BLUR_ELEMENT,
    AUTHENTICATE,

    /* must be last entry */
    LAST_EVENT
};

typedef struct _Event Event;
struct _Event;

void
event_buffer_timeout(guint sec);

void
replay_buffered_events();

/*
 * build event string
 */
Event *
format_event(int type, const gchar *custom_event, ...) G_GNUC_NULL_TERMINATED;

Event *
vformat_event(int type, const gchar *custom_event, va_list vargs);

/*
 * send a already formatted event string over the supported interfaces.
 * returned event string should be freed by `event_free`
 */
void
send_formatted_event(const Event *event);

/*
 * frees a event string
 */
void
event_free(Event *event);

/*
 * build event string and send over the supported interfaces
 * this is the same as calling `format_event` and then `send_formatted_event`
 */
void
send_event(int type, const gchar *custom_event, ...) G_GNUC_NULL_TERMINATED;

void
vsend_event(int type, const gchar *custom_event, va_list vargs);

gchar *
get_modifier_mask(guint state);

void
key_to_event(guint keyval, guint state, guint is_modifier, gint mode);

void
button_to_event(guint buttonval, guint state, gint mode);

#endif
