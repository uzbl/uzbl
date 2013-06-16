#include "js.h"
#include "uzbl-core.h"

#include <stdlib.h>

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
    size_t size = JSStringGetMaximumUTF8CStringSize (str);

    char *result = NULL;

    if (size) {
        result = (gchar *)malloc (size * sizeof (char));
        JSStringGetUTF8CString (str, result, size);
    }

    JSStringRelease (str);

    return result;
}
