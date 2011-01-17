/*
 ** Callbacks
 ** (c) 2009 by Robert Manea et al.
*/

#include "uzbl-core.h"
#include "callbacks.h"
#include "events.h"
#include "util.h"

void
set_proxy_url() {
    SoupURI *suri;

    if(uzbl.net.proxy_url == NULL || *uzbl.net.proxy_url == ' ') {
        soup_session_remove_feature_by_type(uzbl.net.soup_session,
                (GType) SOUP_SESSION_PROXY_URI);
    }
    else {
        suri = soup_uri_new(uzbl.net.proxy_url);
        g_object_set(G_OBJECT(uzbl.net.soup_session),
                SOUP_SESSION_PROXY_URI,
                suri, NULL);
        soup_uri_free(suri);
    }
    return;
}

void
set_authentication_handler() {
    /* Check if WEBKIT_TYPE_SOUP_AUTH_DIALOG feature is set */
    GSList *flist = soup_session_get_features (uzbl.net.soup_session, (GType) WEBKIT_TYPE_SOUP_AUTH_DIALOG);
    guint feature_is_set = g_slist_length(flist);
    g_slist_free(flist);

    if (uzbl.behave.authentication_handler == NULL || *uzbl.behave.authentication_handler == 0) {
        if (!feature_is_set)
            soup_session_add_feature_by_type
                (uzbl.net.soup_session, (GType) WEBKIT_TYPE_SOUP_AUTH_DIALOG);
    } else {
        if (feature_is_set)
            soup_session_remove_feature_by_type
                (uzbl.net.soup_session, (GType) WEBKIT_TYPE_SOUP_AUTH_DIALOG);
    }
    return;
}

void
set_status_background() {
    GdkColor color;
    gdk_color_parse (uzbl.behave.status_background, &color);
    /* labels and hboxes do not draw their own background. applying this
     * on the vbox/main_window is ok as the statusbar is the only affected
     * widget. (if not, we could also use GtkEventBox) */
    if (uzbl.gui.main_window)
        gtk_widget_modify_bg (uzbl.gui.main_window, GTK_STATE_NORMAL, &color);
    else if (uzbl.gui.plug)
        gtk_widget_modify_bg (GTK_WIDGET(uzbl.gui.plug), GTK_STATE_NORMAL, &color);
}

void
set_icon() {
    if(file_exists(uzbl.gui.icon)) {
        if (uzbl.gui.main_window)
            gtk_window_set_icon_from_file (GTK_WINDOW (uzbl.gui.main_window), uzbl.gui.icon, NULL);
    } else {
        g_printerr ("Icon \"%s\" not found. ignoring.\n", uzbl.gui.icon);
    }
}

void
cmd_set_geometry() {
    int ret=0, x=0, y=0;
    unsigned int w=0, h=0;
    if(uzbl.gui.geometry) {
        if(uzbl.gui.geometry[0] == 'm') { /* m/maximize/maximized */
            gtk_window_maximize((GtkWindow *)(uzbl.gui.main_window));
        } else {
            /* we used to use gtk_window_parse_geometry() but that didn't work how it was supposed to */
            ret = XParseGeometry(uzbl.gui.geometry, &x, &y, &w, &h);
            if(ret & XValue)
                gtk_window_move((GtkWindow *)uzbl.gui.main_window, x, y);
            if(ret & WidthValue)
                gtk_window_resize((GtkWindow *)uzbl.gui.main_window, w, h);
        }
    }

    /* update geometry var with the actual geometry
       this is necessary as some WMs don't seem to honour
       the above setting and we don't want to end up with
       wrong geometry information
    */
    retrieve_geometry();
}

void
cmd_set_status() {
    if (!uzbl.behave.show_status) {
        gtk_widget_hide(uzbl.gui.mainbar);
    } else {
        gtk_widget_show(uzbl.gui.mainbar);
    }
    update_title();
}

void
cmd_load_uri() {
	load_uri_imp (uzbl.state.uri);
}

void
cmd_max_conns() {
    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS, uzbl.net.max_conns, NULL);
}

