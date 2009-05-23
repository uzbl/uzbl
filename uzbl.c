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


#define LENGTH(x) (sizeof x / sizeof x[0])
#define MAX_BINDINGS 256

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <webkit/webkit.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <libsoup/soup.h>
#include <signal.h>
#include "uzbl.h"
#include "config.h"

static Uzbl uzbl;
typedef void (*Command)(WebKitWebView*, GArray *argv);

/* commandline arguments (set initial values for the state variables) */
static GOptionEntry entries[] =
{
    { "uri",     'u', 0, G_OPTION_ARG_STRING, &uzbl.state.uri,           "Uri to load at startup (equivalent to 'set uri = URI')", "URI" },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE,   &uzbl.state.verbose,       "Whether to print all messages or just errors.", NULL },
    { "name",    'n', 0, G_OPTION_ARG_STRING, &uzbl.state.instance_name, "Name of the current instance (defaults to Xorg window id)", "NAME" },
    { "config",  'c', 0, G_OPTION_ARG_STRING, &uzbl.state.config_file,   "Config file (this is pretty much equivalent to uzbl < FILE )", "FILE" },
    { NULL,      0, 0, 0, NULL, NULL, NULL }
};


/* associate command names to their properties */
typedef const struct {
    void **ptr;
    int type;
    void (*func)(void);
} uzbl_cmdprop;

enum {TYPE_INT, TYPE_STR};

/* an abbreviation to help keep the table's width humane */
#define PTR(var, t, fun) { .ptr = (void*)&(var), .type = TYPE_##t, .func = fun }

const struct {
    char *name;
    uzbl_cmdprop cp;
} var_name_to_ptr[] = {
/*    variable name         pointer to variable in code          type  callback function    */
/*  --------------------------------------------------------------------------------------- */
    { "uri",                 PTR(uzbl.state.uri,                  STR, cmd_load_uri)},
    { "status_message",      PTR(uzbl.gui.sbar.msg,               STR, update_title)},
    { "show_status",         PTR(uzbl.behave.show_status,         INT, cmd_set_status)},
    { "status_top",          PTR(uzbl.behave.status_top,          INT, move_statusbar)},
    { "status_format",       PTR(uzbl.behave.status_format,       STR, update_title)},
    { "status_pbar_done",    PTR(uzbl.gui.sbar.progress_s,        STR, update_title)},
    { "status_pbar_pending", PTR(uzbl.gui.sbar.progress_u,        STR, update_title)},
    { "status_pbar_width",   PTR(uzbl.gui.sbar.progress_w,        INT, update_title)},
    { "status_background",   PTR(uzbl.behave.status_background,   STR, update_title)},
    { "title_format_long",   PTR(uzbl.behave.title_format_long,   STR, update_title)},
    { "title_format_short",  PTR(uzbl.behave.title_format_short,  STR, update_title)},
    { "insert_mode",         PTR(uzbl.behave.insert_mode,         INT, NULL)},
    { "always_insert_mode",  PTR(uzbl.behave.always_insert_mode,  INT, cmd_always_insert_mode)},
    { "reset_command_mode",  PTR(uzbl.behave.reset_command_mode,  INT, NULL)},
    { "modkey",              PTR(uzbl.behave.modkey,              STR, cmd_modkey)},
    { "load_finish_handler", PTR(uzbl.behave.load_finish_handler, STR, NULL)},
    { "load_start_handler",  PTR(uzbl.behave.load_start_handler,  STR, NULL)},
    { "load_commit_handler", PTR(uzbl.behave.load_commit_handler, STR, NULL)},
    { "history_handler",     PTR(uzbl.behave.history_handler,     STR, NULL)},
    { "download_handler",    PTR(uzbl.behave.download_handler,    STR, NULL)},
    { "cookie_handler",      PTR(uzbl.behave.cookie_handler,      STR, cmd_cookie_handler)},
    { "fifo_dir",            PTR(uzbl.behave.fifo_dir,            STR, cmd_fifo_dir)},
    { "socket_dir",          PTR(uzbl.behave.socket_dir,          STR, cmd_socket_dir)},
    { "http_debug",          PTR(uzbl.behave.http_debug,          INT, cmd_http_debug)},
    { "font_size",           PTR(uzbl.behave.font_size,           INT, cmd_font_size)},
    { "monospace_size",      PTR(uzbl.behave.monospace_size,      INT, cmd_font_size)},
    { "minimum_font_size",   PTR(uzbl.behave.minimum_font_size,   INT, cmd_minimum_font_size)},
    { "disable_plugins",     PTR(uzbl.behave.disable_plugins,     INT, cmd_disable_plugins)},
    { "shell_cmd",           PTR(uzbl.behave.shell_cmd,           STR, NULL)},
    { "proxy_url",           PTR(uzbl.net.proxy_url,              STR, set_proxy_url)},
    { "max_conns",           PTR(uzbl.net.max_conns,              INT, cmd_max_conns)},
    { "max_conns_host",      PTR(uzbl.net.max_conns_host,         INT, cmd_max_conns_host)},
    { "useragent",           PTR(uzbl.net.useragent,              STR, cmd_useragent)},
    { NULL,                  {.ptr = NULL, .type = TYPE_INT, .func = NULL}}
}, *n2v_p = var_name_to_ptr;

const struct {
    char *key;
    guint mask;
} modkeys[] = {
    { "SHIFT",   GDK_SHIFT_MASK   }, // shift
    { "LOCK",    GDK_LOCK_MASK    }, // capslock or shiftlock, depending on xserver's modmappings
    { "CONTROL", GDK_CONTROL_MASK }, // control
    { "MOD1",    GDK_MOD1_MASK    }, // 4th mod - normally alt but depends on modmappings
    { "MOD2",    GDK_MOD2_MASK    }, // 5th mod
    { "MOD3",    GDK_MOD3_MASK    }, // 6th mod
    { "MOD4",    GDK_MOD4_MASK    }, // 7th mod
    { "MOD5",    GDK_MOD5_MASK    }, // 8th mod
    { "BUTTON1", GDK_BUTTON1_MASK }, // 1st mouse button
    { "BUTTON2", GDK_BUTTON2_MASK }, // 2nd mouse button
    { "BUTTON3", GDK_BUTTON3_MASK }, // 3rd mouse button
    { "BUTTON4", GDK_BUTTON4_MASK }, // 4th mouse button
    { "BUTTON5", GDK_BUTTON5_MASK }, // 5th mouse button
    { "SUPER",   GDK_SUPER_MASK   }, // super (since 2.10)
    { "HYPER",   GDK_HYPER_MASK   }, // hyper (since 2.10)
    { "META",    GDK_META_MASK    }, // meta (since 2.10)
    { NULL,      0                }
};


/* construct a hash from the var_name_to_ptr array for quick access */
static void
make_var_to_name_hash() {
    uzbl.comm.proto_var = g_hash_table_new(g_str_hash, g_str_equal);
    while(n2v_p->name) {
        g_hash_table_insert(uzbl.comm.proto_var, n2v_p->name, (gpointer) &n2v_p->cp);
        n2v_p++;
    }
}


/* --- UTILITY FUNCTIONS --- */

char *
itos(int val) {
    char tmp[20];

    snprintf(tmp, sizeof(tmp), "%i", val);
    return g_strdup(tmp);
}

static gchar*
strfree(gchar *str) { g_free(str); return NULL; }  // for freeing & setting to null in one go

static gchar*
argv_idx(const GArray *a, const guint idx) { return g_array_index(a, gchar*, idx); }

