/*
 ** Gui
 ** (c) 2009-2013 by Robert Manea et al.
*/

#ifndef UZBL_GUI_H
#define UZBL_GUI_H

#include "uzbl-core.h"

void
uzbl_gui_init (gboolean plugmode);
void
set_window_property (const gchar *prop, const gchar *value);
gboolean /* FIXME: This should not be public here. */
download_cb (WebKitWebView *web_view, WebKitDownload *download, gpointer user_data);

#endif
