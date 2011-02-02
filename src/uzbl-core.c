/* -*- c-basic-offset: 4; -*- */
// Original code taken from the example webkit-gtk+ application. see notice below.
// Modified code is licensed under the GPL 3.  See LICENSE file.


/*
 * Copyright (C) 2006, 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "uzbl-core.h"
#include "callbacks.h"
#include "events.h"
#include "inspector.h"
#include "config.h"
#include "util.h"

UzblCore uzbl;

/* commandline arguments (set initial values for the state variables) */
const
GOptionEntry entries[] =
{
    { "uri",      'u', 0, G_OPTION_ARG_STRING, &uzbl.state.uri,
        "Uri to load at startup (equivalent to 'uzbl <uri>' or 'set uri = URI' after uzbl has launched)", "URI" },
    { "verbose",  'v', 0, G_OPTION_ARG_NONE,   &uzbl.state.verbose,
        "Whether to print all messages or just errors.", NULL },
    { "name",     'n', 0, G_OPTION_ARG_STRING, &uzbl.state.instance_name,
        "Name of the current instance (defaults to Xorg window id or random for GtkSocket mode)", "NAME" },
    { "config",   'c', 0, G_OPTION_ARG_STRING, &uzbl.state.config_file,
        "Path to config file or '-' for stdin", "FILE" },
    { "socket",   's', 0, G_OPTION_ARG_INT, &uzbl.state.socket_id,
        "Xembed Socket ID", "SOCKET" },
    { "connect-socket",   0, 0, G_OPTION_ARG_STRING_ARRAY, &uzbl.state.connect_socket_names,
        "Connect to server socket for event managing", "CSOCKET" },
    { "print-events", 'p', 0, G_OPTION_ARG_NONE, &uzbl.state.events_stdout,
        "Whether to print events to stdout.", NULL },
    { "geometry", 'g', 0, G_OPTION_ARG_STRING, &uzbl.gui.geometry,
        "Set window geometry (format: 'WIDTHxHEIGHT+-X+-Y' or 'maximized')", "GEOMETRY" },
    { "version",  'V', 0, G_OPTION_ARG_NONE, &uzbl.behave.print_version,
        "Print the version and exit", NULL },
    { NULL,      0, 0, 0, NULL, NULL, NULL }
};

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
    { "cookie_handler",         PTR_V_STR(uzbl.behave.cookie_handler,           1,   cmd_set_cookie_handler)},
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
    { "enforce_96_dpi",         PTR_V_INT(uzbl.behave.enforce_96dpi,            1,   cmd_enforce_96dpi)},
    { "caret_browsing",         PTR_V_INT(uzbl.behave.caret_browsing,           1,   cmd_caret_browsing)},
    { "scrollbars_visible",     PTR_V_INT(uzbl.gui.scrollbars_visible,          1,   cmd_scrollbars_visibility)},

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

    { NULL,                     {.ptr.s = NULL, .type = TYPE_INT, .dump = 0, .writeable = 0, .func = NULL}}
};

/* construct a hash from the var_name_to_ptr array for quick access */
void
create_var_to_name_hash() {
    const struct var_name_to_ptr_t *n2v_p = var_name_to_ptr;
    uzbl.comm.proto_var = g_hash_table_new(g_str_hash, g_str_equal);
    while(n2v_p->name) {
        g_hash_table_insert(uzbl.comm.proto_var,
                (gpointer) n2v_p->name,
                (gpointer) &n2v_p->cp);
        n2v_p++;
    }
}


/* --- UTILITY FUNCTIONS --- */
enum exp_type {EXP_ERR, EXP_SIMPLE_VAR, EXP_BRACED_VAR, EXP_EXPR, EXP_JS, EXP_ESCAPE};
enum exp_type
get_exp_type(const gchar *s) {
    /* variables */
    if(*(s+1) == '(')
        return EXP_EXPR;
    else if(*(s+1) == '{')
        return EXP_BRACED_VAR;
    else if(*(s+1) == '<')
        return EXP_JS;
    else if(*(s+1) == '[')
        return EXP_ESCAPE;
    else
        return EXP_SIMPLE_VAR;

    /*@notreached@*/
return EXP_ERR;
}

/*
 * recurse == 1: don't expand '@(command)@'
 * recurse == 2: don't expand '@<java script>@'
*/
gchar *
expand(const char *s, guint recurse) {
    uzbl_cmdprop *c;
    enum exp_type etype;
    char *end_simple_var = "\t^°!\"§$%&/()=?'`'+~*'#-:,;@<>| \\{}[]¹²³¼½";
    char *ret = NULL;
    char *vend = NULL;
    GError *err = NULL;
    gchar *cmd_stdout = NULL;
    gchar *mycmd = NULL;
    GString *buf = g_string_new("");
    GString *js_ret = g_string_new("");

    while(s && *s) {
        switch(*s) {
            case '\\':
                g_string_append_c(buf, *++s);
                s++;
                break;

            case '@':
                etype = get_exp_type(s);
                s++;

                switch(etype) {
                    case EXP_SIMPLE_VAR:
                        vend = strpbrk(s, end_simple_var);
                        if(!vend) vend = strchr(s, '\0');
                        break;
                    case EXP_BRACED_VAR:
                        s++;
                        vend = strchr(s, '}');
                        if(!vend) vend = strchr(s, '\0');
                        break;
                    case EXP_EXPR:
                        s++;
                        vend = strstr(s, ")@");
                        if(!vend) vend = strchr(s, '\0');
                        break;
                    case EXP_JS:
                        s++;
                        vend = strstr(s, ">@");
                        if(!vend) vend = strchr(s, '\0');
                        break;
                    case EXP_ESCAPE:
                        s++;
                        vend = strstr(s, "]@");
                        if(!vend) vend = strchr(s, '\0');
                        break;
                    /*@notreached@*/
                    case EXP_ERR:
                        break;
                }
                assert(vend);

                ret = g_strndup(s, vend-s);

                if(etype == EXP_SIMPLE_VAR ||
                   etype == EXP_BRACED_VAR) {
                    if( (c = g_hash_table_lookup(uzbl.comm.proto_var, ret)) ) {
                        if(c->type == TYPE_STR && *c->ptr.s != NULL) {
                            g_string_append(buf, (gchar *)*c->ptr.s);
                        }
                        else if(c->type == TYPE_INT) {
                            g_string_append_printf(buf, "%d", *c->ptr.i);
                        }
                        else if(c->type == TYPE_FLOAT) {
                            g_string_append_printf(buf, "%f", *c->ptr.f);
                        }
                    }

                    if(etype == EXP_SIMPLE_VAR)
                        s = vend;
                    else
                        s = vend+1;
                }
                else if(recurse != 1 &&
                        etype == EXP_EXPR) {

                    /* execute program directly */
                    if(ret[0] == '+') {
                        mycmd = expand(ret+1, 1);
                        g_spawn_command_line_sync(mycmd, &cmd_stdout, NULL, NULL, &err);
                        g_free(mycmd);
                    }
                    /* execute program through shell, quote it first */
                    else {
                        mycmd = expand(ret, 1);
                        gchar *quoted = g_shell_quote(mycmd);
                        gchar *tmp = g_strdup_printf("%s %s",
                                uzbl.behave.shell_cmd?uzbl.behave.shell_cmd:"/bin/sh -c",
                                quoted);
                        g_spawn_command_line_sync(tmp, &cmd_stdout, NULL, NULL, &err);
                        g_free(mycmd);
                        g_free(quoted);
                        g_free(tmp);
                    }

                    if (err) {
                        g_printerr("error on running command: %s\n", err->message);
                        g_error_free (err);
                    }
                    else if (*cmd_stdout) {
                        size_t len = strlen(cmd_stdout);

                        if(len > 0 && cmd_stdout[len-1] == '\n')
                            cmd_stdout[--len] = '\0'; /* strip trailing newline */

                        g_string_append(buf, cmd_stdout);
                        g_free(cmd_stdout);
                    }
                    s = vend+2;
                }
                else if(recurse != 2 &&
                        etype == EXP_JS) {

                    /* read JS from file */
                    if(ret[0] == '+') {
                        GArray *tmp = g_array_new(TRUE, FALSE, sizeof(gchar *));
                        mycmd = expand(ret+1, 2);
                        g_array_append_val(tmp, mycmd);

                        run_external_js(uzbl.gui.web_view, tmp, js_ret);
                        g_array_free(tmp, TRUE);
                    }
                    /* JS from string */
                    else {
                        mycmd = expand(ret, 2);
                        eval_js(uzbl.gui.web_view, mycmd, js_ret, "(command)");
                        g_free(mycmd);
                    }

                    if(js_ret->str) {
                        g_string_append(buf, js_ret->str);
                        g_string_free(js_ret, TRUE);
                        js_ret = g_string_new("");
                    }
                    s = vend+2;
                }
                else if(etype == EXP_ESCAPE) {
                    mycmd = expand(ret, 0);
                    char *escaped = g_markup_escape_text(mycmd, strlen(mycmd));

                    g_string_append(buf, escaped);

                    g_free(escaped);
                    g_free(mycmd);
                    s = vend+2;
                }

                g_free(ret);
                ret = NULL;
                break;

            default:
                g_string_append_c(buf, *s);
                s++;
                break;
        }
    }
    g_string_free(js_ret, TRUE);
    return g_string_free(buf, FALSE);
}

char *
itos(int val) {
    char tmp[20];

    snprintf(tmp, sizeof(tmp), "%i", val);
    return g_strdup(tmp);
}

gchar*
strfree(gchar *str) {
    g_free(str);
    return NULL;
}

gchar*
argv_idx(const GArray *a, const guint idx) { return g_array_index(a, gchar*, idx); }

