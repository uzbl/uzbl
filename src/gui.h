#ifndef UZBL_GUI_H
#define UZBL_GUI_H

#include "webkit.h"

void
uzbl_gui_init ();
void
uzbl_gui_free ();

void
uzbl_gui_update_title ();

#ifndef USE_WEBKIT2
void /* TODO: This should not be public. */
handle_download (WebKitDownload *download, const gchar *suggested_destination);
#endif

#endif