void
cmd_max_conns_host() {
    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS_PER_HOST, uzbl.net.max_conns_host, NULL);
}

void
cmd_http_debug() {
    soup_session_remove_feature
        (uzbl.net.soup_session, SOUP_SESSION_FEATURE(uzbl.net.soup_logger));
    /* do we leak if this doesn't get freed? why does it occasionally crash if freed? */
    /*g_free(uzbl.net.soup_logger);*/

    uzbl.net.soup_logger = soup_logger_new(uzbl.behave.http_debug, -1);
    soup_session_add_feature(uzbl.net.soup_session,
            SOUP_SESSION_FEATURE(uzbl.net.soup_logger));
}

WebKitWebSettings*
view_settings() {
    return webkit_web_view_get_settings(uzbl.gui.web_view);
}

void
cmd_font_size() {
    WebKitWebSettings *ws = view_settings();
    if (uzbl.behave.font_size > 0) {
        g_object_set (G_OBJECT(ws), "default-font-size", uzbl.behave.font_size, NULL);
    }

    if (uzbl.behave.monospace_size > 0) {
        g_object_set (G_OBJECT(ws), "default-monospace-font-size",
                      uzbl.behave.monospace_size, NULL);
    } else {
        g_object_set (G_OBJECT(ws), "default-monospace-font-size",
                      uzbl.behave.font_size, NULL);
    }
}

void
cmd_default_font_family() {
    g_object_set (G_OBJECT(view_settings()), "default-font-family",
            uzbl.behave.default_font_family, NULL);
}

void
cmd_monospace_font_family() {
    g_object_set (G_OBJECT(view_settings()), "monospace-font-family",
            uzbl.behave.monospace_font_family, NULL);
}

void
cmd_sans_serif_font_family() {
    g_object_set (G_OBJECT(view_settings()), "sans_serif-font-family",
            uzbl.behave.sans_serif_font_family, NULL);
}

void
cmd_serif_font_family() {
    g_object_set (G_OBJECT(view_settings()), "serif-font-family",
            uzbl.behave.serif_font_family, NULL);
}

void
cmd_cursive_font_family() {
    g_object_set (G_OBJECT(view_settings()), "cursive-font-family",
            uzbl.behave.cursive_font_family, NULL);
}

void
cmd_fantasy_font_family() {
    g_object_set (G_OBJECT(view_settings()), "fantasy-font-family",
            uzbl.behave.fantasy_font_family, NULL);
}

void
cmd_zoom_level() {
    webkit_web_view_set_zoom_level (uzbl.gui.web_view, uzbl.behave.zoom_level);
}

void
cmd_enable_pagecache() {
    g_object_set (G_OBJECT(view_settings()), "enable-page-cache",
            uzbl.behave.enable_pagecache, NULL);
}

void
cmd_disable_plugins() {
    g_object_set (G_OBJECT(view_settings()), "enable-plugins",
            !uzbl.behave.disable_plugins, NULL);
}

void
cmd_disable_scripts() {
    g_object_set (G_OBJECT(view_settings()), "enable-scripts",
            !uzbl.behave.disable_scripts, NULL);
}

void
cmd_minimum_font_size() {
    g_object_set (G_OBJECT(view_settings()), "minimum-font-size",
            uzbl.behave.minimum_font_size, NULL);
}
void
cmd_autoload_img() {
    g_object_set (G_OBJECT(view_settings()), "auto-load-images",
            uzbl.behave.autoload_img, NULL);
}


void
cmd_autoshrink_img() {
    g_object_set (G_OBJECT(view_settings()), "auto-shrink-images",
            uzbl.behave.autoshrink_img, NULL);
}


void
cmd_enable_spellcheck() {
    g_object_set (G_OBJECT(view_settings()), "enable-spell-checking",
            uzbl.behave.enable_spellcheck, NULL);
}

void
cmd_enable_private() {
    g_object_set (G_OBJECT(view_settings()), "enable-private-browsing",
            uzbl.behave.enable_private, NULL);
}

void
cmd_print_bg() {
    g_object_set (G_OBJECT(view_settings()), "print-backgrounds",
            uzbl.behave.print_bg, NULL);
}