static char *
str_replace (const char* search, const char* replace, const char* string) {
    gchar **buf;
    char *ret;

    buf = g_strsplit (string, search, -1);
    ret = g_strjoinv (replace, buf);
    g_strfreev(buf); // somebody said this segfaults

    return ret;
}

static GArray*
read_file_by_line (gchar *path) {
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
        fprintf(stderr, "File '%s' not be read.\n", path);
    }
    
    return lines;
}

static
gchar* parseenv (char* string) {
    extern char** environ;
    gchar* tmpstr = NULL;
    int i = 0;
    

    while (environ[i] != NULL) {
        gchar** env = g_strsplit (environ[i], "=", 2);
        gchar* envname = g_strconcat ("$", env[0], NULL);

        if (g_strrstr (string, envname) != NULL) {
            tmpstr = g_strdup(string);
            g_free (string);
            string = str_replace(envname, env[1], tmpstr);
            g_free (tmpstr);
        }

        g_free (envname);
        g_strfreev (env); // somebody said this breaks uzbl
        i++;
    }

    return string;
}

static sigfunc*
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
clean_up(void) {
    if (uzbl.behave.fifo_dir)
        unlink (uzbl.comm.fifo_path);
    if (uzbl.behave.socket_dir)
        unlink (uzbl.comm.socket_path);

    g_free(uzbl.state.executable_path);
    g_string_free(uzbl.state.keycmd, TRUE);
    g_hash_table_destroy(uzbl.bindings);
    g_hash_table_destroy(uzbl.behave.commands);
}


/* --- SIGNAL HANDLER --- */

static void
catch_sigterm(int s) {
    (void) s;
    clean_up();
}

static void
catch_sigint(int s) {
    (void) s;
    clean_up();
    exit(EXIT_SUCCESS);
}

/* --- CALLBACKS --- */

static gboolean
new_window_cb (WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action, WebKitWebPolicyDecision *policy_decision, gpointer user_data) {
    (void) web_view;
    (void) frame;
    (void) navigation_action;
    (void) policy_decision;
    (void) user_data;
    const gchar* uri = webkit_network_request_get_uri (request);
    if (uzbl.state.verbose)
        printf("New window requested -> %s \n", uri);
    new_window_load_uri(uri);
    return (FALSE);
}

WebKitWebView*
create_web_view_cb (WebKitWebView  *web_view, WebKitWebFrame *frame, gpointer user_data) {
    (void) web_view;
    (void) frame;
    (void) user_data;
    if (uzbl.state.selected_url != NULL) {
        if (uzbl.state.verbose)
            printf("\nNew web view -> %s\n",uzbl.state.selected_url);
        new_window_load_uri(uzbl.state.selected_url);
    } else {
        if (uzbl.state.verbose)
            printf("New web view -> %s\n","Nothing to open, exiting");
    }
    return (NULL);
}

static gboolean
download_cb (WebKitWebView *web_view, GObject *download, gpointer user_data) {
    (void) web_view;
    (void) user_data;
    if (uzbl.behave.download_handler) {
        const gchar* uri = webkit_download_get_uri ((WebKitDownload*)download);
        if (uzbl.state.verbose)
            printf("Download -> %s\n",uri);
        /* if urls not escaped, we may have to escape and quote uri before this call */
        run_handler(uzbl.behave.download_handler, uri);
    }
    return (FALSE);
}

/* scroll a bar in a given direction */
static void
scroll (GtkAdjustment* bar, GArray *argv) {
    gdouble amount;
    gchar *end;

    amount = g_ascii_strtod(g_array_index(argv, gchar*, 0), &end);
    if (*end == '%') amount = gtk_adjustment_get_page_size(bar) * amount * 0.01;
    gtk_adjustment_set_value (bar, gtk_adjustment_get_value(bar)+amount);
}

static void scroll_begin(WebKitWebView* page, GArray *argv) {
    (void) page; (void) argv;
    gtk_adjustment_set_value (uzbl.gui.bar_v, gtk_adjustment_get_lower(uzbl.gui.bar_v));
}

static void scroll_end(WebKitWebView* page, GArray *argv) {
    (void) page; (void) argv;
    gtk_adjustment_set_value (uzbl.gui.bar_v, gtk_adjustment_get_upper(uzbl.gui.bar_v) -
                              gtk_adjustment_get_page_size(uzbl.gui.bar_v));
}

static void scroll_vert(WebKitWebView* page, GArray *argv) {
    (void) page;
    scroll(uzbl.gui.bar_v, argv);
}

static void scroll_horz(WebKitWebView* page, GArray *argv) {
    (void) page;
    scroll(uzbl.gui.bar_h, argv);
}

static void
cmd_set_status() {
    if (!uzbl.behave.show_status) {
        gtk_widget_hide(uzbl.gui.mainbar);
    } else {
        gtk_widget_show(uzbl.gui.mainbar);
    }
    update_title();
}

static void
toggle_status_cb (WebKitWebView* page, GArray *argv) {
    (void)page;
    (void)argv;

    if (uzbl.behave.show_status) {
        gtk_widget_hide(uzbl.gui.mainbar);
    } else {
        gtk_widget_show(uzbl.gui.mainbar);
    }
    uzbl.behave.show_status = !uzbl.behave.show_status;
    update_title();
}

static void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data) {
    (void) page;
    (void) title;
    (void) data;
    //Set selected_url state variable
    g_free(uzbl.state.selected_url);
    uzbl.state.selected_url = NULL;
    if (link) {
        uzbl.state.selected_url = g_strdup(link);
    }
    update_title();
}

static void
title_change_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, const gchar* title, gpointer data) {
    (void) web_view;
    (void) web_frame;
    (void) data;
    if (uzbl.gui.main_title)
        g_free (uzbl.gui.main_title);
    uzbl.gui.main_title = g_strdup (title);
    update_title();
}

static void
progress_change_cb (WebKitWebView* page, gint progress, gpointer data) {
    (void) page;
    (void) data;
    uzbl.gui.sbar.load_progress = progress;
    update_title();
}

static void
load_finish_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data) {
    (void) page;
    (void) frame;
    (void) data;
    if (uzbl.behave.load_finish_handler)
        run_handler(uzbl.behave.load_finish_handler, "");
}

static void
load_start_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data) {
    (void) page;
    (void) frame;
    (void) data;
    if (uzbl.behave.load_start_handler)
        run_handler(uzbl.behave.load_start_handler, "");
}

static void
load_commit_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data) {
    (void) page;
    (void) data;
    g_free (uzbl.state.uri);
    GString* newuri = g_string_new (webkit_web_frame_get_uri (frame));
    uzbl.state.uri = g_string_free (newuri, FALSE);
    if (uzbl.behave.reset_command_mode && uzbl.behave.insert_mode) {
        uzbl.behave.insert_mode = uzbl.behave.always_insert_mode;
        update_title();
    }
    g_string_truncate(uzbl.state.keycmd, 0); // don't need old commands to remain on new page?
    if (uzbl.behave.load_commit_handler)
        run_handler(uzbl.behave.load_commit_handler, uzbl.state.uri);
}

static void
destroy_cb (GtkWidget* widget, gpointer data) {
    (void) widget;
    (void) data;
    gtk_main_quit ();
}

static void
log_history_cb () {
   if (uzbl.behave.history_handler) {
       time_t rawtime;
       struct tm * timeinfo;
       char date [80];
       time ( &rawtime );
       timeinfo = localtime ( &rawtime );
       strftime (date, 80, "\"%Y-%m-%d %H:%M:%S\"", timeinfo);
       run_handler(uzbl.behave.history_handler, date);
   }
}


