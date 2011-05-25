#include "variables.h"
#include "uzbl-core.h"
#include "callbacks.h"
#include "events.h"

/* abbreviations to help keep the table's width humane */
#define PTR_V_STR(var, d, fun) { .ptr.s = &(var), .type = TYPE_STR, .dump = d, .writeable = 1, .func = fun }
#define PTR_V_INT(var, d, fun) { .ptr.i = (int*)&(var), .type = TYPE_INT, .dump = d, .writeable = 1, .func = fun }
#define PTR_V_FLOAT(var, d, fun) { .ptr.f = &(var), .type = TYPE_FLOAT, .dump = d, .writeable = 1, .func = fun }
#define PTR_C_STR(var,    fun) { .ptr.s = &(var), .type = TYPE_STR, .dump = 0, .writeable = 0, .func = fun }
#define PTR_C_INT(var,    fun) { .ptr.i = (int*)&(var), .type = TYPE_INT, .dump = 0, .writeable = 0, .func = fun }
#define PTR_C_FLOAT(var,  fun) { .ptr.f = &(var), .type = TYPE_FLOAT, .dump = 0, .writeable = 0, .func = fun }

const struct var_name_to_ptr_t {
    const char *name;
    uzbl_cmdprop cp;
} var_name_to_ptr[] = {
/*    variable name            pointer to variable in code                     dump callback function    */
/*  ---------------------------------------------------------------------------------------------- */
    { "uri",                    PTR_V_STR(uzbl.state.uri,                       1,   cmd_load_uri)},
    { "verbose",                PTR_V_INT(uzbl.state.verbose,                   1,   NULL)},
    { "print_events",           PTR_V_INT(uzbl.state.events_stdout,             1,   NULL)},
    { "inject_html",            PTR_V_STR(uzbl.behave.inject_html,              0,   cmd_inject_html)},
    { "geometry",               PTR_V_STR(uzbl.gui.geometry,                    1,   cmd_set_geometry)},
    { "show_status",            PTR_V_INT(uzbl.behave.show_status,              1,   cmd_set_status)},
    { "status_top",             PTR_V_INT(uzbl.behave.status_top,               1,   move_statusbar)},
    { "status_format",          PTR_V_STR(uzbl.behave.status_format,            1,   NULL)},
    { "status_format_right",    PTR_V_STR(uzbl.behave.status_format_right,      1,   NULL)},
    { "status_background",      PTR_V_STR(uzbl.behave.status_background,        1,   set_status_background)},
    { "title_format_long",      PTR_V_STR(uzbl.behave.title_format_long,        1,   NULL)},
    { "title_format_short",     PTR_V_STR(uzbl.behave.title_format_short,       1,   NULL)},
    { "icon",                   PTR_V_STR(uzbl.gui.icon,                        1,   set_icon)},
    { "forward_keys",           PTR_V_INT(uzbl.behave.forward_keys,             1,   NULL)},
    { "authentication_handler", PTR_V_STR(uzbl.behave.authentication_handler,   1,   set_authentication_handler)},
    { "scheme_handler",         PTR_V_STR(uzbl.behave.scheme_handler,           1,   NULL)},
    { "download_handler",       PTR_V_STR(uzbl.behave.download_handler,         1,   NULL)},
    { "fifo_dir",               PTR_V_STR(uzbl.behave.fifo_dir,                 1,   cmd_fifo_dir)},
    { "socket_dir",             PTR_V_STR(uzbl.behave.socket_dir,               1,   cmd_socket_dir)},
    { "http_debug",             PTR_V_INT(uzbl.behave.http_debug,               1,   cmd_http_debug)},
    { "shell_cmd",              PTR_V_STR(uzbl.behave.shell_cmd,                1,   NULL)},
    { "proxy_url",              PTR_V_STR(uzbl.net.proxy_url,                   1,   set_proxy_url)},
    { "max_conns",              PTR_V_INT(uzbl.net.max_conns,                   1,   cmd_max_conns)},
    { "max_conns_host",         PTR_V_INT(uzbl.net.max_conns_host,              1,   cmd_max_conns_host)},
    { "useragent",              PTR_V_STR(uzbl.net.useragent,                   1,   cmd_useragent)},
    { "accept_languages",       PTR_V_STR(uzbl.net.accept_languages,            1,   set_accept_languages)},
    { "javascript_windows",     PTR_V_INT(uzbl.behave.javascript_windows,       1,   cmd_javascript_windows)},
    /* requires webkit >=1.1.14 */
    { "view_source",            PTR_V_INT(uzbl.behave.view_source,              0,   cmd_view_source)},

    /* exported WebKitWebSettings properties */
    { "zoom_level",             PTR_V_FLOAT(uzbl.behave.zoom_level,             1,   cmd_zoom_level)},
    { "zoom_type",              PTR_V_INT(uzbl.behave.zoom_type,                1,   cmd_set_zoom_type)},
    { "font_size",              PTR_V_INT(uzbl.behave.font_size,                1,   cmd_font_size)},
    { "default_font_family",    PTR_V_STR(uzbl.behave.default_font_family,      1,   cmd_default_font_family)},
    { "monospace_font_family",  PTR_V_STR(uzbl.behave.monospace_font_family,    1,   cmd_monospace_font_family)},
    { "cursive_font_family",    PTR_V_STR(uzbl.behave.cursive_font_family,      1,   cmd_cursive_font_family)},
    { "sans_serif_font_family", PTR_V_STR(uzbl.behave.sans_serif_font_family,   1,   cmd_sans_serif_font_family)},
    { "serif_font_family",      PTR_V_STR(uzbl.behave.serif_font_family,        1,   cmd_serif_font_family)},
    { "fantasy_font_family",    PTR_V_STR(uzbl.behave.fantasy_font_family,      1,   cmd_fantasy_font_family)},
    { "monospace_size",         PTR_V_INT(uzbl.behave.monospace_size,           1,   cmd_font_size)},
    { "minimum_font_size",      PTR_V_INT(uzbl.behave.minimum_font_size,        1,   cmd_minimum_font_size)},
    { "enable_pagecache",       PTR_V_INT(uzbl.behave.enable_pagecache,         1,   cmd_enable_pagecache)},
    { "disable_plugins",        PTR_V_INT(uzbl.behave.disable_plugins,          1,   cmd_disable_plugins)},
    { "disable_scripts",        PTR_V_INT(uzbl.behave.disable_scripts,          1,   cmd_disable_scripts)},
    { "autoload_images",        PTR_V_INT(uzbl.behave.autoload_img,             1,   cmd_autoload_img)},
    { "autoshrink_images",      PTR_V_INT(uzbl.behave.autoshrink_img,           1,   cmd_autoshrink_img)},
    { "enable_spellcheck",      PTR_V_INT(uzbl.behave.enable_spellcheck,        1,   cmd_enable_spellcheck)},
    { "enable_private",         PTR_V_INT(uzbl.behave.enable_private,           1,   cmd_enable_private)},
    { "print_backgrounds",      PTR_V_INT(uzbl.behave.print_bg,                 1,   cmd_print_bg)},
    { "stylesheet_uri",         PTR_V_STR(uzbl.behave.style_uri,                1,   cmd_style_uri)},
    { "resizable_text_areas",   PTR_V_INT(uzbl.behave.resizable_txt,            1,   cmd_resizable_txt)},
    { "default_encoding",       PTR_V_STR(uzbl.behave.default_encoding,         1,   cmd_default_encoding)},
    { "current_encoding",       PTR_V_STR(uzbl.behave.current_encoding,         1,   set_current_encoding)},
    { "enforce_96_dpi",         PTR_V_INT(uzbl.behave.enforce_96dpi,            1,   cmd_enforce_96dpi)},
    { "caret_browsing",         PTR_V_INT(uzbl.behave.caret_browsing,           1,   cmd_caret_browsing)},

#if !GTK_CHECK_VERSION(3,0,0)
    { "scrollbars_visible",     PTR_V_INT(uzbl.gui.scrollbars_visible,          1,   cmd_scrollbars_visibility)},
#endif

    /* constants (not dumpable or writeable) */
    { "WEBKIT_MAJOR",           PTR_C_INT(uzbl.info.webkit_major,                    NULL)},
    { "WEBKIT_MINOR",           PTR_C_INT(uzbl.info.webkit_minor,                    NULL)},
    { "WEBKIT_MICRO",           PTR_C_INT(uzbl.info.webkit_micro,                    NULL)},
    { "ARCH_UZBL",              PTR_C_STR(uzbl.info.arch,                            NULL)},
    { "COMMIT",                 PTR_C_STR(uzbl.info.commit,                          NULL)},
    { "TITLE",                  PTR_C_STR(uzbl.gui.main_title,                       NULL)},
    { "SELECTED_URI",           PTR_C_STR(uzbl.state.selected_url,                   NULL)},
    { "NAME",                   PTR_C_STR(uzbl.state.instance_name,                  NULL)},
    { "PID",                    PTR_C_STR(uzbl.info.pid_str,                         NULL)},
    { "_",                      PTR_C_STR(uzbl.state.last_result,                    NULL)},

    { NULL,                     {.ptr.s = NULL, .type = TYPE_INT, .dump = 0, .writeable = 0, .func = NULL}}
};

