#ifndef UZBL_SCHEME_H
#define UZBL_SCHEME_H

#include <glib.h>

void
uzbl_scheme_init ();

void
uzbl_scheme_add_handler (const gchar *scheme, const gchar *command);

#endif