/* VIEW funcs (little webkit wrappers) */
#define VIEWFUNC(name) static void view_##name(WebKitWebView *page, GArray *argv){(void)argv; webkit_web_view_##name(page);}
VIEWFUNC(reload)
VIEWFUNC(reload_bypass_cache)
VIEWFUNC(stop_loading)
VIEWFUNC(zoom_in)
VIEWFUNC(zoom_out)
VIEWFUNC(go_back)
VIEWFUNC(go_forward)
#undef VIEWFUNC

/* -- command to callback/function map for things we cannot attach to any signals */
// TODO: reload
static struct {char *name; Command command[2];} cmdlist[] =
{   /* key                   function      no_split      */
    { "back",               {view_go_back, 0}              },
    { "forward",            {view_go_forward, 0}           },
    { "scroll_vert",        {scroll_vert, 0}               },
    { "scroll_horz",        {scroll_horz, 0}               },
    { "scroll_begin",       {scroll_begin, 0}              },
    { "scroll_end",         {scroll_end, 0}                },
    { "reload",             {view_reload, 0},              },
    { "reload_ign_cache",   {view_reload_bypass_cache, 0}  },
    { "stop",               {view_stop_loading, 0},        },
    { "zoom_in",            {view_zoom_in, 0},             }, //Can crash (when max zoom reached?).
    { "zoom_out",           {view_zoom_out, 0},            },
    { "uri",                {load_uri, NOSPLIT}            },
    { "js",                 {run_js, NOSPLIT}              },
    { "script",             {run_external_js, 0}           },
    { "toggle_status",      {toggle_status_cb, 0}          },
    { "spawn",              {spawn, 0}                     },
    { "sync_spawn",         {spawn_sync, 0}                }, // needed for cookie handler
    { "sh",                 {spawn_sh, 0}                  },
    { "sync_sh",            {spawn_sh_sync, 0}             }, // needed for cookie handler
    { "exit",               {close_uzbl, 0}                },
    { "search",             {search_forward_text, NOSPLIT} },
    { "search_reverse",     {search_reverse_text, NOSPLIT} },
    { "toggle_insert_mode", {toggle_insert_mode, 0}        },
    { "runcmd",             {runcmd, NOSPLIT}              }
};

static void
commands_hash(void)
{
    unsigned int i;
    uzbl.behave.commands = g_hash_table_new(g_str_hash, g_str_equal);

    for (i = 0; i < LENGTH(cmdlist); i++)
        g_hash_table_insert(uzbl.behave.commands, cmdlist[i].name, cmdlist[i].command);
}

/* -- CORE FUNCTIONS -- */

void
free_action(gpointer act) {
    Action *action = (Action*)act;
    g_free(action->name);
    if (action->param)
        g_free(action->param);
    g_free(action);
}

Action*
new_action(const gchar *name, const gchar *param) {
    Action *action = g_new(Action, 1);

    action->name = g_strdup(name);
    if (param)
        action->param = g_strdup(param);
    else
        action->param = NULL;

    return action;
}

static bool
file_exists (const char * filename) {
    return (access(filename, F_OK) == 0);
}

static void
toggle_insert_mode(WebKitWebView *page, GArray *argv) {
    (void)page;

    if (argv_idx(argv, 0)) {
        if (strcmp (argv_idx(argv, 0), "0") == 0) {
            uzbl.behave.insert_mode = FALSE;
        } else {
            uzbl.behave.insert_mode = TRUE;
        }
    } else {
        uzbl.behave.insert_mode = ! uzbl.behave.insert_mode;
    }

    update_title();
}

static void
load_uri (WebKitWebView *web_view, GArray *argv) {
    if (argv_idx(argv, 0)) {
        GString* newuri = g_string_new (argv_idx(argv, 0));
        if (g_strrstr (argv_idx(argv, 0), "://") == NULL)
            g_string_prepend (newuri, "http://");
        /* if we do handle cookies, ask our handler for them */
        webkit_web_view_load_uri (web_view, newuri->str);
        g_string_free (newuri, TRUE);
    }
}

static void
run_js (WebKitWebView * web_view, GArray *argv) {
    if (argv_idx(argv, 0))
        webkit_web_view_execute_script (web_view, argv_idx(argv, 0));
}

static void
run_external_js (WebKitWebView * web_view, GArray *argv) {
    if (argv_idx(argv, 0)) {
        GArray* lines = read_file_by_line (argv_idx (argv, 0));
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

        if (argv_idx (argv, 1)) {
            gchar* newjs = str_replace("%s", argv_idx (argv, 1), js);
            g_free (js);
            js = newjs;
        }
        webkit_web_view_execute_script (web_view, js);
        g_free (js);
        g_array_free (lines, TRUE);
    }
}

static void
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

static void
search_forward_text (WebKitWebView *page, GArray *argv) {
    search_text(page, argv, TRUE);
}

static void
search_reverse_text (WebKitWebView *page, GArray *argv) {
    search_text(page, argv, FALSE);
}

static void
new_window_load_uri (const gchar * uri) {
    GString* to_execute = g_string_new ("");
    g_string_append_printf (to_execute, "%s --uri '%s'", uzbl.state.executable_path, uri);
    int i;
    for (i = 0; entries[i].long_name != NULL; i++) {
        if ((entries[i].arg == G_OPTION_ARG_STRING) && (strcmp(entries[i].long_name,"uri")!=0) && (strcmp(entries[i].long_name,"name")!=0)) {
            gchar** str = (gchar**)entries[i].arg_data;
            if (*str!=NULL) {
                g_string_append_printf (to_execute, " --%s '%s'", entries[i].long_name, *str);
            }
        }
    }
    if (uzbl.state.verbose)
        printf("\n%s\n", to_execute->str);
    g_spawn_command_line_async (to_execute->str, NULL);
    g_string_free (to_execute, TRUE);
}

static void
close_uzbl (WebKitWebView *page, GArray *argv) {
    (void)page;
    (void)argv;
    gtk_main_quit ();
}

/* --Statusbar functions-- */
static char*
build_progressbar_ascii(int percent) {
   int width=uzbl.gui.sbar.progress_w;
   int i;
   double l;
   GString *bar = g_string_new("");

   l = (double)percent*((double)width/100.);
   l = (int)(l+.5)>=(int)l ? l+.5 : l;

   for(i=0; i<(int)l; i++)
       g_string_append(bar, uzbl.gui.sbar.progress_s);

   for(; i<width; i++)
       g_string_append(bar, uzbl.gui.sbar.progress_u);

   return g_string_free(bar, FALSE);
}

static void
setup_scanner() {
     const GScannerConfig scan_config = {
             (
              "\t\r\n"
             )            /* cset_skip_characters */,
             (
              G_CSET_a_2_z
              "_#"
              G_CSET_A_2_Z
             )            /* cset_identifier_first */,
             (
              G_CSET_a_2_z
              "_0123456789"
              G_CSET_A_2_Z
              G_CSET_LATINS
              G_CSET_LATINC
             )            /* cset_identifier_nth */,
             ( "" )    /* cpair_comment_single */,

             TRUE         /* case_sensitive */,

             FALSE        /* skip_comment_multi */,
             FALSE        /* skip_comment_single */,
             FALSE        /* scan_comment_multi */,
             TRUE         /* scan_identifier */,
             TRUE         /* scan_identifier_1char */,
             FALSE        /* scan_identifier_NULL */,
             TRUE         /* scan_symbols */,
             FALSE        /* scan_binary */,
             FALSE        /* scan_octal */,
             FALSE        /* scan_float */,
             FALSE        /* scan_hex */,
             FALSE        /* scan_hex_dollar */,
             FALSE        /* scan_string_sq */,
             FALSE        /* scan_string_dq */,
             TRUE         /* numbers_2_int */,
             FALSE        /* int_2_float */,
             FALSE        /* identifier_2_string */,
             FALSE        /* char_2_token */,
             FALSE        /* symbol_2_token */,
             TRUE         /* scope_0_fallback */,
             FALSE,
             TRUE
     };

     uzbl.scan = g_scanner_new(&scan_config);
     while(symp->symbol_name) {
         g_scanner_scope_add_symbol(uzbl.scan, 0,
                         symp->symbol_name,
                         GINT_TO_POINTER(symp->symbol_token));
         symp++;
     }
}

