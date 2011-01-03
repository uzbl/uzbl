/*
 ** Callbacks
 ** (c) 2009 by Robert Manea et al.
*/

void
cmd_load_uri();

void
cmd_set_status();

void
set_proxy_url();

void
set_authentication_handler();

void
set_status_background();

void
set_icon();

void
move_statusbar();

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
cmd_default_font_family();

void
cmd_monospace_font_family();

void
cmd_sans_serif_font_family();

void
cmd_serif_font_family();

void
cmd_cursive_font_family();

void
cmd_fantasy_font_family();

void
cmd_zoom_level();

void
cmd_set_zoom_type();

void
cmd_enable_pagecache();

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
cmd_useragent() ;

void
set_accept_languages();

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

void
cmd_javascript_windows();

void
cmd_set_geometry();

void
cmd_view_source();

void
cmd_scrollbars_visibility();

void
cmd_load_start();

WebKitWebSettings*
view_settings();

void
toggle_zoom_type (WebKitWebView* page, GArray *argv, GString *result);

void
toggle_status_cb (WebKitWebView* page, GArray *argv, GString *result);

void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data);

void
title_change_cb (WebKitWebView* web_view, GParamSpec param_spec);

void
progress_change_cb (WebKitWebView* web_view, GParamSpec param_spec);

void
load_status_change_cb (WebKitWebView* web_view, GParamSpec param_spec);

void
load_error_cb (WebKitWebView* page, WebKitWebFrame* frame, gchar *uri, gpointer web_err, gpointer ud);

void
selection_changed_cb(WebKitWebView *webkitwebview, gpointer ud);

void
destroy_cb (GtkWidget* widget, gpointer data);

gboolean
configure_event_cb(GtkWidget* window, GdkEventConfigure* event);

gboolean
key_press_cb (GtkWidget* window, GdkEventKey* event);

gboolean
key_release_cb (GtkWidget* window, GdkEventKey* event);

gboolean
motion_notify_cb(GtkWidget* window, GdkEventMotion* event, gpointer user_data);

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

void
populate_popup_cb(WebKitWebView *v, GtkMenu *m, void *c);

gboolean
button_press_cb (GtkWidget* window, GdkEventButton* event);

gboolean
button_release_cb (GtkWidget* window, GdkEventButton* event);

gboolean
focus_cb(GtkWidget* window, GdkEventFocus* event, void *ud);

gboolean
scroll_vert_cb(GtkAdjustment *adjust, void *w);

gboolean
scroll_horiz_cb(GtkAdjustment *adjust, void *w);

void
cmd_set_cookie_handler();
