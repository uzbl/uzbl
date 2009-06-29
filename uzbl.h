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
    gchar          *progress_s, *progress_u;
    int            progress_w;
} StatusBar;


/* gui elements */
typedef struct {
    GtkWidget*     main_window;
    GtkPlug*       plug;
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
    gchar*         icon;

    /* WebInspector */
    GtkWidget *inspector_window;
    WebKitWebInspector *inspector;

    StatusBar sbar;
} GUI;


/* external communication*/
enum { FIFO, SOCKET};
typedef struct {
    gchar          *fifo_path;
    gchar          *socket_path;
    /* stores (key)"variable name" -> (value)"pointer to this var*/
    GHashTable     *proto_var;
    gchar          *sync_stdout;
} Communication;


/* internal state */
typedef struct {
    gchar    *uri;
    gchar    *config_file;
    int      socket_id;
    char     *instance_name;
    gchar    *selected_url;
    gchar    *executable_path;
    GString* keycmd;
    gchar*   searchtx;
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
    gchar*   load_start_handler;
    gchar*   load_commit_handler;
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
    gchar*   shell_cmd;
    /* WebKitWebSettings exports */
    guint    font_size;
    guint    monospace_size;
    guint    minimum_font_size;
    gfloat   zoom_level;
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
    guint    mode;  
    gchar*   base_url;
    gchar*   html_endmarker;
    gchar*   insert_indicator;
    gchar*   cmd_indicator;
    GString* html_buffer;
    guint    html_timeout;  

    /* command list: name -> Command  */
    GHashTable* commands;
} Behaviour;

/* javascript */
typedef struct {
    gboolean            initialized;
    JSClassDefinition   classdef;
    JSClassRef          classref;
} Javascript;

/* main uzbl data structure */
typedef struct {
    GUI           gui;
    State         state;
    Network       net;
    Behaviour     behave;
    Communication comm;
    Javascript    js;

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
gchar *
expand_template(const char *template, gboolean escape_markup);

void
setup_scanner();

char *
itos(int val);

char *
str_replace (const char* search, const char* replace, const char* string);

GArray*
read_file_by_line (gchar *path);

gchar*
parseenv (char* string);

void
clean_up(void);

void
catch_sigterm(int s);

sigfunc *
setup_signal(int signe, sigfunc *shandler);

gboolean
set_var_value(gchar *name, gchar *val);

static void
print(WebKitWebView *page, GArray *argv, GString *result);

gboolean
new_window_cb (WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action, WebKitWebPolicyDecision *policy_decision, gpointer user_data);

gboolean
mime_policy_cb(WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request, gchar *mime_type,  WebKitWebPolicyDecision *policy_decision, gpointer user_data);

WebKitWebView*
create_web_view_cb (WebKitWebView  *web_view, WebKitWebFrame *frame, gpointer user_data);

gboolean
download_cb (WebKitWebView *web_view, GObject *download, gpointer user_data);

static void
toggle_zoom_type (WebKitWebView* page, GArray *argv, GString *result);

static void
toggle_status_cb (WebKitWebView* page, GArray *argv, GString *result);

void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data);

void
title_change_cb (WebKitWebView* web_view, WebKitWebFrame* web_frame, const gchar* title, gpointer data);

void
progress_change_cb (WebKitWebView* page, gint progress, gpointer data);

void
load_commit_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data);

void
load_start_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data);

void
load_finish_cb (WebKitWebView* page, WebKitWebFrame* frame, gpointer data);

void
destroy_cb (GtkWidget* widget, gpointer data);

void
log_history_cb ();

void
commands_hash(void);

void
free_action(gpointer act);

Action*
new_action(const gchar *name, const gchar *param);

bool
file_exists (const char * filename);

static void
toggle_insert_mode(WebKitWebView *page, GArray *argv, GString *result);

static void
load_uri (WebKitWebView * web_view, GArray *argv, GString *result);

void
new_window_load_uri (const gchar * uri);

static void
chain (WebKitWebView *page, GArray *argv, GString *result);

static void
keycmd (WebKitWebView *page, GArray *argv, GString *result);

static void
keycmd_nl (WebKitWebView *page, GArray *argv, GString *result);