/* search a PATH style string for an existing file+path combination */
gchar*
find_existing_file(gchar* path_list) {
    int i=0;
    int cnt;
    gchar **split;
    gchar *tmp = NULL;
    gchar *executable;

    if(!path_list)
        return NULL;

    split = g_strsplit(path_list, ":", 0);
    while(split[i])
        i++;

    if(i<=1) {
        tmp = g_strdup(split[0]);
        g_strfreev(split);
        return tmp;
    }
    else
        cnt = i-1;

    i=0;
    tmp = g_strdup(split[cnt]);
    g_strstrip(tmp);
    if(tmp[0] == '/')
        executable = g_strdup_printf("%s", tmp+1);
    else
        executable = g_strdup(tmp);
    g_free(tmp);

    while(i<cnt) {
        tmp = g_strconcat(g_strstrip(split[i]), "/", executable, NULL);
        if(g_file_test(tmp, G_FILE_TEST_EXISTS)) {
            g_strfreev(split);
            return tmp;
        }
        else
            g_free(tmp);
        i++;
    }

    g_free(executable);
    g_strfreev(split);
    return NULL;
}

void
clean_up(void) {
    if(uzbl.info.pid_str) {
        send_event(INSTANCE_EXIT, uzbl.info.pid_str, NULL);
        g_free(uzbl.info.pid_str);
        uzbl.info.pid_str = NULL;
    }

    if(uzbl.state.executable_path) {
        g_free(uzbl.state.executable_path);
        uzbl.state.executable_path = NULL;
    }

    if (uzbl.behave.commands) {
        g_hash_table_destroy(uzbl.behave.commands);
        uzbl.behave.commands = NULL;
    }

    if(uzbl.state.event_buffer) {
        g_ptr_array_free(uzbl.state.event_buffer, TRUE);
        uzbl.state.event_buffer = NULL;
    }

    if (uzbl.behave.fifo_dir) {
        unlink (uzbl.comm.fifo_path);
        g_free(uzbl.comm.fifo_path);
        uzbl.comm.fifo_path = NULL;
    }

    if (uzbl.behave.socket_dir) {
        unlink (uzbl.comm.socket_path);
        g_free(uzbl.comm.socket_path);
        uzbl.comm.socket_path = NULL;
    }
}

gint
get_click_context() {
    GUI *g = &uzbl.gui;
    WebKitHitTestResult *ht;
    guint context;

    if(!uzbl.state.last_button)
        return -1;

    ht = webkit_web_view_get_hit_test_result (g->web_view, uzbl.state.last_button);
    g_object_get (ht, "context", &context, NULL);
	g_object_unref (ht);

    return (gint)context;
}

/* --- SIGNALS --- */
sigfunc*
setup_signal(int signr, sigfunc *shandler) {
    struct sigaction nh, oh;

    nh.sa_handler = shandler;
    sigemptyset(&nh.sa_mask);
    nh.sa_flags = 0;

    if(sigaction(signr, &nh, &oh) < 0)
        return SIG_ERR;

    return NULL;
}

void
empty_event_buffer(int s) {
    (void) s;
    if(uzbl.state.event_buffer) {
        g_ptr_array_free(uzbl.state.event_buffer, TRUE);
        uzbl.state.event_buffer = NULL;
    }
}

/* scroll a bar in a given direction */
void
scroll (GtkAdjustment* bar, gchar *amount_str) {
    gchar *end;
    gdouble max_value;

    gdouble page_size = gtk_adjustment_get_page_size(bar);
    gdouble value = gtk_adjustment_get_value(bar);
    gdouble amount = g_ascii_strtod(amount_str, &end);

    if (*end == '%')
        value += page_size * amount * 0.01;
    else
        value += amount;

    max_value = gtk_adjustment_get_upper(bar) - page_size;

    if (value > max_value)
        value = max_value; /* don't scroll past the end of the page */

    gtk_adjustment_set_value (bar, value);
}

/*
 * scroll vertical 20
 * scroll vertical 20%
 * scroll vertical -40
 * scroll vertical begin
 * scroll vertical end
 * scroll horizontal 10
 * scroll horizontal -500
 * scroll horizontal begin
 * scroll horizontal end
 */
void
scroll_cmd(WebKitWebView* page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar *direction = g_array_index(argv, gchar*, 0);
    gchar *argv1     = g_array_index(argv, gchar*, 1);

    if (g_strcmp0(direction, "horizontal") == 0)
    {
      if (g_strcmp0(argv1, "begin") == 0)
        gtk_adjustment_set_value(uzbl.gui.bar_h, gtk_adjustment_get_lower(uzbl.gui.bar_h));
      else if (g_strcmp0(argv1, "end") == 0)
        gtk_adjustment_set_value (uzbl.gui.bar_h, gtk_adjustment_get_upper(uzbl.gui.bar_h) -
                                  gtk_adjustment_get_page_size(uzbl.gui.bar_h));
      else
        scroll(uzbl.gui.bar_h, argv1);
    }
    else if (g_strcmp0(direction, "vertical") == 0)
    {
      if (g_strcmp0(argv1, "begin") == 0)
        gtk_adjustment_set_value(uzbl.gui.bar_v, gtk_adjustment_get_lower(uzbl.gui.bar_v));
      else if (g_strcmp0(argv1, "end") == 0)
        gtk_adjustment_set_value (uzbl.gui.bar_v, gtk_adjustment_get_upper(uzbl.gui.bar_v) -
                                  gtk_adjustment_get_page_size(uzbl.gui.bar_v));
      else
        scroll(uzbl.gui.bar_v, argv1);
    }
    else
      if(uzbl.state.verbose)
        puts("Unrecognized scroll format");
}


/* VIEW funcs (little webkit wrappers) */
#define VIEWFUNC(name) void view_##name(WebKitWebView *page, GArray *argv, GString *result){(void)argv; (void)result; webkit_web_view_##name(page);}
VIEWFUNC(reload)
VIEWFUNC(reload_bypass_cache)
VIEWFUNC(stop_loading)
VIEWFUNC(zoom_in)
VIEWFUNC(zoom_out)
VIEWFUNC(go_back)
VIEWFUNC(go_forward)
#undef VIEWFUNC

/* -- command to callback/function map for things we cannot attach to any signals */
struct {const char *key; CommandInfo value;} cmdlist[] =
{   /* key                              function      no_split      */
    { "back",                           {view_go_back, 0}               },
    { "forward",                        {view_go_forward, 0}            },
    { "scroll",                         {scroll_cmd, 0}                 },
    { "reload",                         {view_reload, 0},               },
    { "reload_ign_cache",               {view_reload_bypass_cache, 0}   },
    { "stop",                           {view_stop_loading, 0},         },
    { "zoom_in",                        {view_zoom_in, 0},              }, //Can crash (when max zoom reached?).
    { "zoom_out",                       {view_zoom_out, 0},             },
    { "toggle_zoom_type",               {toggle_zoom_type, 0},          },
    { "uri",                            {load_uri, TRUE}                },
    { "js",                             {run_js, TRUE}                  },
    { "script",                         {run_external_js, 0}            },
    { "toggle_status",                  {toggle_status_cb, 0}           },
    { "spawn",                          {spawn_async, 0}                },
    { "sync_spawn",                     {spawn_sync, 0}                 }, // needed for cookie handler
    { "sync_spawn_exec",                {spawn_sync_exec, 0}            }, // needed for load_cookies.sh :(
    { "sh",                             {spawn_sh_async, 0}             },
    { "sync_sh",                        {spawn_sh_sync, 0}              }, // needed for cookie handler
    { "exit",                           {close_uzbl, 0}                 },
    { "search",                         {search_forward_text, TRUE}     },
    { "search_reverse",                 {search_reverse_text, TRUE}     },
    { "search_clear",                   {search_clear, TRUE}            },
    { "dehilight",                      {dehilight, 0}                  },
    { "set",                            {set_var, TRUE}                 },
    { "dump_config",                    {act_dump_config, 0}            },
    { "dump_config_as_events",          {act_dump_config_as_events, 0}  },
    { "chain",                          {chain, 0}                      },
    { "print",                          {print, TRUE}                   },
    { "event",                          {event, TRUE}                   },
    { "request",                        {event, TRUE}                   },
    { "menu_add",                       {menu_add, TRUE}                },
    { "menu_link_add",                  {menu_add_link, TRUE}           },
    { "menu_image_add",                 {menu_add_image, TRUE}          },
    { "menu_editable_add",              {menu_add_edit, TRUE}           },
    { "menu_separator",                 {menu_add_separator, TRUE}      },
    { "menu_link_separator",            {menu_add_separator_link, TRUE} },
    { "menu_image_separator",           {menu_add_separator_image, TRUE}},
    { "menu_editable_separator",        {menu_add_separator_edit, TRUE} },
    { "menu_remove",                    {menu_remove, TRUE}             },
    { "menu_link_remove",               {menu_remove_link, TRUE}        },
    { "menu_image_remove",              {menu_remove_image, TRUE}       },
    { "menu_editable_remove",           {menu_remove_edit, TRUE}        },
    { "hardcopy",                       {hardcopy, TRUE}                },
    { "include",                        {include, TRUE}                 },
    { "show_inspector",                 {show_inspector, 0}             },
    { "add_cookie",                     {add_cookie, 0}                 },
    { "delete_cookie",                  {delete_cookie, 0}              }
};

void
commands_hash(void)
{
    unsigned int i;
    uzbl.behave.commands = g_hash_table_new(g_str_hash, g_str_equal);

    for (i = 0; i < LENGTH(cmdlist); i++)
        g_hash_table_insert(uzbl.behave.commands, (gpointer) cmdlist[i].key, &cmdlist[i].value);
}