static gchar *
expand_template(const char *template) {
     if(!template) return NULL;

     GTokenType token = G_TOKEN_NONE;
     GString *ret = g_string_new("");
     char *buf=NULL;
     int sym;

     g_scanner_input_text(uzbl.scan, template, strlen(template));
     while(!g_scanner_eof(uzbl.scan) && token != G_TOKEN_LAST) {
         token = g_scanner_get_next_token(uzbl.scan);

         if(token == G_TOKEN_SYMBOL) {
             sym = (int)g_scanner_cur_value(uzbl.scan).v_symbol;
             switch(sym) {
                 case SYM_URI:
                     buf = uzbl.state.uri?
                         g_markup_printf_escaped("%s", uzbl.state.uri) :
                         g_strdup("");
                     g_string_append(ret, buf);
                     g_free(buf);
                     break;
                 case SYM_LOADPRGS:
                     buf = itos(uzbl.gui.sbar.load_progress);
                     g_string_append(ret, buf);
                     g_free(buf);
                     break;
                 case SYM_LOADPRGSBAR:
                     buf = build_progressbar_ascii(uzbl.gui.sbar.load_progress);
                     g_string_append(ret, buf);
                     g_free(buf);
                     break;
                 case SYM_TITLE:
                     buf = uzbl.gui.main_title?
                         g_markup_printf_escaped("%s", uzbl.gui.main_title) :
                         g_strdup("");
                     g_string_append(ret, buf);
                     g_free(buf);
                     break;
                 case SYM_SELECTED_URI:
                     buf = uzbl.state.selected_url?
                         g_markup_printf_escaped("%s", uzbl.state.selected_url) :
                         g_strdup("");
                     g_string_append(ret, buf);
                     g_free(buf);
                    break;
                 case SYM_NAME:
                     buf = itos(uzbl.xwin);
                     g_string_append(ret,
                         uzbl.state.instance_name?uzbl.state.instance_name:buf);
                     g_free(buf);
                     break;
                 case SYM_KEYCMD:
                     buf = uzbl.state.keycmd->str?
                         g_markup_printf_escaped("%s", uzbl.state.keycmd->str) :
                         g_strdup("");
                     g_string_append(ret, buf);
                     g_free(buf);
                     break;
                 case SYM_MODE:
                     g_string_append(ret,
                         uzbl.behave.insert_mode?"[I]":"[C]");
                     break;
                 case SYM_MSG:
                     g_string_append(ret,
                         uzbl.gui.sbar.msg?uzbl.gui.sbar.msg:"");
                     break;
                     /* useragent syms */
                 case SYM_WK_MAJ:
                     buf = itos(WEBKIT_MAJOR_VERSION);
                     g_string_append(ret, buf);
                     g_free(buf);
                     break;
                 case SYM_WK_MIN:
                     buf = itos(WEBKIT_MINOR_VERSION);
                     g_string_append(ret, buf);
                     g_free(buf);
                     break;
                 case SYM_WK_MIC:
                     buf = itos(WEBKIT_MICRO_VERSION);
                     g_string_append(ret, buf);
                     g_free(buf);
                     break;
                 case SYM_SYSNAME:
                     g_string_append(ret, uzbl.state.unameinfo.sysname);
                     break;
                 case SYM_NODENAME:
                     g_string_append(ret, uzbl.state.unameinfo.nodename);
                     break;
                 case SYM_KERNREL:
                     g_string_append(ret, uzbl.state.unameinfo.release);
                     break;
                 case SYM_KERNVER:
                     g_string_append(ret, uzbl.state.unameinfo.version);
                     break;
                 case SYM_ARCHSYS:
                     g_string_append(ret, uzbl.state.unameinfo.machine);
                     break;
                 case SYM_ARCHUZBL:
                     g_string_append(ret, ARCH);
                     break;
#ifdef _GNU_SOURCE
                 case SYM_DOMAINNAME:
                     g_string_append(ret, uzbl.state.unameinfo.domainname);
                     break;
#endif
                 case SYM_COMMIT:
                     g_string_append(ret, COMMIT);
                     break;
                 default:
                     break;
             }
         }
         else if(token == G_TOKEN_INT) {
             buf = itos(g_scanner_cur_value(uzbl.scan).v_int);
             g_string_append(ret, buf);
             g_free(buf);
         }
         else if(token == G_TOKEN_IDENTIFIER) {
             g_string_append(ret, (gchar *)g_scanner_cur_value(uzbl.scan).v_identifier);
         }
         else if(token == G_TOKEN_CHAR) {
             g_string_append_c(ret, (gchar)g_scanner_cur_value(uzbl.scan).v_char);
         }
     }

     return g_string_free(ret, FALSE);
}
/* --End Statusbar functions-- */

static void
sharg_append(GArray *a, const gchar *str) {
    const gchar *s = (str ? str : "");
    g_array_append_val(a, s);
}

