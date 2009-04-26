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


#define LENGTH(x)               (sizeof x / sizeof x[0])
#define GDK_Escape 0xff1b

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <webkit/webkit.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

/* housekeeping / internal variables */
static GtkWidget* main_window;
static GtkWidget* mainbar;
static GtkWidget* mainbar_label;
static WebKitWebView* web_view;
static gchar* main_title;
static gchar selected_url[500];
static gint load_progress;
static guint status_context_id;
static Window xwin = 0;
static char fifopath[64];

/* state variables (initial values coming from command line arguments but may be changed later) */
static gchar*   uri         = NULL;
static gchar*   config_file = NULL;
static gboolean verbose     = FALSE;

/* settings from config: group behaviour */
static gchar*   history_handler    = NULL;
static gchar*   fifodir            = NULL;
static gchar*   download_handler   = NULL;
static gboolean always_insert_mode = FALSE;
static gboolean show_status        = FALSE;
static gboolean insert_mode        = FALSE;
static gboolean show_status        = FALSE;
static gboolean status_top         = FALSE;
static gchar*   modkey             = NULL;


typedef struct
{
    const char *binding;
    const char *action;
} Binding;

/* settings from config: group bindings_internal */
static Binding internal_bindings[256];
static int     num_internal_bindings = 0;

/* settings from config: group bindings_external */
static Binding external_bindings[256];
static int     num_external_bindings = 0;

/* commandline arguments (set initial values for the state variables) */
static GOptionEntry entries[] =
{
    { "uri",     'u', 0, G_OPTION_ARG_STRING, &uri,         "Uri to load", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE,   &verbose,     "Be verbose",  NULL },
    { "config",  'c', 0, G_OPTION_ARG_STRING, &config_file, "Config file", NULL },
    { NULL }
};

/* for internal list of commands */
typedef struct
{
    const char *command;
    void (*func_1_param)(WebKitWebView*);
    void (*func_2_params)(WebKitWebView*, char *);
} Command;


static void
update_title (GtkWindow* window);

static gboolean
run_command(const char *command, const char *args);


/* --- CALLBACKS --- */
static void
go_back_cb (GtkWidget* widget, gpointer data) {
    webkit_web_view_go_back (web_view);
}

static void
go_forward_cb (GtkWidget* widget, gpointer data) {
    webkit_web_view_go_forward (web_view);
}

static void
cb_toggle_status() {
    if (show_status) {
    	gtk_widget_hide(mainbar);
    } else {
    	gtk_widget_show(mainbar);
    }
    show_status = !show_status;
    update_title (GTK_WINDOW (main_window));
}

static void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data) {
    /* underflow is allowed */
    //gtk_statusbar_pop (main_statusbar, status_context_id);
    //if (link)
    //    gtk_statusbar_push (main_statusbar, status_context_id, link);
    //TODO implementation roadmap pending..
    
    //ADD HOVER URL TO WINDOW TITLE
    selected_url[0] = '\0';
    if (link) {
        strcpy (selected_url, link);
    }
    update_title (GTK_WINDOW (main_window));
}

static void
title_change_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, const gchar* title, gpointer data) {
    if (main_title)
        g_free (main_title);
    main_title = g_strdup (title);
    update_title (GTK_WINDOW (main_window));
}

static void
progress_change_cb (WebKitWebView* page, gint progress, gpointer data) {
    load_progress = progress;
    update_title (GTK_WINDOW (main_window));
}

static void
load_commit_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data) {
    strcpy (uri, webkit_web_frame_get_uri (frame));
}

static void
destroy_cb (GtkWidget* widget, gpointer data) {
    gtk_main_quit ();
}

static void
log_history_cb () {
   time_t rawtime;
   struct tm * timeinfo;
   char date [80];
   time ( &rawtime );
   timeinfo = localtime ( &rawtime );
   strftime (date, 80, "%Y-%m-%d %H:%M:%S", timeinfo);
   GString* args = g_string_new ("");
   g_string_printf (args, "'%s' '%s' '%s'", uri, "TODO:page title here", date);
   run_command(history_handler, args->str);
}

