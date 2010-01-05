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
        "Socket ID", "SOCKET" },
    { "connect-socket",   0, 0, G_OPTION_ARG_STRING_ARRAY, &uzbl.state.connect_socket_names,
        "Connect to server socket", "CSOCKET" },
    { "geometry", 'g', 0, G_OPTION_ARG_STRING, &uzbl.gui.geometry,
        "Set window geometry (format: WIDTHxHEIGHT+-X+-Y or maximized)", "GEOMETRY" },
    { "version",  'V', 0, G_OPTION_ARG_NONE, &uzbl.behave.print_version,
        "Print the version and exit", NULL },
    { NULL,      0, 0, 0, NULL, NULL, NULL }
};

XDG_Var XDG[] =
{
    { "XDG_CONFIG_HOME", "~/.config" },
    { "XDG_DATA_HOME",   "~/.local/share" },
    { "XDG_CACHE_HOME",  "~/.cache" },
    { "XDG_CONFIG_DIRS", "/etc/xdg" },
    { "XDG_DATA_DIRS",   "/usr/local/share/:/usr/share/" },
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
    { "inject_html",            PTR_V_STR(uzbl.behave.inject_html,              0,   cmd_inject_html)},
    { "geometry",               PTR_V_STR(uzbl.gui.geometry,                    1,   cmd_set_geometry)},
    { "keycmd",                 PTR_V_STR(uzbl.state.keycmd,                    1,   NULL)},
    { "show_status",            PTR_V_INT(uzbl.behave.show_status,              1,   cmd_set_status)},
    { "status_top",             PTR_V_INT(uzbl.behave.status_top,               1,   move_statusbar)},
    { "status_format",          PTR_V_STR(uzbl.behave.status_format,            1,   NULL)},
    { "status_background",      PTR_V_STR(uzbl.behave.status_background,        1,   NULL)},
    { "title_format_long",      PTR_V_STR(uzbl.behave.title_format_long,        1,   NULL)},
    { "title_format_short",     PTR_V_STR(uzbl.behave.title_format_short,       1,   NULL)},
    { "icon",                   PTR_V_STR(uzbl.gui.icon,                        1,   set_icon)},
    { "forward_keys",           PTR_V_INT(uzbl.behave.forward_keys,             1,   NULL)},
    { "download_handler",       PTR_V_STR(uzbl.behave.download_handler,         1,   NULL)},
    { "cookie_handler",         PTR_V_STR(uzbl.behave.cookie_handler,           1,   NULL)},
    { "new_window",             PTR_V_STR(uzbl.behave.new_window,               1,   NULL)},
    { "scheme_handler",         PTR_V_STR(uzbl.behave.scheme_handler,           1,   NULL)},
    { "fifo_dir",               PTR_V_STR(uzbl.behave.fifo_dir,                 1,   cmd_fifo_dir)},
    { "socket_dir",             PTR_V_STR(uzbl.behave.socket_dir,               1,   cmd_socket_dir)},
    { "http_debug",             PTR_V_INT(uzbl.behave.http_debug,               1,   cmd_http_debug)},
    { "shell_cmd",              PTR_V_STR(uzbl.behave.shell_cmd,                1,   NULL)},
    { "proxy_url",              PTR_V_STR(uzbl.net.proxy_url,                   1,   set_proxy_url)},
    { "max_conns",              PTR_V_INT(uzbl.net.max_conns,                   1,   cmd_max_conns)},
    { "max_conns_host",         PTR_V_INT(uzbl.net.max_conns_host,              1,   cmd_max_conns_host)},
    { "useragent",              PTR_V_STR(uzbl.net.useragent,                   1,   cmd_useragent)},
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
    char *end_simple_var = "^°!\"§$%&/()=?'`'+~*'#-.:,;@<>| \\{}[]¹²³¼½";
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
                        eval_js(uzbl.gui.web_view, mycmd, js_ret);
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

char *
str_replace (const char* search, const char* replace, const char* string) {
    gchar **buf;
    char *ret;

    if(!string)
        return NULL;

    buf = g_strsplit (string, search, -1);
    ret = g_strjoinv (replace, buf);
    g_strfreev(buf);

    return ret;
}

GArray*
read_file_by_line (const gchar *path) {
    GIOChannel *chan = NULL;
    gchar *readbuf = NULL;
    gsize len;
    GArray *lines = g_array_new(TRUE, FALSE, sizeof(gchar*));
    int i = 0;

    chan = g_io_channel_new_file(path, "r", NULL);
    if (chan) {
        while (g_io_channel_read_line(chan, &readbuf, &len, NULL, NULL) == G_IO_STATUS_NORMAL) {
            const gchar* val = g_strdup (readbuf);
            g_array_append_val (lines, val);
            g_free (readbuf);
            i ++;
        }

        g_io_channel_unref (chan);
    } else {
        gchar *tmp = g_strdup_printf("File %s can not be read.", path);
        send_event(COMMAND_ERROR, tmp, NULL);
        g_free(tmp);
    }

    return lines;
}

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


/* Returns a new string with environment $variables expanded */
gchar*
parseenv (gchar* string) {
    extern char** environ;
    gchar* tmpstr = NULL, * out;
    int i = 0;

    if(!string)
        return NULL;

    out = g_strdup(string);
    while (environ[i] != NULL) {
        gchar** env = g_strsplit (environ[i], "=", 2);
        gchar* envname = g_strconcat ("$", env[0], NULL);

        if (g_strrstr (string, envname) != NULL) {
            tmpstr = out;
            out = str_replace(envname, env[1], out);
            g_free (tmpstr);
        }

        g_free (envname);
        g_strfreev (env); // somebody said this breaks uzbl
        i++;
    }

    return out;
}


void
clean_up(void) {
    send_event(INSTANCE_EXIT, uzbl.info.pid_str, NULL);
    g_free(uzbl.info.pid_str);

    g_free(uzbl.state.executable_path);
    g_hash_table_destroy(uzbl.behave.commands);

    if(uzbl.state.event_buffer)
        g_ptr_array_free(uzbl.state.event_buffer, TRUE);

    if (uzbl.behave.fifo_dir)
        unlink (uzbl.comm.fifo_path);
    if (uzbl.behave.socket_dir)
        unlink (uzbl.comm.socket_path);
}

gint
get_click_context() {
    GUI *g = &uzbl.gui;
    WebKitHitTestResult *ht;
    guint context;

    if(!uzbl.state.last_button)
        return -1;

    ht = webkit_web_view_get_hit_test_result(g->web_view, uzbl.state.last_button);
    g_object_get(ht, "context", &context, NULL);

    return (gint)context;
}

/* --- SIGNALS --- */
int sigs[] = {SIGTERM, SIGINT, SIGSEGV, SIGILL, SIGFPE, SIGQUIT, SIGALRM, 0};

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
catch_signal(int s) {
    if(s == SIGTERM ||
       s == SIGINT  ||
       s == SIGILL  ||
       s == SIGFPE  ||
       s == SIGQUIT) {
        clean_up();
        exit(EXIT_SUCCESS);
    }
    else if(s == SIGSEGV) {
        clean_up();
        fprintf(stderr, "Program aborted, segmentation fault!\nAttempting to clean up...\n");
        exit(EXIT_FAILURE);
    }
    else if(s == SIGALRM && uzbl.state.event_buffer) {
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
    { "spawn",                          {spawn, 0}                      },
    { "sync_spawn",                     {spawn_sync, 0}                 }, // needed for cookie handler
    { "sh",                             {spawn_sh, 0}                   },
    { "sync_sh",                        {spawn_sh_sync, 0}              }, // needed for cookie handler
    { "talk_to_socket",                 {talk_to_socket, 0}             },
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
    { "include",                        {include, TRUE}                 }
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

bool
file_exists (const char * filename) {
    return (access(filename, F_OK) == 0);
}

void
set_var(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;

    if(!argv_idx(argv, 0))
        return;

    gchar **split = g_strsplit(argv_idx(argv, 0), "=", 2);
    if (split[0] != NULL) {
        gchar *value = parseenv(split[1] ? g_strchug(split[1]) : " ");
        set_var_value(g_strstrip(split[0]), value);
        g_free(value);
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
        item_cmd = g_strdup(split[1]);

    if(split[0]) {
        m = malloc(sizeof(MenuItem));
        m->name = g_strdup(split[0]);
        m->cmd  = g_strdup(item_cmd?item_cmd:"");
        m->context = context;
        m->issep = FALSE;
        g_ptr_array_add(g->menu_items, m);
    }
    else
        g_free(item_cmd);

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

void
include(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    (void) result;
    gchar *pe   = NULL,
          *path = NULL,
          *line;
    int i=0;

    if(!argv_idx(argv, 0))
        return;

    pe = parseenv(argv_idx(argv, 0));
    if((path = find_existing_file(pe))) {
        GArray* lines = read_file_by_line(path);

        while ((line = g_array_index(lines, gchar*, i))) {
            parse_cmd_line (line, NULL);
            i++;
            g_free (line);
        }
        g_array_free (lines, TRUE);

        send_event(FILE_INCLUDED, path, NULL);
        g_free(path);
    }
    g_free(pe);
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
    load_uri_imp (argv_idx (argv, 0));
}

/* Javascript*/

JSValueRef
js_run_command (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject,
                size_t argumentCount, const JSValueRef arguments[],
                JSValueRef* exception) {
    (void) function;
    (void) thisObject;
    (void) exception;

    JSStringRef js_result_string;
    GString *result = g_string_new("");

    if (argumentCount >= 1) {
        JSStringRef arg = JSValueToStringCopy(ctx, arguments[0], NULL);
        size_t arg_size = JSStringGetMaximumUTF8CStringSize(arg);
        char ctl_line[arg_size];
        JSStringGetUTF8CString(arg, ctl_line, arg_size);

        parse_cmd_line(ctl_line, result);

        JSStringRelease(arg);
    }
    js_result_string = JSStringCreateWithUTF8CString(result->str);

    g_string_free(result, TRUE);

    return JSValueMakeString(ctx, js_result_string);
}

JSStaticFunction js_static_functions[] = {
    {"run", js_run_command, kJSPropertyAttributeNone},
};

void
js_init() {
    /* This function creates the class and its definition, only once */
    if (!uzbl.js.initialized) {
        /* it would be pretty cool to make this dynamic */
        uzbl.js.classdef = kJSClassDefinitionEmpty;
        uzbl.js.classdef.staticFunctions = js_static_functions;

        uzbl.js.classref = JSClassCreate(&uzbl.js.classdef);
    }
}


void
eval_js(WebKitWebView * web_view, gchar *script, GString *result) {
    WebKitWebFrame *frame;
    JSGlobalContextRef context;
    JSObjectRef globalobject;

    JSStringRef js_script;
    JSValueRef js_result;
    JSStringRef js_result_string;
    size_t js_result_size;

    js_init();

    frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(web_view));
    context = webkit_web_frame_get_global_context(frame);
    globalobject = JSContextGetGlobalObject(context);

    /* evaluate the script and get return value*/
    js_script = JSStringCreateWithUTF8CString(script);
    js_result = JSEvaluateScript(context, js_script, globalobject, NULL, 0, NULL);
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

    /* cleanup */
    JSStringRelease(js_script);
}

void
run_js (WebKitWebView * web_view, GArray *argv, GString *result) {
    if (argv_idx(argv, 0))
        eval_js(web_view, argv_idx(argv, 0), result);
}

void
run_external_js (WebKitWebView * web_view, GArray *argv, GString *result) {
    (void) result;
    gchar *path = NULL;

    if (argv_idx(argv, 0) &&
        ((path = find_existing_file(argv_idx(argv, 0)))) ) {
        GArray* lines = read_file_by_line (path);
        gchar*  js = NULL;
        int i = 0;
        gchar* line;

        while ((line = g_array_index(lines, gchar*, i))) {
            if (js == NULL) {
                js = g_strdup (line);
            } else {
                gchar* newjs = g_strconcat (js, line, NULL);
                js = newjs;
            }
            i ++;
            g_free (line);
        }

        if (uzbl.state.verbose)
            printf ("External JavaScript file %s loaded\n", argv_idx(argv, 0));

        gchar* newjs = str_replace("%s", argv_idx (argv, 1)?argv_idx (argv, 1):"", js);
        g_free (js);
        js = newjs;

        eval_js (web_view, js, result);
        g_free (js);
        g_array_free (lines, TRUE);
        g_free(path);
    }
}

void
search_text (WebKitWebView *page, GArray *argv, const gboolean forward) {
    if (argv_idx(argv, 0) && (*argv_idx(argv, 0) != '\0')) {
        if (g_strcmp0 (uzbl.state.searchtx, argv_idx(argv, 0)) != 0) {
            webkit_web_view_unmark_text_matches (page);
            webkit_web_view_mark_text_matches (page, argv_idx(argv, 0), FALSE, 0);
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
    if(uzbl.state.searchtx) {
        g_free(uzbl.state.searchtx);
        uzbl.state.searchtx = NULL;
    }
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
new_window_load_uri (const gchar * uri) {
    if (uzbl.behave.new_window) {
        GString *s = g_string_new ("");
        g_string_printf(s, "'%s'", uri);
        run_handler(uzbl.behave.new_window, s->str);
        send_event(NEW_WINDOW, s->str, NULL);
        return;
    }
    GString* to_execute = g_string_new ("");
    g_string_append_printf (to_execute, "%s --uri '%s'", uzbl.state.executable_path, uri);
    int i;
    for (i = 0; entries[i].long_name != NULL; i++) {
        if ((entries[i].arg == G_OPTION_ARG_STRING) &&
                !strcmp(entries[i].long_name,"uri") &&
                !strcmp(entries[i].long_name,"name")) {
            gchar** str = (gchar**)entries[i].arg_data;
            if (*str!=NULL)
                g_string_append_printf (to_execute, " --%s '%s'", entries[i].long_name, *str);
        }
        else if(entries[i].arg == G_OPTION_ARG_STRING_ARRAY) {
            int j;
            gchar **str = *((gchar ***)entries[i].arg_data);
            for(j=0; str[j]; j++)
                g_string_append_printf(to_execute, " --%s '%s'", entries[i].long_name, str[j]);
        }
    }
    if (uzbl.state.verbose)
        printf("\n%s\n", to_execute->str);
    g_spawn_command_line_async (to_execute->str, NULL);
    /* TODO: should we just report the uri as event detail? */
    send_event(NEW_WINDOW, to_execute->str, NULL);
    g_string_free (to_execute, TRUE);
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

// make sure that the args string you pass can properly be interpreted (eg properly escaped against whitespace, quotes etc)
gboolean
run_command (const gchar *command, const guint npre, const gchar **args,
             const gboolean sync, char **output_stdout) {
   //command <uzbl conf> <uzbl pid> <uzbl win id> <uzbl fifo file> <uzbl socket file> [args]
    GError *err = NULL;

    GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
    gchar *pid = itos(getpid());
    gchar *xwin = itos(uzbl.xwin);
    guint i;
    sharg_append(a, command);
    for (i = 0; i < npre; i++) /* add n args before the default vars */
        sharg_append(a, args[i]);
    sharg_append(a, uzbl.state.config_file);
    sharg_append(a, pid);
    sharg_append(a, xwin);
    sharg_append(a, uzbl.comm.fifo_path);
    sharg_append(a, uzbl.comm.socket_path);
    sharg_append(a, uzbl.state.uri);
    sharg_append(a, uzbl.gui.main_title);

    for (i = npre; i < g_strv_length((gchar**)args); i++)
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
    g_free (pid);
    g_free (xwin);
    g_array_free (a, TRUE);
    return result;
}

/*@null@*/ gchar**
split_quoted(const gchar* src, const gboolean unquote) {
    /* split on unquoted space, return array of strings;
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
        else if ((*p == ' ') && !dq && !sq) {
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
spawn(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    gchar *path = NULL;

    //TODO: allow more control over argument order so that users can have some arguments before the default ones from run_command, and some after
    if (argv_idx(argv, 0) &&
            ((path = find_existing_file(argv_idx(argv, 0)))) ) {
        run_command(path, 0,
                ((const gchar **) (argv->data + sizeof(gchar*))),
                FALSE, NULL);
        g_free(path);
    }
}

void
spawn_sync(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    gchar *path = NULL;

    if (argv_idx(argv, 0) &&
            ((path = find_existing_file(argv_idx(argv, 0)))) ) {
        run_command(path, 0,
                ((const gchar **) (argv->data + sizeof(gchar*))),
                    TRUE, &uzbl.comm.sync_stdout);
        g_free(path);
    }
}

void
spawn_sh(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    if (!uzbl.behave.shell_cmd) {
        g_printerr ("spawn_sh: shell_cmd is not set!\n");
        return;
    }

    guint i;
    gchar *spacer = g_strdup("");
    g_array_insert_val(argv, 1, spacer);
    gchar **cmd = split_quoted(uzbl.behave.shell_cmd, TRUE);

    for (i = 1; i < g_strv_length(cmd); i++)
        g_array_prepend_val(argv, cmd[i]);

    if (cmd) run_command(cmd[0], g_strv_length(cmd) + 1, (const gchar **) argv->data, FALSE, NULL);
    g_free (spacer);
    g_strfreev (cmd);
}

void
spawn_sh_sync(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    if (!uzbl.behave.shell_cmd) {
        g_printerr ("spawn_sh_sync: shell_cmd is not set!\n");
        return;
    }

    guint i;
    gchar *spacer = g_strdup("");
    g_array_insert_val(argv, 1, spacer);
    gchar **cmd = split_quoted(uzbl.behave.shell_cmd, TRUE);

    for (i = 1; i < g_strv_length(cmd); i++)
        g_array_prepend_val(argv, cmd[i]);

    if (cmd) run_command(cmd[0], g_strv_length(cmd) + 1, (const gchar **) argv->data,
                         TRUE, &uzbl.comm.sync_stdout);
    g_free (spacer);
    g_strfreev (cmd);
}

void
talk_to_socket(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;

    int fd, len;
    struct sockaddr_un sa;
    char* sockpath;
    ssize_t ret;
    struct pollfd pfd;
    struct iovec* iov;
    guint i;

    if(uzbl.comm.sync_stdout) uzbl.comm.sync_stdout = strfree(uzbl.comm.sync_stdout);

    /* This function could be optimised by storing a hash table of socket paths
       and associated connected file descriptors rather than closing and
       re-opening for every call. Also we could launch a script if socket connect
       fails. */

    /* First element argv[0] is path to socket. Following elements are tokens to
       write to the socket. We write them as a single packet with each token
       separated by an ASCII nul (\0). */
    if(argv->len < 2) {
        g_printerr("talk_to_socket called with only %d args (need at least two).\n",
            (int)argv->len);
        return;
    }

    /* copy socket path, null terminate result */
    sockpath = g_array_index(argv, char*, 0);
    g_strlcpy(sa.sun_path, sockpath, sizeof(sa.sun_path));
    sa.sun_family = AF_UNIX;

    /* create socket file descriptor and connect it to path */
    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(fd == -1) {
        g_printerr("talk_to_socket: creating socket failed (%s)\n", strerror(errno));
        return;
    }
    if(connect(fd, (struct sockaddr*)&sa, sizeof(sa))) {
        g_printerr("talk_to_socket: connect failed (%s)\n", strerror(errno));
        close(fd);
        return;
    }

    /* build request vector */
    iov = g_malloc(sizeof(struct iovec) * (argv->len - 1));
    if(!iov) {
        g_printerr("talk_to_socket: unable to allocated memory for token vector\n");
        close(fd);
        return;
    }
    for(i = 1; i < argv->len; ++i) {
        iov[i - 1].iov_base = g_array_index(argv, char*, i);
        iov[i - 1].iov_len = strlen(iov[i - 1].iov_base) + 1; /* string plus \0 */
    }

    /* write request */
    ret = writev(fd, iov, argv->len - 1);
    g_free(iov);
    if(ret == -1) {
        g_printerr("talk_to_socket: write failed (%s)\n", strerror(errno));
        close(fd);
        return;
    }

    /* wait for a response, with a 500ms timeout */
    pfd.fd = fd;
    pfd.events = POLLIN;
    while(1) {
        ret = poll(&pfd, 1, 500);
        if(ret == 1) break;
        if(ret == 0) errno = ETIMEDOUT;
        if(errno == EINTR) continue;
        g_printerr("talk_to_socket: poll failed while waiting for input (%s)\n",
            strerror(errno));
        close(fd);
        return;
    }

    /* get length of response */
    if(ioctl(fd, FIONREAD, &len) == -1) {
        g_printerr("talk_to_socket: cannot find daemon response length, "
            "ioctl failed (%s)\n", strerror(errno));
        close(fd);
        return;
    }

    /* if there is a response, read it */
    if(len) {
        uzbl.comm.sync_stdout = g_malloc(len + 1);
        if(!uzbl.comm.sync_stdout) {
            g_printerr("talk_to_socket: failed to allocate %d bytes\n", len);
            close(fd);
            return;
        }
        uzbl.comm.sync_stdout[len] = 0; /* ensure result is null terminated */

        ret = read(fd, uzbl.comm.sync_stdout, len);
        if(ret == -1) {
            g_printerr("talk_to_socket: failed to read from socket (%s)\n",
                strerror(errno));
            close(fd);
            return;
        }
    }

    /* clean up */
    close(fd);
    return;
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
                g_string_free(tmp, TRUE);
            }
    }
    else {
        gchar *tmp = g_strdup_printf("%s %s", cmd, param?param:"");
        send_event(COMMAND_ERROR, tmp, NULL);
        g_free(tmp);
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
    gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    return;
}

gboolean
set_var_value(const gchar *name, gchar *val) {
    uzbl_cmdprop *c = NULL;
    char *endp = NULL;
    char *buf = NULL;
    char *invalid_chars = "^°!\"§$%&/()=?'`'+~*'#-.:,;@<>| \\{}[]¹²³¼½";
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
                printf("Invalid variable name\n");
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

/*@null@*/ gchar*
init_fifo(gchar *dir) { /* return dir or, on error, free dir and return NULL */
    if (uzbl.comm.fifo_path) { /* get rid of the old fifo if one exists */
        if (unlink(uzbl.comm.fifo_path) == -1)
            g_warning ("Fifo: Can't unlink old fifo at %s\n", uzbl.comm.fifo_path);
        g_free(uzbl.comm.fifo_path);
        uzbl.comm.fifo_path = NULL;
    }

    GIOChannel *chan = NULL;
    GError *error = NULL;
    gchar *path = build_stream_name(FIFO, dir);

    if (!file_exists(path)) {
        if (mkfifo (path, 0666) == 0) {
            // we don't really need to write to the file, but if we open the file as 'r' we will block here, waiting for a writer to open the file.
            chan = g_io_channel_new_file(path, "r+", &error);
            if (chan) {
                if (g_io_add_watch(chan, G_IO_IN|G_IO_HUP, (GIOFunc) control_fifo, NULL)) {
                    if (uzbl.state.verbose)
                        printf ("init_fifo: created successfully as %s\n", path);
                        send_event(FIFO_SET, path, NULL);
                    uzbl.comm.fifo_path = path;
                    return dir;
                } else g_warning ("init_fifo: could not add watch on %s\n", path);
            } else g_warning ("init_fifo: can't open: %s\n", error->message);
        } else g_warning ("init_fifo: can't create %s: %s\n", path, strerror(errno));
    } else g_warning ("init_fifo: can't create %s: file exists\n", path);

    /* if we got this far, there was an error; cleanup */
    if (error) g_error_free (error);
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

    /* TODO: Do wee need to manually free the IO channel or is this
     *       happening implicitly on unref?
     */
    ret = g_ptr_array_remove_fast(uzbl.comm.connect_chan, chan);
    if(!ret)
        ret = g_ptr_array_remove_fast(uzbl.comm.client_chan, chan);

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
        g_warning ("Error reading: %s\n", error->message);
        remove_socket_from_array(clientchan);
        g_io_channel_shutdown(clientchan, TRUE, &error);
        return FALSE;
    } else if (ret == G_IO_STATUS_EOF) {
        remove_socket_from_array(clientchan);
        /* shutdown and remove channel watch from main loop */
        g_io_channel_shutdown(clientchan, TRUE, &error);
        return FALSE;
    }

    if (ctl_line) {
        parse_cmd_line (ctl_line, result);
        g_string_append_c(result, '\n');
        ret = g_io_channel_write_chars (clientchan, result->str, result->len,
                                        &len, &error);
        if (ret == G_IO_STATUS_ERROR) {
            g_warning ("Error writing: %s", error->message);
        }
        g_io_channel_flush(clientchan, &error);
    }

    if (error) g_error_free (error);
    g_string_free(result, TRUE);
    g_free(ctl_line);
    return TRUE;
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

    GIOChannel *chan = NULL;
    int sock, len;
    struct sockaddr_un local;
    gchar *path = build_stream_name(SOCKET, dir);

    sock = socket (AF_UNIX, SOCK_STREAM, 0);

    local.sun_family = AF_UNIX;
    strcpy (local.sun_path, path);
    unlink (local.sun_path);

    len = strlen (local.sun_path) + sizeof (local.sun_family);
    if (bind (sock, (struct sockaddr *) &local, len) != -1) {
        if (uzbl.state.verbose)
            printf ("init_socket: opened in %s\n", path);
        listen (sock, 5);

        if( (chan = g_io_channel_unix_new(sock)) ) {
            g_io_add_watch(chan, G_IO_IN|G_IO_HUP, (GIOFunc) control_socket, chan);
            uzbl.comm.socket_path = path;
            send_event(SOCKET_SET, path, NULL);
            return dir;
        }
    } else g_warning ("init_socket: could not open in %s: %s\n", path, strerror(errno));

    /* if we got this far, there was an error; cleanup */
    g_free(path);
    g_free(dir);
    return NULL;
}

/*
 NOTE: we want to keep variables like b->title_format_long in their "unprocessed" state
 it will probably improve performance if we would "cache" the processed variant, but for now it works well enough...
*/
// this function may be called very early when the templates are not set (yet), hence the checks
void
update_title (void) {
    Behaviour *b = &uzbl.behave;
    gchar *parsed;

    if (b->show_status) {
        if (b->title_format_short) {
            parsed = expand(b->title_format_short, 0);
            if (uzbl.gui.main_window)
                gtk_window_set_title (GTK_WINDOW(uzbl.gui.main_window), parsed);
            g_free(parsed);
        }
        if (b->status_format) {
            parsed = expand(b->status_format, 0);
            gtk_label_set_markup(GTK_LABEL(uzbl.gui.mainbar_label), parsed);
            g_free(parsed);
        }
        if (b->status_background) {
            GdkColor color;
            gdk_color_parse (b->status_background, &color);
            //labels and hboxes do not draw their own background. applying this on the vbox/main_window is ok as the statusbar is the only affected widget. (if not, we could also use GtkEventBox)
            if (uzbl.gui.main_window)
                gtk_widget_modify_bg (uzbl.gui.main_window, GTK_STATE_NORMAL, &color);
            else if (uzbl.gui.plug)
                gtk_widget_modify_bg (GTK_WIDGET(uzbl.gui.plug), GTK_STATE_NORMAL, &color);
        }
    } else {
        if (b->title_format_long) {
            parsed = expand(b->title_format_long, 0);
            if (uzbl.gui.main_window)
                gtk_window_set_title (GTK_WINDOW(uzbl.gui.main_window), parsed);
            g_free(parsed);
        }
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
      "signal::title-changed",                        (GCallback)title_change_cb,         NULL,
      "signal::selection-changed",                    (GCallback)selection_changed_cb,    NULL,
      "signal::load-progress-changed",                (GCallback)progress_change_cb,      NULL,
      "signal::load-committed",                       (GCallback)load_commit_cb,          NULL,
      "signal::load-started",                         (GCallback)load_start_cb,           NULL,
      "signal::load-finished",                        (GCallback)load_finish_cb,          NULL,
      "signal::load-error",                           (GCallback)load_error_cb,           NULL,
      "signal::hovering-over-link",                   (GCallback)link_hover_cb,           NULL,
      "signal::navigation-policy-decision-requested", (GCallback)navigation_decision_cb,  NULL,
      "signal::new-window-policy-decision-requested", (GCallback)new_window_cb,           NULL,
      "signal::download-requested",                   (GCallback)download_cb,             NULL,
      "signal::create-web-view",                      (GCallback)create_web_view_cb,      NULL,
      "signal::mime-type-policy-decision-requested",  (GCallback)mime_policy_cb,          NULL,
      "signal::populate-popup",                       (GCallback)populate_popup_cb,       NULL,
      "signal::focus-in-event",                       (GCallback)focus_cb,                NULL,
      "signal::focus-out-event",                      (GCallback)focus_cb,                NULL,
      NULL);
}

GtkWidget*
create_mainbar () {
    GUI *g = &uzbl.gui;

    g->mainbar = gtk_hbox_new (FALSE, 0);
    g->mainbar_label = gtk_label_new ("");
    gtk_label_set_selectable((GtkLabel *)g->mainbar_label, TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(g->mainbar_label), PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment (GTK_MISC(g->mainbar_label), 0, 0);
    gtk_misc_set_padding (GTK_MISC(g->mainbar_label), 2, 2);
    gtk_box_pack_start (GTK_BOX (g->mainbar), g->mainbar_label, TRUE, TRUE, 0);

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
        (g_strcmp0(actname, "sync_sh") == 0) ||
        (g_strcmp0(actname, "talk_to_socket") == 0)) {
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

/*@null@*/ gchar*
get_xdg_var (XDG_Var xdg) {
    const gchar* actual_value = getenv (xdg.environmental);
    const gchar* home         = getenv ("HOME");
    gchar* return_value;

    if (! actual_value || strcmp (actual_value, "") == 0) {
        if (xdg.default_value) {
            return_value = str_replace ("~", home, xdg.default_value);
        } else {
            return_value = NULL;
        }
    } else {
        return_value = str_replace("~", home, actual_value);
    }

    return return_value;
}

/*@null@*/ gchar*
find_xdg_file (int xdg_type, const char* filename) {
    /* xdg_type = 0 => config
       xdg_type = 1 => data
       xdg_type = 2 => cache*/

    gchar* xdgv = get_xdg_var (XDG[xdg_type]);
    gchar* temporary_file = g_strconcat (xdgv, filename, NULL);
    g_free (xdgv);

    gchar* temporary_string;
    char*  saveptr;
    char*  buf;

    if (! file_exists (temporary_file) && xdg_type != 2) {
        buf = get_xdg_var (XDG[3 + xdg_type]);
        temporary_string = (char *) strtok_r (buf, ":", &saveptr);
        g_free(buf);

        while ((temporary_string = (char * ) strtok_r (NULL, ":", &saveptr)) && ! file_exists (temporary_file)) {
            g_free (temporary_file);
            temporary_file = g_strconcat (temporary_string, filename, NULL);
        }
    }

    //g_free (temporary_string); - segfaults.

    if (file_exists (temporary_file)) {
        return temporary_file;
    } else {
        g_free(temporary_file);
        return NULL;
    }
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
        GArray* lines = read_file_by_line (s->config_file);
        int i = 0;
        gchar* line;

        while ((line = g_array_index(lines, gchar*, i))) {
            parse_cmd_line (line, NULL);
            i ++;
            g_free (line);
        }
        g_array_free (lines, TRUE);
    } else {
        if (uzbl.state.verbose)
            printf ("No configuration file loaded.\n");
    }

    if(s->connect_socket_names)
        init_connect_socket();

    g_signal_connect_after(n->soup_session, "request-started", G_CALLBACK(handle_cookies), NULL);
}

void handle_cookies (SoupSession *session, SoupMessage *msg, gpointer user_data){
    (void) session;
    (void) user_data;
    //if (!uzbl.behave.cookie_handler)
    //     return;

    soup_message_add_header_handler(msg, "got-headers", "Set-Cookie", G_CALLBACK(save_cookies), NULL);
    GString *s = g_string_new ("");
    SoupURI * soup_uri = soup_message_get_uri(msg);
    g_string_printf(s, "GET '%s' '%s' '%s'", soup_uri->scheme, soup_uri->host, soup_uri->path);
    if(uzbl.behave.cookie_handler)
        run_handler(uzbl.behave.cookie_handler, s->str);

    if(uzbl.behave.cookie_handler &&
            uzbl.comm.sync_stdout && strcmp (uzbl.comm.sync_stdout, "") != 0) {
        char *p = strchr(uzbl.comm.sync_stdout, '\n' );
        if ( p != NULL ) *p = '\0';
        soup_message_headers_replace (msg->request_headers, "Cookie", (const char *) uzbl.comm.sync_stdout);

    }
    if (uzbl.comm.sync_stdout)
        uzbl.comm.sync_stdout = strfree(uzbl.comm.sync_stdout);

    g_string_free(s, TRUE);
}

void
save_cookies (SoupMessage *msg, gpointer user_data){
    (void) user_data;
    GSList *ck;
    char *cookie;
    for (ck = soup_cookies_from_response(msg); ck; ck = ck->next){
        cookie = soup_cookie_to_set_cookie_header(ck->data);
        SoupURI * soup_uri = soup_message_get_uri(msg);
        GString *s = g_string_new ("");
        g_string_printf(s, "PUT '%s' '%s' '%s' '%s'", soup_uri->scheme, soup_uri->host, soup_uri->path, cookie);
        run_handler(uzbl.behave.cookie_handler, s->str);
        g_free (cookie);
        g_string_free(s, TRUE);
    }
    g_slist_free(ck);
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
    uzbl.state.keycmd = g_strdup("");

    for(i=0; sigs[i]; i++) {
        if(setup_signal(sigs[i], catch_signal) == SIG_ERR)
            fprintf(stderr, "uzbl: error hooking %d: %s\n", sigs[i], strerror(errno));
    }
    event_buffer_timeout(10);

    uzbl.info.webkit_major = WEBKIT_MAJOR_VERSION;
    uzbl.info.webkit_minor = WEBKIT_MINOR_VERSION;
    uzbl.info.webkit_micro = WEBKIT_MICRO_VERSION;
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
    if (g_strstr_len (uri, 11, "javascript:") != NULL) {
        eval_js(uzbl.gui.web_view, uri, NULL);
        return;
    }
    newuri = g_string_new (uri);
    if (!soup_uri_new(uri)) {
        GString* fullpath = g_string_new ("");
        if (g_path_is_absolute (newuri->str))
            g_string_assign (fullpath, newuri->str);
        else {
            gchar* wd;
            wd = g_get_current_dir ();
            g_string_assign (fullpath, g_build_filename (wd, newuri->str, NULL));
            free(wd);
        }
        struct stat stat_result;
        if (! g_stat(fullpath->str, &stat_result)) {
            g_string_prepend (fullpath, "file://");
            g_string_assign (newuri, fullpath->str);
        }
        else
            g_string_prepend (newuri, "http://");
        g_string_free (fullpath, TRUE);
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
        uzbl.xwin = GDK_WINDOW_XID (GTK_WIDGET (uzbl.gui.main_window)->window);
    }

    uzbl.gui.scbar_v = (GtkScrollbar*) gtk_vscrollbar_new (NULL);
    uzbl.gui.bar_v = gtk_range_get_adjustment((GtkRange*) uzbl.gui.scbar_v);
    uzbl.gui.scbar_h = (GtkScrollbar*) gtk_hscrollbar_new (NULL);
    uzbl.gui.bar_h = gtk_range_get_adjustment((GtkRange*) uzbl.gui.scbar_h);
    gtk_widget_set_scroll_adjustments ((GtkWidget*) uzbl.gui.web_view, uzbl.gui.bar_h, uzbl.gui.bar_v);

    if(!uzbl.state.instance_name)
        uzbl.state.instance_name = itos((int)uzbl.xwin);

    GString *tmp = g_string_new("");
    g_string_printf(tmp, "%d", getpid());
    uzbl.info.pid_str = g_string_free(tmp, FALSE);
    send_event(INSTANCE_START, uzbl.info.pid_str, NULL);

    if(uzbl.state.plug_mode) {
        char *t = itos(gtk_plug_get_id(uzbl.gui.plug));
        send_event(PLUG_CREATED, t, NULL);
        g_free(t);
    }

    /* generate an event with a list of built in commands */
    builtins();

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