// make sure that the args string you pass can properly be interpreted (eg properly escaped against whitespace, quotes etc)
static gboolean
run_command (const gchar *command, const guint npre, const gchar **args,
             const gboolean sync, char **stdout) {
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
        if (*stdout) *stdout = strfree(*stdout);
        
        result = g_spawn_sync(NULL, (gchar **)a->data, NULL, G_SPAWN_SEARCH_PATH,
                              NULL, NULL, stdout, NULL, NULL, &err);
    } else result = g_spawn_async(NULL, (gchar **)a->data, NULL, G_SPAWN_SEARCH_PATH,
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

static gchar**
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

static void
spawn(WebKitWebView *web_view, GArray *argv) {
    (void)web_view;
    //TODO: allow more control over argument order so that users can have some arguments before the default ones from run_command, and some after
    if (argv_idx(argv, 0))
        run_command(argv_idx(argv, 0), 0, ((const gchar **) (argv->data + sizeof(gchar*))), FALSE, NULL);
}

static void
spawn_sync(WebKitWebView *web_view, GArray *argv) {
    (void)web_view;
    
    if (argv_idx(argv, 0))
        run_command(argv_idx(argv, 0), 0, ((const gchar **) (argv->data + sizeof(gchar*))),
                    TRUE, &uzbl.comm.sync_stdout);
}

static void
spawn_sh(WebKitWebView *web_view, GArray *argv) {
    (void)web_view;
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

static void
spawn_sh_sync(WebKitWebView *web_view, GArray *argv) {
    (void)web_view;
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

static void
parse_command(const char *cmd, const char *param) {
    Command *c;

    if ((c = g_hash_table_lookup(uzbl.behave.commands, cmd))) {

            guint i;
            gchar **par = split_quoted(param, TRUE);
            GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));

            if (c[1] == NOSPLIT) { /* don't split */
                sharg_append(a, param);
            } else if (par) {
                for (i = 0; i < g_strv_length(par); i++)
                    sharg_append(a, par[i]);
            }
            c[0](uzbl.gui.web_view, a);
            g_strfreev (par);
            g_array_free (a, TRUE);

    } else
        g_printerr ("command \"%s\" not understood. ignoring.\n", cmd);
}

/* command parser */
static void
setup_regex() {
    uzbl.comm.get_regex  = g_regex_new("^[Gg][a-zA-Z]*\\s+([^ \\n]+)$",
            G_REGEX_OPTIMIZE, 0, NULL);
    uzbl.comm.set_regex  = g_regex_new("^[Ss][a-zA-Z]*\\s+([^ ]+)\\s*=\\s*([^\\n].*)$",
            G_REGEX_OPTIMIZE, 0, NULL);
    uzbl.comm.bind_regex = g_regex_new("^[Bb][a-zA-Z]*\\s+?(.*[^ ])\\s*?=\\s*([a-z][^\\n].+)$",
            G_REGEX_UNGREEDY|G_REGEX_OPTIMIZE, 0, NULL);
    uzbl.comm.act_regex = g_regex_new("^[Aa][a-zA-Z]*\\s+([^ \\n]+)\\s*([^\\n]*)?$",
            G_REGEX_OPTIMIZE, 0, NULL);
    uzbl.comm.keycmd_regex = g_regex_new("^[Kk][a-zA-Z]*\\s+([^\\n]+)$",
            G_REGEX_OPTIMIZE, 0, NULL);
}

static gboolean
get_var_value(gchar *name) {
    uzbl_cmdprop *c;

    if( (c = g_hash_table_lookup(uzbl.comm.proto_var, name)) ) {
        if(c->type == TYPE_STR)
            printf("VAR: %s VALUE: %s\n", name, (char *)*c->ptr);
        else if(c->type == TYPE_INT)
            printf("VAR: %s VALUE: %d\n", name, (int)*c->ptr);
    }
    return TRUE;
}

static void
set_proxy_url() {
    SoupURI *suri;

    if(*uzbl.net.proxy_url == ' '
       || uzbl.net.proxy_url == NULL) {
        soup_session_remove_feature_by_type(uzbl.net.soup_session,
                (GType) SOUP_SESSION_PROXY_URI);
    }
    else {
        suri = soup_uri_new(uzbl.net.proxy_url);
        g_object_set(G_OBJECT(uzbl.net.soup_session),
                SOUP_SESSION_PROXY_URI,
                suri, NULL);
        soup_uri_free(suri);
    }
    return;
}

static void
cmd_load_uri() {
    GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
    g_array_append_val (a, uzbl.state.uri);
    load_uri(uzbl.gui.web_view, a);
    g_array_free (a, TRUE);
}

static void 
cmd_always_insert_mode() {
    uzbl.behave.insert_mode =
        uzbl.behave.always_insert_mode ?  TRUE : FALSE;
    update_title();
}

static void
cmd_max_conns() {
    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS, uzbl.net.max_conns, NULL);
}

static void
cmd_max_conns_host() {
    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS_PER_HOST, uzbl.net.max_conns_host, NULL);
}

static void
cmd_http_debug() {
    soup_session_remove_feature
        (uzbl.net.soup_session, SOUP_SESSION_FEATURE(uzbl.net.soup_logger));
    /* do we leak if this doesn't get freed? why does it occasionally crash if freed? */
    /*g_free(uzbl.net.soup_logger);*/

    uzbl.net.soup_logger = soup_logger_new(uzbl.behave.http_debug, -1);
    soup_session_add_feature(uzbl.net.soup_session,
            SOUP_SESSION_FEATURE(uzbl.net.soup_logger));
}

static void
cmd_font_size() {
    WebKitWebSettings *ws = webkit_web_view_get_settings(uzbl.gui.web_view);
    if (uzbl.behave.font_size > 0) {
        g_object_set (G_OBJECT(ws), "default-font-size", uzbl.behave.font_size, NULL);
    }
    
    if (uzbl.behave.monospace_size > 0) {
        g_object_set (G_OBJECT(ws), "default-monospace-font-size",
                      uzbl.behave.monospace_size, NULL);
    } else {
        g_object_set (G_OBJECT(ws), "default-monospace-font-size",
                      uzbl.behave.font_size, NULL);
    }
}

static void
cmd_disable_plugins() {
    WebKitWebSettings *ws = webkit_web_view_get_settings(uzbl.gui.web_view);
    g_object_set (G_OBJECT(ws), "enable-plugins", !uzbl.behave.disable_plugins, NULL);
}

static void
cmd_minimum_font_size() {
    WebKitWebSettings *ws = webkit_web_view_get_settings(uzbl.gui.web_view);
    g_object_set (G_OBJECT(ws), "minimum-font-size", uzbl.behave.minimum_font_size, NULL);
}

static void
cmd_cookie_handler() {
    gchar **split = g_strsplit(uzbl.behave.cookie_handler, " ", 2);
    if ((g_strcmp0(split[0], "sh") == 0) ||
        (g_strcmp0(split[0], "spawn") == 0)) {
        g_free (uzbl.behave.cookie_handler);
        uzbl.behave.cookie_handler =
            g_strdup_printf("sync_%s %s", split[0], split[1]);
    }
    g_strfreev (split);
}

static void
cmd_fifo_dir() {
    uzbl.behave.fifo_dir = init_fifo(uzbl.behave.fifo_dir);
}

static void
cmd_socket_dir() {
    uzbl.behave.socket_dir = init_socket(uzbl.behave.socket_dir);
}

static void
cmd_modkey() {
    int i;
    char *buf;

    buf = g_utf8_strup(uzbl.behave.modkey, -1);
    uzbl.behave.modmask = 0;

    if(uzbl.behave.modkey) 
        g_free(uzbl.behave.modkey);
    uzbl.behave.modkey = buf;

    for (i = 0; modkeys[i].key != NULL; i++) {
        if (g_strrstr(buf, modkeys[i].key))
            uzbl.behave.modmask |= modkeys[i].mask;
    }
}

static void
cmd_useragent() {
    if (*uzbl.net.useragent == ' ') {
        g_free (uzbl.net.useragent);
        uzbl.net.useragent = NULL;
    } else {
        gchar *ua = expand_template(uzbl.net.useragent);
        if (ua)
            g_object_set(G_OBJECT(uzbl.net.soup_session), SOUP_SESSION_USER_AGENT, ua, NULL);
        g_free(uzbl.net.useragent);
        uzbl.net.useragent = ua;
    }
}

static void
move_statusbar() {
    gtk_widget_ref(uzbl.gui.scrolled_win);
    gtk_widget_ref(uzbl.gui.mainbar);
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
    gtk_widget_unref(uzbl.gui.scrolled_win);
    gtk_widget_unref(uzbl.gui.mainbar);
    gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    return;
}

static gboolean
set_var_value(gchar *name, gchar *val) {
    uzbl_cmdprop *c = NULL;
    char *endp = NULL;

    if( (c = g_hash_table_lookup(uzbl.comm.proto_var, name)) ) {
        /* check for the variable type */
        if (c->type == TYPE_STR) {
            g_free(*c->ptr);
            *c->ptr = g_strdup(val);
        } else if(c->type == TYPE_INT) {
            int *ip = GPOINTER_TO_INT(c->ptr);
            *ip = (int)strtoul(val, &endp, 10);
        }

        /* invoke a command specific function */
        if(c->func) c->func();
    }
    return TRUE;
}

