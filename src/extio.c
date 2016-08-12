#include "extio.h"
#include "util.h"

#define HEADER_SIZE 8

static void
read_header_async (GInputStream        *stream,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data);
static GVariant*
read_header_finish (GInputStream  *stream,
                    GAsyncResult  *result,
                    GError       **error);
static void
read_payload_async (GInputStream        *stream,
                    ExtIOMessageType     type,
                    gsize                size,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data);
static GVariant*
read_payload_finish (GInputStream      *stream,
                     GAsyncResult      *result,
                     ExtIOMessageType  *messagetype,
                     GError           **error);

static void
read_header_read_all_cb (GObject      *source,
                         GAsyncResult *res,
                         gpointer      user_data);

void
read_header_async (GInputStream        *stream,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
    void *buffer = g_malloc (HEADER_SIZE);
    GTask *task = g_task_new (stream, NULL, callback, user_data);

    g_task_set_task_data (task, buffer, g_free);

    g_input_stream_read_all_async (stream, buffer, HEADER_SIZE,
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   read_header_read_all_cb,
                                   task);
}

void
read_header_read_all_cb (GObject      *source,
                         GAsyncResult *res,
                         gpointer      user_data)
{
    GInputStream *stream = G_INPUT_STREAM (source);
    GTask *task = G_TASK (user_data);
    gsize read;
    GError *error;
    void *buffer = g_task_get_task_data (task);

    if (!g_input_stream_read_all_finish (stream, res, &read, &error)) {
        g_task_return_error (task, error);
        return;
    }

    const GVariantType *type = g_variant_type_new ("(ii)");
    GVariant *var = g_variant_new_from_data (type, buffer, read, FALSE,
                                             NULL, NULL);

    g_task_return_pointer (task, var, NULL);
}

GVariant*
read_header_finish (GInputStream  *stream,
                    GAsyncResult  *result,
                    GError       **error)
{
    UZBL_UNUSED (stream);
    GTask *task = G_TASK (result);

    return (GVariant*) g_task_propagate_pointer (task, error);
}


typedef struct _ReadPayloadData ReadPayloadData;
struct _ReadPayloadData {
    ExtIOMessageType type;
    void *buffer;
};

static void
read_payload_read_all_cb (GObject      *source,
                          GAsyncResult *res,
                          gpointer      user_data);
static void
read_payload_data_free (gpointer data);

void
read_payload_async (GInputStream        *stream,
                    ExtIOMessageType     type,
                    gsize                size,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    ReadPayloadData *data = g_slice_new (ReadPayloadData);
    data->buffer = g_malloc (size);
    data->type = type;

    GTask *task = g_task_new (stream, NULL, callback, user_data);
    g_task_set_task_data (task, data, read_payload_data_free);
    g_input_stream_read_all_async (stream, data->buffer, size,
                                   G_PRIORITY_DEFAULT,
                                   NULL,
                                   read_payload_read_all_cb,
                                   task);
}

void
read_payload_data_free (gpointer data)
{
    ReadPayloadData *rpd = (ReadPayloadData*)data;
    g_free (rpd->buffer);
    g_slice_free (ReadPayloadData, rpd);
}

void
read_payload_read_all_cb (GObject      *source,
                          GAsyncResult *res,
                          gpointer      user_data)
{
    GInputStream *stream = G_INPUT_STREAM (source);
    GTask *task = G_TASK (user_data);
    gsize read;
    GError *error = NULL;
    ReadPayloadData *data = g_task_get_task_data (task);

    if (!g_input_stream_read_all_finish (stream, res, &read, &error)) {
        g_task_return_error (task, error);
        return;
    }

    const GVariantType *type = uzbl_extio_get_variant_type (data->type);
    GVariant *var = g_variant_new_from_data (type, data->buffer, read, FALSE,
                                             NULL, NULL);
    g_task_return_pointer (task, var, NULL);
}

GVariant*
read_payload_finish (GInputStream      *stream,
                     GAsyncResult      *result,
                     ExtIOMessageType  *messagetype,
                     GError           **error)
{
    UZBL_UNUSED (stream);
    GTask *task = G_TASK (result);
    ReadPayloadData *data = g_task_get_task_data (task);

    *messagetype = data->type;
    return (GVariant*) g_task_propagate_pointer (task, error);
}

static void
read_header_cb (GObject      *source,
                GAsyncResult *res,
                gpointer      user_data);
static void
read_payload_cb (GObject     *source,
                GAsyncResult *res,
                gpointer      user_data);