/* -- command to callback/function map for things we cannot attach to any signals */
// TODO: reload, home, quit
static Command commands[] =
{
    { "back",     &go_back_cb,                    NULL },
    { "forward",  &go_forward_cb,                 NULL },
    { "refresh",  &webkit_web_view_reload,        NULL }, //Buggy
    { "stop",     &webkit_web_view_stop_loading,  NULL },
    { "zoom_in",  &webkit_web_view_zoom_in,       NULL }, //Can crash (when max zoom reached?).
    { "zoom_out", &webkit_web_view_zoom_out,      NULL },
    { "uri",      NULL, &webkit_web_view_load_uri      },
    { "toggle_status", &cb_toggle_status, NULL}
//{ "get uri",  &webkit_web_view_get_uri},
};

/* -- CORE FUNCTIONS -- */

static bool
file_exists (const char * filename)
{
    FILE *file = fopen (filename, "r");
    if (file) {
        fclose (file);
        return true;
    }
    return false;
}

// make sure to put '' around args, so that if there is whitespace we can still keep arguments together.
static gboolean
run_command(const char *command, const char *args) {
   //command <uzbl conf> <uzbl pid> <uzbl win id> <uzbl fifo file> [args]
    GString* to_execute = g_string_new ("");
    gboolean result;
    g_string_printf (to_execute, "%s '%s' '%i' '%i' '%s' %s", command, config_file, (int) getpid() , (int) xwin, "/tmp/uzbl_25165827", args);
    result = system(to_execute->str);
    printf("Called %s.  Result: %s\n", to_execute->str, (result ? "FALSE" : "TRUE" ));
    return result;
}

static void
parse_command(const char *command) {
    int i;
    Command *c = NULL;
    char * command_name  = strtok (command, " ");
    char * command_param = strtok (NULL,  " ,"); //dunno how this works, but it seems to work

    Command *c_tmp = NULL;
    for (i = 0; i < LENGTH (commands); i++) {
        c_tmp = &commands[i];
        if (strncmp (command_name, c_tmp->command, strlen (c_tmp->command)) == 0) {
            c = c_tmp;
        }
    }
    if (c != NULL) {
        if (c->func_2_params != NULL) {
            if (command_param != NULL) {
                printf ("command executing: \"%s %s\"\n", command_name, command_param);
                c->func_2_params (web_view, command_param);
            } else {
                if (c->func_1_param != NULL) {
                    printf ("command executing: \"%s\"\n", command_name);
                    c->func_1_param (web_view);
                } else {
                    fprintf (stderr, "command needs a parameter. \"%s\" is not complete\n", command_name);
                }
            }
        } else if (c->func_1_param != NULL) {
            printf ("command executing: \"%s\"\n", command_name);
            c->func_1_param (web_view);
        }
    } else {
        fprintf (stderr, "command \"%s\" not understood. ignoring.\n", command);
    }
}
 
static void
*control_fifo() {
    if (fifodir) {
        sprintf (fifopath, "%s/uzbl_%d", fifodir, (int) xwin);
    } else {
        sprintf (fifopath, "/tmp/uzbl_%d", (int) xwin);
    }
 
    if (mkfifo (fifopath, 0666) == -1) {
        printf ("Possible error creating fifo\n");
    }
 
    printf ("Control fifo opened in %s\n", fifopath);
 
    while (true) {
        FILE *fifo = fopen (fifopath, "r");
        if (!fifo) {
            printf ("Could not open %s for reading\n", fifopath);
            return NULL;
        }
        
        char buffer[256];
        memset (buffer, 0, sizeof (buffer));
        while (!feof (fifo) && fgets (buffer, sizeof (buffer), fifo)) {
            if (strcmp (buffer, "\n")) {
                buffer[strlen (buffer) - 1] = '\0'; // Remove newline
                parse_command (buffer);
            }
        }
    }
    
    return NULL;
} 
 
static void
setup_threading () {
    pthread_t control_thread;
    pthread_create(&control_thread, NULL, control_fifo, NULL);
}