static void
runcmd(WebKitWebView* page, GArray *argv) {
    (void) page;
    parse_cmd_line(argv_idx(argv, 0));
}

static void
parse_cmd_line(const char *ctl_line) {
    gchar **tokens;

    /* SET command */
    if(ctl_line[0] == 's' || ctl_line[0] == 'S') {
        tokens = g_regex_split(uzbl.comm.set_regex, ctl_line, 0);
        if(tokens[0][0] == 0) {
            gchar* value = parseenv(g_strdup(tokens[2]));
            set_var_value(tokens[1], value);
            g_strfreev(tokens);
            g_free(value);
        }
        else
            printf("Error in command: %s\n", tokens[0]);
    }
    /* GET command */
    else if(ctl_line[0] == 'g' || ctl_line[0] == 'G') {
        tokens = g_regex_split(uzbl.comm.get_regex, ctl_line, 0);
        if(tokens[0][0] == 0) {
            get_var_value(tokens[1]);
            g_strfreev(tokens);
        }
        else
            printf("Error in command: %s\n", tokens[0]);
    }
    /* BIND command */
    else if(ctl_line[0] == 'b' || ctl_line[0] == 'B') {
        tokens = g_regex_split(uzbl.comm.bind_regex, ctl_line, 0);
        if(tokens[0][0] == 0) {
            gchar* value = parseenv(g_strdup(tokens[2]));
            add_binding(tokens[1], value);
            g_strfreev(tokens);
            g_free(value);
        }
        else
            printf("Error in command: %s\n", tokens[0]);
    }
    /* ACT command */
    else if(ctl_line[0] == 'A' || ctl_line[0] == 'a') {
        tokens = g_regex_split(uzbl.comm.act_regex, ctl_line, 0);
        if(tokens[0][0] == 0) {
            parse_command(tokens[1], tokens[2]);
            g_strfreev(tokens);
        }
        else
            printf("Error in command: %s\n", tokens[0]);
    }
    /* KEYCMD command */
    else if(ctl_line[0] == 'K' || ctl_line[0] == 'k') {
        tokens = g_regex_split(uzbl.comm.keycmd_regex, ctl_line, 0);
        if(tokens[0][0] == 0) {
            /* should incremental commands want each individual "keystroke"
               sent in a loop or the whole string in one go like now? */
            g_string_assign(uzbl.state.keycmd, tokens[1]);
            run_keycmd(FALSE);
            if (g_strstr_len(ctl_line, 7, "n") || g_strstr_len(ctl_line, 7, "N"))
                run_keycmd(TRUE);
            update_title();
            g_strfreev(tokens);
        }
    }
    /* Comments */
    else if(   (ctl_line[0] == '#')
            || (ctl_line[0] == ' ')
            || (ctl_line[0] == '\n'))
        ; /* ignore these lines */
    else
        printf("Command not understood (%s)\n", ctl_line);

    return;
}

static gchar*
build_stream_name(int type, const gchar* dir) {
    char *xwin_str;
    State *s = &uzbl.state;
    gchar *str;

    xwin_str = itos((int)uzbl.xwin);
    if (type == FIFO) {
        str = g_strdup_printf
            ("%s/uzbl_fifo_%s", dir,
             s->instance_name ? s->instance_name : xwin_str);
    } else if (type == SOCKET) {
        str = g_strdup_printf
            ("%s/uzbl_socket_%s", dir,
             s->instance_name ? s->instance_name : xwin_str );
    }
    g_free(xwin_str);
    return str;
}

static gboolean
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

    parse_cmd_line(ctl_line);
    g_free(ctl_line);

    return TRUE;
}

static gchar*
init_fifo(gchar *dir) { /* return dir or, on error, free dir and return NULL */
    if (uzbl.comm.fifo_path) { /* get rid of the old fifo if one exists */
        if (unlink(uzbl.comm.fifo_path) == -1)
            g_warning ("Fifo: Can't unlink old fifo at %s\n", uzbl.comm.fifo_path);
        g_free(uzbl.comm.fifo_path);
        uzbl.comm.fifo_path = NULL;
    }

    if (*dir == ' ') { /* space unsets the variable */
        g_free (dir);
        return NULL;
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

static gboolean
control_stdin(GIOChannel *gio, GIOCondition condition) {
    (void) condition;
    gchar *ctl_line = NULL;
    GIOStatus ret;

    ret = g_io_channel_read_line(gio, &ctl_line, NULL, NULL, NULL);
    if ( (ret == G_IO_STATUS_ERROR) || (ret == G_IO_STATUS_EOF) )
        return FALSE;

    parse_cmd_line(ctl_line);
    g_free(ctl_line);

    return TRUE;
}

static void
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

static gboolean
control_socket(GIOChannel *chan) {
    struct sockaddr_un remote;
    char buffer[512], *ctl_line;
    char temp[128];
    int sock, clientsock, n, done;
    unsigned int t;

    sock = g_io_channel_unix_get_fd(chan);

    memset (buffer, 0, sizeof (buffer));

    t          = sizeof (remote);
    clientsock = accept (sock, (struct sockaddr *) &remote, &t);

    done = 0;
    do {
        memset (temp, 0, sizeof (temp));
        n = recv (clientsock, temp, 128, 0);
        if (n == 0) {
            buffer[strlen (buffer)] = '\0';
            done = 1;
        }
        if (!done)
            strcat (buffer, temp);
    } while (!done);

    if (strcmp (buffer, "\n") < 0) {
        buffer[strlen (buffer) - 1] = '\0';
    } else {
        buffer[strlen (buffer)] = '\0';
    }
    close (clientsock);
    ctl_line = g_strdup(buffer);
    parse_cmd_line (ctl_line);

/*
   TODO: we should be able to do it with this.  but glib errors out with "Invalid argument"
    GError *error = NULL;
    gsize len;
    GIOStatus ret;
    ret = g_io_channel_read_line(chan, &ctl_line, &len, NULL, &error);
    if (ret == G_IO_STATUS_ERROR)
        g_error ("Error reading: %s\n", error->message);

    printf("Got line %s (%u bytes) \n",ctl_line, len);
    if(ctl_line) {
       parse_line(ctl_line);
*/

    g_free(ctl_line);
    return TRUE;
}

static gchar*
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
static void
update_title (void) {
    Behaviour *b = &uzbl.behave;
    gchar *parsed;

    if (b->show_status) {
        if (b->title_format_short) {
            parsed = expand_template(b->title_format_short);
            gtk_window_set_title (GTK_WINDOW(uzbl.gui.main_window), parsed);
            g_free(parsed);
        }
        if (b->status_format) {
            parsed = expand_template(b->status_format);
            gtk_label_set_markup(GTK_LABEL(uzbl.gui.mainbar_label), parsed);
            g_free(parsed);
        }
        if (b->status_background) {
            GdkColor color;
            gdk_color_parse (b->status_background, &color);
            //labels and hboxes do not draw their own background.  applying this on the window is ok as we the statusbar is the only affected widget.  (if not, we could also use GtkEventBox)
            gtk_widget_modify_bg (uzbl.gui.main_window, GTK_STATE_NORMAL, &color);
        }
    } else {
        if (b->title_format_long) {
            parsed = expand_template(b->title_format_long);
            gtk_window_set_title (GTK_WINDOW(uzbl.gui.main_window), parsed);
            g_free(parsed);
        }
    }
}

static gboolean
key_press_cb (GtkWidget* window, GdkEventKey* event)
{
    //TRUE to stop other handlers from being invoked for the event. FALSE to propagate the event further.

    (void) window;

    if (event->type != GDK_KEY_PRESS || event->keyval == GDK_Page_Up || event->keyval == GDK_Page_Down
        || event->keyval == GDK_Up || event->keyval == GDK_Down || event->keyval == GDK_Left || event->keyval == GDK_Right || event->keyval == GDK_Shift_L || event->keyval == GDK_Shift_R)
        return FALSE;

    /* turn off insert mode (if always_insert_mode is not used) */
    if (uzbl.behave.insert_mode && (event->keyval == GDK_Escape)) {
        uzbl.behave.insert_mode = uzbl.behave.always_insert_mode;
        update_title();
        return TRUE;
    }

    if (uzbl.behave.insert_mode && (((event->state & uzbl.behave.modmask) != uzbl.behave.modmask) || (!uzbl.behave.modmask)))
        return FALSE;

    if (event->keyval == GDK_Escape) {
        g_string_truncate(uzbl.state.keycmd, 0);
        update_title();
        return TRUE;
    }

    //Insert without shift - insert from clipboard; Insert with shift - insert from primary
    if (event->keyval == GDK_Insert) {
        gchar * str;
        if ((event->state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) {
            str = gtk_clipboard_wait_for_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY));
        } else {
            str = gtk_clipboard_wait_for_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));
        }
        if (str) {
            g_string_append (uzbl.state.keycmd, str);
            update_title ();
            g_free (str);
        }
        return TRUE;
    }

    if ((event->keyval == GDK_BackSpace) && (uzbl.state.keycmd->len > 0)) {
        g_string_truncate(uzbl.state.keycmd, uzbl.state.keycmd->len - 1);
        update_title();
    }

    gboolean key_ret = FALSE;
    if ((event->keyval == GDK_Return) || (event->keyval == GDK_KP_Enter))
        key_ret = TRUE;
    if (!key_ret) g_string_append(uzbl.state.keycmd, event->string);

    run_keycmd(key_ret);
    update_title();
    if (key_ret) return (!uzbl.behave.insert_mode);
    return TRUE;
}