void
cmd_style_uri() {
    g_object_set (G_OBJECT(view_settings()), "user-stylesheet-uri",
            uzbl.behave.style_uri, NULL);
}

void
cmd_resizable_txt() {
    g_object_set (G_OBJECT(view_settings()), "resizable-text-areas",
            uzbl.behave.resizable_txt, NULL);
}

void
cmd_default_encoding() {
    g_object_set (G_OBJECT(view_settings()), "default-encoding",
            uzbl.behave.default_encoding, NULL);
}

void
cmd_enforce_96dpi() {
    g_object_set (G_OBJECT(view_settings()), "enforce-96-dpi",
            uzbl.behave.enforce_96dpi, NULL);
}

void
cmd_caret_browsing() {
    g_object_set (G_OBJECT(view_settings()), "enable-caret-browsing",
            uzbl.behave.caret_browsing, NULL);
}

void
cmd_fifo_dir() {
    uzbl.behave.fifo_dir = init_fifo(uzbl.behave.fifo_dir);
}

void
cmd_socket_dir() {
    uzbl.behave.socket_dir = init_socket(uzbl.behave.socket_dir);
}

void
cmd_inject_html() {
    if(uzbl.behave.inject_html) {
        webkit_web_view_load_html_string (uzbl.gui.web_view,
                uzbl.behave.inject_html, NULL);
    }
}

void
cmd_useragent() {
    if (*uzbl.net.useragent == ' ') {
        g_free (uzbl.net.useragent);
        uzbl.net.useragent = NULL;
    } else {
        g_object_set(G_OBJECT(uzbl.net.soup_session), SOUP_SESSION_USER_AGENT,
            uzbl.net.useragent, NULL);
    }
}

void
set_accept_languages() {
    if (*uzbl.net.accept_languages == ' ') {
        g_free (uzbl.net.accept_languages);
        uzbl.net.accept_languages = NULL;
    } else {
        g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_ACCEPT_LANGUAGE, uzbl.net.accept_languages, NULL);
    }
}

void
cmd_javascript_windows() {
    g_object_set (G_OBJECT(view_settings()), "javascript-can-open-windows-automatically",
            uzbl.behave.javascript_windows, NULL);
}

void
cmd_scrollbars_visibility() {
    if(uzbl.gui.scrollbars_visible) {
        uzbl.gui.bar_h = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (uzbl.gui.scrolled_win));
        uzbl.gui.bar_v = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (uzbl.gui.scrolled_win));
    }
    else {
        uzbl.gui.bar_v = gtk_range_get_adjustment (GTK_RANGE (uzbl.gui.scbar_v));
        uzbl.gui.bar_h = gtk_range_get_adjustment (GTK_RANGE (uzbl.gui.scbar_h));
    }

    set_webview_scroll_adjustments();
}

/* requires webkit >=1.1.14 */
void
cmd_view_source() {
    webkit_web_view_set_view_source_mode(uzbl.gui.web_view,
            (gboolean) uzbl.behave.view_source);
}

void
cmd_set_zoom_type () {
    if(uzbl.behave.zoom_type)
        webkit_web_view_set_full_content_zoom (uzbl.gui.web_view, TRUE);
    else
        webkit_web_view_set_full_content_zoom (uzbl.gui.web_view, FALSE);
}

void
toggle_zoom_type (WebKitWebView* page, GArray *argv, GString *result) {
    (void)argv;
    (void)result;

    webkit_web_view_set_full_content_zoom (page, !webkit_web_view_get_full_content_zoom (page));
}

void
toggle_status_cb (WebKitWebView* page, GArray *argv, GString *result) {
    (void)page;
    (void)argv;
    (void)result;

    if (uzbl.behave.show_status) {
        gtk_widget_hide(uzbl.gui.mainbar);
    } else {
        gtk_widget_show(uzbl.gui.mainbar);
    }
    uzbl.behave.show_status = !uzbl.behave.show_status;
    update_title();
}

