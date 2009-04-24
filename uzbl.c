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

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static GtkWidget*     main_window;
static WebKitWebView* web_view;

static gchar* uri 	= NULL;
static gchar* fifodir 	= NULL;
static gint mechmode = 0;
static char fifopath[64];

static GOptionEntry entries[] =
{
	{ "uri",     'u',  0, G_OPTION_ARG_STRING, &uri,     "Uri to load", NULL },
	{ "fifo-dir", 'd', 0, G_OPTION_ARG_STRING, &fifodir, "Directory to place FIFOs", NULL },
	{ "mechmode", 'm', 0, G_OPTION_ARG_INT, &mechmode, "Enable output suitable for machine processing", NULL },
	{ NULL }
};

static GtkWidget* create_browser ()
{
	GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

	web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
	gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));

	return scrolled_window;
}

static GtkWidget* create_window ()
{
	GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
	gtk_widget_set_name (window, "Uzbl Browser");
	g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);

	return window;
}

static void parse_command(char *command)
{
	if (!strcmp(command, "forward"))
	{
		printf("Going forward\n");
		webkit_web_view_go_forward (web_view);
	}
	else if (!strncmp(command, "back", 4)) // backward
	{
		printf("Going back\n");
		webkit_web_view_go_back (web_view);
	}
	else if (!strcmp(command, "exit") || !strcmp(command, "quit") || !strcmp(command, "die"))
	{
		gtk_main_quit();
	}
	else if (!strncmp("http://", command, 7))
	{
		printf("Loading URI \"%s\"\n", command);
		uri = command;
		webkit_web_view_load_uri (web_view, uri);
	}
	else
	{
		printf("Unhandled command \"%s\"\n", command);
	}
}

static void *control_fifo()
{
	if (fifodir) {
		sprintf(fifopath, "%s/uzbl_%d", fifodir, getpid());
	} else {
		sprintf(fifopath, "/tmp/uzbl_%d", getpid());
	}

	if (mkfifo (fifopath, 0666) == -1) {
		printf("Possible error creating fifo\n");
	}

	if (mechmode) {
		printf("%s\n", fifopath);
	} else {
		printf ("Opened control fifo in %s\n", fifopath);
	}

	while (true)
	{
		FILE *fifo = fopen(fifopath, "r");
		if (!fifo) {
			printf("Could not open %s for reading\n", fifopath);
			return NULL;
		}

		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		while (!feof(fifo) && fgets(buffer, sizeof(buffer), fifo)) {
			if (strcmp(buffer, "\n")) {
				buffer[strlen(buffer)-1] = '\0'; // Remove newline
				parse_command(buffer);
			}
		}
	}

	return NULL;
}

int main (int argc, char* argv[])
{
	if (!g_thread_supported ())
		g_thread_init (NULL);

	gtk_init (&argc, &argv);

	GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), create_browser (), TRUE, TRUE, 0);

	main_window = create_window ();
	gtk_container_add (GTK_CONTAINER (main_window), vbox);
	GError *error = NULL;

	GOptionContext* context = g_option_context_new ("- The Usable Browser, controlled entirely through a FIFO");
	g_option_context_add_main_entries (context, entries, NULL);
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, &error);

	if (uri) {
		webkit_web_view_load_uri (web_view, uri);
	} else {
		webkit_web_view_load_uri (web_view, "http://google.com");
	}

	gtk_widget_grab_focus (GTK_WIDGET (web_view));
	gtk_widget_show_all (main_window);

	pthread_t control_thread;

	pthread_create(&control_thread, NULL, control_fifo, NULL);

	gtk_main();

	/*pthread_join(control_thread, NULL); For some reason it doesn't terminate upon the browser closing when this is enabled. */

	printf("Shutting down...\n");

	// Remove FIFO
	unlink(fifopath);

	return 0;
}