void
builtins() {
    unsigned int i,
             len = LENGTH(cmdlist);
    GString *command_list = g_string_new("");

    for (i = 0; i < len; i++) {
        g_string_append(command_list, cmdlist[i].key);
        g_string_append_c(command_list, ' ');
    }

    send_event(BUILTINS, command_list->str, NULL);
    g_string_free(command_list, TRUE);
}

/* -- CORE FUNCTIONS -- */

void
set_var(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;

    if(!argv_idx(argv, 0))
        return;

    gchar **split = g_strsplit(argv_idx(argv, 0), "=", 2);
    if (split[0] != NULL) {
        gchar *value = split[1] ? g_strchug(split[1]) : " ";
        set_var_value(g_strstrip(split[0]), value);
    }
    g_strfreev(split);
}

void
add_to_menu(GArray *argv, guint context) {
    GUI *g = &uzbl.gui;
    MenuItem *m;
    gchar *item_cmd = NULL;

    if(!argv_idx(argv, 0))
        return;

    gchar **split = g_strsplit(argv_idx(argv, 0), "=", 2);
    if(!g->menu_items)
        g->menu_items = g_ptr_array_new();

    if(split[1])
        item_cmd = split[1];

    if(split[0]) {
        m = malloc(sizeof(MenuItem));
        m->name = g_strdup(split[0]);
        m->cmd  = g_strdup(item_cmd?item_cmd:"");
        m->context = context;
        m->issep = FALSE;
        g_ptr_array_add(g->menu_items, m);
    }

    g_strfreev(split);
}

void
menu_add(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    add_to_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT);

}

void
menu_add_link(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    add_to_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK);
}

void
menu_add_image(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    add_to_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);
}

void
menu_add_edit(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    add_to_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE);
}

void
add_separator_to_menu(GArray *argv, guint context) {
    GUI *g = &uzbl.gui;
    MenuItem *m;
    gchar *sep_name;

    if(!g->menu_items)
        g->menu_items = g_ptr_array_new();

    if(!argv_idx(argv, 0))
        return;
    else
        sep_name = argv_idx(argv, 0);

    m = malloc(sizeof(MenuItem));
    m->name    = g_strdup(sep_name);
    m->cmd     = NULL;
    m->context = context;
    m->issep   = TRUE;
    g_ptr_array_add(g->menu_items, m);
}

void
menu_add_separator(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    add_separator_to_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT);
}

void
menu_add_separator_link(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    add_separator_to_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK);
}
void
menu_add_separator_image(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    add_separator_to_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);
}

void
menu_add_separator_edit(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    add_separator_to_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE);
}

void
remove_from_menu(GArray *argv, guint context) {
    GUI *g = &uzbl.gui;
    MenuItem *mi;
    gchar *name = NULL;
    guint i=0;

    if(!g->menu_items)
        return;

    if(!argv_idx(argv, 0))
        return;
    else
        name = argv_idx(argv, 0);

    for(i=0; i < g->menu_items->len; i++) {
        mi = g_ptr_array_index(g->menu_items, i);

        if((context == mi->context) && !strcmp(name, mi->name)) {
            g_free(mi->name);
            g_free(mi->cmd);
            g_free(mi);
            g_ptr_array_remove_index(g->menu_items, i);
        }
    }
}

void
menu_remove(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    remove_from_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT);
}

void
menu_remove_link(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    remove_from_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK);
}

void
menu_remove_image(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    remove_from_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);
}

void
menu_remove_edit(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;

    remove_from_menu(argv, WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE);
}

void
event(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    GString *event_name;
    gchar **split = NULL;

    if(!argv_idx(argv, 0))
       return;

    split = g_strsplit(argv_idx(argv, 0), " ", 2);
    if(split[0])
        event_name = g_string_ascii_up(g_string_new(split[0]));
    else
        return;

    send_event(0, split[1]?split[1]:"", event_name->str);

    g_string_free(event_name, TRUE);
    g_strfreev(split);
}

void
print(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar* buf;

    buf = expand(argv_idx(argv, 0), 0);
    g_string_assign(result, buf);
    g_free(buf);
}

void
hardcopy(WebKitWebView *page, GArray *argv, GString *result) {
    (void) argv;
    (void) result;

    webkit_web_frame_print(webkit_web_view_get_main_frame(page));
}

/* just a wrapper so parse_cmd_line can be used with for_each_line_in_file */
static void
parse_cmd_line_cb(const char *line, void *user_data) {
    (void) user_data;
    parse_cmd_line(line, NULL);
}

void
include(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;
    gchar *path = argv_idx(argv, 0);

    if(!path)
        return;

    if((path = find_existing_file(path))) {
        if(!for_each_line_in_file(path, parse_cmd_line_cb, NULL)) {
            gchar *tmp = g_strdup_printf("File %s can not be read.", path);
            send_event(COMMAND_ERROR, tmp, NULL);
            g_free(tmp);
        }

        send_event(FILE_INCLUDED, path, NULL);
        g_free(path);
    }
}

void
show_inspector(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) argv; (void) result;

    webkit_web_inspector_show(uzbl.gui.inspector);
}

void
add_cookie(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar *host, *path, *name, *value;
    gboolean secure = 0;
    SoupDate *expires = NULL;

    if(argv->len != 6)
        return;

    // Parse with same syntax as ADD_COOKIE event
    host = argv_idx (argv, 0);
    path = argv_idx (argv, 1);
    name = argv_idx (argv, 2);
    value = argv_idx (argv, 3);
    secure = strcmp (argv_idx (argv, 4), "https") == 0;
    if (strlen (argv_idx (argv, 5)) != 0)
        expires = soup_date_new_from_time_t (
            strtoul (argv_idx (argv, 5), NULL, 10));

    // Create new cookie
    SoupCookie * cookie = soup_cookie_new (name, value, host, path, -1);
    soup_cookie_set_secure (cookie, secure);
    if (expires)
        soup_cookie_set_expires (cookie, expires);

    // Add cookie to jar
    uzbl.net.soup_cookie_jar->in_manual_add = 1;
    soup_cookie_jar_add_cookie (SOUP_COOKIE_JAR (uzbl.net.soup_cookie_jar), cookie);
    uzbl.net.soup_cookie_jar->in_manual_add = 0;
}

void
delete_cookie(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;

    if(argv->len < 4)
        return;

    SoupCookie * cookie = soup_cookie_new (
        argv_idx (argv, 2),
        argv_idx (argv, 3),
        argv_idx (argv, 0),
        argv_idx (argv, 1),
        0);

    uzbl.net.soup_cookie_jar->in_manual_add = 1;
    soup_cookie_jar_delete_cookie (SOUP_COOKIE_JAR (uzbl.net.soup_cookie_jar), cookie);
    uzbl.net.soup_cookie_jar->in_manual_add = 0;
}

void
act_dump_config() {
    dump_config();
}

void
act_dump_config_as_events() {
    dump_config_as_events();
}

void
load_uri (WebKitWebView *web_view, GArray *argv, GString *result) {
    (void) web_view; (void) result;
    set_var_value("uri", argv_idx(argv, 0));
}

/* Javascript*/
void
eval_js(WebKitWebView * web_view, gchar *script, GString *result, const char *file) {
    WebKitWebFrame *frame;
    JSGlobalContextRef context;
    JSObjectRef globalobject;
    JSStringRef js_file;
    JSStringRef js_script;
    JSValueRef js_result;
    JSValueRef js_exc = NULL;
    JSStringRef js_result_string;
    size_t js_result_size;

    frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view));
    context = webkit_web_frame_get_global_context(frame);
    globalobject = JSContextGetGlobalObject(context);

    /* evaluate the script and get return value*/
    js_script = JSStringCreateWithUTF8CString(script);
    js_file = JSStringCreateWithUTF8CString(file);
    js_result = JSEvaluateScript(context, js_script, globalobject, js_file, 0, &js_exc);
    if (js_result && !JSValueIsUndefined(context, js_result)) {
        js_result_string = JSValueToStringCopy(context, js_result, NULL);
        js_result_size = JSStringGetMaximumUTF8CStringSize(js_result_string);

        if (js_result_size) {
            char js_result_utf8[js_result_size];
            JSStringGetUTF8CString(js_result_string, js_result_utf8, js_result_size);
            g_string_assign(result, js_result_utf8);
        }

        JSStringRelease(js_result_string);
    }
    else if (js_exc) {
        size_t size;
        JSStringRef prop, val;
        JSObjectRef exc = JSValueToObject(context, js_exc, NULL);

        printf("Exception occured while executing script:\n");

        /* Print file */
        prop = JSStringCreateWithUTF8CString("sourceURL");
        val = JSValueToStringCopy(context, JSObjectGetProperty(context, exc, prop, NULL), NULL);
        size = JSStringGetMaximumUTF8CStringSize(val);
        if(size) {
            char cstr[size];
            JSStringGetUTF8CString(val, cstr, size);
            printf("At %s", cstr);
        }
        JSStringRelease(prop);
        JSStringRelease(val);

        /* Print line */
        prop = JSStringCreateWithUTF8CString("line");
        val = JSValueToStringCopy(context, JSObjectGetProperty(context, exc, prop, NULL), NULL);
        size = JSStringGetMaximumUTF8CStringSize(val);
        if(size) {
            char cstr[size];
            JSStringGetUTF8CString(val, cstr, size);
            printf(":%s: ", cstr);
        }
        JSStringRelease(prop);
        JSStringRelease(val);

        /* Print message */
        val = JSValueToStringCopy(context, exc, NULL);
        size = JSStringGetMaximumUTF8CStringSize(val);
        if(size) {
            char cstr[size];
            JSStringGetUTF8CString(val, cstr, size);
            printf("%s\n", cstr);
        }
        JSStringRelease(val);
    }

    /* cleanup */
    JSStringRelease(js_script);
    JSStringRelease(js_file);
}

