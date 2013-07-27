/* Original code taken from the example webkit-gtk+ application. see notice
 * below. Modified code is licensed under the GPL 3.  See LICENSE file. */

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

#include "commands.h"
#ifndef UZBL_LIBRARY
#include "config.h"
#endif
#include "events.h"
#include "gui.h"
#include "inspector.h"
#include "io.h"
#include "js.h"
#ifndef USE_WEBKIT2
#include "scheme.h"
#include "soup.h"
#endif
#include "status-bar.h"
#include "util.h"
#include "variables.h"

#include <JavaScriptCore/JavaScript.h>

#include <stdlib.h>

/* =========================== PUBLIC API =========================== */

UzblCore uzbl;

/* Commandline arguments. */
static const GOptionEntry
options[] = {
    { "uri",            'u', 0, G_OPTION_ARG_STRING,       &uzbl.state.uri,
        "Uri to load at startup (equivalent to 'uzbl <uri>' after uzbl has launched)", "URI" },
    { "verbose",        'v', 0, G_OPTION_ARG_NONE,         &uzbl.state.verbose,
        "Whether to print all messages or just errors.",                                                  NULL },
    { "named",          'n', 0, G_OPTION_ARG_STRING,       &uzbl.state.instance_name,
        "Name of the current instance (defaults to Xorg window id or random for GtkSocket mode)",         "NAME" },
    { "config",         'c', 0, G_OPTION_ARG_STRING,       &uzbl.state.config_file,
        "Path to config file or '-' for stdin",                                                           "FILE" },
    /* TODO: explain the difference between these two options */
    { "socket",         's', 0, G_OPTION_ARG_INT,          &uzbl.state.socket_id,
        "Xembed socket ID, this window should embed itself",                                              "SOCKET" },
    { "embed",          'e', 0, G_OPTION_ARG_NONE,         &uzbl.state.embed,
        "Whether this window should expect to be embedded",                                               NULL },
    { "connect-socket",  0,  0, G_OPTION_ARG_STRING_ARRAY, &uzbl.state.connect_socket_names,
        "Connect to server socket for event managing",                                                    "CSOCKET" },
    { "print-events",   'p', 0, G_OPTION_ARG_NONE,         &uzbl.state.events_stdout,
        "Whether to print events to stdout.",                                                             NULL },
    { "geometry",       'g', 0, G_OPTION_ARG_STRING,       &uzbl.gui.geometry,
        "Set window geometry (format: 'WIDTHxHEIGHT+-X+-Y' or 'maximized')",                              "GEOMETRY" },
    { "version",        'V', 0, G_OPTION_ARG_NONE,         &uzbl.behave.print_version,
        "Print the version and exit",                                                                     NULL },
    { NULL,      0, 0, 0, NULL, NULL, NULL }
};

static void
ensure_xdg_vars ();

/* Set up gtk, gobject, variable defaults and other things that tests and other
 * external applications need to do anyhow. */
void
uzbl_initialize (int argc, char **argv)
{
    /* Initialize variables */
    g_mutex_init (&uzbl.state.reply_lock);
    g_cond_init (&uzbl.state.reply_cond);

    uzbl.state.socket_id       = 0;
    uzbl.state.plug_mode       = FALSE;

    uzbl.state.executable_path = g_strdup (argv[0]);
    uzbl.state.selected_url    = NULL;
    uzbl.state.searchtx        = NULL;

#ifdef USE_WEBKIT2
    uzbl.info.webkit_major     = webkit_get_major_version ();
    uzbl.info.webkit_minor     = webkit_get_minor_version ();
    uzbl.info.webkit_micro     = webkit_get_micro_version ();
    uzbl.info.webkit_ua_major  = 0; /* TODO: What are these in WebKit2? */
    uzbl.info.webkit_ua_minor  = 0;
#else
    uzbl.info.webkit_major     = webkit_major_version ();
    uzbl.info.webkit_minor     = webkit_minor_version ();
    uzbl.info.webkit_micro     = webkit_micro_version ();
    uzbl.info.webkit_ua_major  = WEBKIT_USER_AGENT_MAJOR_VERSION;
    uzbl.info.webkit_ua_minor  = WEBKIT_USER_AGENT_MINOR_VERSION;
#endif
    uzbl.info.webkit2          =
#ifdef USE_WEBKIT2
        TRUE
#else
        FALSE
#endif
        ;
    uzbl.info.arch             = ARCH;
    uzbl.info.commit           = COMMIT;

    uzbl.state.last_result     = NULL;

    /* Parse commandline arguments. */
    GOptionContext *context = g_option_context_new ("[ uri ] - load a uri by default");
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_parse (context, &argc, &argv, NULL);
    g_option_context_free (context);

    /* Only print version. */
    if (uzbl.behave.print_version) {
        printf ("Commit: %s\n", COMMIT);
        exit (EXIT_SUCCESS);
    }

    /* Embedded mode. */
    if (uzbl.state.socket_id || uzbl.state.embed) {
        uzbl.state.plug_mode = TRUE;
    }

#if !GLIB_CHECK_VERSION (2, 31, 0)
    if (!g_thread_supported ()) {
        g_thread_init (NULL);
    }
#endif

    /* HTTP client. */
#ifndef USE_WEBKIT2 /* FIXME: This seems important... */
    uzbl.net.soup_session = webkit_get_default_session ();
    uzbl_soup_init (uzbl.net.soup_session);
#endif

    uzbl_io_init ();
    uzbl_js_init ();
    uzbl_variables_init ();
    uzbl_commands_init ();
    uzbl_events_init ();

#ifndef USE_WEBKIT2
    uzbl_scheme_init ();
#endif

    /* XDG */
    ensure_xdg_vars ();

    /* GUI */
    gtk_init (&argc, &argv);

    uzbl_gui_init ();

    uzbl.state.started = TRUE;
}

