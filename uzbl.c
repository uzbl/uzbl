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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdkkeysyms.h>
#include <webkit/webkit.h>

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

static GtkWidget*     main_window;
static GtkWidget*     modeline;
static WebKitWebView* web_view;

static gchar* history_file;
static gchar* home_page;
static gchar* uri       = NULL;
static gchar* fifodir   = NULL;
static gint   mechmode  = 0;
static char   fifopath[64];
static bool   modevis = FALSE;

static GOptionEntry entries[] =
{
  { "uri",      'u', 0, G_OPTION_ARG_STRING, &uri,      "Uri to load",                                   NULL },
  { "fifo-dir", 'd', 0, G_OPTION_ARG_STRING, &fifodir,  "Directory to place FIFOs",                      NULL },
  { "mechmode", 'm', 0, G_OPTION_ARG_INT,    &mechmode, "Enable output suitable for machine processing", NULL },
  { NULL }
};

struct command
{
  char command[256];
  void (*func)(WebKitWebView*);
};

static struct command commands[256];
static int            numcmds = 0;

static void parse_command(char*);

static bool parse_modeline (GtkWidget* mode, GdkEventKey* event)
{
  if ((event->type==GDK_KEY_PRESS) && (event->keyval==GDK_Return))
    parse_command (gtk_entry_get_text (modeline));

  return false;
}

static void log_history_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data)
{
  strncpy (uri, webkit_web_frame_get_uri (frame), strlen (webkit_web_frame_get_uri (frame)));
  
  FILE * output_file = fopen (history_file, "a");
  if (output_file == NULL)
    {
      fprintf (stderr, "Cannot open %s for logging\n", history_file);
    }
  else
    {
      time_t rawtime;
      struct tm * timeinfo;
      char buffer [80];
      time (&rawtime);
      timeinfo = localtime (&rawtime);
      strftime (buffer,80,"%Y-%m-%d %H:%M:%S",timeinfo);
      
      fprintf (output_file, "%s %s\n",buffer, uri);
      fclose (output_file);
    }
}

static void toggle_command_mode ()
{
  if (modevis)
    {
      gtk_widget_hide (modeline);
      gtk_widget_grab_default (modeline);
    }
  else
    {
      gtk_widget_show (modeline);
      gtk_widget_grab_focus (modeline);
    }
  modevis = ! modevis;
}

static gboolean key_press_cb (WebKitWebView* page, GdkEventKey* event)
{
  gboolean result=FALSE; //TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.
  if ((event->type==GDK_KEY_PRESS) && (event->keyval==GDK_Escape))
    {
      toggle_command_mode ();
      result=TRUE;
    }
 
  return(result);
}

static GtkWidget* create_browser ()
{
  GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_NEVER);

  web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (web_view));

  g_signal_connect (G_OBJECT (web_view), "load-committed", G_CALLBACK (log_history_cb), web_view);

  return scrolled_window;
}

static GtkWidget* create_window ()
{
  GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  gtk_widget_set_name (window, "Uzbl Browser");
  g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (G_OBJECT (window), "key-press-event", G_CALLBACK(key_press_cb), NULL);

  return window;
}

static GtkWidget* create_modeline ()
{
  GtkWidget* modeline = gtk_entry_new ();
  g_signal_connect (G_OBJECT (modeline), "key-press-event", G_CALLBACK(parse_modeline), modeline);

  return modeline;
}

static void parse_command(char *command)
{
  int  i    = 0;
  bool done = false;
  char* strtimes = NULL;

  void (*func)(WebKitWebView*);
  int times = 1;

  for (i = 0; i < numcmds && ! done; i++)
    {
      if (!strncmp (command, commands[i].command, strlen (commands[i].command)))
        {
          func = commands[i].func;
          done = true;

          if (strlen (command) > strlen (commands[i].command))
            {
              strtimes = (char *) command + strlen (commands[i].command);
              printf("%s\n", strtimes);
              times = atoi (strtimes);
            }
        }
    }

  if(done)
    {
      int j;
      for (j = 0; j < times; j++)
        {
          func (web_view);
        }
    }
  else
    {
      if (!strncmp ("http://", command, 7))
        {
          printf ("Loading URI \"%s\"\n", command);
          uri = command;
          webkit_web_view_load_uri (web_view, uri);
        }
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

static void add_command (char* cmdstr, void* function)
{
  strncpy(commands[numcmds].command, cmdstr, strlen(cmdstr));
  commands[numcmds].func = function;
  numcmds++;
}

static bool setup_gtk (int argc, char* argv[])
{
  gtk_init (&argc, &argv);

  GtkWidget* vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), create_browser (), TRUE, TRUE, 0);
  modeline = create_modeline ();
  gtk_box_pack_start (GTK_BOX (vbox), modeline, FALSE, FALSE, 0);

  main_window = create_window ();
  gtk_container_add (GTK_CONTAINER (main_window), vbox);
  GError *error = NULL;

  GOptionContext* context = g_option_context_new ("- The Usable Browser, controlled entirely through a FIFO");
  g_option_context_add_main_entries (context, entries, NULL);
  g_option_context_add_group (context, gtk_get_option_group (TRUE));
  g_option_context_parse (context, &argc, &argv, &error);

  if (uri)
    {
      webkit_web_view_load_uri (web_view, uri);
    }

  gtk_widget_grab_focus (GTK_WIDGET (web_view));
  gtk_widget_show_all (main_window);
  gtk_widget_hide(modeline);

  return true;
}

static void setup_commands ()
{
  //This func. is nice but currently it cannot be used for functions that require arguments or return data. --sentientswitch

  add_command("b",  &webkit_web_view_go_back);
  add_command("f",  &webkit_web_view_go_forward);
  add_command("r",  &webkit_web_view_reload); //Buggy
  add_command("s",  &webkit_web_view_stop_loading);
  add_command("z+", &webkit_web_view_zoom_in); //Can crash (when max zoom reached?).
  add_command("z-", &webkit_web_view_zoom_out); //Crashes as zoom +
  //add_command("get uri", &webkit_web_view_get_uri);
}

static void setup_threading ()
{
  pthread_t control_thread;
  pthread_create(&control_thread, NULL, control_fifo, NULL);
}

static void setup_settings ()
{
  GKeyFile* config = g_key_file_new ();
  gboolean  res    = g_key_file_load_from_file (config, "./config", G_KEY_FILE_NONE, NULL); //TODO: pass config file as argument

  if (res)
    {
      printf ("Config loaded\n");
    }
  else
    {
      fprintf (stderr, "config loading failed\n"); //TODO: exit codes with gtk? 
    }

  history_file = g_key_file_get_value (config, "behavior", "history_file", NULL);
  if (history_file)
    {
      printf ("Setting history file to: %s\n", history_file);
    }
  else
    {
      printf ("History logging disabled\n");
    }

  home_page = g_key_file_get_value (config, "behavior", "home_page", NULL);
  if (home_page)
    {
      printf ("Setting home page to: %s\n", home_page);
    }
  else
    {
      printf ("Home page disabled\n");
    }
}

int main (int argc, char* argv[])
{
  if (!g_thread_supported ())
    g_thread_init (NULL);

  setup_settings ();
  setup_gtk (argc, argv);
  setup_commands ();
  setup_threading ();
  gtk_main ();

  printf ("Shutting down...\n");

  unlink (fifopath);

  return 0;
}