void
uzbl_extio_read_message_async (GInputStream        *stream,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
    GTask *task = g_task_new (stream, NULL, callback, user_data);

    read_header_async (stream, read_header_cb, (gpointer) task);
}

void
read_header_cb (GObject      *source,
                GAsyncResult *res,
                gpointer      user_data)
{
    GInputStream *stream = G_INPUT_STREAM (source);
    GTask *task = G_TASK (user_data);

    GError *error = NULL;

    GVariant *header;

    if (!(header = read_header_finish (stream, res, &error))) {
        g_task_return_error (task, error);
        return;
    }

    gint32 type;
    gint32 size;

    g_variant_get (header, "(ii)", &type, &size);
    UZBL_UNUSED (type);
    read_payload_async (stream, type, size, read_payload_cb, (gpointer) task);
}

void
read_payload_cb (GObject     *source,
                GAsyncResult *res,
                gpointer      user_data)
{
    GInputStream *stream = G_INPUT_STREAM (source);
    GTask *task = G_TASK (user_data);
    GError *error = NULL;
    ExtIOMessageType messagetype;
    GVariant *var;

    if (!(var = read_payload_finish (stream, res, &messagetype, &error))) {
        g_task_return_error (task, error);
        return;
    }

    g_task_set_task_data (task, GINT_TO_POINTER (messagetype), NULL);
    g_task_return_pointer (task, var, g_free);
}

GVariant *
uzbl_extio_read_message_finish (GInputStream      *stream,
                                GAsyncResult      *result,
                                ExtIOMessageType  *messagetype,
                                GError           **error)
{
    UZBL_UNUSED (stream);
    GTask *task = G_TASK (result);

    *messagetype = GPOINTER_TO_INT (g_task_get_task_data (task));
    return (GVariant*) g_task_propagate_pointer (task, error);
}

const GVariantType *
uzbl_extio_get_variant_type (ExtIOMessageType type)
{
    switch (type) {
    case EXT_HELO:
        return G_VARIANT_TYPE ("i");
    case EXT_FOCUS:
    case EXT_BLUR:
        return G_VARIANT_TYPE ("s");
    }

    return 0;
}

void
uzbl_extio_send_message (GOutputStream    *stream,
                         ExtIOMessageType  type,
                         GVariant         *message)
{
    gsize size = g_variant_get_size (message);
    GVariant *header = g_variant_new ("(ii)", type, size);

    gsize headersize = g_variant_get_size (header);
    gchar *buf = g_malloc (headersize);
    g_variant_store (header, buf);

    g_output_stream_write (stream, buf, headersize, NULL, NULL);
    g_variant_unref (header);
    g_free (buf);

    buf = g_malloc (size);
    g_variant_store (message, buf);
    g_output_stream_write (stream, buf, size, NULL, NULL);
}

void
uzbl_extio_send_new_messagev (GOutputStream    *stream,
                              ExtIOMessageType  type,
                              va_list *vargs)
{
    GVariant *message = uzbl_extio_new_messagev (type, vargs);
    uzbl_extio_send_message (stream, type, message);
    g_variant_unref (message);
}

void
uzbl_extio_send_new_message (GOutputStream    *stream,
                             ExtIOMessageType  type,
                             ...)
{
    va_list vargs;
    va_start (vargs, type);
    uzbl_extio_send_new_messagev (stream, type, &vargs);
    va_end (vargs);
}

GVariant *
uzbl_extio_new_messagev (ExtIOMessageType type, va_list *vargs)
{
    const gchar *end;
    const GVariantType *vt = uzbl_extio_get_variant_type (type);
    const gchar *frmt = g_variant_type_peek_string (vt);
    GVariant *var = g_variant_new_va (frmt, &end, vargs);
    g_variant_ref_sink (var);
    return var;
}

GVariant *
uzbl_extio_new_message (ExtIOMessageType type,
                        ...)
{
    va_list vargs;
    va_start (vargs, type);
    GVariant *var = uzbl_extio_new_messagev (type, &vargs);
    va_end (vargs);
    return var;
}

void
uzbl_extio_get_message_data (ExtIOMessageType type,
                             GVariant *var, ...)
{
    va_list vargs;
    va_start (vargs, var);

    const gchar *end;
    const GVariantType *vt = uzbl_extio_get_variant_type (type);
    const gchar *frmt = g_variant_type_peek_string (vt);
    g_variant_get_va (var, frmt, &end, &vargs);
    va_end (vargs);
}