void
uzbl_free ()
{
    uzbl_io_free ();
}

#ifndef UZBL_LIBRARY
/* ========================= MAIN  FUNCTION ========================= */

static void
read_config_file ();
static void
clean_up ();

int
main (int argc, char *argv[])
{
    Window xwin;

    uzbl_initialize (argc, argv);

    /* Initialize the inspector. */
    uzbl_inspector_init ();

    if (uzbl.gui.main_window) {
        /* We need to ensure there is a window, before we can get XID. */
        gtk_widget_realize (GTK_WIDGET (uzbl.gui.main_window));
        xwin = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (uzbl.gui.main_window)));

        gchar *xwin_str = g_strdup_printf ("%d", (int)xwin);
        g_setenv ("UZBL_XID", xwin_str, TRUE);
        g_free (xwin_str);

        gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    }

    uzbl.info.pid     = getpid ();
    uzbl.info.pid_str = g_strdup_printf ("%d", uzbl.info.pid);
    g_setenv ("UZBL_PID", uzbl.info.pid_str, TRUE);

    if (!uzbl.state.instance_name) {
        uzbl.state.instance_name = uzbl.info.pid_str;
    }

    uzbl_events_send (INSTANCE_START, NULL,
        TYPE_INT, uzbl.info.pid,
        NULL);

    if (uzbl.state.plug_mode) {
        uzbl_events_send (PLUG_CREATED, NULL,
            TYPE_INT, gtk_plug_get_id (uzbl.gui.plug),
            NULL);
    }

    /* Generate an event with a list of built in commands. */
    uzbl_commands_send_builtin_event ();

    /* Check uzbl is in window mode before getting/setting geometry */
    if (uzbl.gui.main_window && uzbl.gui.geometry) {
        GArray *args = uzbl_commands_args_new ();
        uzbl_commands_args_append (args, g_strdup (uzbl.gui.geometry));
        uzbl_commands_run_argv ("geometry", args, NULL);
        uzbl_commands_args_free (args);
    }

    gboolean verbose_override = uzbl.state.verbose;

    /* Finally show the window */
    if (uzbl.gui.main_window) {
        gtk_widget_show_all (GTK_WIDGET (uzbl.gui.main_window));
    } else {
        gtk_widget_show_all (GTK_WIDGET (uzbl.gui.plug));
    }

    guint i;

    /* Load default config. */
    for (i = 0; default_config[i].command; ++i) {
        uzbl_commands_run (default_config[i].command, NULL);
    }

    /* Read configuration file */
    read_config_file ();

    if (uzbl.state.exit) {
        goto main_exit;
    }

    /* Update status bar. */
    uzbl_gui_update_title ();

    /* Options overriding. */
    if (verbose_override > uzbl.state.verbose) {
        uzbl.state.verbose = verbose_override;
    }

    gchar *uri_override = (uzbl.state.uri ? g_strdup (uzbl.state.uri) : NULL);
    if ((1 < argc) && !uzbl.state.uri) {
        uri_override = g_strdup (argv[1]);
    }

    if (uri_override) {
        GArray *argv = uzbl_commands_args_new ();
        uzbl_commands_args_append (argv, uri_override);
        uzbl_commands_run_argv ("uri", argv, NULL);
        uzbl_commands_args_free (argv);
        uri_override = NULL;
    }

    /* Verbose feedback. */
    if (uzbl.state.verbose) {
        printf ("Uzbl start location: %s\n", argv[0]);
        if (uzbl.state.socket_id) {
            printf ("plug_id %d\n", (int)gtk_plug_get_id (uzbl.gui.plug));
        } else {
            printf ("window_id %d\n", (int)xwin);
        }
        printf ("pid %i\n", getpid ());
        printf ("name: %s\n", uzbl.state.instance_name);
        printf ("commit: %s\n", uzbl.info.commit);
    }

    uzbl.state.gtk_started = TRUE;

    gtk_main ();

main_exit:
    /* Cleanup and exit. */
    clean_up ();

    return EXIT_SUCCESS;
}
#endif

/* ===================== HELPER IMPLEMENTATIONS ===================== */

typedef enum {
    XDG_BEGIN,

    XDG_DATA = XDG_BEGIN,
    XDG_CONFIG,
    XDG_CACHE,

    XDG_END
} XdgDir;

