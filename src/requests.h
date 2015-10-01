#ifndef UZBL_REQUESTS_H
#define UZBL_REQUESTS_H

#include <glib.h>

GString *
uzbl_requests_send (gint64 timeout, const gchar *request, ...) G_GNUC_NULL_TERMINATED;

#endif
