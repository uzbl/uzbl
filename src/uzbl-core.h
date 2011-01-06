/* -*- c-basic-offset: 4; -*-

 * See LICENSE for license details
 *
 * Changelog:
 * ---------
 *
 * (c) 2009 by Robert Manea
 *     - introduced struct concept
 *
 */

#define _POSIX_SOURCE

#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <webkit/webkit.h>
#include <libsoup/soup.h>
#include <JavaScriptCore/JavaScript.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <poll.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <assert.h>

#include "cookie-jar.h"

#define LENGTH(x) (sizeof x / sizeof x[0])

/* gui elements */
typedef struct {
    GtkWidget*     main_window;
    gchar*         geometry;
    GtkPlug*       plug;
    GtkWidget*     scrolled_win;
    GtkWidget*     vbox;

    /* Mainbar */
    GtkWidget*     mainbar;
    GtkWidget*     mainbar_label_left;
    GtkWidget*     mainbar_label_right;

    GtkScrollbar*  scbar_v;   // Horizontal and Vertical Scrollbar
    GtkScrollbar*  scbar_h;   // (These are still hidden)
    GtkAdjustment* bar_v; // Information about document length
    GtkAdjustment* bar_h; // and scrolling position
    int            scrollbars_visible;
    WebKitWebView* web_view;
    gchar*         main_title;
    gchar*         icon;

    /* WebInspector */
    GtkWidget *inspector_window;
    WebKitWebInspector *inspector;

    /* custom context menu item */
    GPtrArray    *menu_items;
} GUI;


/* external communication*/
enum { FIFO, SOCKET};
typedef struct {
    gchar          *fifo_path;
    gchar          *socket_path;
    /* stores (key)"variable name" -> (value)"pointer to var*/
    GHashTable     *proto_var;

    gchar          *sync_stdout;
    GPtrArray      *connect_chan;
    GPtrArray      *client_chan;
} Communication;


/* internal state */
typedef struct {
    gchar    *uri;
    gchar    *config_file;
    int      socket_id;
    char     *instance_name;
    gchar    *selected_url;
    gchar    *last_selected_url;
    gchar    *executable_path;
    gchar*   searchtx;
    gboolean verbose;
    gboolean events_stdout;
    GPtrArray *event_buffer;
    gchar**   connect_socket_names;
    GdkEventButton *last_button;
    gboolean plug_mode;
} State;


/* networking */
typedef struct {
    SoupSession   *soup_session;
    UzblCookieJar *soup_cookie_jar;
    SoupLogger *soup_logger;
    char *proxy_url;
    char *useragent;
    char *accept_languages;
    gint max_conns;
    gint max_conns_host;
} Network;


/* behaviour */
typedef struct {
    /* Status bar */
    gchar*   status_format;
    gchar*   status_format_right;
    gchar*   status_background;

    /* Window title */
    gchar*   title_format_short;
    gchar*   title_format_long;

    gchar*   fifo_dir;
    gchar*   socket_dir;
    gchar*   cookie_handler;
    gchar*   authentication_handler;
    gchar*   default_font_family;
    gchar*   monospace_font_family;
    gchar*   sans_serif_font_family;
    gchar*   serif_font_family;
    gchar*   fantasy_font_family;
    gchar*   cursive_font_family;
    gchar*   scheme_handler;
    gchar*   download_handler;
    gboolean show_status;
    gboolean forward_keys;
    gboolean status_top;
    guint    modmask;
    guint    http_debug;
    gchar*   shell_cmd;
    guint    view_source;
    /* WebKitWebSettings exports */
    guint    font_size;
    guint    monospace_size;
    guint    minimum_font_size;
    gfloat   zoom_level;
    gboolean zoom_type;
    guint    enable_pagecache;
    guint    disable_plugins;
    guint    disable_scripts;
    guint    autoload_img;
    guint    autoshrink_img;
    guint    enable_spellcheck;
    guint    enable_private;
    guint    print_bg;
    gchar*   style_uri;
    guint    resizable_txt;
    gchar*   default_encoding;
    guint    enforce_96dpi;
    gchar    *inject_html;
    guint    caret_browsing;
    guint    javascript_windows;
    guint    mode;
    gchar*   base_url;
    gboolean print_version;

    /* command list: (key)name -> (value)Command  */
    /* command list: (key)name -> (value)Command  */
    GHashTable* commands;
    /* event lookup: (key)event_id -> (value)event_name */
    GHashTable *event_lookup;
} Behaviour;

/* javascript */
typedef struct {
    gboolean            initialized;
    JSClassDefinition   classdef;
    JSClassRef          classref;
} Javascript;

/* static information */
typedef struct {
    int   webkit_major;
    int   webkit_minor;
    int   webkit_micro;
    gchar *arch;
    gchar *commit;
    gchar *pid_str;
} Info;

/* main uzbl data structure */
typedef struct {
    GUI           gui;
    State         state;
    Network       net;
    Behaviour     behave;
    Communication comm;
    Javascript    js;
    Info          info;

    Window        xwin;
} UzblCore;

/* Main Uzbl object */
extern UzblCore uzbl;

typedef void sigfunc(int);

/* uzbl variables */
enum ptr_type {TYPE_INT, TYPE_STR, TYPE_FLOAT};
typedef struct {
    enum ptr_type type;
    union {
        int *i;
        float *f;
        gchar **s;
    } ptr;
    int dump;
    int writeable;
    /*@null@*/ void (*func)(void);
} uzbl_cmdprop;