static void
update_title (GtkWindow* window) {
    GString* string_long = g_string_new ("");
    GString* string_short = g_string_new ("");
    if (!always_insert_mode)
        g_string_append (string_long, (insert_mode ? "[I] " : "[C] "));
    g_string_append (string_long, main_title);
    g_string_append (string_short, main_title);
    g_string_append (string_long, " - Uzbl browser");
    g_string_append (string_short, " - Uzbl browser");
    if (load_progress < 100)
        g_string_append_printf (string_long, " (%d%%)", load_progress);

    if (selected_url[0]!=0) {
        g_string_append_printf (string_long, " -> (%s)", selected_url);
    }

    gchar* title_long = g_string_free (string_long, FALSE);
    gchar* title_short = g_string_free (string_short, FALSE);

    if (show_status) {
        gtk_window_set_title (window, title_short);
	gtk_label_set_text(mainbar_label, title_long);
    } else {
        gtk_window_set_title (window, title_long);
    }

    g_free (title_long);
    g_free (title_short);
}
 
static gboolean
key_press_cb (WebKitWebView* page, GdkEventKey* event)
{
    int i;
    gboolean result=FALSE; //TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
    if (event->type != GDK_KEY_PRESS) 
        return result;

    //TURN OFF INSERT MODE
    if (insert_mode && (event->keyval == GDK_Escape)) {
        insert_mode = FALSE;
        update_title (GTK_WINDOW (main_window));
        return TRUE;
    }

    //TURN ON INSERT MODE
    if (!insert_mode && (event->string[0] == 'i')) {
        insert_mode = TRUE;
        update_title (GTK_WINDOW (main_window));
        return TRUE;
    }

    //INTERNAL KEYS
    if (always_insert_mode || !insert_mode) {
        for (i = 0; i < num_internal_bindings; i++) {
            if (event->string[0] == internal_bindings[i].binding[0]) {
                parse_command (internal_bindings[i].action);
                result = TRUE;
            }	
        }
    }
    if (!result)
        result = (insert_mode ? FALSE : TRUE);      

    return result;
}