void
link_hover_cb (WebKitWebView* page, const gchar* title, const gchar* link, gpointer data) {
    (void) page;
    (void) title;
    (void) data;
    State *s = &uzbl.state;

    if(s->selected_url) {
        if(s->last_selected_url)
            g_free(s->last_selected_url);
        s->last_selected_url = g_strdup(s->selected_url);
    }
    else {
        if(s->last_selected_url) g_free(s->last_selected_url);
        s->last_selected_url = NULL;
    }

    g_free(s->selected_url);
    s->selected_url = NULL;

    if (link) {
        s->selected_url = g_strdup(link);

        if(s->last_selected_url &&
           g_strcmp0(s->selected_url, s->last_selected_url))
            send_event(LINK_UNHOVER, s->last_selected_url, NULL);

        send_event(LINK_HOVER, s->selected_url, NULL);
    }
    else if(s->last_selected_url) {
            send_event(LINK_UNHOVER, s->last_selected_url, NULL);
    }

    update_title();
}

void
title_change_cb (WebKitWebView* web_view, GParamSpec param_spec) {
    (void) web_view;
    (void) param_spec;
    const gchar *title = webkit_web_view_get_title(web_view);
    if (uzbl.gui.main_title)
        g_free (uzbl.gui.main_title);
    uzbl.gui.main_title = title ? g_strdup (title) : g_strdup ("(no title)");
    update_title();
    send_event(TITLE_CHANGED, uzbl.gui.main_title, NULL);
    g_setenv("UZBL_TITLE", uzbl.gui.main_title, TRUE);
}

void
progress_change_cb (WebKitWebView* web_view, GParamSpec param_spec) {
    (void) param_spec;
    int progress = webkit_web_view_get_progress(web_view) * 100;
    gchar *prg_str = itos(progress);
    send_event(LOAD_PROGRESS, prg_str, NULL);
    g_free(prg_str);
}

void
load_status_change_cb (WebKitWebView* web_view, GParamSpec param_spec) {
    (void) param_spec;

    WebKitWebFrame *frame = webkit_web_view_get_main_frame(web_view);
    WebKitLoadStatus status = webkit_web_view_get_load_status(web_view);
    switch(status) {
        case WEBKIT_LOAD_PROVISIONAL:
            send_event(LOAD_START, uzbl.state.uri, NULL);
            break;
        case WEBKIT_LOAD_COMMITTED:
            g_free (uzbl.state.uri);
            GString* newuri = g_string_new (webkit_web_frame_get_uri (frame));
            uzbl.state.uri = g_string_free (newuri, FALSE);
            g_setenv("UZBL_URI", uzbl.state.uri, TRUE);

            send_event(LOAD_COMMIT, webkit_web_frame_get_uri (frame), NULL);
            break;
        case WEBKIT_LOAD_FINISHED:
            send_event(LOAD_FINISH, webkit_web_frame_get_uri(frame), NULL);
            break;
        case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
            break; /* we don't do anything with this (yet) */
        case WEBKIT_LOAD_FAILED:
            break; /* load_error_cb will handle this case */
    }

}

void
selection_changed_cb(WebKitWebView *webkitwebview, gpointer ud) {
    (void)ud;
    gchar *tmp;

    webkit_web_view_copy_clipboard(webkitwebview);
    tmp = gtk_clipboard_wait_for_text(gtk_clipboard_get (GDK_SELECTION_CLIPBOARD));
    send_event(SELECTION_CHANGED, tmp, NULL);
    g_free(tmp);
}

void
load_error_cb (WebKitWebView* page, WebKitWebFrame* frame, gchar *uri, gpointer web_err, gpointer ud) {
    (void) page;
    (void) frame;
    (void) ud;
    GError *err = web_err;
    gchar *details;

    details = g_strdup_printf("%s %d:%s", uri, err->code, err->message);
    send_event(LOAD_ERROR, details, NULL);
    g_free(details);
}

void
destroy_cb (GtkWidget* widget, gpointer data) {
    (void) widget;
    (void) data;
    gtk_main_quit ();
}

