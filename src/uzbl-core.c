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
#include "events.h"
#include "gui.h"
#include "inspector.h"
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

                    uzbl_variables_expand (ret, buf);

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
                        GArray *tmp = g_array_new(TRUE, FALSE, sizeof (gchar *));
                        mycmd = expand(ret+1, 2);
                        g_array_append_val(tmp, mycmd);

                        cmd_js_file(uzbl.gui.web_view, tmp, js_ret);
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
        uzbl_events_send (INSTANCE_EXIT, NULL,
            TYPE_INT, uzbl.info.pid,
            NULL);
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

/* -- CORE FUNCTIONS -- */

gboolean
valid_name(const gchar* name) {
    char *invalid_chars = "\t^°!\"§$%&/()=?'`'+~*'#-:,;@<>| \\{}[]¹²³¼½";
    return strpbrk(name, invalid_chars) == NULL;
}

void
update_title(void) {
    Behaviour *b = &uzbl.behave;

    const gchar *title_format = b->title_format_long;

    /* Update the status bar if shown */
    if (uzbl_variables_get_int ("show_status")) {
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

    if (g_strcmp0(s->config_file, "-") == 0) {
        s->config_file = NULL;
        uzbl_io_init_stdin ();
    }

    else if (!s->config_file) {
        s->config_file = find_xdg_file(0, "/uzbl/config");
    }

    /* Load config file, if any */
    if (s->config_file) {
        uzbl_commands_load_file (s->config_file);
        g_setenv("UZBL_CONFIG", s->config_file, TRUE);
    } else if (uzbl.state.verbose)
        printf ("No configuration file loaded.\n");

    if (s->connect_socket_names)
        uzbl_io_init_connect_socket ();
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

    uzbl_events_init ();

    /* HTTP client */
    uzbl.net.soup_session = webkit_get_default_session();
    uzbl_soup_init (uzbl.net.soup_session);

    uzbl_commands_init ();
    uzbl_variables_init ();

    /* XDG */
    ensure_xdg_vars();

    /* GUI */
    gtk_init(&argc, &argv);

    uzbl_gui_init ();
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

    uzbl_events_send (INSTANCE_START, NULL, TYPE_INT, uzbl.info.pid, NULL);

    if (uzbl.state.plug_mode) {
        uzbl_events_send (PLUG_CREATED, NULL, TYPE_INT, gtk_plug_get_id (uzbl.gui.plug), NULL);
    }

    /* Generate an event with a list of built in commands */
    uzbl_commands_send_builtin_event ();

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
    uzbl_inspector_init ();

    /* Options overriding */
    if (verbose_override > uzbl.state.verbose)
        uzbl.state.verbose = verbose_override;

    if (uri_override) {
        uzbl_variables_set ("uri", uri_override);
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