void
run_js (WebKitWebView * web_view, GArray *argv, GString *result) {
    if (argv_idx(argv, 0))
        eval_js(web_view, argv_idx(argv, 0), result, "(command)");
}

void
run_external_js (WebKitWebView * web_view, GArray *argv, GString *result) {
    (void) result;
    gchar *path = NULL;

    if (argv_idx(argv, 0) &&
        ((path = find_existing_file(argv_idx(argv, 0)))) ) {
        gchar *file_contents = NULL;

        GIOChannel *chan = g_io_channel_new_file(path, "r", NULL);
        if (chan) {
            gsize len;
            g_io_channel_read_to_end(chan, &file_contents, &len, NULL);
            g_io_channel_unref (chan);
        }

        if (uzbl.state.verbose)
            printf ("External JavaScript file %s loaded\n", argv_idx(argv, 0));

        gchar *js = str_replace("%s", argv_idx (argv, 1) ? argv_idx (argv, 1) : "", file_contents);
        g_free (file_contents);

        eval_js (web_view, js, result, path);
        g_free (js);
        g_free(path);
    }
}

void
search_text (WebKitWebView *page, GArray *argv, const gboolean forward) {
    if (argv_idx(argv, 0) && (*argv_idx(argv, 0) != '\0')) {
        if (g_strcmp0 (uzbl.state.searchtx, argv_idx(argv, 0)) != 0) {
            webkit_web_view_unmark_text_matches (page);
            webkit_web_view_mark_text_matches (page, argv_idx(argv, 0), FALSE, 0);
            g_free (uzbl.state.searchtx);
            uzbl.state.searchtx = g_strdup(argv_idx(argv, 0));
        }
    }


    if (uzbl.state.searchtx) {
        if (uzbl.state.verbose)
            printf ("Searching: %s\n", uzbl.state.searchtx);
        webkit_web_view_set_highlight_text_matches (page, TRUE);
        webkit_web_view_search_text (page, uzbl.state.searchtx, FALSE, forward, TRUE);
    }
}

void
search_clear(WebKitWebView *page, GArray *argv, GString *result) {
    (void) argv;
    (void) result;

    webkit_web_view_unmark_text_matches (page);
    uzbl.state.searchtx = strfree (uzbl.state.searchtx);
}

void
search_forward_text (WebKitWebView *page, GArray *argv, GString *result) {
    (void) result;
    search_text(page, argv, TRUE);
}

void
search_reverse_text (WebKitWebView *page, GArray *argv, GString *result) {
    (void) result;
    search_text(page, argv, FALSE);
}

void
dehilight (WebKitWebView *page, GArray *argv, GString *result) {
    (void) argv; (void) result;
    webkit_web_view_set_highlight_text_matches (page, FALSE);
}

void
chain (WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar *a = NULL;
    gchar **parts = NULL;
    guint i = 0;
    while ((a = argv_idx(argv, i++))) {
        parts = g_strsplit (a, " ", 2);
        if (parts[0])
          parse_command(parts[0], parts[1], result);
        g_strfreev (parts);
    }
}

void
close_uzbl (WebKitWebView *page, GArray *argv, GString *result) {
    (void)page;
    (void)argv;
    (void)result;
    gtk_main_quit ();
}

void
sharg_append(GArray *a, const gchar *str) {
    const gchar *s = (str ? str : "");
    g_array_append_val(a, s);
}

/* make sure that the args string you pass can properly be interpreted (eg
 * properly escaped against whitespace, quotes etc) */
gboolean
run_command (const gchar *command, const gchar **args, const gboolean sync,
             char **output_stdout) {
    GError *err = NULL;

    GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
    guint i;

    sharg_append(a, command);

    for (i = 0; i < g_strv_length((gchar**)args); i++)
        sharg_append(a, args[i]);

    gboolean result;
    if (sync) {
        if (*output_stdout) *output_stdout = strfree(*output_stdout);

        result = g_spawn_sync(NULL, (gchar **)a->data, NULL, G_SPAWN_SEARCH_PATH,
                              NULL, NULL, output_stdout, NULL, NULL, &err);
    } else
        result = g_spawn_async(NULL, (gchar **)a->data, NULL, G_SPAWN_SEARCH_PATH,
                                  NULL, NULL, NULL, &err);

    if (uzbl.state.verbose) {
        GString *s = g_string_new("spawned:");
        for (i = 0; i < (a->len); i++) {
            gchar *qarg = g_shell_quote(g_array_index(a, gchar*, i));
            g_string_append_printf(s, " %s", qarg);
            g_free (qarg);
        }
        g_string_append_printf(s, " -- result: %s", (result ? "true" : "false"));
        printf("%s\n", s->str);
        g_string_free(s, TRUE);
        if(output_stdout) {
            printf("Stdout: %s\n", *output_stdout);
        }
    }
    if (err) {
        g_printerr("error on run_command: %s\n", err->message);
        g_error_free (err);
    }
    g_array_free (a, TRUE);
    return result;
}

/*@null@*/ gchar**
split_quoted(const gchar* src, const gboolean unquote) {
    /* split on unquoted space or tab, return array of strings;
       remove a layer of quotes and backslashes if unquote */
    if (!src) return NULL;

    gboolean dq = FALSE;
    gboolean sq = FALSE;
    GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
    GString *s = g_string_new ("");
    const gchar *p;
    gchar **ret;
    gchar *dup;
    for (p = src; *p != '\0'; p++) {
        if ((*p == '\\') && unquote) g_string_append_c(s, *++p);
        else if (*p == '\\') { g_string_append_c(s, *p++);
                               g_string_append_c(s, *p); }
        else if ((*p == '"') && unquote && !sq) dq = !dq;
        else if (*p == '"' && !sq) { g_string_append_c(s, *p);
                                     dq = !dq; }
        else if ((*p == '\'') && unquote && !dq) sq = !sq;
        else if (*p == '\'' && !dq) { g_string_append_c(s, *p);
                                      sq = ! sq; }
        else if ((*p == ' ' || *p == '\t') && !dq && !sq) {
            dup = g_strdup(s->str);
            g_array_append_val(a, dup);
            g_string_truncate(s, 0);
        } else g_string_append_c(s, *p);
    }
    dup = g_strdup(s->str);
    g_array_append_val(a, dup);
    ret = (gchar**)a->data;
    g_array_free (a, FALSE);
    g_string_free (s, TRUE);
    return ret;
}

void
spawn(GArray *argv, gboolean sync, gboolean exec) {
    gchar *path = NULL;
    gchar *arg_car = argv_idx(argv, 0);
    const gchar **arg_cdr = &g_array_index(argv, const gchar *, 1);

    if (arg_car && (path = find_existing_file(arg_car))) {
        if (uzbl.comm.sync_stdout)
            uzbl.comm.sync_stdout = strfree(uzbl.comm.sync_stdout);
        run_command(path, arg_cdr, sync, sync?&uzbl.comm.sync_stdout:NULL);
        // run each line of output from the program as a command
        if (sync && exec && uzbl.comm.sync_stdout) {
            gchar *head = uzbl.comm.sync_stdout;
            gchar *tail;
            while ((tail = strchr (head, '\n'))) {
                *tail = '\0';
                parse_cmd_line(head, NULL);
                head = tail + 1;
            }
        }
        g_free(path);
    }
}

void
spawn_async(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn(argv, FALSE, FALSE);
}

void
spawn_sync(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn(argv, TRUE, FALSE);
}

void
spawn_sync_exec(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn(argv, TRUE, TRUE);
}

void
spawn_sh(GArray *argv, gboolean sync) {
    if (!uzbl.behave.shell_cmd) {
        g_printerr ("spawn_sh: shell_cmd is not set!\n");
        return;
    }
    guint i;

    gchar **cmd = split_quoted(uzbl.behave.shell_cmd, TRUE);
    gchar *cmdname = g_strdup(cmd[0]);
    g_array_insert_val(argv, 1, cmdname);

    for (i = 1; i < g_strv_length(cmd); i++)
        g_array_prepend_val(argv, cmd[i]);

    if (cmd) {
        if (uzbl.comm.sync_stdout)
            uzbl.comm.sync_stdout = strfree(uzbl.comm.sync_stdout);

        run_command(cmd[0], (const gchar **) argv->data,
                    sync, sync?&uzbl.comm.sync_stdout:NULL);
    }
    g_free (cmdname);
    g_strfreev (cmd);
}

void
spawn_sh_async(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn_sh(argv, FALSE);
}

void
spawn_sh_sync(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn_sh(argv, TRUE);
}

void
parse_command(const char *cmd, const char *param, GString *result) {
    CommandInfo *c;
    GString *tmp = g_string_new("");

    if ((c = g_hash_table_lookup(uzbl.behave.commands, cmd))) {
            guint i;
            gchar **par = split_quoted(param, TRUE);
            GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));

            if (c->no_split) { /* don't split */
                sharg_append(a, param);
            } else if (par) {
                for (i = 0; i < g_strv_length(par); i++)
                    sharg_append(a, par[i]);
            }

            if (result == NULL) {
                GString *result_print = g_string_new("");

                c->function(uzbl.gui.web_view, a, result_print);
                if (result_print->len)
                    printf("%*s\n", (int)result_print->len, result_print->str);

                g_string_free(result_print, TRUE);
            } else {
                c->function(uzbl.gui.web_view, a, result);
            }
            g_strfreev (par);
            g_array_free (a, TRUE);

            if(strcmp("set", cmd)     &&
               strcmp("event", cmd)   &&
               strcmp("request", cmd)) {
                g_string_printf(tmp, "%s %s", cmd, param?param:"");
                send_event(COMMAND_EXECUTED, tmp->str, NULL);
            }
    }
    else {
		g_string_printf (tmp, "%s %s", cmd, param?param:"");
        send_event(COMMAND_ERROR, tmp->str, NULL);
    }
    g_string_free(tmp, TRUE);
}