static GtkWidget*
create_browser () {
    GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_NEVER); //todo: some sort of display of position/total length. like what emacs does

    web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
    gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));

    g_signal_connect (G_OBJECT (web_view), "title-changed", G_CALLBACK (title_change_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "load-progress-changed", G_CALLBACK (progress_change_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "load-committed", G_CALLBACK (load_commit_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "load-committed", G_CALLBACK (log_history_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "hovering-over-link", G_CALLBACK (link_hover_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "key-press-event", G_CALLBACK (key_press_cb), web_view);

    return scrolled_window;
}

static GtkWidget*
create_mainbar () {
    mainbar = gtk_hbox_new (FALSE, 0);
    mainbar_label = gtk_label_new ("");  
    gtk_misc_set_alignment (mainbar_label, 0, 0);
    gtk_misc_set_padding (mainbar_label, 2, 2);
    gtk_box_pack_start (GTK_BOX (mainbar), mainbar_label, TRUE, TRUE, 0);
    return mainbar;
}

static
GtkWidget* create_window () {
    GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
    gtk_widget_set_name (window, "Uzbl browser");
    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_cb), NULL);

    return window;
}

static void
add_binding (char *binding, char *action, bool internal) {
    Binding bind = {binding, action};
    if (internal) {
        internal_bindings[num_internal_bindings] = bind;
        num_internal_bindings ++;
    } else {
        external_bindings[num_external_bindings] = bind;
        num_external_bindings ++;
    }
}

static void
settings_init () {
    GKeyFile* config;
    gboolean res = NULL;
    gchar** keysi = NULL;
    gchar** keyse = NULL;

    if (! config_file) {
        const char* xdg = getenv ("XDG_CONFIG_HOME");
        char* conf[256];
        if (xdg) {
            printf("XDG_CONFIG_DIR: %s\n", xdg);
            strcpy (conf, xdg);
            strcat (conf, "/uzbl");
            if (file_exists (conf)) {
                printf ("Config file %s found.\n", conf);
                config_file = &conf;
            }
        }
    }

    if (config_file) {
        config = g_key_file_new ();
        res = g_key_file_load_from_file (config, config_file, G_KEY_FILE_NONE, NULL);
    	  if(res) {
            printf ("Config %s loaded\n", config_file);
    	  } else {
            fprintf (stderr, "Config %s loading failed\n", config_file);
        }
    } else {
        printf ("No configuration.\n");
    }

    if (res) {
        history_handler    = g_key_file_get_value   (config, "behavior", "history_handler", NULL);
        download_handler   = g_key_file_get_value   (config, "behavior", "download_handler", NULL);
        always_insert_mode = g_key_file_get_boolean (config, "behavior", "always_insert_mode", NULL);
        show_status        = g_key_file_get_boolean (config, "behavior", "show_status", NULL);
        modkey             = g_key_file_get_value   (config, "behavior", "modkey", NULL);
        keysi              = g_key_file_get_keys    (config, "bindings_internal", NULL, NULL);
        keyse              = g_key_file_get_keys    (config, "bindings_external", NULL, NULL);
        if (! fifodir)
            fifodir        = g_key_file_get_value   (config, "behavior", "fifodir", NULL);
    }
	
    if (history_handler) {
        printf ("History handler: %s\n", history_handler);
    } else {
        printf ("History handler disabled\n");
    }

    if (download_handler) {
        printf ("Download manager: %s\n", download_handler);
    } else {
        printf ("Download manager disabled\n");
    }

    if (fifodir) {
        printf ("Fifo directory: %s\n", fifodir);
    } else {
        printf ("Fifo directory: /tmp\n");
    }

    printf ("Always insert mode: %s\n", (always_insert_mode ? "TRUE" : "FALSE"));

    printf ("Show status: %s\n", (show_status ? "TRUE" : "FALSE"));

    status_top = g_key_file_get_boolean (config, "behavior", "status_top", NULL);
    printf ("Status top: %s\n", (status_top ? "TRUE" : "FALSE"));

    if (modkey) {
        printf ("Modkey: %s\n", modkey);
    } else {
        printf ("Modkey disabled\n");
    }

    if (keysi) {
	      int i = 0;
        for (i = 0; keysi[i]; i++) {
            gchar *binding = g_key_file_get_string (config, "bindings_internal", keysi[i], NULL);
            printf ("Action: %s, Binding: %s (internal)\n", g_strdup (keysi[i]), binding);
            add_binding (binding, g_strdup (keysi[i]), true);
        }
    }
    if (keyse) {
	      int i = 0;
        for (i = 0; keyse[i]; i++) {
            gchar *binding = g_key_file_get_string (config, "bindings_external", keyse[i], NULL);
            printf ("Action: %s, Binding: %s (external)\n", g_strdup (keyse[i]), binding);
            add_binding (binding, g_strdup (keyse[i]), false);
        }
    }
}

int
main (int argc, char* argv[]) {
    gtk_init (&argc, &argv);
    if (!g_thread_supported ())
        g_thread_init (NULL);

    GError *error = NULL;
    GOptionContext* context = g_option_context_new ("- some stuff here maybe someday");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_parse (context, &argc, &argv, &error);

    settings_init ();
    if (always_insert_mode)
        insert_mode = TRUE;

    GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
    if (status_top)
        gtk_box_pack_start (GTK_BOX (vbox), create_mainbar (), FALSE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_browser (), TRUE, TRUE, 0);
    if (!status_top)
        gtk_box_pack_start (GTK_BOX (vbox), create_mainbar (), FALSE, TRUE, 0);

    main_window = create_window ();
    gtk_container_add (GTK_CONTAINER (main_window), vbox);

    webkit_web_view_load_uri (web_view, uri);

    gtk_widget_grab_focus (GTK_WIDGET (web_view));
    gtk_widget_show_all (main_window);
    xwin = GDK_WINDOW_XID (GTK_WIDGET (main_window)->window);
    printf("window_id %i\n",(int) xwin);
    printf("pid %i\n", getpid ());

    if (!show_status)
    	gtk_widget_hide(mainbar);

    setup_threading ();

    gtk_main ();

    unlink (fifopath);
    return 0;
}
