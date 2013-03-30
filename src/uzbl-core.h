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
    gboolean       custom_context_menu;
    GPtrArray     *menu_items;
} UzblGui;

/* External communication */
typedef struct {
    gchar          *fifo_path;
    gchar          *socket_path;

    GPtrArray      *connect_chan;
    GPtrArray      *client_chan;
} UzblCommunication;

/* Internal state */
typedef struct {
    gchar          *uri;
    gchar          *config_file;
    char           *instance_name;
    gchar          *selected_url;
    gchar          *last_selected_url;
    gchar          *executable_path;
    gchar          *searchtx;
    UzblFindOptions searchoptions;
    UzblFindOptions lastsearchoptions;
    gboolean        searchforward;
    gboolean        verbose;
    gboolean        embed;
    GdkEventButton *last_button;
    gchar          *last_result;
    gboolean        plug_mode;
    gboolean        load_failed;
    gchar          *disk_cache_directory;
    gchar          *web_extensions_directory;
    JSGlobalContextRef jscontext;

    /* Events */
    int             socket_id;
    gboolean        events_stdout;
    gboolean        handle_multi_button;
    GPtrArray      *event_buffer;
    gchar         **connect_socket_names;
} UzblState;

/* Networking */
typedef struct {
    SoupSession    *soup_session;
    UzblCookieJar  *soup_cookie_jar;
    GHashTable     *pending_auths;
    SoupLogger     *soup_logger;
    char           *proxy_url;
    char           *useragent;
    char           *accept_languages;
    gint            max_conns;
    gint            max_conns_host;
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
    gchar   *authentication_handler;
    gchar   *scheme_handler;
    gchar   *request_handler;
    gchar   *download_handler;

    gboolean forward_keys;
    guint    http_debug;
    gchar   *shell_cmd;
    guint    view_source;
    gboolean maintain_history;

    gboolean print_version;

    gfloat zoom_step;

    /* command list: (key)name -> (value)Command  */
    GHashTable *commands;
    /* variables: (key)name -> (value)uzbl_cmdprop */
    GHashTable *proto_var;
} UzblBehaviour;

/* Static information */
typedef struct {
    int     webkit_major;
    int     webkit_minor;
    int     webkit_micro;
    int     webkit2;
    gchar  *arch;
    gchar  *commit;

    pid_t   pid;
    gchar  *pid_str;
} UzblInfo;

/* Main uzbl data structure */
typedef struct {
    UzblGui           gui;
    UzblState         state;
    UzblNetwork       net;
    UzblBehaviour     behave;
    UzblCommunication comm;
    UzblInfo          info;
} UzblCore;

extern UzblCore uzbl; /* Main Uzbl object */

void
uzbl_initialize (int argc, char **argv);

#endif
