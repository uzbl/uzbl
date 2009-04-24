// Original code taken from the example webkit-gtk+ application. see notice below.

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

#include <gtk/gtk.h>
#include <webkit/webkit.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

static GtkWidget*     main_window;
static GtkWidget*     uri_entry;
static GtkStatusbar*  main_statusbar;
static WebKitWebView* web_view;
static gchar*         main_title;
static gint           load_progress;
static guint          status_context_id;

static gchar* uri          = NULL;
static gboolean verbose    = FALSE;

static GOptionEntry entries[] =
{
    { "uri",     'u',  0, G_OPTION_ARG_STRING, &uri,     "Uri to load", NULL },
    { "verbose", 'v',  0, G_OPTION_ARG_NONE,   &verbose, "Be verbose", NULL },
    { NULL }
};


static void activate_uri_entry_cb (GtkWidget* entry, gpointer data)
{
    const gchar* uri = gtk_entry_get_text (GTK_ENTRY (entry));
    g_assert (uri);
    webkit_web_view_load_uri (web_view, uri);
}

static void update_title (GtkWindow* window)
{
    GString* string = g_string_new (main_title);
    g_string_append (string, " - Uzbl browser");
    if (load_progress < 100)
        g_string_append_printf (string, " (%d%%)", load_progress);
    gchar* title = g_string_free (string, FALSE);
    gtk_window_set_title (window, title);
    g_free (title);
}

static void link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data)
{
    /* underflow is allowed */
    gtk_statusbar_pop (main_statusbar, status_context_id);
    if (link)
        gtk_statusbar_push (main_statusbar, status_context_id, link);
}

static void title_change_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, const gchar* title, gpointer data)
{
    if (main_title)
        g_free (main_title);
    main_title = g_strdup (title);
    update_title (GTK_WINDOW (main_window));
}

static void progress_change_cb (WebKitWebView* page, gint progress, gpointer data)
{
    load_progress = progress;
    update_title (GTK_WINDOW (main_window));
}

static void load_commit_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data)
{
    const gchar* uri = webkit_web_frame_get_uri(frame);
    if (uri)
        gtk_entry_set_text (GTK_ENTRY (uri_entry), uri);
}

static void destroy_cb (GtkWidget* widget, gpointer data)
{
    gtk_main_quit ();
}

static void go_back_cb (GtkWidget* widget, gpointer data)
{
    webkit_web_view_go_back (web_view);
}

static void go_forward_cb (GtkWidget* widget, gpointer data)
{
    webkit_web_view_go_forward (web_view);
}

static GtkWidget* create_browser ()
{
    GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

    web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
    gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));

    g_signal_connect (G_OBJECT (web_view), "title-changed", G_CALLBACK (title_change_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "load-progress-changed", G_CALLBACK (progress_change_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "load-committed", G_CALLBACK (load_commit_cb), web_view);
    g_signal_connect (G_OBJECT (web_view), "hovering-over-link", G_CALLBACK (link_hover_cb), web_view);

    return scrolled_window;
}

static GtkWidget* create_statusbar ()
{
    main_statusbar = GTK_STATUSBAR (gtk_statusbar_new ());
    status_context_id = gtk_statusbar_get_context_id (main_statusbar, "Link Hover");

    return (GtkWidget*)main_statusbar;
}

static GtkWidget* create_window ()
{
    GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
    gtk_widget_set_name (window, "Uzbl browser");
    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_cb), NULL);

    return window;
}


static bool parse_command(char *command)
{
    bool output = true;

    if(strcmp(command, "forward") == 0)
    {
        printf("Going forward\n");
        webkit_web_view_go_forward (web_view);
    }
    else if(strcmp(command, "back") == 0)
    {
        printf("Going back\n");
        webkit_web_view_go_back (web_view);
    }
    else if(strncmp("http://", command, 7) == 0)
    {
        printf("Loading URI \"%s\"\n", command);
        uri = command;
        webkit_web_view_load_uri (web_view, uri);
    }
    else
    {
        output = false;
    }

    return output;
}

static void control_fifo()
{
    char *cmd = (char *)malloc(1024);
    int num, fd;
    char *fifoname = "/tmp/uzbl";

    umask (0);
    mknod (fifoname, S_IFIFO | 0666 , 0); /* Do some stuff to work with multiple instances later foo-$PID or something */
    printf ("Opened control fifo in %s\n", fifoname);

    while (true)
    {
        fd = open (fifoname, O_RDONLY);
        printf ("Looping...\n");
        while (num > 0)
        {
            if ((num = read(fd, cmd, 300)) == -1)
                perror("read");
            else
            {
                cmd[num - 1] = '\0';
                if(! parse_command(cmd))
                    printf("Unknown command \"%s\".\n", cmd);
                cmd[0] = '\0';
            }
        }
        num = 1;
    }
    printf("Oops, this code should never be run.\n");
}

int main (int argc, char* argv[])
{
    if (!g_thread_supported ())
        g_thread_init (NULL);

    gtk_init (&argc, &argv);

    GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
    gtk_box_pack_start (GTK_BOX (vbox), create_browser (), TRUE, TRUE, 0);

    if(! hidestatus)
        gtk_box_pack_start (GTK_BOX (vbox), create_statusbar (), FALSE, FALSE, 0);

    main_window = create_window ();
    gtk_container_add (GTK_CONTAINER (main_window), vbox);
    GError *error = NULL;
    
    GOptionContext* context = g_option_context_new ("- some stuff here maybe someday");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_parse (context, &argc, &argv, &error);

    webkit_web_view_load_uri (web_view, uri);

    gtk_widget_grab_focus (GTK_WIDGET (web_view));
    gtk_widget_show_all (main_window);

    pthread_t control_thread;
    int ret;

    ret = pthread_create(&control_thread, NULL, control_fifo, (void*) NULL);
    gtk_main ();
    /*pthread_join(control_thread, NULL); For some reason it doesn't terminate upon the browser closing when this is enabled. */
    return 0;
}
