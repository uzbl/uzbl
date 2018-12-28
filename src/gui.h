#ifndef UZBL_GUI_H
#define UZBL_GUI_H

#include "webkit.h"

void
uzbl_gui_update_title ();

WebKitWebContext*
create_web_context (const gchar *cache_dir,
                    const gchar *data_dir);
#endif