gboolean
configure_event_cb(GtkWidget* window, GdkEventConfigure* event) {
    (void) window;
    (void) event;
    gchar *lastgeo = NULL;

    lastgeo = g_strdup(uzbl.gui.geometry);
    retrieve_geometry();

    if(strcmp(lastgeo, uzbl.gui.geometry))
        send_event(GEOMETRY_CHANGED, uzbl.gui.geometry, NULL);
    g_free(lastgeo);

    return FALSE;
}

gboolean
focus_cb(GtkWidget* window, GdkEventFocus* event, void *ud) {
    (void) window;
    (void) event;
    (void) ud;

    if(event->in)
        send_event(FOCUS_GAINED, "", NULL);
    else
        send_event(FOCUS_LOST, "", NULL);

    return FALSE;
}

gboolean
key_press_cb (GtkWidget* window, GdkEventKey* event) {
    (void) window;

    if(event->type == GDK_KEY_PRESS)
        key_to_event(event->keyval, GDK_KEY_PRESS);

    return uzbl.behave.forward_keys ? FALSE : TRUE;
}

gboolean
key_release_cb (GtkWidget* window, GdkEventKey* event) {
    (void) window;

    if(event->type == GDK_KEY_RELEASE)
        key_to_event(event->keyval, GDK_KEY_RELEASE);

    return uzbl.behave.forward_keys ? FALSE : TRUE;
}

gboolean
button_press_cb (GtkWidget* window, GdkEventButton* event) {
    (void) window;
    gint context;
    gchar *details;
    gboolean propagate = FALSE,
             sendev    = FALSE;

    if(event->type == GDK_BUTTON_PRESS) {
        if(uzbl.state.last_button)
            gdk_event_free((GdkEvent *)uzbl.state.last_button);
        uzbl.state.last_button = (GdkEventButton *)gdk_event_copy((GdkEvent *)event);

        context = get_click_context(NULL);
        /* left click */
        if(event->button == 1) {
            if((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE))
                send_event(FORM_ACTIVE, "button1", NULL);
            else if((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT))
                send_event(ROOT_ACTIVE, "button1", NULL);
        }
        else if(event->button == 2 && !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)) {
            sendev    = TRUE;
            propagate = TRUE;
        }
        else if(event->button > 3) {
            sendev    = TRUE;
            propagate = TRUE;
        }

        if(sendev) {
            details = g_strdup_printf("Button%d", event->button);
            send_event(KEY_PRESS, details, NULL);
            g_free(details);
        }
    }

    return propagate;
}

gboolean
button_release_cb (GtkWidget* window, GdkEventButton* event) {
    (void) window;
    gint context;
    gchar *details;
    gboolean propagate = FALSE,
             sendev    = FALSE;

    context = get_click_context(NULL);
    if(event->type == GDK_BUTTON_RELEASE) {
        if(event->button == 2 && !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)) {
            sendev    = TRUE;
            propagate = TRUE;
        }
        else if(event->button > 3) {
            sendev    = TRUE;
            propagate = TRUE;
        }

        if(sendev) {
            details = g_strdup_printf("Button%d", event->button);
            send_event(KEY_RELEASE, details, NULL);
            g_free(details);
        }
    }

    return propagate;
}

gboolean
motion_notify_cb(GtkWidget* window, GdkEventMotion* event, gpointer user_data) {
    (void) window;
    (void) event;
    (void) user_data;

    gchar *details = g_strdup_printf("%.0lf %.0lf %u", event->x, event->y, event->state);
    send_event(PTR_MOVE, details, NULL);
    g_free(details);

    return FALSE;
}

