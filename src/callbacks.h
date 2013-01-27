/*
 ** Callbacks
 ** (c) 2009 by Robert Manea et al.
*/

/*@null@*/ WebKitWebView*
create_web_view_cb (WebKitWebView  *web_view, WebKitWebFrame *frame, gpointer user_data);

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
