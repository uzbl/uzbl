#include <webkit2/webkit-web-extension.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include "uzbl-ext.h"
#include "extio.h"
#include "util.h"

struct _UzblExt {
    GIOStream *stream;
};

UzblExt*
uzbl_ext_new ()
{
    UzblExt *ext = g_new (UzblExt, 1);
    return ext;
}

void
read_message_cb (GObject *stream,
                 GAsyncResult *res,
                 gpointer user_data);

void
uzbl_ext_init_io (UzblExt *ext, int in, int out)
{
    GInputStream *input = g_unix_input_stream_new (in, TRUE);
    GOutputStream *output = g_unix_output_stream_new (out, TRUE);

    ext->stream = g_simple_io_stream_new (input, output);

    uzbl_extio_read_message_async (input, read_message_cb, ext);
}

void
read_message_cb (GObject *source,
                 GAsyncResult *res,
                 gpointer user_data)
{
    UzblExt *ext = (UzblExt*) user_data;
    GInputStream *stream = G_INPUT_STREAM (source);
    GError *error = NULL;
    GVariant *message;
    ExtIOMessageType messagetype;

    if (!(message = uzbl_extio_read_message_finish (stream, res, &messagetype, &error))) {
        g_warning ("reading message from core: %s", error->message);
        g_error_free (error);
        return;
    }

    switch (messagetype) {
    default:
        {
            gchar *pmsg = g_variant_print (message, TRUE);
            g_debug ("got unrecognised message %s", pmsg);
            g_free (pmsg);
            break;
        }
    }
    g_variant_unref (message);

    uzbl_extio_read_message_async (stream, read_message_cb, ext);
}

static void
web_page_created_callback (WebKitWebExtension *extension,
                           WebKitWebPage      *web_page,
                           gpointer            user_data);

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *extension,
                                                GVariant           *user_data)
{
    gchar *pretty_data = g_variant_print (user_data, TRUE);
    g_debug ("Initializing web extension with %s", pretty_data);
    g_free (pretty_data);

    UzblExt *ext = uzbl_ext_new ();
    const gchar *name;
    gint64 in;
    gint64 out;

    g_variant_get (user_data, "(sxx)", &name, &in, &out);
    g_debug ("The name is %s", name);
    uzbl_ext_init_io (ext, in, out);

    g_signal_connect (extension, "page-created",
                      G_CALLBACK (web_page_created_callback),
                      ext);
}

void
web_page_created_callback (WebKitWebExtension *extension,
                           WebKitWebPage      *web_page,
                           gpointer            user_data)
{
    UZBL_UNUSED (extension);
    UZBL_UNUSED (web_page);
    UzblExt *ext = (UzblExt*)user_data;

    g_debug ("Web page created");

    GVariant *message = g_variant_new ("s", "Hello");
    uzbl_extio_send_message (g_io_stream_get_output_stream (ext->stream),
                             EXT_HELO, message);
    g_variant_unref (message);
}