gboolean
navigation_decision_cb (WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action, WebKitWebPolicyDecision *policy_decision, gpointer user_data) {
    (void) web_view;
    (void) frame;
    (void) navigation_action;
    (void) user_data;

    const gchar* uri = webkit_network_request_get_uri (request);
    gboolean decision_made = FALSE;

    if (uzbl.state.verbose)
        printf("Navigation requested -> %s\n", uri);

    if (uzbl.behave.scheme_handler) {
        GString *s = g_string_new ("");
        g_string_printf(s, "'%s'", uri);

        run_handler(uzbl.behave.scheme_handler, s->str);

        if(uzbl.comm.sync_stdout && strcmp (uzbl.comm.sync_stdout, "") != 0) {
            char *p = strchr(uzbl.comm.sync_stdout, '\n' );
            if ( p != NULL ) *p = '\0';
            if (!strcmp(uzbl.comm.sync_stdout, "USED")) {
                webkit_web_policy_decision_ignore(policy_decision);
                decision_made = TRUE;
            }
        }
        if (uzbl.comm.sync_stdout)
            uzbl.comm.sync_stdout = strfree(uzbl.comm.sync_stdout);

        g_string_free(s, TRUE);
    }
    if (!decision_made)
        webkit_web_policy_decision_use(policy_decision);

    return TRUE;
}

gboolean
new_window_cb (WebKitWebView *web_view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action,
        WebKitWebPolicyDecision *policy_decision, gpointer user_data) {
    (void) web_view;
    (void) frame;
    (void) navigation_action;
    (void) policy_decision;
    (void) user_data;

    if (uzbl.state.verbose)
        printf("New window requested -> %s \n", webkit_network_request_get_uri (request));

    /* This event function causes troubles with `target="_blank"` anchors.
     * Either we:
     *  1. Comment it out and target blank links are ignored.
     *  2. Uncomment it and two windows are opened when you click on target
     *     blank links.
     *
     * This problem is caused by create_web_view_cb also being called whenever
     * this callback is triggered thus resulting in the doubled events.
     *
     * We are leaving this uncommented as we would rather links open twice
     * than not at all.
     */
    send_event(NEW_WINDOW, webkit_network_request_get_uri (request), NULL);

    webkit_web_policy_decision_ignore(policy_decision);
    return TRUE;
}

gboolean
mime_policy_cb(WebKitWebView *web_view, WebKitWebFrame *frame, WebKitNetworkRequest *request, gchar *mime_type,  WebKitWebPolicyDecision *policy_decision, gpointer user_data) {
    (void) frame;
    (void) request;
    (void) user_data;

    /* If we can display it, let's display it... */
    if (webkit_web_view_can_show_mime_type (web_view, mime_type)) {
        webkit_web_policy_decision_use (policy_decision);
        return TRUE;
    }

    /* ...everything we can't display is downloaded */
    webkit_web_policy_decision_download (policy_decision);
    return TRUE;
}

void
request_starting_cb(WebKitWebView *web_view, WebKitWebFrame *frame, WebKitWebResource *resource,
        WebKitNetworkRequest *request, WebKitNetworkResponse *response, gpointer user_data) {
    (void) web_view;
    (void) frame;
    (void) resource;
    (void) response;
    (void) user_data;

    send_event(REQUEST_STARTING, webkit_network_request_get_uri(request), NULL);
}

void
create_web_view_js2_cb (WebKitWebView* web_view, GParamSpec param_spec) {
    (void) web_view;
    (void) param_spec;

    const gchar* uri = webkit_web_view_get_uri(web_view);

    if (strncmp(uri, "javascript:", strlen("javascript:")) == 0) {
        eval_js(uzbl.gui.web_view, (gchar*) uri + strlen("javascript:"), NULL, "javascript:");
        gtk_widget_destroy(GTK_WIDGET(web_view));
    }
    else
        send_event(NEW_WINDOW, uri, NULL);
}


gboolean
create_web_view_js_cb (WebKitWebView* web_view, gpointer user_data) {
    (void) web_view;
    (void) user_data;

    g_object_connect (web_view, "signal::notify::uri",
                            G_CALLBACK(create_web_view_js2_cb), NULL, NULL);
    return TRUE;
}


/*@null@*/ WebKitWebView*
create_web_view_cb (WebKitWebView  *web_view, WebKitWebFrame *frame, gpointer user_data) {
    (void) web_view;
    (void) frame;
    (void) user_data;

    if (uzbl.state.verbose)
        printf("New web view -> javascript link...\n");

    WebKitWebView* new_view = WEBKIT_WEB_VIEW(webkit_web_view_new());

    g_signal_connect (new_view, "web-view-ready",
                        G_CALLBACK(create_web_view_js_cb), NULL);
    return new_view;
}

