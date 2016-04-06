#include <webkit2/webkit-web-extension.h>
#include "uzbl-ext.h"
#define UZBL_UNUSED(var) (void)var

struct _UzblExt {
};

UzblExt*
uzbl_ext_new ()
{
    UzblExt *ext = g_new (UzblExt, 1);
    return ext;
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
    UZBL_UNUSED (user_data);

    g_debug ("Web page created");
}