void
move_statusbar() {
    if (!uzbl.gui.scrolled_win &&
            !uzbl.gui.mainbar)
        return;

    g_object_ref(uzbl.gui.scrolled_win);
    g_object_ref(uzbl.gui.mainbar);
    gtk_container_remove(GTK_CONTAINER(uzbl.gui.vbox), uzbl.gui.scrolled_win);
    gtk_container_remove(GTK_CONTAINER(uzbl.gui.vbox), uzbl.gui.mainbar);

    if(uzbl.behave.status_top) {
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.mainbar, FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE, TRUE, 0);
    }
    else {
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.mainbar, FALSE, TRUE, 0);
    }
    g_object_unref(uzbl.gui.scrolled_win);
    g_object_unref(uzbl.gui.mainbar);
    gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    return;
}

gboolean
set_var_value(const gchar *name, gchar *val) {
    uzbl_cmdprop *c = NULL;
    char *endp = NULL;
    char *buf = NULL;
    char *invalid_chars = "\t^°!\"§$%&/()=?'`'+~*'#-:,;@<>| \\{}[]¹²³¼½";
    GString *msg;

    if( (c = g_hash_table_lookup(uzbl.comm.proto_var, name)) ) {
        if(!c->writeable) return FALSE;

        msg = g_string_new(name);

        /* check for the variable type */
        if (c->type == TYPE_STR) {
            buf = g_strdup(val);
            g_free(*c->ptr.s);
            *c->ptr.s = buf;
            g_string_append_printf(msg, " str %s", buf);

        } else if(c->type == TYPE_INT) {
            *c->ptr.i = (int)strtoul(val, &endp, 10);
            g_string_append_printf(msg, " int %d", *c->ptr.i);

        } else if (c->type == TYPE_FLOAT) {
            *c->ptr.f = strtod(val, &endp);
            g_string_append_printf(msg, " float %f", *c->ptr.f);
        }

        send_event(VARIABLE_SET, msg->str, NULL);
        g_string_free(msg,TRUE);

        /* invoke a command specific function */
        if(c->func) c->func();
    } else {
        /* check wether name violates our naming scheme */
        if(strpbrk(name, invalid_chars)) {
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

        msg = g_string_new(name);
        g_string_append_printf(msg, " str %s", buf);
        send_event(VARIABLE_SET, msg->str, NULL);
        g_string_free(msg,TRUE);
    }
    update_title();
    return TRUE;
}

void
parse_cmd_line(const char *ctl_line, GString *result) {
    size_t len=0;
    gchar *ctlstrip = NULL;
    gchar *exp_line = NULL;
    gchar *work_string = NULL;

    work_string = g_strdup(ctl_line);

    /* strip trailing newline */
    len = strlen(ctl_line);
    if (work_string[len - 1] == '\n')
        ctlstrip = g_strndup(work_string, len - 1);
    else
        ctlstrip = g_strdup(work_string);
    g_free(work_string);

    if( strcmp(g_strchug(ctlstrip), "") &&
        strcmp(exp_line = expand(ctlstrip, 0), "")
      ) {
            /* ignore comments */
            if((exp_line[0] == '#'))
                ;

            /* parse a command */
            else {
                gchar **tokens = NULL;

                tokens = g_strsplit(exp_line, " ", 2);
                parse_command(tokens[0], tokens[1], result);
                g_strfreev(tokens);
            }
        g_free(exp_line);
    }

    g_free(ctlstrip);
}

/*@null@*/ gchar*
build_stream_name(int type, const gchar* dir) {
    State *s = &uzbl.state;
    gchar *str = NULL;

    if (type == FIFO) {
        str = g_strdup_printf
            ("%s/uzbl_fifo_%s", dir, s->instance_name);
    } else if (type == SOCKET) {
        str = g_strdup_printf
            ("%s/uzbl_socket_%s", dir, s->instance_name);
    }
    return str;
}

gboolean
control_fifo(GIOChannel *gio, GIOCondition condition) {
    if (uzbl.state.verbose)
        printf("triggered\n");
    gchar *ctl_line;
    GIOStatus ret;
    GError *err = NULL;

    if (condition & G_IO_HUP)
        g_error ("Fifo: Read end of pipe died!\n");

    if(!gio)
       g_error ("Fifo: GIOChannel broke\n");

    ret = g_io_channel_read_line(gio, &ctl_line, NULL, NULL, &err);
    if (ret == G_IO_STATUS_ERROR) {
        g_error ("Fifo: Error reading: %s\n", err->message);
        g_error_free (err);
    }

    parse_cmd_line(ctl_line, NULL);
    g_free(ctl_line);

    return TRUE;
}

gboolean
attach_fifo(gchar *path) {
    GError *error = NULL;
    /* we don't really need to write to the file, but if we open the
     * file as 'r' we will block here, waiting for a writer to open
     * the file. */
    GIOChannel *chan = g_io_channel_new_file(path, "r+", &error);
    if (chan) {
        if (g_io_add_watch(chan, G_IO_IN|G_IO_HUP, (GIOFunc) control_fifo, NULL)) {
            if (uzbl.state.verbose)
                printf ("attach_fifo: created successfully as %s\n", path);
            send_event(FIFO_SET, path, NULL);
            uzbl.comm.fifo_path = path;
            g_setenv("UZBL_FIFO", uzbl.comm.fifo_path, TRUE);
            return TRUE;
        } else g_warning ("attach_fifo: could not add watch on %s\n", path);
    } else g_warning ("attach_fifo: can't open: %s\n", error->message);

    if (error) g_error_free (error);
    return FALSE;
}

/*@null@*/ gchar*
init_fifo(gchar *dir) { /* return dir or, on error, free dir and return NULL */
    if (uzbl.comm.fifo_path) { /* get rid of the old fifo if one exists */
        if (unlink(uzbl.comm.fifo_path) == -1)
            g_warning ("Fifo: Can't unlink old fifo at %s\n", uzbl.comm.fifo_path);
        g_free(uzbl.comm.fifo_path);
        uzbl.comm.fifo_path = NULL;
    }

    gchar *path = build_stream_name(FIFO, dir);

    if (!file_exists(path)) {
        if (mkfifo (path, 0666) == 0 && attach_fifo(path)) {
            return dir;
        } else g_warning ("init_fifo: can't create %s: %s\n", path, strerror(errno));
    } else {
        /* the fifo exists. but is anybody home? */
        int fd = open(path, O_WRONLY|O_NONBLOCK);
        if(fd < 0) {
            /* some error occurred, presumably nobody's on the read end.
             * we can attach ourselves to it. */
            if(attach_fifo(path))
                return dir;
            else
                g_warning("init_fifo: can't attach to %s: %s\n", path, strerror(errno));
        } else {
            /* somebody's there, we can't use that fifo. */
            close(fd);
            /* whatever, this instance can live without a fifo. */
            g_warning ("init_fifo: can't create %s: file exists and is occupied\n", path);
        }
    }

    /* if we got this far, there was an error; cleanup */
    g_free(dir);
    g_free(path);
    return NULL;
}

gboolean
control_stdin(GIOChannel *gio, GIOCondition condition) {
    (void) condition;
    gchar *ctl_line = NULL;
    GIOStatus ret;

    ret = g_io_channel_read_line(gio, &ctl_line, NULL, NULL, NULL);
    if ( (ret == G_IO_STATUS_ERROR) || (ret == G_IO_STATUS_EOF) )
        return FALSE;

    parse_cmd_line(ctl_line, NULL);
    g_free(ctl_line);

    return TRUE;
}

void
create_stdin () {
    GIOChannel *chan = NULL;
    GError *error = NULL;

    chan = g_io_channel_unix_new(fileno(stdin));
    if (chan) {
        if (!g_io_add_watch(chan, G_IO_IN|G_IO_HUP, (GIOFunc) control_stdin, NULL)) {
            g_error ("Stdin: could not add watch\n");
        } else {
            if (uzbl.state.verbose)
                printf ("Stdin: watch added successfully\n");
        }
    } else {
        g_error ("Stdin: Error while opening: %s\n", error->message);
    }
    if (error) g_error_free (error);
}

gboolean
remove_socket_from_array(GIOChannel *chan) {
    gboolean ret = 0;

    ret = g_ptr_array_remove_fast(uzbl.comm.connect_chan, chan);
    if(!ret)
        ret = g_ptr_array_remove_fast(uzbl.comm.client_chan, chan);

    if(ret)
        g_io_channel_unref (chan);
    return ret;
}

gboolean
control_socket(GIOChannel *chan) {
    struct sockaddr_un remote;
    unsigned int t = sizeof(remote);
    GIOChannel *iochan;
    int clientsock;

    clientsock = accept (g_io_channel_unix_get_fd(chan),
                         (struct sockaddr *) &remote, &t);

    if(!uzbl.comm.client_chan)
        uzbl.comm.client_chan = g_ptr_array_new();

    if ((iochan = g_io_channel_unix_new(clientsock))) {
        g_io_channel_set_encoding(iochan, NULL, NULL);
        g_io_add_watch(iochan, G_IO_IN|G_IO_HUP,
                       (GIOFunc) control_client_socket, iochan);
        g_ptr_array_add(uzbl.comm.client_chan, (gpointer)iochan);
    }
    return TRUE;
}

void
init_connect_socket() {
    int sockfd, replay = 0;
    struct sockaddr_un local;
    GIOChannel *chan;
    gchar **name = NULL;

    if(!uzbl.comm.connect_chan)
        uzbl.comm.connect_chan = g_ptr_array_new();

    name = uzbl.state.connect_socket_names;

    while(name && *name) {
        sockfd = socket (AF_UNIX, SOCK_STREAM, 0);
        local.sun_family = AF_UNIX;
        strcpy (local.sun_path, *name);

        if(!connect(sockfd, (struct sockaddr *) &local, sizeof(local))) {
            if ((chan = g_io_channel_unix_new(sockfd))) {
                g_io_channel_set_encoding(chan, NULL, NULL);
                g_io_add_watch(chan, G_IO_IN|G_IO_HUP,
                        (GIOFunc) control_client_socket, chan);
                g_ptr_array_add(uzbl.comm.connect_chan, (gpointer)chan);
                replay++;
            }
        }
        else
            g_warning("Error connecting to socket: %s\n", *name);
        name++;
    }

    /* replay buffered events */
    if(replay)
        send_event_socket(NULL);
}

gboolean
control_client_socket(GIOChannel *clientchan) {
    char *ctl_line;
    GString *result = g_string_new("");
    GError *error = NULL;
    GIOStatus ret;
    gsize len;

    ret = g_io_channel_read_line(clientchan, &ctl_line, &len, NULL, &error);
    if (ret == G_IO_STATUS_ERROR) {
        g_warning ("Error reading: %s", error->message);
        g_clear_error (&error);
        ret = g_io_channel_shutdown (clientchan, TRUE, &error); 
        remove_socket_from_array (clientchan);
        if (ret == G_IO_STATUS_ERROR) {
            g_warning ("Error closing: %s", error->message);
            g_clear_error (&error);
        }
        return FALSE;
    } else if (ret == G_IO_STATUS_EOF) {
        /* shutdown and remove channel watch from main loop */
        ret = g_io_channel_shutdown (clientchan, TRUE, &error); 
        remove_socket_from_array (clientchan);
        if (ret == G_IO_STATUS_ERROR) {
            g_warning ("Error closing: %s", error->message);
            g_clear_error (&error);
        }
        return FALSE;
    }

    if (ctl_line) {
        parse_cmd_line (ctl_line, result);
        g_string_append_c(result, '\n');
        ret = g_io_channel_write_chars (clientchan, result->str, result->len,
                                        &len, &error);
        if (ret == G_IO_STATUS_ERROR) {
            g_warning ("Error writing: %s", error->message);
            g_clear_error (&error);
        }
        if (g_io_channel_flush(clientchan, &error) == G_IO_STATUS_ERROR) {
            g_warning ("Error flushing: %s", error->message);
            g_clear_error (&error);
        }
    }

    g_string_free(result, TRUE);
    g_free(ctl_line);
    return TRUE;
}


gboolean
attach_socket(gchar *path, struct sockaddr_un *local) {
    GIOChannel *chan = NULL;
    int sock = socket (AF_UNIX, SOCK_STREAM, 0);

    if (bind (sock, (struct sockaddr *) local, sizeof(*local)) != -1) {
        if (uzbl.state.verbose)
            printf ("init_socket: opened in %s\n", path);

        if(listen (sock, 5) < 0)
            g_warning ("attach_socket: could not listen on %s: %s\n", path, strerror(errno));

        if( (chan = g_io_channel_unix_new(sock)) ) {
            g_io_add_watch(chan, G_IO_IN|G_IO_HUP, (GIOFunc) control_socket, chan);
            uzbl.comm.socket_path = path;
            send_event(SOCKET_SET, path, NULL);
            g_setenv("UZBL_SOCKET", uzbl.comm.socket_path, TRUE);
            return TRUE;
        }
    } else g_warning ("attach_socket: could not bind to %s: %s\n", path, strerror(errno));

    return FALSE;
}


/*@null@*/ gchar*
init_socket(gchar *dir) { /* return dir or, on error, free dir and return NULL */
    if (uzbl.comm.socket_path) { /* remove an existing socket should one exist */
        if (unlink(uzbl.comm.socket_path) == -1)
            g_warning ("init_socket: couldn't unlink socket at %s\n", uzbl.comm.socket_path);
        g_free(uzbl.comm.socket_path);
        uzbl.comm.socket_path = NULL;
    }

    if (*dir == ' ') {
        g_free(dir);
        return NULL;
    }

    struct sockaddr_un local;
    gchar *path = build_stream_name(SOCKET, dir);

    local.sun_family = AF_UNIX;
    strcpy (local.sun_path, path);

    if(!file_exists(path) && attach_socket(path, &local)) {
        /* it's free for the taking. */
        return dir;
    } else {
        /* see if anybody's listening on the socket path we want. */
        int sock = socket (AF_UNIX, SOCK_STREAM, 0);
        if(connect(sock, (struct sockaddr *) &local, sizeof(local)) < 0) {
            /* some error occurred, presumably nobody's listening.
             * we can attach ourselves to it. */
            unlink(path);
            if(attach_socket(path, &local))
                return dir;
            else
                g_warning("init_socket: can't attach to existing socket %s: %s\n", path, strerror(errno));
        } else {
            /* somebody's there, we can't use that socket path. */
            close(sock);
            /* whatever, this instance can live without a socket. */
            g_warning ("init_socket: can't create %s: socket exists and is occupied\n", path);
        }
    }

    /* if we got this far, there was an error; cleanup */
    g_free(path);
    g_free(dir);
    return NULL;
}

void
update_title (void) {
    Behaviour *b = &uzbl.behave;

    const gchar *title_format = b->title_format_long;

    /* Update the status bar if shown */
    if (b->show_status) {
        title_format = b->title_format_short;

        /* Left side */
        if (b->status_format && GTK_IS_LABEL(uzbl.gui.mainbar_label_left)) {
            gchar *parsed = expand(b->status_format, 0);
            gtk_label_set_markup(GTK_LABEL(uzbl.gui.mainbar_label_left), parsed);
            g_free(parsed);
        }

        /* Right side */
        if (b->status_format_right && GTK_IS_LABEL(uzbl.gui.mainbar_label_right)) {
            gchar *parsed = expand(b->status_format_right, 0);
            gtk_label_set_markup(GTK_LABEL(uzbl.gui.mainbar_label_right), parsed);
            g_free(parsed);
        }
    }

    /* Update window title */
    /* If we're starting up or shutting down there might not be a window yet. */
    gboolean have_main_window = !uzbl.state.plug_mode && GTK_IS_WINDOW(uzbl.gui.main_window);
    if (title_format && have_main_window) {
        gchar *parsed = expand(title_format, 0);
        const gchar *current_title = gtk_window_get_title (GTK_WINDOW(uzbl.gui.main_window));
        /* xmonad hogs CPU if the window title updates too frequently, so we
         * don't set it unless we need to. */
        if(!current_title || strcmp(current_title, parsed))
            gtk_window_set_title (GTK_WINDOW(uzbl.gui.main_window), parsed);
        g_free(parsed);
    }
}

void
create_browser () {
    GUI *g = &uzbl.gui;

    g->web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());

    g_object_connect((GObject*)g->web_view,
      "signal::key-press-event",                      (GCallback)key_press_cb,            NULL,
      "signal::key-release-event",                    (GCallback)key_release_cb,          NULL,
      "signal::button-press-event",                   (GCallback)button_press_cb,         NULL,
      "signal::button-release-event",                 (GCallback)button_release_cb,       NULL,
      "signal::motion-notify-event",                  (GCallback)motion_notify_cb,        NULL,
      "signal::notify::title",                        (GCallback)title_change_cb,         NULL,
      "signal::selection-changed",                    (GCallback)selection_changed_cb,    NULL,
      "signal::notify::progress",                     (GCallback)progress_change_cb,      NULL,
      "signal::notify::load-status",                  (GCallback)load_status_change_cb,   NULL,
      "signal::load-error",                           (GCallback)load_error_cb,           NULL,
      "signal::hovering-over-link",                   (GCallback)link_hover_cb,           NULL,
      "signal::navigation-policy-decision-requested", (GCallback)navigation_decision_cb,  NULL,
      "signal::new-window-policy-decision-requested", (GCallback)new_window_cb,           NULL,
      "signal::download-requested",                   (GCallback)download_cb,             NULL,
      "signal::create-web-view",                      (GCallback)create_web_view_cb,      NULL,
      "signal::mime-type-policy-decision-requested",  (GCallback)mime_policy_cb,          NULL,
      "signal::resource-request-starting",            (GCallback)request_starting_cb,     NULL,
      "signal::populate-popup",                       (GCallback)populate_popup_cb,       NULL,
      "signal::focus-in-event",                       (GCallback)focus_cb,                NULL,
      "signal::focus-out-event",                      (GCallback)focus_cb,                NULL,
      NULL);
}