void
download_progress_cb(WebKitDownload *download, GParamSpec *pspec, gpointer user_data) {
    (void) pspec; (void) user_data;

    gdouble progress;
    g_object_get(download, "progress", &progress, NULL);

    const gchar *dest_uri = webkit_download_get_destination_uri(download);
    const gchar *dest_path = dest_uri + strlen("file://");

    gchar *details = g_strdup_printf("%s %.2lf", dest_path, progress);
    send_event(DOWNLOAD_PROGRESS, details, NULL);
    g_free(details);
}

void
download_status_cb(WebKitDownload *download, GParamSpec *pspec, gpointer user_data) {
    (void) pspec; (void) user_data;

    WebKitDownloadStatus status;
    g_object_get(download, "status", &status, NULL);

    switch(status) {
        case WEBKIT_DOWNLOAD_STATUS_CREATED:
        case WEBKIT_DOWNLOAD_STATUS_STARTED:
        case WEBKIT_DOWNLOAD_STATUS_ERROR:
        case WEBKIT_DOWNLOAD_STATUS_CANCELLED:
            return; /* these are irrelevant */
        case WEBKIT_DOWNLOAD_STATUS_FINISHED:
        {
            const gchar *dest_uri = webkit_download_get_destination_uri(download);
            const gchar *dest_path = dest_uri + strlen("file://");
            send_event(DOWNLOAD_COMPLETE, dest_path, NULL);
        }
    }
}

gboolean
download_cb(WebKitWebView *web_view, WebKitDownload *download, gpointer user_data) {
    (void) web_view; (void) user_data;

    /* get the URI being downloaded */
    const gchar *uri = webkit_download_get_uri(download);

    if (uzbl.state.verbose)
        printf("Download requested -> %s\n", uri);

    if (!uzbl.behave.download_handler) {
        webkit_download_cancel(download);
        return FALSE; /* reject downloads when there's no download handler */
    }

    /* get a reasonable suggestion for a filename */
    const gchar *suggested_filename;
    g_object_get(download, "suggested-filename", &suggested_filename, NULL);

    /* get the mimetype of the download */
    const gchar *content_type = NULL;
    WebKitNetworkResponse *r  = webkit_download_get_network_response(download);
    /* downloads can be initiated from the context menu, in that case there is
       no network response yet and trying to get one would crash. */
    if(WEBKIT_IS_NETWORK_RESPONSE(r)) {
        SoupMessage        *m = webkit_network_response_get_message(r);
        SoupMessageHeaders *h = NULL;
        g_object_get(m, "response-headers", &h, NULL);
        if(h) /* some versions of libsoup don't have "response-headers" here */
            content_type = soup_message_headers_get_one(h, "Content-Type");
    }

    if(!content_type)
        content_type = "application/octet-stream";

    /* get the filesize of the download, as given by the server.
       (this may be inaccurate, there's nothing we can do about that.)  */
    unsigned int total_size = webkit_download_get_total_size(download);

    gchar *ev = g_strdup_printf("'%s' '%s' '%s' %d", uri, suggested_filename,
                                                     content_type, total_size);
    run_handler(uzbl.behave.download_handler, ev);
    g_free(ev);

    /* no response, cancel the download */
    if(!uzbl.comm.sync_stdout) {
        webkit_download_cancel(download);
        return FALSE;
    }

    /* no response, cancel the download */
    if(uzbl.comm.sync_stdout[0] == 0) {
        webkit_download_cancel(download);
        uzbl.comm.sync_stdout = strfree(uzbl.comm.sync_stdout);
        return FALSE;
    }

    /* we got a response, it's the path we should download the file to */
    gchar *destination_path = uzbl.comm.sync_stdout;
    uzbl.comm.sync_stdout = NULL;

    /* presumably people don't need newlines in their filenames. */
    char *p = strchr(destination_path, '\n');
    if ( p != NULL ) *p = '\0';

    /* set up progress callbacks */
    g_signal_connect(download, "notify::status",   G_CALLBACK(download_status_cb),   NULL);
    g_signal_connect(download, "notify::progress", G_CALLBACK(download_progress_cb), NULL);

    /* convert relative path to absolute path */
    if(destination_path[0] != '/') {
        gchar *rel_path = destination_path;
        gchar *cwd = g_get_current_dir();
        destination_path = g_strconcat(cwd, "/", destination_path, NULL);
        g_free(cwd);
        g_free(rel_path);
    }

    send_event(DOWNLOAD_STARTED, destination_path, NULL);

    /* convert absolute path to file:// URI */
    gchar *destination_uri = g_strconcat("file://", destination_path, NULL);
    g_free(destination_path);

    webkit_download_set_destination_uri(download, destination_uri);
    g_free(destination_uri);

    return TRUE;
}

