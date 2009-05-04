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
} State;

typedef struct {
    SoupSession *soup_session;
    SoupLogger *soup_logger;
    char *proxy_url;
    char *useragent;
    gint max_conns;
    gint max_conns_host;
} Network;


/* main uzbl data structure */
typedef struct {
    GUI     gui;
    Communication comm;
    State   state;
    Network net;
    Window  xwin;
} Uzbl;

typedef struct {
    char* name;
    char* param;
} Action;


void
eprint(const char *errstr, ...);

char *
estrdup(const char *str);

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
run_command(const char *command, const char *args);

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

/* vi: set et ts=4: */