GtkWidget*
create_mainbar () {
    GUI *g = &uzbl.gui;

    g->mainbar = gtk_hbox_new (FALSE, 0);

    /* Left panel */
    g->mainbar_label_left = gtk_label_new ("");
    gtk_label_set_selectable(GTK_LABEL(g->mainbar_label_left), TRUE);
    gtk_misc_set_alignment (GTK_MISC(g->mainbar_label_left), 0, 0);
    gtk_misc_set_padding (GTK_MISC(g->mainbar_label_left), 2, 2);

    gtk_box_pack_start (GTK_BOX (g->mainbar), g->mainbar_label_left, FALSE, FALSE, 0);

    /* Right panel */
    g->mainbar_label_right = gtk_label_new ("");
    gtk_label_set_selectable(GTK_LABEL(g->mainbar_label_right), TRUE);
    gtk_misc_set_alignment (GTK_MISC(g->mainbar_label_right), 1, 0);
    gtk_misc_set_padding (GTK_MISC(g->mainbar_label_right), 2, 2);
    gtk_label_set_ellipsize(GTK_LABEL(g->mainbar_label_right), PANGO_ELLIPSIZE_START);

    gtk_box_pack_start (GTK_BOX (g->mainbar), g->mainbar_label_right, TRUE, TRUE, 0);

    g_object_connect((GObject*)g->mainbar,
      "signal::key-press-event",                    (GCallback)key_press_cb,    NULL,
      "signal::key-release-event",                  (GCallback)key_release_cb,  NULL,
      NULL);

    return g->mainbar;
}