static void
run_keycmd(const gboolean key_ret) {
    /* run the keycmd immediately if it isn't incremental and doesn't take args */
    Action *action;
    if ((action = g_hash_table_lookup(uzbl.bindings, uzbl.state.keycmd->str))) {
        g_string_truncate(uzbl.state.keycmd, 0);
        parse_command(action->name, action->param);
        return;
    }

    /* try if it's an incremental keycmd or one that takes args, and run it */
    GString* short_keys = g_string_new ("");
    GString* short_keys_inc = g_string_new ("");
    unsigned int i;
    for (i=0; i<(uzbl.state.keycmd->len); i++) {
        g_string_append_c(short_keys, uzbl.state.keycmd->str[i]);
        g_string_assign(short_keys_inc, short_keys->str);
        g_string_append_c(short_keys, '_');
        g_string_append_c(short_keys_inc, '*');

        gboolean exec_now = FALSE;
        if ((action = g_hash_table_lookup(uzbl.bindings, short_keys->str))) {
            if (key_ret) exec_now = TRUE; /* run normal cmds only if return was pressed */
        } else if ((action = g_hash_table_lookup(uzbl.bindings, short_keys_inc->str))) {
            if (key_ret) { /* just quit the incremental command on return */
                g_string_truncate(uzbl.state.keycmd, 0);
                break;
            } else exec_now = TRUE; /* always exec incr. commands on keys other than return */
        }

        if (exec_now) {
            GString* parampart = g_string_new (uzbl.state.keycmd->str);
            GString* actionname = g_string_new ("");
            GString* actionparam = g_string_new ("");
            g_string_erase (parampart, 0, i+1);
            if (action->name)
                g_string_printf (actionname, action->name, parampart->str);
            if (action->param)
                g_string_printf (actionparam, action->param, parampart->str);
            parse_command(actionname->str, actionparam->str);
            g_string_free (actionname, TRUE);
            g_string_free (actionparam, TRUE);
            g_string_free (parampart, TRUE);
            if (key_ret)
                g_string_truncate(uzbl.state.keycmd, 0);
            break;
        }

        g_string_truncate(short_keys, short_keys->len - 1);
    }
    g_string_free (short_keys, TRUE);
    g_string_free (short_keys_inc, TRUE);
}

static GtkWidget*
create_browser () {
    GUI *g = &uzbl.gui;

    GtkWidget* scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    //main_window_ref = g_object_ref(scrolled_window);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_NEVER); //todo: some sort of display of position/total length. like what emacs does

    g->web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
    gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET (g->web_view));

    g_signal_connect (G_OBJECT (g->web_view), "title-changed", G_CALLBACK (title_change_cb), g->web_view);
    g_signal_connect (G_OBJECT (g->web_view), "load-progress-changed", G_CALLBACK (progress_change_cb), g->web_view);
    g_signal_connect (G_OBJECT (g->web_view), "load-committed", G_CALLBACK (load_commit_cb), g->web_view);
    g_signal_connect (G_OBJECT (g->web_view), "load-started", G_CALLBACK (load_start_cb), g->web_view);
    g_signal_connect (G_OBJECT (g->web_view), "load-finished", G_CALLBACK (log_history_cb), g->web_view);
    g_signal_connect (G_OBJECT (g->web_view), "load-finished", G_CALLBACK (load_finish_cb), g->web_view);
    g_signal_connect (G_OBJECT (g->web_view), "hovering-over-link", G_CALLBACK (link_hover_cb), g->web_view);
    g_signal_connect (G_OBJECT (g->web_view), "new-window-policy-decision-requested", G_CALLBACK (new_window_cb), g->web_view);
    g_signal_connect (G_OBJECT (g->web_view), "download-requested", G_CALLBACK (download_cb), g->web_view);
    g_signal_connect (G_OBJECT (g->web_view), "create-web-view", G_CALLBACK (create_web_view_cb), g->web_view);

    return scrolled_window;
}

static GtkWidget*
create_mainbar () {
    GUI *g = &uzbl.gui;

    g->mainbar = gtk_hbox_new (FALSE, 0);

    /* keep a reference to the bar so we can re-pack it at runtime*/
    //sbar_ref = g_object_ref(g->mainbar);

    g->mainbar_label = gtk_label_new ("");
    gtk_label_set_selectable((GtkLabel *)g->mainbar_label, TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(g->mainbar_label), PANGO_ELLIPSIZE_END);
    gtk_misc_set_alignment (GTK_MISC(g->mainbar_label), 0, 0);
    gtk_misc_set_padding (GTK_MISC(g->mainbar_label), 2, 2);
    gtk_box_pack_start (GTK_BOX (g->mainbar), g->mainbar_label, TRUE, TRUE, 0);
    return g->mainbar;
}

static
GtkWidget* create_window () {
    GtkWidget* window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
    gtk_widget_set_name (window, "Uzbl browser");
    g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (destroy_cb), NULL);
    g_signal_connect (G_OBJECT (window), "key-press-event", G_CALLBACK (key_press_cb), NULL);

    return window;
}

