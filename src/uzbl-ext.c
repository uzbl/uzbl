#include <webkit2/webkit-web-extension.h>
#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>
#include "uzbl-ext.h"
#include "extio.h"
#include "util.h"

// The unstable API is generated from the DOM specs and things might break
// between versions, especially if using new and experimental features.
#define WEBKIT_DOM_USE_UNSTABLE_API
#include <webkitdom/WebKitDOMDOMWindowUnstable.h>

struct _UzblExt {
    GIOStream     *stream;
    WebKitWebPage *web_page;
};

UzblExt*
uzbl_ext_new ()
{
    UzblExt *ext = g_new0 (UzblExt, 1);
    return ext;
}

static void
ext_scroll_web_page (UzblExt      *ext,
                     ScrollAxis    axis,
                     ScrollAction  action,
                     ScrollMode    mode,
                     gint32        amount);

static void
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
    case EXT_SCROLL:
        {
            gchar axis, action, mode;
            guint32 amount;
            uzbl_extio_get_message_data (EXT_SCROLL, message,
                &axis, &action, &mode, &amount);
            ext_scroll_web_page (ext, axis, action, mode, amount);
            break;
        }
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

static void
document_loaded_callback (WebKitWebPage *web_page,
                          gpointer       user_data);
static void
dom_focus_callback (WebKitDOMEventTarget *target,
                    WebKitDOMEvent       *event,
                    gpointer              user_data);
static void
dom_blur_callback (WebKitDOMEventTarget *target,
                   WebKitDOMEvent       *event,
                   gpointer              user_data);


G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *extension,
                                                GVariant           *user_data)
{
    gchar *pretty_data = g_variant_print (user_data, TRUE);
    g_debug ("Initializing web extension with %s", pretty_data);
    g_free (pretty_data);

    UzblExt *ext = uzbl_ext_new ();
    gint proto;
    gint64 in;
    gint64 out;

    g_variant_get (user_data, "(ixx)", &proto, &in, &out);
    uzbl_ext_init_io (ext, in, out);

    uzbl_extio_send_new_message (g_io_stream_get_output_stream (ext->stream),
                                 EXT_HELO, EXTIO_PROTOCOL);

    if (proto != EXTIO_PROTOCOL) {
        g_warning ("Extension loaded into incompatible version of uzbl (expected %d, was %d)", EXTIO_PROTOCOL, proto);
        return;
    }

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
    UzblExt *ext = (UzblExt*)user_data;

    g_debug ("Web page created");
    ext->web_page = web_page;
    g_signal_connect (web_page, "document-loaded",
                      G_CALLBACK (document_loaded_callback), ext);
}

void
document_loaded_callback (WebKitWebPage *web_page,
                          gpointer       user_data)
{
    UzblExt *ext = (UzblExt*)user_data;
    WebKitDOMDocument *doc = webkit_web_page_get_dom_document (web_page);

    webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (doc),
        "focus", G_CALLBACK (dom_focus_callback), TRUE, ext);
    webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (doc),
        "blur",  G_CALLBACK (dom_blur_callback), TRUE, ext);
}

void
dom_focus_callback (WebKitDOMEventTarget *target,
                    WebKitDOMEvent       *event,
                    gpointer              user_data)
{
    UZBL_UNUSED (target);

    UzblExt *ext = (UzblExt*)user_data;
    WebKitDOMEventTarget *etarget = webkit_dom_event_get_target (event);
    gchar *name = webkit_dom_node_get_node_name (WEBKIT_DOM_NODE (etarget));

    uzbl_extio_send_new_message (g_io_stream_get_output_stream (ext->stream),
                                 EXT_FOCUS, name);
}

void
dom_blur_callback (WebKitDOMEventTarget *target,
                   WebKitDOMEvent       *event,
                   gpointer user_data)
{
    UZBL_UNUSED (target);

    UzblExt *ext = (UzblExt*)user_data;
    WebKitDOMEventTarget *etarget = webkit_dom_event_get_target (event);
    gchar *name = webkit_dom_node_get_node_name (WEBKIT_DOM_NODE (etarget));

    uzbl_extio_send_new_message (g_io_stream_get_output_stream (ext->stream),
                                 EXT_BLUR, name);
}

void
ext_scroll_web_page (UzblExt      *ext,
                     ScrollAxis    axis,
                     ScrollAction  action,
                     ScrollMode    mode,
                     gint32        amount)
{
    WebKitDOMDocument *doc = webkit_web_page_get_dom_document (ext->web_page);
    WebKitDOMDOMWindow *win = webkit_dom_document_get_default_view (doc);
    WebKitDOMElement *body = WEBKIT_DOM_ELEMENT (
        webkit_dom_document_get_body (doc));

    if (action == SCROLL_BEGIN) {
        webkit_dom_dom_window_scroll_to (
            win,
            axis == SCROLL_HORIZONTAL ? 0
                                      : webkit_dom_dom_window_get_scroll_x (win),
            axis == SCROLL_HORIZONTAL ? webkit_dom_dom_window_get_scroll_y (win)
                                      : 0
        );
    } else if (action == SCROLL_END) {
        webkit_dom_dom_window_scroll_to (
            win,
            axis == SCROLL_HORIZONTAL ? webkit_dom_element_get_scroll_width (body)
                                      : webkit_dom_dom_window_get_scroll_x (win),
            axis == SCROLL_HORIZONTAL ? webkit_dom_dom_window_get_scroll_y (win)
                                      : webkit_dom_element_get_scroll_height (body)
        );
    } else if (action == SCROLL_SET) {
        if (mode == SCROLL_PERCENTAGE) {
            glong page = (axis == SCROLL_HORIZONTAL) ? webkit_dom_element_get_scroll_width (body)
                                                     : webkit_dom_element_get_scroll_height (body);
            amount = (gint32) page * amount * 0.01;
        }

        webkit_dom_dom_window_scroll_to (
            win,
            axis == SCROLL_HORIZONTAL ? amount
                                      : webkit_dom_dom_window_get_scroll_x (win),
            axis == SCROLL_HORIZONTAL ? webkit_dom_dom_window_get_scroll_y (win)
                                      : amount
        );
    } else if (action == SCROLL_TRANSLATE) {
        if (mode == SCROLL_PERCENTAGE) {
            glong screen = (axis == SCROLL_HORIZONTAL) ? webkit_dom_dom_window_get_inner_width (win)
                                                       : webkit_dom_dom_window_get_inner_height (win);
            amount = (gint32) screen * amount * 0.01;
        }

        webkit_dom_dom_window_scroll_by (
            win,
            axis == SCROLL_HORIZONTAL ? amount : 0,
            axis == SCROLL_HORIZONTAL ? 0 : amount
        );
    }
}