GtkWidget*
create_window () {
    GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
    gtk_widget_set_name (window, "Uzbl browser");

    /* if the window has been made small, it shouldn't try to resize itself due
     * to a long statusbar. */
    GdkGeometry hints;
    hints.min_height = -1;
    hints.min_width  =  1;
    gtk_window_set_geometry_hints (GTK_WINDOW (window), window, &hints, GDK_HINT_MIN_SIZE);

    g_signal_connect (G_OBJECT (window), "destroy",         G_CALLBACK (destroy_cb),         NULL);
    g_signal_connect (G_OBJECT (window), "configure-event", G_CALLBACK (configure_event_cb), NULL);

    return window;
}

GtkPlug*
create_plug () {
    GtkPlug* plug = GTK_PLUG (gtk_plug_new (uzbl.state.socket_id));
    g_signal_connect (G_OBJECT (plug), "destroy", G_CALLBACK (destroy_cb), NULL);
    g_signal_connect (G_OBJECT (plug), "key-press-event", G_CALLBACK (key_press_cb), NULL);
    g_signal_connect (G_OBJECT (plug), "key-release-event", G_CALLBACK (key_release_cb), NULL);

    return plug;
}


gchar**
inject_handler_args(const gchar *actname, const gchar *origargs, const gchar *newargs) {
    /*
      If actname is one that calls an external command, this function will inject
      newargs in front of the user-provided args in that command line.  They will
      come become after the body of the script (in sh) or after the name of
      the command to execute (in spawn).
      i.e. sh <body> <userargs> becomes sh <body> <ARGS> <userargs> and
      spawn <command> <userargs> becomes spawn <command> <ARGS> <userargs>.

      The return value consist of two strings: the action (sh, ...) and its args.

      If act is not one that calls an external command, then the given action merely
      gets duplicated.
    */
    GArray *rets = g_array_new(TRUE, FALSE, sizeof(gchar*));
    /* Arrr! Here be memory leaks */
    gchar *actdup = g_strdup(actname);
    g_array_append_val(rets, actdup);

    if ((g_strcmp0(actname, "spawn") == 0) ||
        (g_strcmp0(actname, "sh") == 0) ||
        (g_strcmp0(actname, "sync_spawn") == 0) ||
        (g_strcmp0(actname, "sync_sh") == 0)) {
        guint i;
        GString *a = g_string_new("");
        gchar **spawnparts = split_quoted(origargs, FALSE);
        g_string_append_printf(a, "%s", spawnparts[0]); /* sh body or script name */
        if (newargs) g_string_append_printf(a, " %s", newargs); /* handler args */

        for (i = 1; i < g_strv_length(spawnparts); i++) /* user args */
            if (spawnparts[i]) g_string_append_printf(a, " %s", spawnparts[i]);

        g_array_append_val(rets, a->str);
        g_string_free(a, FALSE);
        g_strfreev(spawnparts);
    } else {
        gchar *origdup = g_strdup(origargs);
        g_array_append_val(rets, origdup);
    }
    return (gchar**)g_array_free(rets, FALSE);
}

void
run_handler (const gchar *act, const gchar *args) {
    /* Consider this code a temporary hack to make the handlers usable.
       In practice, all this splicing, injection, and reconstruction is
       inefficient, annoying and hard to manage.  Potential pitfalls arise
       when the handler specific args 1) are not quoted  (the handler
       callbacks should take care of this)  2) are quoted but interfere
       with the users' own quotation.  A more ideal solution is
       to refactor parse_command so that it doesn't just take a string
       and execute it; rather than that, we should have a function which
       returns the argument vector parsed from the string.  This vector
       could be modified (e.g. insert additional args into it) before
       passing it to the next function that actually executes it.  Though
       it still isn't perfect for chain actions..  will reconsider & re-
       factor when I have the time. -duc */

    if (!act) return;
    char **parts = g_strsplit(act, " ", 2);
    if (!parts || !parts[0]) return;
    if (g_strcmp0(parts[0], "chain") == 0) {
        GString *newargs = g_string_new("");
        gchar **chainparts = split_quoted(parts[1], FALSE);

        /* for every argument in the chain, inject the handler args
           and make sure the new parts are wrapped in quotes */
        gchar **cp = chainparts;
        gchar quot = '\'';
        gchar *quotless = NULL;
        gchar **spliced_quotless = NULL; // sigh -_-;
        gchar **inpart = NULL;

        while (*cp) {
            if ((**cp == '\'') || (**cp == '\"')) { /* strip old quotes */
                quot = **cp;
                quotless = g_strndup(&(*cp)[1], strlen(*cp) - 2);
            } else quotless = g_strdup(*cp);

            spliced_quotless = g_strsplit(quotless, " ", 2);
            inpart = inject_handler_args(spliced_quotless[0], spliced_quotless[1], args);
            g_strfreev(spliced_quotless);

            g_string_append_printf(newargs, " %c%s %s%c", quot, inpart[0], inpart[1], quot);
            g_free(quotless);
            g_strfreev(inpart);
            cp++;
        }

        parse_command(parts[0], &(newargs->str[1]), NULL);
        g_string_free(newargs, TRUE);
        g_strfreev(chainparts);

    } else {
        gchar **inparts;
        gchar *inparts_[2];
        if (parts[1]) {
            /* expand the user-specified arguments */
            gchar* expanded = expand(parts[1], 0);
            inparts = inject_handler_args(parts[0], expanded, args);
            g_free(expanded);
        } else {
            inparts_[0] = parts[0];
            inparts_[1] = g_strdup(args);
            inparts = inparts_;
        }

        parse_command(inparts[0], inparts[1], NULL);

        if (inparts != inparts_) {
            g_free(inparts[0]);
            g_free(inparts[1]);
        } else
            g_free(inparts[1]);
    }
    g_strfreev(parts);
}

void
settings_init () {
    State *s = &uzbl.state;
    Network *n = &uzbl.net;

    int i;
    for (i = 0; default_config[i].command != NULL; i++) {
        parse_cmd_line(default_config[i].command, NULL);
    }

    if (g_strcmp0(s->config_file, "-") == 0) {
        s->config_file = NULL;
        create_stdin();
    }

    else if (!s->config_file) {
        s->config_file = find_xdg_file (0, "/uzbl/config");
    }

    if (s->config_file) {
        if(!for_each_line_in_file(s->config_file, parse_cmd_line_cb, NULL)) {
            gchar *tmp = g_strdup_printf("File %s can not be read.", s->config_file);
            send_event(COMMAND_ERROR, tmp, NULL);
            g_free(tmp);
        }
        g_setenv("UZBL_CONFIG", s->config_file, TRUE);
    } else if (uzbl.state.verbose)
        printf ("No configuration file loaded.\n");

    if(s->connect_socket_names)
        init_connect_socket();

    g_signal_connect(n->soup_session, "authenticate", G_CALLBACK(handle_authentication), NULL);
}

void handle_authentication (SoupSession *session, SoupMessage *msg, SoupAuth *auth, gboolean retrying, gpointer user_data) {

    (void) user_data;

    if(uzbl.behave.authentication_handler && *uzbl.behave.authentication_handler != 0) {
        gchar *info, *host, *realm;
        gchar *p;

        soup_session_pause_message(session, msg);

        /* Sanitize strings */
            info  = g_strdup(soup_auth_get_info(auth));
            host  = g_strdup(soup_auth_get_host(auth));
            realm = g_strdup(soup_auth_get_realm(auth));
            for (p = info; *p; p++)  if (*p == '\'') *p = '\"';
            for (p = host; *p; p++)  if (*p == '\'') *p = '\"';
            for (p = realm; *p; p++) if (*p == '\'') *p = '\"';

        GString *s = g_string_new ("");
        g_string_printf(s, "'%s' '%s' '%s' '%s'",
            info, host, realm, retrying?"TRUE":"FALSE");

        run_handler(uzbl.behave.authentication_handler, s->str);

        if (uzbl.comm.sync_stdout && strcmp (uzbl.comm.sync_stdout, "") != 0) {
            char  *username, *password;
            int    number_of_endls=0;

            username = uzbl.comm.sync_stdout;

            for (p = uzbl.comm.sync_stdout; *p; p++) {
                if (*p == '\n') {
                    *p = '\0';
                    if (++number_of_endls == 1)
                        password = p + 1;
                }
            }

            /* If stdout was correct (contains exactly two lines of text) do
             * authenticate. */
            if (number_of_endls == 2)
                soup_auth_authenticate(auth, username, password);
        }

        if (uzbl.comm.sync_stdout)
            uzbl.comm.sync_stdout = strfree(uzbl.comm.sync_stdout);

        soup_session_unpause_message(session, msg);

        g_string_free(s, TRUE);
        g_free(info);
        g_free(host);
        g_free(realm);
    }
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
    GString *msg;

    if(!c->dump)
        return;

    /* check for the variable type */
    msg = g_string_new((char *)k);
    if (c->type == TYPE_STR) {
        g_string_append_printf(msg, " str %s", *c->ptr.s ? *c->ptr.s : " ");
    } else if(c->type == TYPE_INT) {
        g_string_append_printf(msg, " int %d", *c->ptr.i);
    } else if (c->type == TYPE_FLOAT) {
        g_string_append_printf(msg, " float %f", *c->ptr.f);
    }

    send_event(VARIABLE_SET, msg->str, NULL);
    g_string_free(msg, TRUE);
}

