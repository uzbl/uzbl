#ifndef UZBL_JS_H
#define UZBL_JS_H

#include <glib.h>
#include <gio/gio.h>
#include <JavaScriptCore/JavaScript.h>

#define UZBL_JS_ERROR uzbl_js_error_quark ()

typedef enum {
    UZBL_JS_ERROR_EVAL
} UzblJsError;

typedef enum {
    JSCTX_UZBL,
    JSCTX_CLEAN,
    JSCTX_PAGE
} UzblJSContext;

GQuark
uzbl_js_error_quark ();

JSValueRef
uzbl_js_get (JSContextRef ctx,
             JSObjectRef  obj,
             const gchar *prop);

void
uzbl_js_set (JSContextRef          ctx,
             JSObjectRef           obj,
             const gchar          *prop,
             JSValueRef            val,
             JSPropertyAttributes  props);

gchar *
uzbl_js_to_string (JSContextRef ctx,
                   JSValueRef   obj);

gchar *
uzbl_js_extract_string (JSStringRef str);

JSObjectRef
uzbl_js_object (JSContextRef  ctx,
                const gchar  *prop);

void
uzbl_js_run_string_async (UzblJSContext        context,
                          const gchar         *script,
                          GAsyncReadyCallback  callback,
                          gpointer             data);

void
uzbl_js_run_file_async (UzblJSContext        context,
                        const gchar         *path,
                        GArray              *args,
                        GAsyncReadyCallback  callback,
                        gpointer             data);

gchar *
uzbl_js_run_finish (GObject       *source,
                    GAsyncResult  *result,
                    GError       **error);

#endif
