#ifndef UZBL_VARIABLES_H
#define UZBL_VARIABLES_H

#include "type.h"

#include <glib.h>

gboolean
uzbl_variables_is_valid (const gchar *name);

gboolean
uzbl_variables_set (const gchar *name, gchar *val);
gboolean
uzbl_variables_toggle (const gchar *name, GArray *values);

gchar *
uzbl_variables_expand (const gchar *str);

gchar *
uzbl_variables_get_string (const gchar *name);
int
uzbl_variables_get_int (const gchar *name);
unsigned long long
uzbl_variables_get_ull (const gchar *name);
gdouble
uzbl_variables_get_double (const gchar *name);

void
uzbl_variables_dump ();
void
uzbl_variables_dump_events ();

#endif
