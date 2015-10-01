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
#include "config.h"
#include "events.h"
#include "gui.h"
#include "io.h"
#include "setup.h"
#ifndef USE_WEBKIT2
#include "soup.h"
#endif
#include "type.h"
#include "util.h"
#include "variables.h"

#ifdef HAVE_LIBSOUP_CHECK_VERSION
#include <libsoup/soup-version.h>
#endif

#include <JavaScriptCore/JavaScript.h>

#include <stdlib.h>

/* =========================== PUBLIC API =========================== */

UzblCore uzbl;

static void
ensure_xdg_vars ();
static void
read_config_file (const gchar *file);

/* Set up gtk, gobject, variable defaults and other things that tests and other
 * external applications need to do anyhow. */
void
uzbl_init (int *argc, char ***argv)
{
    gchar *uri = NULL;
    gboolean verbose = FALSE;
    gchar *config_file = NULL;
    gchar **connect_socket_names = NULL;
    gboolean print_events = FALSE;
    gchar *geometry = NULL;
    gboolean print_version = FALSE;
    gboolean bug_info = FALSE;

    /* Commandline arguments. */
    const GOptionEntry
    options[] = {
        { "uri",               'u', 0, G_OPTION_ARG_STRING,       &uri,
            "Uri to load at startup (equivalent to 'uzbl <uri>' after uzbl has launched)", "URI" },
        { "verbose",           'v', 0, G_OPTION_ARG_NONE,         &verbose,
            "Whether to print all messages or just errors.",                                                 NULL },
        { "named",             'n', 0, G_OPTION_ARG_STRING,       &uzbl.state.instance_name,
            "Name of the current instance (defaults to Xorg window id or random for GtkSocket mode)",        "NAME" },
        { "config",            'c', 0, G_OPTION_ARG_STRING,       &config_file,
            "Path to config file or '-' for stdin",                                                          "FILE" },
        /* TODO: explain the difference between these two options */
        { "xembed-socket",     's', 0, G_OPTION_ARG_INT,          &uzbl.state.xembed_socket_id,
            "Xembed socket ID, this window should embed itself",                                             "SOCKET" },
        { "connect-socket",     0,  0, G_OPTION_ARG_STRING_ARRAY, &connect_socket_names,
            "Connect to server socket for event managing",                                                   "CSOCKET" },
        { "print-events",      'p', 0, G_OPTION_ARG_NONE,         &print_events,
            "Whether to print events to stdout.",                                                            NULL },
        { "geometry",          'g', 0, G_OPTION_ARG_STRING,       &geometry,
            "Set window geometry (format: 'WIDTHxHEIGHT+-X+-Y' or 'maximized')",                             "GEOMETRY" },
        { "version",           'V', 0, G_OPTION_ARG_NONE,         &print_version,
            "Print the version and exit",                                                                    NULL },
        { "bug-info",          'B', 0, G_OPTION_ARG_NONE,         &bug_info,
            "Print information for a bug report and exit",                                                   NULL },
#ifdef USE_WEBKIT2
        { "web-extensions-dir", 0,  0, G_OPTION_ARG_STRING,       &uzbl.state.web_extensions_directory,
            "Directory that will be searched for webkit extensions",                                         "DIR" },
#endif
        { NULL,      0, 0, 0, NULL, NULL, NULL }
    };

    /* Parse commandline arguments. */
    GOptionContext *context = g_option_context_new ("");
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_parse (context, argc, argv, NULL);
    g_option_context_free (context);

#ifdef USE_WEBKIT2
    if (!uzbl.state.web_extensions_directory) {
        uzbl.state.web_extensions_directory = LIBDIR "/web-extensions";
    }
#endif

    /* Print bug information. */
    if (bug_info) {
        printf ("Commit: %s\n", COMMIT);
        printf ("GTK compile: %d.%d.%d\n",
            GTK_MAJOR_VERSION,
            GTK_MINOR_VERSION,
            GTK_MICRO_VERSION);
        printf ("GTK run: %d.%d.%d\n",
            gtk_major_version,
            gtk_minor_version,
            gtk_micro_version);
        printf ("WebKit compile: %d.%d.%d\n",
            WEBKIT_MAJOR_VERSION,
            WEBKIT_MINOR_VERSION,
            WEBKIT_MICRO_VERSION);
#ifdef USE_WEBKIT2
#define webkit_version(type) webkit_get_##type##_version ()
#else
#define webkit_version(type) webkit_##type##_version ()
#endif
        printf ("WebKit run: %d.%d.%d\n",
            webkit_version (major),
            webkit_version (minor),
            webkit_version (micro));
#undef webkit_version
        printf ("WebKit2: %d\n",
#ifdef USE_WEBKIT2
            1
#else
            0
#endif
            );
#ifdef HAVE_LIBSOUP_CHECK_VERSION
        printf ("libsoup compile: %d.%d.%d\n",
            SOUP_MAJOR_VERSION,
            SOUP_MINOR_VERSION,
            SOUP_MICRO_VERSION);
        printf ("libsoup run: %u.%u.%u\n",
            soup_get_major_version (),
            soup_get_minor_version (),
            soup_get_micro_version ());
#else
        printf ("libsoup compile: < 2.41.1\n");
#endif
        exit (EXIT_SUCCESS);
    }

    /* Only print version. */
    if (print_version) {
        printf ("Commit: %s\n", COMMIT);
        exit (EXIT_SUCCESS);
    }

    /* Embedded mode. */
    if (uzbl.state.xembed_socket_id) {
        uzbl.state.plug_mode = TRUE;
    }

#if !GLIB_CHECK_VERSION (2, 31, 0)
    if (!g_thread_supported ()) {
        g_thread_init (NULL);
    }
#endif

#if USE_WEBKIT2
    WebKitWebContext *webkit_context = webkit_web_context_get_default ();

#if WEBKIT_CHECK_VERSION (2, 3, 5)
    /* Use this in the hopes that one day uzbl itself can be multi-threaded. */
    WebKitProcessModel model =
#if WEBKIT_CHECK_VERSION (2, 3, 90)
        WEBKIT_PROCESS_MODEL_MULTIPLE_SECONDARY_PROCESSES
#else
        WEBKIT_PROCESS_MODEL_ONE_SECONDARY_PROCESS_PER_WEB_VIEW
#endif
        ;
    /* TODO: expose command line option for this. */
    webkit_web_context_set_process_model (webkit_context, model);
#if WEBKIT_CHECK_VERSION (2, 9, 4)
    /* TODO: expose command line option for this. */
    webkit_web_context_set_web_process_count_limit (webkit_context, 0);
#endif
    webkit_web_context_set_web_extensions_directory (webkit_context,
                                                     uzbl.state.web_extensions_directory);
#endif
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
    uzbl_requests_init ();

#ifndef USE_WEBKIT2
    uzbl_scheme_init ();
#endif

    /* Initialize the GUI. */
    uzbl_gui_init ();
    uzbl_inspector_init ();

#if WEBKIT_CHECK_VERSION (2, 9, 4)
    uzbl_variables_setup_data_manager ();
#endif

    /* Uzbl has now been started. */
    uzbl.state.started = TRUE;

    /* XDG */
    ensure_xdg_vars ();

    /* Connect to the event manager(s). */
    gchar **name = connect_socket_names;
    while (name && *name) {
        uzbl_io_init_connect_socket (*name++);
    }
    uzbl_io_flush_buffer ();

    /* Send the startup event. */
    pid_t pid = getpid ();
    gchar *pid_str = g_strdup_printf ("%d", pid);
    g_setenv ("UZBL_PID", pid_str, TRUE);

    if (!uzbl.state.instance_name) {
        uzbl.state.instance_name = g_strdup (pid_str);
    }
    g_free (pid_str);

    uzbl_events_send (INSTANCE_START, NULL,
        TYPE_INT, pid,
        NULL);

    /* Generate an event with a list of built in commands. */
    uzbl_commands_send_builtin_event ();

    /* Set variables based on flags. */
    if (verbose) {
        uzbl_variables_set ("verbose", "1");
    }
    if (print_events) {
        uzbl_variables_set ("print_events", "1");
    }

    /* Load default config. */
    const gchar * const *default_command = default_config;
    while (default_command && *default_command) {
        uzbl_commands_run (*default_command++, NULL);
    }

    /* Load provided configuration file. */
    read_config_file (config_file);

    if (uzbl.gui.main_window) {
        /* We need to ensure there is a window, before we can get XID. */
        gtk_widget_realize (uzbl.gui.main_window);
        Window xwin = GDK_WINDOW_XID (gtk_widget_get_window (uzbl.gui.main_window));

        gchar *xwin_str = g_strdup_printf ("%d", (int)xwin);
        g_setenv ("UZBL_XID", xwin_str, TRUE);
        g_free (xwin_str);
    }

    if (uzbl.state.plug_mode) {
        uzbl_events_send (PLUG_CREATED, NULL,
            TYPE_INT, gtk_plug_get_id (uzbl.gui.plug),
            NULL);
    }

    /* Navigate to a URI if requested. */
    if (uri) {
        GArray *argv = uzbl_commands_args_new ();
        uzbl_commands_args_append (argv, g_strdup (uri));
        uzbl_commands_run_argv ("uri", argv, NULL);
        uzbl_commands_args_free (argv);
    }

    /* Set the geometry if requested. */
    if (uzbl.gui.main_window && geometry) {
        GArray *args = uzbl_commands_args_new ();
        uzbl_commands_args_append (args, g_strdup (geometry));
        uzbl_commands_run_argv ("geometry", args, NULL);
        uzbl_commands_args_free (args);
    }

    /* Finally show the window */
    if (uzbl.gui.main_window) {
        gtk_widget_show_all (uzbl.gui.main_window);
    } else {
        gtk_widget_show_all (GTK_WIDGET (uzbl.gui.plug));
    }

    /* Apply the show_status variable. All widgets are shown with the above
     * call. Unfortunately, GTK has the wonderful thing where all widgets must
     * be explicitly shown and there's no way to exclude widgets from "all", so
     * this is necessary here. */
    gtk_widget_set_visible (uzbl.gui.status_bar, uzbl_variables_get_int ("show_status"));

    /* Update status bar. */
    uzbl_gui_update_title ();
}

