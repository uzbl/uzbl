#include <glib.h>
#include <gio/gio.h>

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
