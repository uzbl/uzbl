#include "js.h"
#include "uzbl-core.h"

#include <stdlib.h>

/* =========================== PUBLIC API =========================== */

void
uzbl_js_init ()
{
    uzbl.state.jscontext = JSGlobalContextCreate (NULL);

    JSObjectRef global = JSContextGetGlobalObject (uzbl.state.jscontext);
    JSObjectRef uzbl_obj = JSObjectMake (uzbl.state.jscontext, NULL, NULL);

    uzbl_js_set (uzbl.state.jscontext,
        global, "uzbl", uzbl_obj,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete);
}

JSObjectRef
uzbl_js_object (JSContextRef ctx, const gchar *prop)
{
    JSObjectRef global = JSContextGetGlobalObject (ctx);
    JSValueRef val = uzbl_js_get (ctx, global, prop);

    return JSValueToObject (ctx, val, NULL);
}

JSValueRef
uzbl_js_get (JSContextRef ctx, JSObjectRef obj, const gchar *prop)
{
    JSStringRef js_prop;
    JSValueRef js_prop_val;

    js_prop = JSStringCreateWithUTF8CString (prop);
    js_prop_val = JSObjectGetProperty (ctx, obj, js_prop, NULL);

    JSStringRelease (js_prop);

    return js_prop_val;
}

void
uzbl_js_set (JSContextRef ctx, JSObjectRef obj, const gchar *prop, JSValueRef val, JSPropertyAttributes props)
{
    JSStringRef name = JSStringCreateWithUTF8CString (prop);

    JSObjectSetProperty (ctx, obj, name, val, props, NULL);

    JSStringRelease (name);
}

gchar *
uzbl_js_to_string (JSContextRef ctx, JSValueRef val)
{
    JSStringRef str = JSValueToStringCopy (ctx, val, NULL);
    gchar *result = uzbl_js_extract_string (str);
    JSStringRelease (str);

    return result;
}

gchar *
uzbl_js_extract_string (JSStringRef str)
{
    size_t max_size = JSStringGetMaximumUTF8CStringSize (str);
    gchar *gstr = (gchar *)malloc (max_size * sizeof (gchar));
    JSStringGetUTF8CString (str, gstr, max_size);

    return gstr;
}
