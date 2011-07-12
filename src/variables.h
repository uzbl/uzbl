/*
 * Uzbl Variables
 */

#ifndef __VARIABLES__
#define __VARIABLES__

#include <glib.h>
#include <webkit/webkit.h>

gboolean    set_var_value(const gchar *name, gchar *val);
void        expand_variable(GString *buf, const gchar *name);
void        variables_hash();
void        dump_config();
void        dump_config_as_events();

void        uri_change_cb (WebKitWebView *web_view, GParamSpec param_spec);
void        set_show_status();
void        set_geometry();

#endif