void
uzbl_free ()
{
    uzbl_events_send (INSTANCE_EXIT, NULL,
        TYPE_INT, getpid (),
        NULL);

    uzbl_inspector_free ();
    uzbl_gui_free ();
    uzbl_requests_free ();
    uzbl_commands_free ();
    uzbl_variables_free ();
    uzbl_io_free ();

    if (uzbl.gui.menu_items) {
        g_ptr_array_free (uzbl.gui.menu_items, TRUE);
    }
}

#ifndef UZBL_LIBRARY
/* ========================= MAIN  FUNCTION ========================= */

static void
clean_up ();

int
main (int argc, char *argv[])
{
    if (!gtk_init_check (&argc, &argv)) {
        fprintf (stderr, "Failed to initialize GTK\n");
        return EXIT_FAILURE;
    }

    uzbl_init (&argc, &argv);

    if (uzbl.state.exit) {
        goto main_exit;
    }

    if (uzbl.gui.web_view) {
        gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    }

    /* Verbose feedback. */
    if (uzbl_variables_get_int ("verbose")) {
        printf ("Uzbl start location: %s\n", argv[0]);
        if (uzbl.state.xembed_socket_id) {
            printf ("plug_id %d\n", (int)gtk_plug_get_id (uzbl.gui.plug));
        } else {
            Window xwin = GDK_WINDOW_XID (gtk_widget_get_window (GTK_WIDGET (uzbl.gui.main_window)));

            printf ("window_id %d\n", (int)xwin);
        }
        printf ("pid %i\n", getpid ());
        printf ("name: %s\n", uzbl.state.instance_name);
        gchar *commit = uzbl_variables_get_string ("COMMIT");
        printf ("commit: %s\n", commit);
        g_free (commit);
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

static gchar *
find_xdg_file (XdgDir dir, const char* basename);

void
read_config_file (const gchar *file)
{
    gchar *file_free = NULL;

    if (!g_strcmp0 (file, "-")) {
        file = NULL;
        uzbl_io_init_stdin ();
    } else if (!file) {
        file_free = find_xdg_file (XDG_CONFIG, "/uzbl/config");
        file = file_free;
    }

    /* Load config file, if any. */
    if (file) {
        uzbl_commands_load_file (file);
        g_setenv ("UZBL_CONFIG", file, TRUE);
    } else {
        uzbl_debug ("No configuration file loaded.\n");
    }

    g_free (file_free);
}

#ifndef UZBL_LIBRARY
void
clean_up ()
{
    if (uzbl.state.jscontext) {
        JSGlobalContextRelease (uzbl.state.jscontext);
    }

    if (uzbl.net.soup_cookie_jar) {
        g_object_unref (uzbl.net.soup_cookie_jar);
        uzbl.net.soup_cookie_jar = NULL;
    }

    uzbl_free ();
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
