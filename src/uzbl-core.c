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
#include "gui.h"
#include "inspector.h"
#include "config.h"
#include "util.h"
#include "menu.h"
#include "io.h"
#include "variables.h"
#include "type.h"
#include "soup.h"

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

                    expand_variable(buf, ret);

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
        send_event (INSTANCE_EXIT, NULL, TYPE_INT, uzbl.info.pid, NULL);
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

static void
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
    else if (*end == '!')
        value = amount;
    else
        value += amount;

    max_value = gtk_adjustment_get_upper(bar) - page_size;

    if (value < 0)
        value = 0; /* don't scroll past the beginning of the page */
    if (value > max_value)
        value = max_value; /* don't scroll past the end of the page */

    gtk_adjustment_set_value (bar, value);
}

/* -- CORE FUNCTIONS -- */

/* just a wrapper so parse_cmd_line can be used with for_each_line_in_file */
static void
parse_cmd_line_cb(const char *line, void *user_data) {
    (void) user_data;
    parse_cmd_line(line, NULL);
}

void
run_command_file (const gchar *path) {
    if(!for_each_line_in_file(path, parse_cmd_line_cb, NULL)) {
        gchar *tmp = g_strdup_printf("File %s can not be read.", path);
        send_event(COMMAND_ERROR, NULL, TYPE_STR, tmp, NULL);
        g_free(tmp);
    }
}