/* Functions */
char *
itos(int val);

gchar*
strfree(gchar *str);

void
clean_up(void);

void
catch_sigterm(int s);

sigfunc *
setup_signal(int signe, sigfunc *shandler);

gboolean
set_var_value(const gchar *name, gchar *val);

void
load_uri_imp(gchar *uri);

void
print(WebKitWebView *page, GArray *argv, GString *result);

void
commands_hash(void);

void
load_uri (WebKitWebView * web_view, GArray *argv, GString *result);

void
chain (WebKitWebView *page, GArray *argv, GString *result);

void
close_uzbl (WebKitWebView *page, GArray *argv, GString *result);

gboolean
run_command(const gchar *command, const gchar **args, const gboolean sync,
            char **output_stdout);

void
spawn_async(WebKitWebView *web_view, GArray *argv, GString *result);

void
spawn_sh_async(WebKitWebView *web_view, GArray *argv, GString *result);

void
spawn_sync(WebKitWebView *web_view, GArray *argv, GString *result);

void
spawn_sh_sync(WebKitWebView *web_view, GArray *argv, GString *result);

void
spawn_sync_exec(WebKitWebView *web_view, GArray *argv, GString *result);

void
parse_command(const char *cmd, const char *param, GString *result);

void
parse_cmd_line(const char *ctl_line, GString *result);

/*@null@*/ gchar*
build_stream_name(int type, const gchar *dir);

gboolean
control_fifo(GIOChannel *gio, GIOCondition condition);

/*@null@*/ gchar*
init_fifo(gchar *dir);

gboolean
control_stdin(GIOChannel *gio, GIOCondition condition);

void
create_stdin();

/*@null@*/ gchar*
init_socket(gchar *dir);

gboolean
control_socket(GIOChannel *chan);

gboolean
control_client_socket(GIOChannel *chan);

void
update_title (void);

gboolean
key_press_cb (GtkWidget* window, GdkEventKey* event);

gboolean
key_release_cb (GtkWidget* window, GdkEventKey* event);

void
initialize (int argc, char *argv[]);

void
create_browser ();

GtkWidget*
create_mainbar ();

GtkWidget*
create_window ();

GtkPlug*
create_plug ();

void
run_handler (const gchar *act, const gchar *args);

void
settings_init ();

void
search_text (WebKitWebView *page, GArray *argv, const gboolean forward);

void
search_forward_text (WebKitWebView *page, GArray *argv, GString *result);

void
search_reverse_text (WebKitWebView *page, GArray *argv, GString *result);

void
search_clear(WebKitWebView *page, GArray *argv, GString *result);

void
dehilight (WebKitWebView *page, GArray *argv, GString *result);

void
run_js (WebKitWebView * web_view, GArray *argv, GString *result);

void
run_external_js (WebKitWebView * web_view, GArray *argv, GString *result);

void
eval_js(WebKitWebView * web_view, gchar *script, GString *result, const gchar *script_file);

void
handle_authentication (SoupSession *session,
                            SoupMessage *msg,
                            SoupAuth    *auth,
                            gboolean     retrying,
                            gpointer     user_data);

void handle_cookies (SoupSession *session,
                            SoupMessage *msg,
                            gpointer     user_data);

void
set_var(WebKitWebView *page, GArray *argv, GString *result);

void
act_dump_config();

void
act_dump_config_as_events();

void
dump_var_hash(gpointer k, gpointer v, gpointer ud);

void
dump_key_hash(gpointer k, gpointer v, gpointer ud);

void
dump_config();

void
dump_config_as_events();

void
retrieve_geometry();

void
set_webview_scroll_adjustments();

void
event(WebKitWebView *page, GArray *argv, GString *result);

void
init_connect_socket();

gboolean
remove_socket_from_array(GIOChannel *chan);

void
menu_add(WebKitWebView *page, GArray *argv, GString *result);

void
menu_add_link(WebKitWebView *page, GArray *argv, GString *result);

void
menu_add_image(WebKitWebView *page, GArray *argv, GString *result);

void
menu_add_edit(WebKitWebView *page, GArray *argv, GString *result);

void
menu_add_separator(WebKitWebView *page, GArray *argv, GString *result);

void
menu_add_separator_link(WebKitWebView *page, GArray *argv, GString *result);

void
menu_add_separator_image(WebKitWebView *page, GArray *argv, GString *result);

void
menu_add_separator_edit(WebKitWebView *page, GArray *argv, GString *result);

void
menu_remove(WebKitWebView *page, GArray *argv, GString *result);

void
menu_remove_link(WebKitWebView *page, GArray *argv, GString *result);

void
menu_remove_image(WebKitWebView *page, GArray *argv, GString *result);

void
menu_remove_edit(WebKitWebView *page, GArray *argv, GString *result);

gint
get_click_context();

void
hardcopy(WebKitWebView *page, GArray *argv, GString *result);

void
include(WebKitWebView *page, GArray *argv, GString *result);

void
show_inspector(WebKitWebView *page, GArray *argv, GString *result);

void
add_cookie(WebKitWebView *page, GArray *argv, GString *result);

void
delete_cookie(WebKitWebView *page, GArray *argv, GString *result);

void
builtins();

typedef void (*Command)(WebKitWebView*, GArray *argv, GString *result);

typedef struct {
    Command function;
    gboolean no_split;
} CommandInfo;

typedef struct {
    gchar *name;
    gchar *cmd;
    gboolean issep;
    guint context;
} MenuItem;


/* vi: set et ts=4: */
