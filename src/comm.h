#ifndef UZBL_COMM_H
#define UZBL_COMM_H

#include <glib.h>

void
uzbl_comm_string_append_double (GString *buf, double val);

GString *
uzbl_comm_vformat (const gchar *directive, const gchar *function, va_list vargs);

#endif