gboolean
scroll_vert_cb(GtkAdjustment *adjust, void *w)
{
    (void) w;

    gdouble value = gtk_adjustment_get_value(adjust);
    gdouble min = gtk_adjustment_get_lower(adjust);
    gdouble max = gtk_adjustment_get_upper(adjust);
    gdouble page = gtk_adjustment_get_page_size(adjust);
    gchar* details;
    details = g_strdup_printf("%g %g %g %g", value, min, max, page);

    send_event(SCROLL_VERT, details, NULL);

    g_free(details);

    return (FALSE);
}

gboolean
scroll_horiz_cb(GtkAdjustment *adjust, void *w)
{
    (void) w;

    gdouble value = gtk_adjustment_get_value(adjust);
    gdouble min = gtk_adjustment_get_lower(adjust);
    gdouble max = gtk_adjustment_get_upper(adjust);
    gdouble page = gtk_adjustment_get_page_size(adjust);
    gchar* details;
    details = g_strdup_printf("%g %g %g %g", value, min, max, page);

    send_event(SCROLL_HORIZ, details, NULL);

    g_free(details);

    return (FALSE);
}

void
run_menu_command(GtkWidget *menu, const char *line) {
    (void) menu;

    parse_cmd_line(line, NULL);
}


void
populate_popup_cb(WebKitWebView *v, GtkMenu *m, void *c) {
    (void) v;
    (void) c;
    GUI *g = &uzbl.gui;
    GtkWidget *item;
    MenuItem *mi;
    guint i=0;
    gint context, hit=0;

    if(!g->menu_items)
        return;

    /* check context */
    if((context = get_click_context(NULL)) == -1)
        return;


    for(i=0; i < uzbl.gui.menu_items->len; i++) {
        hit = 0;
        mi = g_ptr_array_index(uzbl.gui.menu_items, i);

        if((mi->context > WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT) &&
                (context & mi->context)) {
            if(mi->issep) {
                item = gtk_separator_menu_item_new();
                gtk_menu_shell_append(GTK_MENU_SHELL(m), item);
                gtk_widget_show(item);
            }
            else {
                item = gtk_menu_item_new_with_label(mi->name);
                g_signal_connect(item, "activate",
                        G_CALLBACK(run_menu_command), mi->cmd);
                gtk_menu_shell_append(GTK_MENU_SHELL(m), item);
                gtk_widget_show(item);
            }
            hit++;
        }

        if((mi->context == WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT)  &&
                (context <= WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT) &&
                !hit) {
            if(mi->issep) {
                item = gtk_separator_menu_item_new();
                gtk_menu_shell_append(GTK_MENU_SHELL(m), item);
                gtk_widget_show(item);
            }
            else {
                item = gtk_menu_item_new_with_label(mi->name);
                g_signal_connect(item, "activate",
                        G_CALLBACK(run_menu_command), mi->cmd);
                gtk_menu_shell_append(GTK_MENU_SHELL(m), item);
                gtk_widget_show(item);
            }
        }
    }
}

void
cmd_set_cookie_handler() {
  if(uzbl.behave.cookie_handler[0] == 0) {
      g_free(uzbl.behave.cookie_handler);
      uzbl.behave.cookie_handler = NULL;
  }

  uzbl_cookie_jar_set_handler(uzbl.net.soup_cookie_jar, uzbl.behave.cookie_handler);
}
