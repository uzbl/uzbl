#ifndef UZBL_JS_H
#define UZBL_JS_H

#include "webkit.h"

#include <JavaScriptCore/JavaScript.h>

JSObjectRef
uzbl_js_object (JSContextRef ctx, const gchar *prop);
JSValueRef
uzbl_js_get (JSContextRef ctx, JSObjectRef obj, const gchar *prop);
void
uzbl_js_set (JSContextRef ctx, JSObjectRef obj, const gchar *prop, JSValueRef val, JSPropertyAttributes props);
gchar *
uzbl_js_to_string (JSContextRef ctx, JSValueRef obj);
gchar *
uzbl_js_extract_string (JSStringRef str);

#endif