static gchar *
get_xdg_var (gboolean user, XdgDir type);

typedef struct {
    const gchar *environment;
    const gchar *default_value;
} XdgVar;

static const XdgVar
xdg_user[] = {
    { "XDG_DATA_HOME",   "~/.local/share" },
    { "XDG_CONFIG_HOME", "~/.config" },
    { "XDG_CACHE_HOME",  "~/.cache" }
};

void
ensure_xdg_vars ()
{
    XdgDir i;

    for (i = XDG_DATA; i < XDG_END; ++i) {
        gchar *xdg = get_xdg_var (TRUE, i);

        if (!xdg) {
            continue;
        }

        g_setenv (xdg_user[i].environment, xdg, FALSE);

        g_free (xdg);
    }
}

#ifndef UZBL_LIBRARY
static gchar *
find_xdg_file (XdgDir dir, const char* basename);

void
read_config_file ()
{
    if (!g_strcmp0 (uzbl.state.config_file, "-")) {
        uzbl.state.config_file = NULL;
        uzbl_io_init_stdin ();
    } else if (!uzbl.state.config_file) {
        uzbl.state.config_file = find_xdg_file (XDG_CONFIG, "/uzbl/config");
    }

    /* Load config file, if any. */
    if (uzbl.state.config_file) {
        uzbl_commands_load_file (uzbl.state.config_file);
        g_setenv ("UZBL_CONFIG", uzbl.state.config_file, TRUE);
    } else {
        uzbl_debug ("No configuration file loaded.\n");
    }

    if (uzbl.state.connect_socket_names) {
        gchar **name = uzbl.state.connect_socket_names;

        while (name && *name) {
            uzbl_io_init_connect_socket (*name++);
        }
    }
}

void
clean_up ()
{
    g_mutex_clear (&uzbl.state.reply_lock);
    g_cond_clear (&uzbl.state.reply_cond);

    if (uzbl.info.pid_str) {
        uzbl_events_send (INSTANCE_EXIT, NULL,
            TYPE_INT, uzbl.info.pid,
            NULL);
        g_free (uzbl.info.pid_str);
        uzbl.info.pid_str = NULL;
    }

    g_free (uzbl.state.last_result);

    if (uzbl.state.jscontext) {
        JSGlobalContextRelease (uzbl.state.jscontext);
    }

    if (uzbl.state.executable_path) {
        g_free (uzbl.state.executable_path);
        uzbl.state.executable_path = NULL;
    }

    if (uzbl.behave.commands) {
        g_hash_table_destroy (uzbl.behave.commands);
        uzbl.behave.commands = NULL;
    }

    if (uzbl.state.event_buffer) {
        g_ptr_array_free (uzbl.state.event_buffer, TRUE);
        uzbl.state.event_buffer = NULL;
    }

    if (uzbl.state.reply) {
        g_free (uzbl.state.reply);
        uzbl.state.reply = NULL;
    }

    if (uzbl.behave.status_background) {
        g_free (uzbl.behave.status_background);
        uzbl.behave.status_background = NULL;
    }

    uzbl_variables_free ();

    if (uzbl.net.soup_cookie_jar) {
        g_object_unref (uzbl.net.soup_cookie_jar);
        uzbl.net.soup_cookie_jar = NULL;
    }

    if (uzbl.state.cmd_q) {
        g_async_queue_unref (uzbl.state.cmd_q);
        uzbl.state.cmd_q = NULL;
    }

    if (uzbl.state.io_thread) {
        g_thread_unref (uzbl.state.io_thread);
        uzbl.state.io_thread = NULL;
    }
}
#endif

static const XdgVar
xdg_system[] = {
    { "XDG_CONFIG_DIRS", "/etc/xdg" },
    { "XDG_DATA_DIRS",   "/usr/local/share/:/usr/share/" }
};

gchar *
get_xdg_var (gboolean user, XdgDir dir)
{
    XdgVar const *vars = user ? xdg_user : xdg_system;
    XdgVar xdg = vars[dir];

    const gchar *actual_value = getenv (xdg.environment);

    if (!actual_value || !actual_value[0]) {
        actual_value = xdg.default_value;
    }

    if (!actual_value) {
        return NULL;
    }

    /* TODO: Handle home == NULL. */
    const gchar *home = getenv ("HOME");

    return str_replace("~", home, actual_value);
}

#ifndef UZBL_LIBRARY
gchar *
find_xdg_file (XdgDir dir, const char* basename)
{
    gchar *dirs = get_xdg_var (TRUE, dir);
    gchar *path = g_strconcat (dirs, basename, NULL);
    g_free (dirs);

    if (file_exists (path)) {
        return path; /* We found the file. */
    }

    g_free (path);

    if (dir == XDG_CACHE) {
        return NULL; /* There's no system cache directory. */
    }

    /* The file doesn't exist in the expected directory, check if it exists in
     * one of the system-wide directories. */
    char *system_dirs = get_xdg_var (FALSE, dir);
    path = find_existing_file_options (system_dirs, basename);
    g_free (system_dirs);

    return path;
}
#endif