void
dump_config_as_events() {
    g_hash_table_foreach(uzbl.comm.proto_var, dump_var_hash_as_event, NULL);
}

void
retrieve_geometry() {
    int w, h, x, y;
    GString *buf = g_string_new("");

    gtk_window_get_size(GTK_WINDOW(uzbl.gui.main_window), &w, &h);
    gtk_window_get_position(GTK_WINDOW(uzbl.gui.main_window), &x, &y);

    g_string_printf(buf, "%dx%d+%d+%d", w, h, x, y);

    if(uzbl.gui.geometry)
        g_free(uzbl.gui.geometry);
    uzbl.gui.geometry = g_string_free(buf, FALSE);
}

void
set_webview_scroll_adjustments() {
#if GTK_CHECK_VERSION(2,91,0)
    gtk_scrollable_set_hadjustment (GTK_SCROLLABLE(uzbl.gui.web_view), uzbl.gui.bar_h);
    gtk_scrollable_set_vadjustment (GTK_SCROLLABLE(uzbl.gui.web_view), uzbl.gui.bar_v);
#else
    gtk_widget_set_scroll_adjustments (GTK_WIDGET (uzbl.gui.web_view),
      uzbl.gui.bar_h, uzbl.gui.bar_v);
#endif

    g_object_connect((GObject*)uzbl.gui.bar_v,
      "signal::value-changed",  (GCallback)scroll_vert_cb,  NULL,
      "signal::changed",        (GCallback)scroll_vert_cb,  NULL,
      NULL);

    g_object_connect((GObject*)uzbl.gui.bar_h,
      "signal::value-changed",  (GCallback)scroll_horiz_cb, NULL,
      "signal::changed",        (GCallback)scroll_horiz_cb, NULL,
      NULL);
}

/* set up gtk, gobject, variable defaults and other things that tests and other
 * external applications need to do anyhow */
void
initialize(int argc, char *argv[]) {
    int i;

    for(i=0; i<argc; ++i) {
        if(!strcmp(argv[i], "-s") || !strcmp(argv[i], "--socket")) {
            uzbl.state.plug_mode = TRUE;
            break;
        }
    }

    if (!g_thread_supported ())
        g_thread_init (NULL);
    gtk_init (&argc, &argv);

    uzbl.state.executable_path = g_strdup(argv[0]);
    uzbl.state.selected_url = NULL;
    uzbl.state.searchtx = NULL;

    GOptionContext* context = g_option_context_new ("[ uri ] - load a uri by default");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_parse (context, &argc, &argv, NULL);
    g_option_context_free(context);

    if (uzbl.behave.print_version) {
        printf("Commit: %s\n", COMMIT);
        exit(EXIT_SUCCESS);
    }

    uzbl.net.soup_session = webkit_get_default_session();

    uzbl.net.soup_cookie_jar = uzbl_cookie_jar_new();
    soup_session_add_feature(uzbl.net.soup_session, SOUP_SESSION_FEATURE(uzbl.net.soup_cookie_jar));

    /* TODO: move the handler setup to event_buffer_timeout and disarm the
     * handler in empty_event_buffer? */
    if(setup_signal(SIGALRM, empty_event_buffer) == SIG_ERR)
        fprintf(stderr, "uzbl: error hooking %d: %s\n", SIGALRM, strerror(errno));
    event_buffer_timeout(10);

    uzbl.info.webkit_major = webkit_major_version();
    uzbl.info.webkit_minor = webkit_minor_version();
    uzbl.info.webkit_micro = webkit_micro_version();
    uzbl.info.arch         = ARCH;
    uzbl.info.commit       = COMMIT;

    commands_hash ();
    create_var_to_name_hash();

    create_mainbar();
    create_browser();
}

void
load_uri_imp(gchar *uri) {
    GString* newuri;
    SoupURI* soup_uri;

    /* Strip leading whitespaces */
    while (*uri) {
        if (!isspace(*uri)) break;
        uri++;
    }

    if (g_strstr_len (uri, 11, "javascript:") != NULL) {
        eval_js(uzbl.gui.web_view, uri, NULL, "javascript:");
        return;
    }

    newuri = g_string_new (uri);
    soup_uri = soup_uri_new(uri);

    if (!soup_uri) {
        gchar* fullpath;
        if (g_path_is_absolute (newuri->str))
            fullpath = newuri->str;
        else {
            gchar* wd = g_get_current_dir ();
            fullpath = g_build_filename (wd, newuri->str, NULL);
            g_free(wd);
        }
        struct stat stat_result;
        if (! g_stat(fullpath, &stat_result))
            g_string_printf (newuri, "file://%s", fullpath);
        else
            g_string_prepend (newuri, "http://");
    } else {
        soup_uri_free(soup_uri);
    }

    /* if we do handle cookies, ask our handler for them */
    webkit_web_view_load_uri (uzbl.gui.web_view, newuri->str);
    g_string_free (newuri, TRUE);
}


#ifndef UZBL_LIBRARY
/** -- MAIN -- **/
int
main (int argc, char* argv[]) {
    initialize(argc, argv);

    uzbl.gui.scrolled_win = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (uzbl.gui.scrolled_win),
        GTK_POLICY_NEVER, GTK_POLICY_NEVER);

    gtk_container_add (GTK_CONTAINER (uzbl.gui.scrolled_win),
        GTK_WIDGET (uzbl.gui.web_view));

    uzbl.gui.vbox = gtk_vbox_new (FALSE, 0);

    /* initial packing */
    gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.mainbar, FALSE, TRUE, 0);

    if (uzbl.state.plug_mode) {
        uzbl.gui.plug = create_plug ();
        gtk_container_add (GTK_CONTAINER (uzbl.gui.plug), uzbl.gui.vbox);
        gtk_widget_show_all (GTK_WIDGET (uzbl.gui.plug));
        /* in xembed mode the window has no unique id and thus
         * socket/fifo names aren't unique either.
         * we use a custom randomizer to create a random id
        */
        struct timeval tv;
        gettimeofday(&tv, NULL);
        srand((unsigned int)tv.tv_sec*tv.tv_usec);
        uzbl.xwin = rand();
    } else {
        uzbl.gui.main_window = create_window ();
        gtk_container_add (GTK_CONTAINER (uzbl.gui.main_window), uzbl.gui.vbox);
        gtk_widget_show_all (uzbl.gui.main_window);
        uzbl.xwin = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (uzbl.gui.main_window)));
    }

    uzbl.gui.scbar_v = (GtkScrollbar*) gtk_vscrollbar_new (NULL);
    uzbl.gui.bar_v = gtk_range_get_adjustment((GtkRange*) uzbl.gui.scbar_v);
    uzbl.gui.scbar_h = (GtkScrollbar*) gtk_hscrollbar_new (NULL);
    uzbl.gui.bar_h = gtk_range_get_adjustment((GtkRange*) uzbl.gui.scbar_h);

    set_webview_scroll_adjustments();

    gchar *xwin = g_strdup_printf("%d", (int)uzbl.xwin);
    g_setenv("UZBL_XID", xwin, TRUE);

    if(!uzbl.state.instance_name)
        uzbl.state.instance_name = g_strdup(xwin);

    g_free(xwin);

    uzbl.info.pid_str = g_strdup_printf("%d", getpid());
    g_setenv("UZBL_PID", uzbl.info.pid_str, TRUE);
    send_event(INSTANCE_START, uzbl.info.pid_str, NULL);

    if(uzbl.state.plug_mode) {
        char *t = itos(gtk_plug_get_id(uzbl.gui.plug));
        send_event(PLUG_CREATED, t, NULL);
        g_free(t);
    }

    /* generate an event with a list of built in commands */
    builtins();

    if (!uzbl.state.plug_mode)
        gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));

    if (uzbl.state.verbose) {
        printf("Uzbl start location: %s\n", argv[0]);
        if (uzbl.state.socket_id)
            printf("plug_id %i\n", gtk_plug_get_id(uzbl.gui.plug));
        else
            printf("window_id %i\n",(int) uzbl.xwin);
        printf("pid %i\n", getpid ());
        printf("name: %s\n", uzbl.state.instance_name);
        printf("commit: %s\n", uzbl.info.commit);
    }

    /* Check uzbl is in window mode before getting/setting geometry */
    if (uzbl.gui.main_window) {
        if(uzbl.gui.geometry)
            cmd_set_geometry();
        else
            retrieve_geometry();
    }

    gchar *uri_override = (uzbl.state.uri ? g_strdup(uzbl.state.uri) : NULL);
    if (argc > 1 && !uzbl.state.uri)
        uri_override = g_strdup(argv[1]);
    gboolean verbose_override = uzbl.state.verbose;

    settings_init ();

    if (!uzbl.behave.show_status)
        gtk_widget_hide(uzbl.gui.mainbar);
    else
        update_title();

    /* WebInspector */
    set_up_inspector();

    if (verbose_override > uzbl.state.verbose)
        uzbl.state.verbose = verbose_override;

    if (uri_override) {
        set_var_value("uri", uri_override);
        g_free(uri_override);
    }

    gtk_main ();
    clean_up();

    return EXIT_SUCCESS;
}
#endif

/* vi: set et ts=4: */
