#ifndef UZBL_UZBL_CORE_H
#define UZBL_UZBL_CORE_H

#include "webkit.h"

#include "cookie-jar.h"

#include <sys/types.h>

#define uzbl_debug(...) if (uzbl.state.verbose) printf(__VA_ARGS__)

/* GUI elements */
typedef struct {
    /* Window */
    GtkWidget     *main_window;
    gchar         *geometry;
    GtkPlug       *plug;
    GtkWidget     *scrolled_win;
    GtkWidget     *vbox;

    /* Status bar */
    GtkWidget     *status_bar;

    /* Scrolling */
    GtkAdjustment *bar_v;     /* Information about document length */
    GtkAdjustment *bar_h;     /* and scrolling position */

    /* Web page */
    WebKitWebView *web_view;
    gchar         *main_title;
    gchar         *icon;
    gchar         *icon_name;

    /* Inspector */
    GtkWidget          *inspector_window;
    WebKitWebInspector *inspector;

    /* Context menu */
    gboolean       default_context_menu;
    GPtrArray     *menu_items;
} UzblGui;

/* Internal state */
typedef struct {
    gchar          *uri;
    gchar          *config_file;
    char           *instance_name;
    gchar          *selected_url;
    gchar          *last_selected_url;
    gchar          *executable_path;
    gboolean        verbose;
    gboolean        embed;
    GdkEventButton *last_button;
    gchar          *last_result;
    gboolean        plug_mode;
    gboolean        frozen;
    gboolean        load_failed;
    gchar          *disk_cache_directory;
    gchar          *web_extensions_directory;
    JSGlobalContextRef jscontext;

    gboolean        started;
    gboolean        gtk_started;
    gboolean        exit;

    /* Events */
    int             socket_id;
    gboolean        events_stdout;
    gboolean        handle_multi_button;
    gchar         **connect_socket_names;
} UzblState;

/* Networking */
typedef struct {
    SoupSession    *soup_session;
    UzblCookieJar  *soup_cookie_jar;
    SoupLogger     *soup_logger;
    gchar          *accept_languages;
} UzblNetwork;

/* Behaviour */
typedef struct {
    /* Status bar */
    gchar   *status_format;
    gchar   *status_format_right;
    gchar   *status_background;
    gboolean status_top;

    /* Window title */
    gchar   *title_format_short;
    gchar   *title_format_long;

    /* Communication */
    gchar   *fifo_dir;
    gchar   *socket_dir;

    /* Handlers */
    gchar   *scheme_handler;
    gchar   *request_handler;
    gchar   *download_handler;
    gchar   *mime_handler;
    gchar   *authentication_handler;

    gboolean forward_keys;
    gchar   *http_debug;
    gchar   *shell_cmd;
    guint    view_source;
    gboolean maintain_history;

    gboolean print_version;

    gdouble zoom_step;

    /* variables: (key)name -> (value)uzbl_cmdprop */
    GHashTable *proto_var;
} UzblBehaviour;

/* Static information */
typedef struct {
    int     webkit_major;
    int     webkit_minor;
    int     webkit_micro;
    int     webkit_ua_major;
    int     webkit_ua_minor;
    int     webkit2;
    gchar  *arch;
    gchar  *commit;

    pid_t   pid;
    gchar  *pid_str;
} UzblInfo;

struct _UzblCommands;
typedef struct _UzblCommands UzblCommands;

struct _UzblIO;
typedef struct _UzblIO UzblIO;

struct _UzblRequests;
typedef struct _UzblRequests UzblRequests;

/* Main uzbl data structure */
typedef struct {
    UzblGui           gui;
    UzblState         state;
    UzblNetwork       net;
    UzblBehaviour     behave;
    UzblInfo          info;

    UzblCommands     *commands;
    UzblIO           *io;
    UzblRequests     *requests;
} UzblCore;

extern UzblCore uzbl; /* Main Uzbl object */

void
uzbl_initialize (int argc, char **argv);
void
uzbl_free ();

#endif
