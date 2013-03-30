#ifndef UZBL_WEBKIT_H
#define UZBL_WEBKIT_H

#ifdef USE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

#if GTK_CHECK_VERSION (2, 91, 0)
#include <gtk/gtkx.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libsoup/soup.h>
#include <glib.h>

#ifdef USE_WEBKIT2
typedef WebKitSettings WebKitWebSettings;
typedef WebKitFindOptions UzblFindOptions;
#else
/* Use same symbols as WebKit2 for find options. */
typedef enum {
    WEBKIT_FIND_OPTIONS_NONE,

    /* Other options are unsupported in WebKit1. */
    WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE = 1 << 0,
    WEBKIT_FIND_OPTIONS_BACKWARDS        = 1 << 3,
    WEBKIT_FIND_OPTIONS_WRAP_AROUND      = 1 << 4
} UzblFindOptions;
#endif

#endif
