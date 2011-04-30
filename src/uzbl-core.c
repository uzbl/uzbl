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
#include "menu.h"
#include "io.h"

UzblCore uzbl;

/* commandline arguments (set initial values for the state variables) */
const
GOptionEntry entries[] = {
    { "uri",      'u', 0, G_OPTION_ARG_STRING, &uzbl.state.uri,
        "Uri to load at startup (equivalent to 'uzbl <uri>' or 'set uri = URI' after uzbl has launched)", "URI" },
    { "verbose",  'v', 0, G_OPTION_ARG_NONE,   &uzbl.state.verbose,
        "Whether to print all messages or just errors.", NULL },
    { "named",    'n', 0, G_OPTION_ARG_STRING, &uzbl.state.instance_name,
        "Name of the current instance (defaults to Xorg window id or random for GtkSocket mode)", "NAME" },
    { "config",   'c', 0, G_OPTION_ARG_STRING, &uzbl.state.config_file,
        "Path to config file or '-' for stdin", "FILE" },
    /* TODO: explain the difference between these two options */
    { "socket",   's', 0, G_OPTION_ARG_INT, &uzbl.state.socket_id,
        "Xembed socket ID, this window should embed itself", "SOCKET" },
    { "embed",    'e', 0, G_OPTION_ARG_NONE, &uzbl.state.embed,
        "Whether this window should expect to be embedded", NULL },
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
    { "_",                      PTR_C_STR(uzbl.state.last_result,                    NULL)},

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
enum exp_type {
  EXP_ERR, EXP_SIMPLE_VAR, EXP_BRACED_VAR, EXP_EXPR, EXP_JS, EXP_ESCAPE
};

enum exp_type
get_exp_type(const gchar *s) {
  switch(*(s+1)) {
    case '(': return EXP_EXPR;
    case '{': return EXP_BRACED_VAR;
    case '<': return EXP_JS;
    case '[': return EXP_ESCAPE;
    default:  return EXP_SIMPLE_VAR;
  }
}

/*
 * recurse == 1: don't expand '@(command)@'
 * recurse == 2: don't expand '@<java script>@'
*/
gchar*
expand(const char* s, guint recurse) {
    uzbl_cmdprop* c;
    enum exp_type etype;
    char*         end_simple_var = "\t^°!\"§$%&/()=?'`'+~*'#-:,;@<>| \\{}[]¹²³¼½";
    char*         ret = NULL;
    char*         vend = NULL;
    GError*       err = NULL;
    gchar*        cmd_stdout = NULL;
    gchar*        mycmd = NULL;
    GString*      buf = g_string_new("");
    GString*      js_ret = g_string_new("");

    while (s && *s) {
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

void
clean_up(void) {
    if (uzbl.info.pid_str) {
        send_event (INSTANCE_EXIT, NULL, TYPE_INT, getpid(), NULL);
        g_free(uzbl.info.pid_str);
        uzbl.info.pid_str = NULL;
    }

    if (uzbl.state.executable_path) {
        g_free(uzbl.state.executable_path);
        uzbl.state.executable_path = NULL;
    }

    if (uzbl.behave.commands) {
        g_hash_table_destroy(uzbl.behave.commands);
        uzbl.behave.commands = NULL;
    }

    if (uzbl.state.event_buffer) {
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
scroll(GtkAdjustment* bar, gchar *amount_str) {
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
    GtkAdjustment *bar = NULL;

    if (g_strcmp0(direction, "horizontal") == 0)
        bar = uzbl.gui.bar_h;
    else if (g_strcmp0(direction, "vertical") == 0)
        bar = uzbl.gui.bar_v;
    else {
        if(uzbl.state.verbose)
            puts("Unrecognized scroll format");
        return;
    }

    if (g_strcmp0(argv1, "begin") == 0)
        gtk_adjustment_set_value(bar, gtk_adjustment_get_lower(bar));
    else if (g_strcmp0(argv1, "end") == 0)
        gtk_adjustment_set_value (bar, gtk_adjustment_get_upper(bar) -
                                gtk_adjustment_get_page_size(bar));
    else
        scroll(bar, argv1);
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
CommandInfo cmdlist[] =
{   /* key                              function      no_split      */
    { "back",                           view_go_back, 0                },
    { "forward",                        view_go_forward, 0             },
    { "scroll",                         scroll_cmd, 0                  },
    { "reload",                         view_reload, 0                 },
    { "reload_ign_cache",               view_reload_bypass_cache, 0    },
    { "stop",                           view_stop_loading, 0           },
    { "zoom_in",                        view_zoom_in, 0                }, //Can crash (when max zoom reached?).
    { "zoom_out",                       view_zoom_out, 0               },
    { "toggle_zoom_type",               toggle_zoom_type, 0            },
    { "uri",                            load_uri, TRUE                 },
    { "js",                             run_js, TRUE                   },
    { "script",                         run_external_js, 0             },
    { "toggle_status",                  toggle_status_cb, 0            },
    { "spawn",                          spawn_async, 0                 },
    { "sync_spawn",                     spawn_sync, 0                  },
    { "sync_spawn_exec",                spawn_sync_exec, 0             }, // needed for load_cookies.sh :(
    { "sh",                             spawn_sh_async, 0              },
    { "sync_sh",                        spawn_sh_sync, 0               },
    { "exit",                           close_uzbl, 0                  },
    { "search",                         search_forward_text, TRUE      },
    { "search_reverse",                 search_reverse_text, TRUE      },
    { "search_clear",                   search_clear, TRUE             },
    { "dehilight",                      dehilight, 0                   },
    { "set",                            set_var, TRUE                  },
    { "dump_config",                    act_dump_config, 0             },
    { "dump_config_as_events",          act_dump_config_as_events, 0   },
    { "chain",                          chain, 0                       },
    { "print",                          print, TRUE                    },
    { "event",                          event, TRUE                    },
    { "request",                        event, TRUE                    },
    { "menu_add",                       menu_add, TRUE                 },
    { "menu_link_add",                  menu_add_link, TRUE            },
    { "menu_image_add",                 menu_add_image, TRUE           },
    { "menu_editable_add",              menu_add_edit, TRUE            },
    { "menu_separator",                 menu_add_separator, TRUE       },
    { "menu_link_separator",            menu_add_separator_link, TRUE  },
    { "menu_image_separator",           menu_add_separator_image, TRUE },
    { "menu_editable_separator",        menu_add_separator_edit, TRUE  },
    { "menu_remove",                    menu_remove, TRUE              },
    { "menu_link_remove",               menu_remove_link, TRUE         },
    { "menu_image_remove",              menu_remove_image, TRUE        },
    { "menu_editable_remove",           menu_remove_edit, TRUE         },
    { "hardcopy",                       hardcopy, TRUE                 },
    { "include",                        include, TRUE                  },
    { "show_inspector",                 show_inspector, 0              },
    { "add_cookie",                     add_cookie, 0                  },
    { "delete_cookie",                  delete_cookie, 0               },
    { "clear_cookies",                  clear_cookies, 0               },
    { "download",                       download, 0                    }
};

void
commands_hash(void) {
    unsigned int i;
    uzbl.behave.commands = g_hash_table_new(g_str_hash, g_str_equal);

    for (i = 0; i < LENGTH(cmdlist); i++)
        g_hash_table_insert(uzbl.behave.commands, (gpointer) cmdlist[i].key, &cmdlist[i]);
}


void
builtins() {
    unsigned int i;
    unsigned int len = LENGTH(cmdlist);
    GString*     command_list = g_string_new("");

    for (i = 0; i < len; i++) {
        g_string_append(command_list, cmdlist[i].key);
        g_string_append_c(command_list, ' ');
    }

    send_event(BUILTINS, NULL, TYPE_STR, command_list->str, NULL);
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

    send_event(0, event_name->str, TYPE_FORMATTEDSTR, split[1] ? split[1] : "", NULL);

    g_string_free(event_name, TRUE);
    g_strfreev(split);
}

void
print(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar* buf;

    if(!result)
        return;

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
            send_event(COMMAND_ERROR, NULL, TYPE_STR, tmp, NULL);
            g_free(tmp);
        }

        send_event(FILE_INCLUDED, NULL, TYPE_STR, path, NULL);
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
clear_cookies(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) argv; (void) result;

    // Replace the current cookie jar with a new empty jar
    soup_session_remove_feature (uzbl.net.soup_session,
        SOUP_SESSION_FEATURE (uzbl.net.soup_cookie_jar));
    g_object_unref (G_OBJECT (uzbl.net.soup_cookie_jar));
    uzbl.net.soup_cookie_jar = uzbl_cookie_jar_new ();
    soup_session_add_feature(uzbl.net.soup_session,
        SOUP_SESSION_FEATURE (uzbl.net.soup_cookie_jar));
}

void
download(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void) result;

    const gchar *uri         = argv_idx(argv, 0);
    const gchar *destination = NULL;
    if(argv->len > 1)
        destination = argv_idx(argv, 1);

    WebKitNetworkRequest *req = webkit_network_request_new(uri);
    WebKitDownload *download = webkit_download_new(req);

    download_cb(web_view, download, destination);

    if(webkit_download_get_destination_uri(download))
        webkit_download_start(download);
    else
        g_object_unref(download);

    g_object_unref(req);
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
load_uri(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void) web_view; (void) result;
    gchar * uri = argv_idx(argv, 0);
    set_var_value("uri", uri ? uri : "");
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
    if (result && js_result && !JSValueIsUndefined(context, js_result)) {
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
    g_free(uzbl.state.searchtx);
    uzbl.state.searchtx = NULL;
}

void
search_forward_text (WebKitWebView *page, GArray *argv, GString *result) {
    (void) result;
    search_text(page, argv, TRUE);
}

void
search_reverse_text(WebKitWebView *page, GArray *argv, GString *result) {
    (void) result;
    search_text(page, argv, FALSE);
}

void
dehilight(WebKitWebView *page, GArray *argv, GString *result) {
    (void) argv; (void) result;
    webkit_web_view_set_highlight_text_matches (page, FALSE);
}

void
chain(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    guint i = 0;
    const gchar *cmd;
    GString *r = g_string_new ("");
    while ((cmd = argv_idx(argv, i++))) {
        GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
        const CommandInfo *c = parse_command_parts(cmd, a);
        if (c)
            run_parsed_command(c, a, r);
        g_array_free (a, TRUE);
    }
    if(result)
        g_string_assign (result, r->str);

    g_string_free(r, TRUE);
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
        if (*output_stdout)
            g_free(*output_stdout);

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
spawn(GArray *argv, GString *result, gboolean exec) {
    gchar *path = NULL;
    gchar *arg_car = argv_idx(argv, 0);
    const gchar **arg_cdr = &g_array_index(argv, const gchar *, 1);

    if (arg_car && (path = find_existing_file(arg_car))) {
        gchar *r = NULL;
        run_command(path, arg_cdr, result != NULL, result ? &r : NULL);
        if(result) {
            g_string_assign(result, r);
            // run each line of output from the program as a command
            if (exec && r) {
                gchar *head = r;
                gchar *tail;
                while ((tail = strchr (head, '\n'))) {
                    *tail = '\0';
                    parse_cmd_line(head, NULL);
                    head = tail + 1;
                }
            }
        }
        g_free(r);
        g_free(path);
    }
}

void
spawn_async(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn(argv, NULL, FALSE);
}

void
spawn_sync(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view;
    spawn(argv, result, FALSE);
}

void
spawn_sync_exec(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view;
    if(!result) {
        GString *force_result = g_string_new("");
        spawn(argv, force_result, TRUE);
        g_string_free (force_result, TRUE);
    } else
        spawn(argv, result, TRUE);
}

void
spawn_sh(GArray *argv, GString *result) {
    if (!uzbl.behave.shell_cmd) {
        g_printerr ("spawn_sh: shell_cmd is not set!\n");
        return;
    }
    guint i;

    gchar **cmd = split_quoted(uzbl.behave.shell_cmd, TRUE);
    if(!cmd)
        return;

    gchar *cmdname = g_strdup(cmd[0]);
    g_array_insert_val(argv, 1, cmdname);

    for (i = 1; i < g_strv_length(cmd); i++)
        g_array_prepend_val(argv, cmd[i]);

    if (result) {
        gchar *r = NULL;
        run_command(cmd[0], (const gchar **) argv->data, TRUE, &r);
        g_string_assign(result, r);
        g_free(r);
    } else
        run_command(cmd[0], (const gchar **) argv->data, FALSE, NULL);

    g_free (cmdname);
    g_strfreev (cmd);
}

void
spawn_sh_async(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn_sh(argv, NULL);
}

void
spawn_sh_sync(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn_sh(argv, result);
}

void
run_parsed_command(const CommandInfo *c, GArray *a, GString *result) {
    /* send the COMMAND_EXECUTED event, except for set and event/request commands */
    if(strcmp("set", c->key)   &&
       strcmp("event", c->key) &&
       strcmp("request", c->key)) {
        // FIXME, build string inside send_event
        GString *param = g_string_new("");
        const gchar *p;
        guint i = 0;
        while ((p = argv_idx(a, i++)))
            g_string_append_printf(param, " '%s'", p);

	/* might be destructive on array a */
        c->function(uzbl.gui.web_view, a, result);

        send_event(COMMAND_EXECUTED, NULL,
            TYPE_NAME, c->key,
            TYPE_FORMATTEDSTR, param->str,
            NULL);
        g_string_free(param, TRUE);
    }
    else
        c->function(uzbl.gui.web_view, a, result);

    if(result) {
        g_free(uzbl.state.last_result);
        uzbl.state.last_result = g_strdup(result->str);
    }
}

void
parse_command_arguments(const gchar *p, GArray *a, gboolean no_split) {
    if (no_split && p) { /* pass the parameters through in one chunk */
        sharg_append(a, g_strdup(p));
        return;
    }

    gchar **par = split_quoted(p, TRUE);
    if (par) {
        guint i;
        for (i = 0; i < g_strv_length(par); i++)
            sharg_append(a, g_strdup(par[i]));
        g_strfreev (par);
    }
}

const CommandInfo *
parse_command_parts(const gchar *line, GArray *a) {
    CommandInfo *c = NULL;

    gchar *exp_line = expand(line, 0);
    if(exp_line[0] == '\0') {
        g_free(exp_line);
        return NULL;
    }

    /* separate the line into the command and its parameters */
    gchar **tokens = g_strsplit(exp_line, " ", 2);

    /* look up the command */
    c = g_hash_table_lookup(uzbl.behave.commands, tokens[0]);

    if(!c) {
        send_event(COMMAND_ERROR, NULL,
            TYPE_STR, exp_line,
            NULL);
        g_free(exp_line);
        g_strfreev(tokens);
        return NULL;
    }

    gchar *p = g_strdup(tokens[1]);
    g_free(exp_line);
    g_strfreev(tokens);

    /* parse the arguments */
    parse_command_arguments(p, a, c->no_split);
    g_free(p);

    return c;
}

void
parse_command(const char *cmd, const char *params, GString *result) {
    CommandInfo *c = g_hash_table_lookup(uzbl.behave.commands, cmd);
    if(c) {
        GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));

        parse_command_arguments(params, a, c->no_split);
        run_parsed_command(c, a, result);

        g_array_free (a, TRUE);
    } else {
        send_event(COMMAND_ERROR, NULL,
            TYPE_NAME, cmd,
            TYPE_STR, params ? params : "",
            NULL);
    }
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
    if (!uzbl.state.plug_mode)
        gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    return;
}

gboolean
valid_name(const gchar* name) {
    char *invalid_chars = "\t^°!\"§$%&/()=?'`'+~*'#-:,;@<>| \\{}[]¹²³¼½";
    return strpbrk(name, invalid_chars) == NULL;
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
parse_cmd_line(const char *ctl_line, GString *result) {
    gchar *work_string = g_strdup(ctl_line);

    /* strip trailing newline, and any other whitespace in front */
    g_strstrip(work_string);

    if( strcmp(work_string, "") ) {
        if((work_string[0] != '#')) { /* ignore comments */
            GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
            const CommandInfo *c = parse_command_parts(work_string, a);
            if(c)
                run_parsed_command(c, a, result);
            g_array_free (a, TRUE);
        }
    }

    g_free(work_string);
}

void
update_title(void) {
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
create_scrolled_win() {
    GUI* g = &uzbl.gui;

    g->web_view     = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g->scrolled_win = gtk_scrolled_window_new(NULL, NULL);
    WebKitWebFrame *wf = webkit_web_view_get_main_frame (g->web_view);

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(g->scrolled_win),
        GTK_POLICY_NEVER,
        GTK_POLICY_NEVER
    );

    gtk_container_add(
        GTK_CONTAINER(g->scrolled_win),
        GTK_WIDGET(g->web_view)
    );

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
      "signal::notify::uri",                          (GCallback)uri_change_cb,           NULL,
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

    g_object_connect (G_OBJECT (wf),
      "signal::scrollbars-policy-changed",            (GCallback)scrollbars_policy_cb,    NULL,
      NULL);
}


GtkWidget*
create_mainbar() {
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
create_window() {
    GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
    gtk_widget_set_name (window, "Uzbl browser");
    gtk_window_set_title(GTK_WINDOW(window), "Uzbl browser");

#if GTK_CHECK_VERSION(3,0,0)
    gtk_window_set_has_resize_grip (window, FALSE);
#endif

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
create_plug() {
    if(uzbl.state.embed) uzbl.state.socket_id = 0;
    GtkPlug* plug = GTK_PLUG (gtk_plug_new (uzbl.state.socket_id));
    g_signal_connect (G_OBJECT (plug), "destroy", G_CALLBACK (destroy_cb), NULL);
    g_signal_connect (G_OBJECT (plug), "key-press-event", G_CALLBACK (key_press_cb), NULL);
    g_signal_connect (G_OBJECT (plug), "key-release-event", G_CALLBACK (key_release_cb), NULL);

    return plug;
}

void
settings_init () {
    State*   s = &uzbl.state;
    Network* n = &uzbl.net;
    int      i;
    
    /* Load default config */
    for (i = 0; default_config[i].command != NULL; i++) {
        parse_cmd_line(default_config[i].command, NULL);
    }

    if (g_strcmp0(s->config_file, "-") == 0) {
        s->config_file = NULL;
        create_stdin();
    }

    else if (!s->config_file) {
        s->config_file = find_xdg_file(0, "/uzbl/config");
    }

    /* Load config file, if any */
    if (s->config_file) {
        if (!for_each_line_in_file(s->config_file, parse_cmd_line_cb, NULL)) {
            gchar *tmp = g_strdup_printf("File %s can not be read.", s->config_file);
            send_event(COMMAND_ERROR, NULL, TYPE_STR, tmp, NULL);
            g_free(tmp);
        }
        g_setenv("UZBL_CONFIG", s->config_file, TRUE);
    } else if (uzbl.state.verbose)
        printf ("No configuration file loaded.\n");

    if (s->connect_socket_names)
        init_connect_socket();

    g_signal_connect(n->soup_session, "authenticate", G_CALLBACK(handle_authentication), NULL);
}


void handle_authentication (SoupSession *session, SoupMessage *msg, SoupAuth *auth, gboolean retrying, gpointer user_data) {
    (void) user_data;

    if (uzbl.behave.authentication_handler && *uzbl.behave.authentication_handler != 0) {
        soup_session_pause_message(session, msg);

        GString *result = g_string_new ("");

        gchar *info  = g_strdup(soup_auth_get_info(auth));
        gchar *host  = g_strdup(soup_auth_get_host(auth));
        gchar *realm = g_strdup(soup_auth_get_realm(auth));

        GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
        const CommandInfo *c = parse_command_parts(uzbl.behave.authentication_handler, a);
        if(c) {
            sharg_append(a, info);
            sharg_append(a, host);
            sharg_append(a, realm);
            sharg_append(a, retrying ? "TRUE" : "FALSE");

            run_parsed_command(c, a, result);
        }
        g_array_free(a, TRUE);

        if (result->len > 0) {
            char  *username, *password;
            int    number_of_endls=0;

            username = result->str;

            gchar *p;
            for (p = result->str; *p; p++) {
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

        soup_session_unpause_message(session, msg);

        g_string_free(result, TRUE);
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

    if(c->dump)
        send_set_var_event(k, c);
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

/* Set up gtk, gobject, variable defaults and other things that tests and other
 * external applications need to do anyhow */
void
initialize(int argc, char** argv) {
    /* Initialize variables */
    uzbl.state.socket_id       = 0;
    uzbl.state.plug_mode       = FALSE;

    uzbl.state.executable_path = g_strdup(argv[0]);
    uzbl.state.selected_url    = NULL;
    uzbl.state.searchtx        = NULL;

    uzbl.info.webkit_major     = webkit_major_version();
    uzbl.info.webkit_minor     = webkit_minor_version();
    uzbl.info.webkit_micro     = webkit_micro_version();
    uzbl.info.arch             = ARCH;
    uzbl.info.commit           = COMMIT;

    uzbl.state.last_result  = NULL;

    /* Parse commandline arguments */
    GOptionContext* context = g_option_context_new ("[ uri ] - load a uri by default");
    g_option_context_add_main_entries(context, entries, NULL);
    g_option_context_add_group(context, gtk_get_option_group (TRUE));
    g_option_context_parse(context, &argc, &argv, NULL);
    g_option_context_free(context);

    /* Only print version */
    if (uzbl.behave.print_version) {
        printf("Commit: %s\n", COMMIT);
        exit(EXIT_SUCCESS);
    }

    /* Embedded mode */
    if (uzbl.state.socket_id || uzbl.state.embed)
        uzbl.state.plug_mode = TRUE;

    if (!g_thread_supported())
        g_thread_init(NULL);


    /* TODO: move the handler setup to event_buffer_timeout and disarm the
     * handler in empty_event_buffer? */
    if (setup_signal(SIGALRM, empty_event_buffer) == SIG_ERR)
        fprintf(stderr, "uzbl: error hooking %d: %s\n", SIGALRM, strerror(errno));
    event_buffer_timeout(10);

    
    /* HTTP client */
    uzbl.net.soup_session      = webkit_get_default_session();
    uzbl.net.soup_cookie_jar   = uzbl_cookie_jar_new();

    soup_session_add_feature(uzbl.net.soup_session, SOUP_SESSION_FEATURE(uzbl.net.soup_cookie_jar));


    commands_hash();
    create_var_to_name_hash();

    /* GUI */
    gtk_init(&argc, &argv);
    create_mainbar();
    create_scrolled_win();

    uzbl.gui.vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(uzbl.gui.vbox), uzbl.gui.mainbar, FALSE, TRUE, 0);
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

    /* Embedded mode */
    if (uzbl.state.plug_mode) {
        uzbl.gui.plug = create_plug();
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
    }
    
    /* Windowed mode */
    else {
        uzbl.gui.main_window = create_window();
        gtk_container_add (GTK_CONTAINER (uzbl.gui.main_window), uzbl.gui.vbox);
        gtk_widget_show_all (uzbl.gui.main_window);

        uzbl.xwin = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (uzbl.gui.main_window)));

        gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    }

    /* Scrolling */
    uzbl.gui.bar_h = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (uzbl.gui.scrolled_win));
    uzbl.gui.bar_v = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (uzbl.gui.scrolled_win));

    g_object_connect(G_OBJECT (uzbl.gui.bar_v),
      "signal::value-changed",  (GCallback)scroll_vert_cb,  NULL,
      "signal::changed",        (GCallback)scroll_vert_cb,  NULL,
      NULL);

    g_object_connect(G_OBJECT (uzbl.gui.bar_h),
      "signal::value-changed",  (GCallback)scroll_horiz_cb, NULL,
      "signal::changed",        (GCallback)scroll_horiz_cb, NULL,
      NULL);

    gchar *xwin = g_strdup_printf("%d", (int)uzbl.xwin);
    g_setenv("UZBL_XID", xwin, TRUE);

    if(!uzbl.state.instance_name)
        uzbl.state.instance_name = g_strdup(xwin);

    g_free(xwin);

    uzbl.info.pid_str = g_strdup_printf("%d", getpid());
    g_setenv("UZBL_PID", uzbl.info.pid_str, TRUE);
    send_event(INSTANCE_START, NULL, TYPE_INT, getpid(), NULL);

    if (uzbl.state.plug_mode) {
        send_event(PLUG_CREATED, NULL, TYPE_INT, gtk_plug_get_id (uzbl.gui.plug), NULL);
    }

    /* Generate an event with a list of built in commands */
    builtins();

    /* Check uzbl is in window mode before getting/setting geometry */
    if (uzbl.gui.main_window) {
        if (uzbl.gui.geometry)
            cmd_set_geometry();
        else
            retrieve_geometry();
    }

    gchar *uri_override = (uzbl.state.uri ? g_strdup(uzbl.state.uri) : NULL);
    if (argc > 1 && !uzbl.state.uri)
        uri_override = g_strdup(argv[1]);

    gboolean verbose_override = uzbl.state.verbose;

    /* Read configuration file */
    settings_init();

    /* Update status bar */
    if (!uzbl.behave.show_status)
        gtk_widget_hide(uzbl.gui.mainbar);
    else
        update_title();

    /* WebInspector */
    set_up_inspector();

    /* Options overriding */
    if (verbose_override > uzbl.state.verbose)
        uzbl.state.verbose = verbose_override;

    if (uri_override) {
        set_var_value("uri", uri_override);
        g_free(uri_override);
    }

    /* Verbose feedback */
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


    gtk_main();

    /* Cleanup and exit*/
    clean_up();

    return EXIT_SUCCESS;
}
#endif

/* vi: set et ts=4: */
