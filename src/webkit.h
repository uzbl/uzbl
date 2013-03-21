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

#endif
