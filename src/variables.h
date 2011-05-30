/*
 * Uzbl Variables
 */

#ifndef __VARIABLES__
#define __VARIABLES__

#include <glib.h>
#include "type.h"

/* Uzbl variables */

typedef struct {
    enum ptr_type type;
    union {
        int *i;
        float *f;
        gchar **s;
    } ptr;
    int dump;
    int writeable;
    /*@null@*/ void (*func)(void);
} uzbl_cmdprop;

gboolean    set_var_value(const gchar *name, gchar *val);
void        variables_hash();
void        dump_config();
void        dump_config_as_events();


void        cmd_load_uri();
void        cmd_set_status();
void        set_proxy_url();
void        set_authentication_handler();
void        set_status_background();
void        set_icon();
void        move_statusbar();
void        cmd_http_debug();
void        cmd_max_conns();
void        cmd_max_conns_host();

/* exported WebKitWebSettings properties */
void        cmd_font_size();
void        cmd_default_font_family();
void        cmd_monospace_font_family();
void        cmd_sans_serif_font_family();
void        cmd_serif_font_family();
void        cmd_cursive_font_family();
void        cmd_fantasy_font_family();
void        cmd_zoom_level();
void        cmd_set_zoom_type();
void        cmd_enable_pagecache();
void        cmd_disable_plugins();
void        cmd_disable_scripts();
void        cmd_minimum_font_size();
void        cmd_fifo_dir();
void        cmd_socket_dir();
void        cmd_useragent() ;
void        set_accept_languages();
void        cmd_autoload_img();
void        cmd_autoshrink_img();
void        cmd_enable_spellcheck();
void        cmd_enable_private();
void        cmd_print_bg();
void        cmd_style_uri();
void        cmd_resizable_txt();
void        cmd_default_encoding();
void        set_current_encoding();
void        cmd_enforce_96dpi();
void        cmd_inject_html();
void        cmd_caret_browsing();
void        cmd_javascript_windows();
void        cmd_set_geometry();
void        cmd_view_source();
void        cmd_scrollbars_visibility();

#endif
