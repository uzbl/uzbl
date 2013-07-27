#ifndef UZBL_REQUESTS_H
#define UZBL_REQUESTS_H

#include <glib.h>

void
uzbl_requests_set_reply (const gchar *reply);

GString *
uzbl_requests_send (const gchar *request, ...) G_GNUC_NULL_TERMINATED;

#endif
