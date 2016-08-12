#ifndef UZBL_EXTIO_H
#define UZBL_EXTIO_H

#include <glib.h>
#include <gio/gio.h>

#define EXTIO_PROTOCOL 1

typedef enum {
    EXT_HELO,
    EXT_FOCUS,
    EXT_BLUR
} ExtIOMessageType;

void
uzbl_extio_read_message_async (GInputStream        *stream,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data);

GVariant *
uzbl_extio_read_message_finish (GInputStream      *stream,
                                GAsyncResult      *result,
                                ExtIOMessageType  *messagetype,
                                GError           **error);

const GVariantType *
uzbl_extio_get_variant_type (ExtIOMessageType type);

void
uzbl_extio_send_message (GOutputStream    *stream,
                         ExtIOMessageType  type,
                         GVariant         *message);

void
uzbl_extio_send_new_messagev (GOutputStream    *stream,
                              ExtIOMessageType  type,
                              va_list *vargs);
void
uzbl_extio_send_new_message (GOutputStream    *stream,
                             ExtIOMessageType  type,
                             ...);

GVariant *
uzbl_extio_new_messagev (ExtIOMessageType type,
                         va_list *vargs);

GVariant *
uzbl_extio_new_message (ExtIOMessageType type, ...);

void
uzbl_extio_get_message_data (ExtIOMessageType type,
                             GVariant *var, ...);

#endif
