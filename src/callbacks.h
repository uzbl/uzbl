/*
 ** Callbacks
 ** (c) 2009 by Robert Manea et al.
*/

gboolean
navigation_decision_cb (WebKitWebView *web_view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action,
        WebKitWebPolicyDecision *policy_decision, gpointer user_data);

gboolean
new_window_cb (WebKitWebView *web_view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action,
        WebKitWebPolicyDecision *policy_decision, gpointer user_data);

gboolean
mime_policy_cb(WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request,
        gchar *mime_type,  WebKitWebPolicyDecision *policy_decision, gpointer user_data);

void
request_starting_cb(WebKitWebView *web_view, WebKitWebFrame *frame, WebKitWebResource *resource,
        WebKitNetworkRequest *request, WebKitNetworkResponse *response, gpointer user_data);

/*@null@*/ WebKitWebView*
create_web_view_cb (WebKitWebView  *web_view, WebKitWebFrame *frame, gpointer user_data);

gboolean
download_cb (WebKitWebView *web_view, WebKitDownload *download, gpointer user_data);

#if WEBKIT_CHECK_VERSION (1, 9, 0)
gboolean
context_menu_cb (WebKitWebView *web_view, GtkWidget *default_menu, WebKitHitTestResult *hit_test_result, gboolean triggered_with_keyboard, gpointer user_data);
#else
void
populate_popup_cb(WebKitWebView *v, GtkMenu *m, void *c);
#endif

gboolean
focus_cb(GtkWidget* window, GdkEventFocus* event, void *ud);

gboolean
scroll_vert_cb(GtkAdjustment *adjust, void *w);

gboolean
scroll_horiz_cb(GtkAdjustment *adjust, void *w);

void
close_web_view_cb(WebKitWebView *webview, gpointer user_data);