/* Javascript*/
void
eval_js(WebKitWebView * web_view, const gchar *script, GString *result, const char *file) {
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
search_text (WebKitWebView *page, const gchar *key, const gboolean forward) {
    if (key && (*key != '\0')) {
        if (g_strcmp0 (uzbl.state.searchtx, key) != 0) {
            webkit_web_view_unmark_text_matches (page);
            webkit_web_view_mark_text_matches (page, key, FALSE, 0);
            g_free (uzbl.state.searchtx);
            uzbl.state.searchtx = g_strdup (key);
        }
    }

    if (uzbl.state.searchtx) {
        if (uzbl.state.verbose)
            printf ("Searching: %s\n", uzbl.state.searchtx);
        webkit_web_view_set_highlight_text_matches (page, TRUE);
        webkit_web_view_search_text (page, uzbl.state.searchtx, FALSE, forward, TRUE);
    }
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

/*@null@*/ static gchar**
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
        if ((*p == '\\') && unquote && p[1]) g_string_append_c(s, *++p);
        else if (*p == '\\' && p[1]) { g_string_append_c(s, *p++);
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

    if (!argv_idx(argv, 0))
        return;

    const gchar **args = &g_array_index(argv, const gchar *, 1);

    path = find_existing_file(argv_idx(argv, 0));
    if(path) {
        gchar *r = NULL;
        run_command(path, args, result != NULL, result ? &r : NULL);
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
    } else {
        g_printerr ("Failed to spawn child process: %s not found\n", argv_idx(argv, 0));
    }
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

    g_array_insert_val(argv, 1, cmd[0]);

    for (i = g_strv_length(cmd)-1; i > 0; i--)
        g_array_prepend_val(argv, cmd[i]);

    if (result) {
        gchar *r = NULL;
        run_command(cmd[0], (const gchar **) argv->data, TRUE, &r);
        g_string_assign(result, r);
        g_free(r);
    } else
        run_command(cmd[0], (const gchar **) argv->data, FALSE, NULL);

    g_strfreev (cmd);
}

void
run_parsed_command(const CommandInfo *c, GArray *a, GString *result) {
    /* send the COMMAND_EXECUTED event, except for set and event/request commands */
    if(strcmp("set", c->key)   &&
       strcmp("event", c->key) &&
       strcmp("request", c->key)) {
        Event *event = format_event (COMMAND_EXECUTED, NULL,
            TYPE_NAME, c->key,
            TYPE_STR_ARRAY, a,
            NULL);

        /* might be destructive on array a */
        c->function(uzbl.gui.web_view, a, result);

        send_formatted_event (event);
        event_free (event);
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

gboolean
valid_name(const gchar* name) {
    char *invalid_chars = "\t^°!\"§$%&/()=?'`'+~*'#-:,;@<>| \\{}[]¹²³¼½";
    return strpbrk(name, invalid_chars) == NULL;
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
    if (get_show_status()) {
        title_format = b->title_format_short;

        gchar *parsed = expand(b->status_format, 0);
        uzbl_status_bar_update_left(uzbl.gui.status_bar, parsed);
        g_free(parsed);

        parsed = expand(b->status_format_right, 0);
        uzbl_status_bar_update_right(uzbl.gui.status_bar, parsed);
        g_free(parsed);
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
settings_init () {
    State*   s = &uzbl.state;
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
        run_command_file(s->config_file);
        g_setenv("UZBL_CONFIG", s->config_file, TRUE);
    } else if (uzbl.state.verbose)
        printf ("No configuration file loaded.\n");

    if (s->connect_socket_names)
        init_connect_socket();
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
#ifdef USE_WEBKIT2
    uzbl.info.webkit2          = 1;
#else
    uzbl.info.webkit2          = 0;
#endif
    uzbl.info.arch             = ARCH;
    uzbl.info.commit           = COMMIT;

    uzbl.state.last_result  = NULL;

    /* BUG There isn't a getter for this; need to maintain separately. */
    uzbl.behave.maintain_history = TRUE;

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

#if !GLIB_CHECK_VERSION(2, 31, 0)
    if (!g_thread_supported())
        g_thread_init(NULL);
#endif

    /* TODO: move the handler setup to event_buffer_timeout and disarm the
     * handler in empty_event_buffer? */
    if (setup_signal(SIGALRM, empty_event_buffer) == SIG_ERR)
        fprintf(stderr, "uzbl: error hooking %d: %s\n", SIGALRM, strerror(errno));
    event_buffer_timeout(10);

    /* HTTP client */
    uzbl.net.soup_session = webkit_get_default_session();
    uzbl_soup_init (uzbl.net.soup_session);

    commands_hash();
    variables_hash();

    /* XDG */
    ensure_xdg_vars();

    /* GUI */
    gtk_init(&argc, &argv);

    uzbl_gui_init (uzbl.state.plug_mode);
}


#ifndef UZBL_LIBRARY
/** -- MAIN -- **/
int
main (int argc, char* argv[]) {
    Window xwin;

    initialize(argc, argv);

    if (uzbl.gui.main_window) {
        /* We need to ensure there is a window, before we can get XID */
        gtk_widget_realize (GTK_WIDGET (uzbl.gui.main_window));
        xwin = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (uzbl.gui.main_window)));

        gchar *xwin_str = g_strdup_printf("%d", (int)xwin);
        g_setenv("UZBL_XID", xwin_str, TRUE);
        g_free (xwin_str);

        gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    }

    uzbl.info.pid     = getpid();
    uzbl.info.pid_str = g_strdup_printf("%d", uzbl.info.pid);
    g_setenv("UZBL_PID", uzbl.info.pid_str, TRUE);

    if(!uzbl.state.instance_name)
        uzbl.state.instance_name = uzbl.info.pid_str;

    send_event(INSTANCE_START, NULL, TYPE_INT, uzbl.info.pid, NULL);

    if (uzbl.state.plug_mode) {
        send_event(PLUG_CREATED, NULL, TYPE_INT, gtk_plug_get_id (uzbl.gui.plug), NULL);
    }

    /* Generate an event with a list of built in commands */
    builtins();

    /* Check uzbl is in window mode before getting/setting geometry */
    if (uzbl.gui.main_window && uzbl.gui.geometry) {
        gchar *geometry = g_strdup(uzbl.gui.geometry);
        set_geometry(geometry);
        g_free(geometry);
    }

    gchar *uri_override = (uzbl.state.uri ? g_strdup(uzbl.state.uri) : NULL);
    if (argc > 1 && !uzbl.state.uri)
        uri_override = g_strdup(argv[1]);

    gboolean verbose_override = uzbl.state.verbose;

    /* Finally show the window */
    if (uzbl.gui.main_window) {
        gtk_widget_show_all (GTK_WIDGET (uzbl.gui.main_window));
    } else {
        gtk_widget_show_all (GTK_WIDGET (uzbl.gui.plug));
    }

    /* Read configuration file */
    settings_init();

    /* Update status bar */
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
            printf("plug_id %i\n", (int)gtk_plug_get_id(uzbl.gui.plug));
        else
            printf("window_id %i\n",(int) xwin);
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
