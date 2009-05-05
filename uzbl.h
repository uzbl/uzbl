typedef struct {
    char* name;
    char* param;
} Action;


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

static void
control_fifo(GIOChannel *fd);

static void
create_fifo();

static void
*control_socket();
 
static void
setup_threading ();

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

/* static void */
/* cookie_recieved_action (SoupCookieJar *jar, */
				   /* SoupCookie    *old_cookie, */
				   /* SoupCookie    *new_cookie, */
				   /* gpointer       user_data); */
/* static void */
/* catch_cookies (SoupSession *session, */
/* 			   SoupMessage *msg, */
/* 			   gpointer     user_data); */
/* 				/\* SoupSocket  *socket, *\/ */
/* 				/\* gpointer     user_data); *\/ */

static void handle_cookies (SoupSession *session,
							SoupMessage *msg,
							gpointer     user_data);
static void
save_cookies (SoupMessage *msg,
			  gpointer     user_data);
/* vi: set et ts=4: */
