#ifndef UZBL_GUI_H
#define UZBL_GUI_H

#include "webkit.h"

void
uzbl_gui_init ();

gboolean /* FIXME: This should not be public here. */
download_cb (WebKitWebView *view, WebKitDownload *download, gpointer data);

#endif