static void
keycmd_bs (WebKitWebView *page, GArray *argv, GString *result);

static void
close_uzbl (WebKitWebView *page, GArray *argv, GString *result);

gboolean
run_command(const gchar *command, const guint npre,
            const gchar **args, const gboolean sync, char **output_stdout);

static void
spawn(WebKitWebView *web_view, GArray *argv, GString *result);

static void
spawn_sh(WebKitWebView *web_view, GArray *argv, GString *result);

static void
spawn_sync(WebKitWebView *web_view, GArray *argv, GString *result);

static void
spawn_sh_sync(WebKitWebView *web_view, GArray *argv, GString *result);

static void
parse_command(const char *cmd, const char *param, GString *result);

static void
parse_cmd_line(const char *ctl_line, GString *result);

gchar*
build_stream_name(int type, const gchar *dir);

gboolean
control_fifo(GIOChannel *gio, GIOCondition condition);

gchar*
init_fifo(gchar *dir);

gboolean
control_stdin(GIOChannel *gio, GIOCondition condition);

void
create_stdin();

gchar*
init_socket(gchar *dir);

gboolean
control_socket(GIOChannel *chan);

static gboolean
control_client_socket(GIOChannel *chan);

static void
update_title (void);

gboolean
key_press_cb (GtkWidget* window, GdkEventKey* event);

void
run_keycmd(const gboolean key_ret);

void
exec_paramcmd(const Action* act, const guint i);

GtkWidget*
create_browser ();

GtkWidget*
create_mainbar ();

GtkWidget*
create_window ();

static
GtkPlug* create_plug ();

static void
run_handler (const gchar *act, const gchar *args);

void
add_binding (const gchar *key, const gchar *act);

gchar*
get_xdg_var (XDG_Var xdg);

gchar*
find_xdg_file (int xdg_type, char* filename);

void
settings_init ();

void
search_text (WebKitWebView *page, GArray *argv, const gboolean forward);

static void
search_forward_text (WebKitWebView *page, GArray *argv, GString *result);

static void
search_reverse_text (WebKitWebView *page, GArray *argv, GString *result);

static void
dehilight (WebKitWebView *page, GArray *argv, GString *result);

static void
run_js (WebKitWebView * web_view, GArray *argv, GString *result);

static void
run_external_js (WebKitWebView * web_view, GArray *argv, GString *result);

static void
eval_js(WebKitWebView * web_view, gchar *script, GString *result);

void handle_cookies (SoupSession *session,
                            SoupMessage *msg,
                            gpointer     user_data);
void
save_cookies (SoupMessage *msg,
                gpointer     user_data);

static void
set_var(WebKitWebView *page, GArray *argv, GString *result);

static void
act_bind(WebKitWebView *page, GArray *argv, GString *result);

void
act_dump_config();

void
render_html();

void
set_timeout(int seconds);

void
dump_var_hash(gpointer k, gpointer v, gpointer ud);

void
dump_key_hash(gpointer k, gpointer v, gpointer ud);

void
dump_config();

typedef void (*Command)(WebKitWebView*, GArray *argv, GString *result);
typedef struct {
    Command function;
    gboolean no_split;
} CommandInfo;

/* Command callbacks */
void
cmd_load_uri();

void
cmd_set_status();

void
set_proxy_url();

void
set_icon();

void
cmd_cookie_handler();

void
move_statusbar();

void
cmd_always_insert_mode();

void
cmd_http_debug();

void
cmd_max_conns();

void
cmd_max_conns_host();

/* exported WebKitWebSettings properties */

void
cmd_font_size();

void
cmd_zoom_level();

void
cmd_disable_plugins();

void
cmd_disable_scripts();

void
cmd_minimum_font_size();

void
cmd_fifo_dir();

void
cmd_socket_dir();

void
cmd_modkey();

void
cmd_useragent() ;

void
cmd_autoload_img();

void
cmd_autoshrink_img();

void
cmd_enable_spellcheck();

void
cmd_enable_private();

void
cmd_print_bg();

void 
cmd_style_uri();

void 
cmd_resizable_txt();

void 
cmd_default_encoding();

void 
cmd_enforce_96dpi();

void
cmd_inject_html();

void 
cmd_caret_browsing();

/* vi: set et ts=4: */
