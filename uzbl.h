/* 
 * See LICENSE for license details
 *
 * Changelog:
 * ---------
 *
 * (c) 2009 by Robert Manea
 *     - introduced struct concept
 *     - statusbar template
 *     
 */

/* statusbar symbols */
enum { SYM_TITLE, SYM_URI, SYM_NAME, 
       SYM_LOADPRGS, SYM_LOADPRGSBAR,
       SYM_KEYCMD, SYM_MODE};
const struct {
    gchar *symbol_name;
    guint symbol_token;
} symbols[] = {
    {"NAME",                 SYM_NAME},
    {"URI",                  SYM_URI},
    {"TITLE",                SYM_TITLE},
    {"KEYCMD",               SYM_KEYCMD},
    {"MODE",                 SYM_MODE},
    {"LOAD_PROGRESS",        SYM_LOADPRGS},
    {"LOAD_PROGRESSBAR",     SYM_LOADPRGSBAR},
    {NULL,                   0}
}, *symp = symbols;

/* status bar elements */
typedef struct {
    gint           load_progress;
} StatusBar;


/* gui elements */
typedef struct {
    GtkWidget*     main_window;
    GtkWidget*     mainbar;
    GtkWidget*     mainbar_label;
    GtkScrollbar*  scbar_v;   // Horizontal and Vertical Scrollbar
    GtkScrollbar*  scbar_h;   // (These are still hidden)
    GtkAdjustment* bar_v; // Information about document length
    GtkAdjustment* bar_h; // and scrolling position
    WebKitWebView* web_view;
    gchar*         main_title;

    StatusBar sbar;
} GUI;


/* external communication*/
enum { FIFO, SOCKET};
typedef struct {
    char           fifo_path[64];
    char           socket_path[108];
} Communication;


/* internal state */
typedef struct {
    gchar    *uri;
    gchar    *config_file;
    gchar    *instance_name;
    gchar    config_file_path[500];
    gchar    selected_url[500];
    char     executable_path[500];
    GString* keycmd;
    gchar    searchtx[500];
    struct utsname unameinfo; /* system info */
} State;


/* networking */
typedef struct {
    SoupSession *soup_session;
    SoupLogger *soup_logger;
    char *proxy_url;
    char *useragent;
    gint max_conns;
    gint max_conns_host;
} Network;


/* behaviour */
typedef struct {
    gchar    *status_format;
    gchar*   history_handler;
    gchar*   fifo_dir;
    gchar*   socket_dir;
    gchar*   download_handler;
    gchar*   cookie_handler;
    gboolean always_insert_mode;
    gboolean show_status;
    gboolean insert_mode;
    gboolean status_top;
    gchar*   modkey;
    guint    modmask;
    guint    http_debug;

    /* command list: name -> Command  */
    GHashTable* commands;
} Behaviour;


/* main uzbl data structure */
typedef struct {
    GUI           gui;
    State         state;
    Network       net;
    Behaviour     behave;
    Communication comm;

    Window        xwin;
    GScanner      *scan;

    /* group bindings: key -> action */
    GHashTable* bindings;
} Uzbl;


typedef struct {
    char* name;
    char* param;
} Action;



/* Functions */
static void
setup_scanner();

char *
itos(int val);

static gboolean
new_window_cb (WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action, WebKitWebPolicyDecision *policy_decision, gpointer user_data);

WebKitWebView*
create_web_view_cb (WebKitWebView  *web_view, WebKitWebFrame *frame, gpointer user_data);

static gboolean
download_cb (WebKitWebView *web_view, GObject *download, gpointer user_data);

static void
toggle_status_cb (WebKitWebView* page, const char *param);

static void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data);

static void
title_change_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, const gchar* title, gpointer data);

static void
progress_change_cb (WebKitWebView* page, gint progress, gpointer data);

static void
load_commit_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data);

static void
destroy_cb (GtkWidget* widget, gpointer data);

static void
log_history_cb ();

static void
commands_hash(void);

void
free_action(gpointer act);

Action*
new_action(const gchar *name, const gchar *param);

static bool
file_exists (const char * filename);

void
set_insert_mode(WebKitWebView *page, const gchar *param);

static void
load_uri (WebKitWebView * web_view, const gchar *param);

static void
new_window_load_uri (const gchar * uri);

static void
close_uzbl (WebKitWebView *page, const char *param);

static gboolean
run_command_async(const char *command, const char *args);

static gboolean
run_command_sync(const char *command, const char *args, char **stdout);

static void
spawn(WebKitWebView *web_view, const char *param);

static void
parse_command(const char *cmd, const char *param);

static void
parse_line(char *line);

void
build_stream_name(int type);

static void
control_fifo(GIOChannel *gio, GIOCondition condition);

static void
create_fifo();

static void
create_socket();

static void
control_socket(GIOChannel *chan);
 

static void
update_title (void);
 
static gboolean
key_press_cb (WebKitWebView* page, GdkEventKey* event);

static GtkWidget*
create_browser ();

static GtkWidget*
create_mainbar ();

static
GtkWidget* create_window ();

static void
add_binding (const gchar *key, const gchar *act);

static void
settings_init ();

static void
search_text (WebKitWebView *page, const char *param);

static void
run_js (WebKitWebView * web_view, const gchar *param);

static char *
str_replace (const char* search, const char* replace, const char* string);

static void handle_cookies (SoupSession *session,
							SoupMessage *msg,
							gpointer     user_data);
static void
save_cookies (SoupMessage *msg,
			  gpointer     user_data);
/* vi: set et ts=4: */