static void
run_handler (const gchar *act, const gchar *args) {
    char **parts = g_strsplit(act, " ", 2);
    if (!parts) return;
    else if ((g_strcmp0(parts[0], "spawn") == 0)
             || (g_strcmp0(parts[0], "sh") == 0)
             || (g_strcmp0(parts[0], "sync_spawn") == 0)
             || (g_strcmp0(parts[0], "sync_sh") == 0)) {
        guint i;
        GString *a = g_string_new ("");
        char **spawnparts;
        spawnparts = split_quoted(parts[1], FALSE);
        g_string_append_printf(a, "%s", spawnparts[0]);
        if (args) g_string_append_printf(a, " %s", args); /* append handler args before user args */
        
        for (i = 1; i < g_strv_length(spawnparts); i++) /* user args */
            g_string_append_printf(a, " %s", spawnparts[i]);
        parse_command(parts[0], a->str);
        g_string_free (a, TRUE);
        g_strfreev (spawnparts);
    } else
        parse_command(parts[0], parts[1]);
    g_strfreev (parts);
}

static void
add_binding (const gchar *key, const gchar *act) {
    char **parts = g_strsplit(act, " ", 2);
    Action *action;

    if (!parts)
        return;

    //Debug:
    if (uzbl.state.verbose)
        printf ("Binding %-10s : %s\n", key, act);
    action = new_action(parts[0], parts[1]);

    g_hash_table_replace(uzbl.bindings, g_strdup(key), action);
    g_strfreev(parts);
}

static gchar*
get_xdg_var (XDG_Var xdg) {
    const gchar* actual_value = getenv (xdg.environmental);
    const gchar* home         = getenv ("HOME");

    gchar* return_value = str_replace ("~", home, actual_value);

    if (! actual_value || strcmp (actual_value, "") == 0) {
        if (xdg.default_value) {
            return_value = str_replace ("~", home, xdg.default_value);
        } else {
            return_value = NULL;
        }
    }
    return return_value;
}

static gchar*
find_xdg_file (int xdg_type, char* filename) {
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
        return NULL;
    }
}
static void
settings_init () {
    State *s = &uzbl.state;
    Network *n = &uzbl.net;
    int i;
    for (i = 0; default_config[i].command != NULL; i++) {
        parse_cmd_line(default_config[i].command);
    }

    if (!s->config_file) {
        s->config_file = find_xdg_file (0, "/uzbl/config");
    }

    if (s->config_file) {
        GArray* lines = read_file_by_line (s->config_file);
        int i = 0;
        gchar* line;

        while ((line = g_array_index(lines, gchar*, i))) {
            parse_cmd_line (line);
            i ++;
            g_free (line);
        }
        g_array_free (lines, TRUE);
    } else {
        if (uzbl.state.verbose)
            printf ("No configuration file loaded.\n");
    }

    g_signal_connect(n->soup_session, "request-queued", G_CALLBACK(handle_cookies), NULL);
}

static void handle_cookies (SoupSession *session, SoupMessage *msg, gpointer user_data){
    (void) session;
    (void) user_data;
    if (!uzbl.behave.cookie_handler) return;

    soup_message_add_header_handler(msg, "got-headers", "Set-Cookie", G_CALLBACK(save_cookies), NULL);
    GString *s = g_string_new ("");
    SoupURI * soup_uri = soup_message_get_uri(msg);
    g_string_printf(s, "GET '%s' '%s'", soup_uri->host, soup_uri->path);
    run_handler(uzbl.behave.cookie_handler, s->str);

    if(uzbl.comm.sync_stdout)
        soup_message_headers_replace (msg->request_headers, "Cookie", uzbl.comm.sync_stdout);
    //printf("stdout: %s\n", uzbl.comm.sync_stdout);   // debugging
    if (uzbl.comm.sync_stdout) uzbl.comm.sync_stdout = strfree(uzbl.comm.sync_stdout);
        
    g_string_free(s, TRUE);
}

static void
save_cookies (SoupMessage *msg, gpointer user_data){
    (void) user_data;
    GSList *ck;
    char *cookie;
    for (ck = soup_cookies_from_response(msg); ck; ck = ck->next){
        cookie = soup_cookie_to_set_cookie_header(ck->data);
        SoupURI * soup_uri = soup_message_get_uri(msg);
        GString *s = g_string_new ("");
        g_string_printf(s, "PUT '%s' '%s' '%s'", soup_uri->host, soup_uri->path, cookie);
        run_handler(uzbl.behave.cookie_handler, s->str);
        g_free (cookie);
        g_string_free(s, TRUE);
    }
    g_slist_free(ck);
}

int
main (int argc, char* argv[]) {
    gtk_init (&argc, &argv);
    if (!g_thread_supported ())
        g_thread_init (NULL);
    uzbl.state.executable_path = g_strdup(argv[0]);
    uzbl.state.selected_url = NULL;
    uzbl.state.searchtx = NULL;

    GOptionContext* context = g_option_context_new ("- some stuff here maybe someday");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group (context, gtk_get_option_group (TRUE));
    g_option_context_parse (context, &argc, &argv, NULL);
    g_option_context_free(context);
    /* initialize hash table */
    uzbl.bindings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_action);

    uzbl.net.soup_session = webkit_get_default_session();
    uzbl.state.keycmd = g_string_new("");

    if(setup_signal(SIGTERM, catch_sigterm) == SIG_ERR)
        fprintf(stderr, "uzbl: error hooking SIGTERM\n");
    if(setup_signal(SIGINT, catch_sigint) == SIG_ERR)
        fprintf(stderr, "uzbl: error hooking SIGINT\n");

    if(uname(&uzbl.state.unameinfo) == -1)
        g_printerr("Can't retrieve unameinfo.  Your useragent might appear wrong.\n");

    uzbl.gui.sbar.progress_s = g_strdup("=");
    uzbl.gui.sbar.progress_u = g_strdup("·");
    uzbl.gui.sbar.progress_w = 10;

    setup_regex();
    setup_scanner();
    commands_hash ();
    make_var_to_name_hash();

    uzbl.gui.vbox = gtk_vbox_new (FALSE, 0);

    uzbl.gui.scrolled_win = create_browser();
    create_mainbar();

    /* initial packing */
    gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.mainbar, FALSE, TRUE, 0);

    uzbl.gui.main_window = create_window ();
    gtk_container_add (GTK_CONTAINER (uzbl.gui.main_window), uzbl.gui.vbox);


    gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    gtk_widget_show_all (uzbl.gui.main_window);
    uzbl.xwin = GDK_WINDOW_XID (GTK_WIDGET (uzbl.gui.main_window)->window);

    if (uzbl.state.verbose) {
        printf("Uzbl start location: %s\n", argv[0]);
        printf("window_id %i\n",(int) uzbl.xwin);
        printf("pid %i\n", getpid ());
        printf("name: %s\n", uzbl.state.instance_name);
    }

    uzbl.gui.scbar_v = (GtkScrollbar*) gtk_vscrollbar_new (NULL);
    uzbl.gui.bar_v = gtk_range_get_adjustment((GtkRange*) uzbl.gui.scbar_v);
    uzbl.gui.scbar_h = (GtkScrollbar*) gtk_hscrollbar_new (NULL);
    uzbl.gui.bar_h = gtk_range_get_adjustment((GtkRange*) uzbl.gui.scbar_h);
    gtk_widget_set_scroll_adjustments ((GtkWidget*) uzbl.gui.web_view, uzbl.gui.bar_h, uzbl.gui.bar_v);

    settings_init ();

    if (!uzbl.behave.show_status)
        gtk_widget_hide(uzbl.gui.mainbar);
    else
        update_title();

    create_stdin();

    if(uzbl.state.uri) {
        GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
        g_array_append_val(a, uzbl.state.uri);
        load_uri (uzbl.gui.web_view, a);
        g_array_free (a, TRUE);
    }

    gtk_main ();
    clean_up();

    return EXIT_SUCCESS;
}

/* vi: set et ts=4: */
