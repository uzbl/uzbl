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
    REQUEST_STARTING,
    KEY_PRESS, KEY_RELEASE, COMMAND_EXECUTED,
    LINK_HOVER, TITLE_CHANGED, GEOMETRY_CHANGED,
    WEBINSPECTOR, NEW_WINDOW, SELECTION_CHANGED,
    VARIABLE_SET, FIFO_SET, SOCKET_SET,
    INSTANCE_START, INSTANCE_EXIT, LOAD_PROGRESS,
    LINK_UNHOVER, FORM_ACTIVE, ROOT_ACTIVE,
    FOCUS_LOST, FOCUS_GAINED, FILE_INCLUDED,
    PLUG_CREATED, COMMAND_ERROR, BUILTINS,
    PTR_MOVE, SCROLL_VERT, SCROLL_HORIZ,
    DOWNLOAD_STARTED, DOWNLOAD_PROGRESS, DOWNLOAD_COMPLETE,
    ADD_COOKIE, DELETE_COOKIE,

    /* must be last entry */
    LAST_EVENT
};

void
event_buffer_timeout(guint sec);

void
replay_buffered_events();

void
vsend_event(int type, const gchar *custom_event, va_list vargs);

void
send_event(int type, const gchar *custom_event, ...) G_GNUC_NULL_TERMINATED;

void
key_to_event(guint keyval, guint state, int mode);

#endif