/* construct a hash from the var_name_to_ptr array for quick access */
void
variables_hash() {
    const struct var_name_to_ptr_t *n2v_p = var_name_to_ptr;
    uzbl.comm.proto_var = g_hash_table_new(g_str_hash, g_str_equal);
    while(n2v_p->name) {
        g_hash_table_insert(uzbl.comm.proto_var,
                (gpointer) n2v_p->name,
                (gpointer) &n2v_p->cp);
        n2v_p++;
    }
}

void
send_set_var_event(const char *name, const uzbl_cmdprop *c) {
    /* check for the variable type */
    switch(c->type) {
    case TYPE_STR:
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "str",
            TYPE_STR, *c->ptr.s ? *c->ptr.s : " ",
            NULL);
        break;
    case TYPE_INT:
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "int",
            TYPE_INT, *c->ptr.i,
            NULL);
        break;
    case TYPE_FLOAT:
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "float",
            TYPE_FLOAT, *c->ptr.f,
            NULL);
        break;
    default:
        g_assert_not_reached();
    }
}

gboolean
set_var_value(const gchar *name, gchar *val) {
    uzbl_cmdprop *c = NULL;
    char *endp = NULL;
    char *buf = NULL;

    g_assert(val != NULL);

    if( (c = g_hash_table_lookup(uzbl.comm.proto_var, name)) ) {
        if(!c->writeable) return FALSE;

        switch(c->type) {
        case TYPE_STR:
            buf = g_strdup(val);
            g_free(*c->ptr.s);
            *c->ptr.s = buf;
            break;
        case TYPE_INT:
            *c->ptr.i = (int)strtoul(val, &endp, 10);
            break;
        case TYPE_FLOAT:
            *c->ptr.f = strtod(val, &endp);
            break;
        default:
            g_assert_not_reached();
        }

        send_set_var_event(name, c);

        /* invoke a command specific function */
        if(c->func) c->func();
    } else {
        /* check wether name violates our naming scheme */
        if(!valid_name(name)) {
            if (uzbl.state.verbose)
                printf("Invalid variable name: %s\n", name);
            return FALSE;
        }

        /* custom vars */
        c = g_malloc(sizeof(uzbl_cmdprop));
        c->type = TYPE_STR;
        c->dump = 0;
        c->func = NULL;
        c->writeable = 1;
        buf = g_strdup(val);
        c->ptr.s = g_malloc(sizeof(char *));
        *c->ptr.s = buf;
        g_hash_table_insert(uzbl.comm.proto_var,
                g_strdup(name), (gpointer) c);

        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "str",
            TYPE_STR, buf,
            NULL);
    }
    update_title();
    return TRUE;
}

void
dump_var_hash(gpointer k, gpointer v, gpointer ud) {
    (void) ud;
    uzbl_cmdprop *c = v;

    if(!c->dump)
        return;

    if(c->type == TYPE_STR)
        printf("set %s = %s\n", (char *)k, *c->ptr.s ? *c->ptr.s : " ");
    else if(c->type == TYPE_INT)
        printf("set %s = %d\n", (char *)k, *c->ptr.i);
    else if(c->type == TYPE_FLOAT)
        printf("set %s = %f\n", (char *)k, *c->ptr.f);
}

void
dump_config() {
    g_hash_table_foreach(uzbl.comm.proto_var, dump_var_hash, NULL);
}

void
dump_var_hash_as_event(gpointer k, gpointer v, gpointer ud) {
    (void) ud;
    uzbl_cmdprop *c = v;

    if(c->dump)
        send_set_var_event(k, c);
}

void
dump_config_as_events() {
    g_hash_table_foreach(uzbl.comm.proto_var, dump_var_hash_as_event, NULL);
}
