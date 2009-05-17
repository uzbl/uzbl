/* -*- c-basic-offset: 4; -*- 

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

#define STATUS_DEFAULT "<span background=\"darkblue\" foreground=\"white\"> MODE </span> <span background=\"red\" foreground=\"white\">KEYCMD</span> (LOAD_PROGRESS%)  <b>TITLE</b>  - Uzbl browser"
#define TITLE_LONG_DEFAULT "KEYCMD MODE TITLE - Uzbl browser <NAME> > SELECTED_URI"
#define TITLE_SHORT_DEFAULT "TITLE - Uzbl browser <NAME>"


enum {
  /* statusbar symbols */
  SYM_TITLE, SYM_URI, SYM_NAME,
  SYM_LOADPRGS, SYM_LOADPRGSBAR,
  SYM_KEYCMD, SYM_MODE, SYM_MSG,
  SYM_SELECTED_URI,
  /* useragent symbols */
  SYM_WK_MAJ, SYM_WK_MIN, SYM_WK_MIC,
  SYM_SYSNAME, SYM_NODENAME,
  SYM_KERNREL, SYM_KERNVER,
  SYM_ARCHSYS, SYM_ARCHUZBL,
  SYM_DOMAINNAME, SYM_COMMIT
};

const struct {
    gchar *symbol_name;
    guint symbol_token;
} symbols[] = {
    {"NAME",                 SYM_NAME},
    {"URI",                  SYM_URI},
    {"TITLE",                SYM_TITLE},
    {"SELECTED_URI",         SYM_SELECTED_URI},
    {"KEYCMD",               SYM_KEYCMD},
    {"MODE",                 SYM_MODE},
    {"MSG",                  SYM_MSG},
    {"LOAD_PROGRESS",        SYM_LOADPRGS},
    {"LOAD_PROGRESSBAR",     SYM_LOADPRGSBAR},

    {"WEBKIT_MAJOR",         SYM_WK_MAJ},
    {"WEBKIT_MINOR",         SYM_WK_MIN},
    {"WEBKIT_MICRO",         SYM_WK_MIC},
    {"SYSNAME",              SYM_SYSNAME},
    {"NODENAME",             SYM_NODENAME},
    {"KERNREL",              SYM_KERNREL},
    {"KERNVER",              SYM_KERNVER},
    {"ARCH_SYSTEM",          SYM_ARCHSYS},
    {"ARCH_UZBL",            SYM_ARCHUZBL},
    {"DOMAINNAME",           SYM_DOMAINNAME},
    {"COMMIT",               SYM_COMMIT},
    {NULL,                   0}
}, *symp = symbols;

/* status bar elements */
typedef struct {
    gint           load_progress;
    gchar          *msg;
} StatusBar;


/* gui elements */
typedef struct {
    GtkWidget*     main_window;
    GtkWidget*     scrolled_win;
    GtkWidget*     vbox;
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
    gchar          *fifo_path;
    gchar          *socket_path;
    /* stores (key)"variable name" -> (value)"pointer to this var*/
    GHashTable     *proto_var;
    /* command parsing regexes */
    GRegex         *set_regex;
    GRegex         *act_regex;
    GRegex         *keycmd_regex;
    GRegex         *get_regex;
    GRegex         *bind_regex;
} Communication;


/* internal state */
typedef struct {
    gchar    *uri;
    gchar    *config_file;
    char    *instance_name;
    gchar    config_file_path[500];
    gchar    selected_url[500];
    char     executable_path[500];
    GString* keycmd;
    gchar    searchtx[500];
    struct utsname unameinfo; /* system info */
    gboolean verbose;
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
    gchar*   load_finish_handler;
    gchar*   status_format;
    gchar*   title_format_short;
    gchar*   title_format_long;
    gchar*   status_background;
    gchar*   history_handler;
    gchar*   fifo_dir;
    gchar*   socket_dir;
    gchar*   download_handler;
    gchar*   cookie_handler;
    gboolean always_insert_mode;
    gboolean show_status;
    gboolean insert_mode;
    gboolean status_top;
    gboolean reset_command_mode;
    gchar*   modkey;
    guint    modmask;
    guint    http_debug;
    guint    default_font_size;
    guint    minimum_font_size;
    gchar*   shell_cmd;

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

typedef void sigfunc(int);

/* XDG Stuff */

typedef struct {
    gchar* environmental;
    gchar* default_value;
} XDG_Var;

XDG_Var XDG[] = 
{
    { "XDG_CONFIG_HOME", "~/.config" },
    { "XDG_DATA_HOME",   "~/.local/share" },
    { "XDG_CACHE_HOME",  "~/.cache" },
    { "XDG_CONFIG_DIRS", "/etc/xdg" },
    { "XDG_DATA_DIRS",   "/usr/local/share/:/usr/share/" },
};

/* Functions */
static void
setup_scanner();

char *
itos(int val);

static char *
str_replace (const char* search, const char* replace, const char* string);

static void
clean_up(void);

static void
catch_sigterm(int s);

static sigfunc *
setup_signal(int signe, sigfunc *shandler);

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
load_finish_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data);

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
run_command(const char *command, const char *args, const gboolean sync, char **stdout);

static void
spawn(WebKitWebView *web_view, const char *param);

static void
spawn_sh(WebKitWebView *web_view, const char *param);

static void
parse_command(const char *cmd, const char *param);

static void
runcmd(WebKitWebView *page, const char *param);

static void
parse_cmd_line(const char *ctl_line);

static gchar*
build_stream_name(int type, const gchar *dir);

static gboolean
var_is(const char *x, const char *y);

static gchar*
set_useragent(gchar *val);

static gboolean
control_fifo(GIOChannel *gio, GIOCondition condition);

static gchar*
init_fifo(gchar *dir);

static gboolean
control_stdin(GIOChannel *gio, GIOCondition condition);

static void
create_stdin();

static gchar*
init_socket(gchar *dir);

static gboolean
control_socket(GIOChannel *chan);

static void
update_title (void);

static gboolean
key_press_cb (WebKitWebView* page, GdkEventKey* event);

static void
run_keycmd(const gboolean key_ret);

static GtkWidget*
create_browser ();

static GtkWidget*
create_mainbar ();

static
GtkWidget* create_window ();

static void
add_binding (const gchar *key, const gchar *act);

static gchar*
get_xdg_var (XDG_Var xdg);

static gchar*
find_xdg_file (int xdg_type, char* filename);

static void
settings_init ();

static void
search_forward_text (WebKitWebView *page, const char *param);

static void
search_reverse_text (WebKitWebView *page, const char *param);

static void
run_js (WebKitWebView * web_view, const gchar *param);

static void handle_cookies (SoupSession *session,
							SoupMessage *msg,
							gpointer     user_data);
static void
save_cookies (SoupMessage *msg,
			  gpointer     user_data);
/* vi: set et ts=4: */
