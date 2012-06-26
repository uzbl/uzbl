/*
 * Uzbl Variables
 */

#ifndef __VARIABLES__
#define __VARIABLES__

#include <glib.h>
#ifdef USE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

#include "type.h"

uzbl_cmdprop *get_var_c(const gchar *name);

gboolean    set_var_value(const gchar *name, gchar *val);
void        expand_variable(GString *buf, const gchar *name);
void        variables_hash();

gchar *get_var_value_string_c(const uzbl_cmdprop *c);
gchar *get_var_value_string(const char *name);
int get_var_value_int_c(const uzbl_cmdprop *c);
int get_var_value_int(const char *name);
float get_var_value_float_c(const uzbl_cmdprop *c);
float get_var_value_float(const char *name);

void set_var_value_string_c(uzbl_cmdprop *c, const gchar *val);
void set_var_value_int_c(uzbl_cmdprop *c, int f);
void set_var_value_float_c(uzbl_cmdprop *c, float f);

void send_set_var_event(const char *name, const uzbl_cmdprop *c);

void        dump_config();
void        dump_config_as_events();

void        uri_change_cb (WebKitWebView *web_view, GParamSpec param_spec);

void        set_zoom_type(int);
int         get_zoom_type();

gchar      *get_geometry();
void        set_geometry(const gchar *);

int         get_show_status();
void        set_show_status(int);

#endif
