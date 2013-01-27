/*
 ** Callbacks
 ** (c) 2009 by Robert Manea et al.
*/

#include "uzbl-core.h"
#include "callbacks.h"
#include "events.h"
#include "menu.h"
#include "type.h"
#include "variables.h"

#include <gdk/gdk.h>

void
link_hover_cb (WebKitWebView *page, const gchar *title, const gchar *link, gpointer data) {
    (void) page; (void) title; (void) data;
    State *s = &uzbl.state;

    if(s->last_selected_url)
        g_free(s->last_selected_url);

    if(s->selected_url) {
        s->last_selected_url = g_strdup(s->selected_url);
        g_free(s->selected_url);
        s->selected_url = NULL;
    } else
        s->last_selected_url = NULL;

    if(s->last_selected_url && g_strcmp0(link, s->last_selected_url))
        send_event(LINK_UNHOVER, NULL, TYPE_STR, s->last_selected_url, NULL);

    if (link) {
        s->selected_url = g_strdup(link);
        send_event(LINK_HOVER,   NULL, TYPE_STR, s->selected_url, NULL);
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
    send_event(TITLE_CHANGED, NULL, TYPE_STR, uzbl.gui.main_title, NULL);
    g_setenv("UZBL_TITLE", uzbl.gui.main_title, TRUE);
}

void
progress_change_cb (WebKitWebView* web_view, GParamSpec param_spec) {
    (void) param_spec;
    int progress = webkit_web_view_get_progress(web_view) * 100;
    send_event(LOAD_PROGRESS, NULL, TYPE_INT, progress, NULL);
}

void
load_status_change_cb (WebKitWebView* web_view, GParamSpec param_spec) {
    (void) param_spec;

    WebKitWebFrame  *frame;
    WebKitLoadStatus status = webkit_web_view_get_load_status(web_view);
    switch(status) {
        case WEBKIT_LOAD_PROVISIONAL:
            send_event(LOAD_START,  NULL, TYPE_STR, uzbl.state.uri ? uzbl.state.uri : "", NULL);
            break;
        case WEBKIT_LOAD_COMMITTED:
            frame = webkit_web_view_get_main_frame(web_view);
            send_event(LOAD_COMMIT, NULL, TYPE_STR, webkit_web_frame_get_uri (frame), NULL);
            break;
        case WEBKIT_LOAD_FINISHED:
            send_event(LOAD_FINISH, NULL, TYPE_STR, uzbl.state.uri, NULL);
            break;
        case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
            break; /* we don't do anything with this (yet) */
        case WEBKIT_LOAD_FAILED:
            break; /* load_error_cb will handle this case */
    }
}

gboolean
load_error_cb (WebKitWebView* page, WebKitWebFrame* frame, gchar *uri, gpointer web_err, gpointer ud) {
    (void) page; (void) frame; (void) ud;
    GError *err = web_err;

    send_event (LOAD_ERROR, NULL,
        TYPE_STR, uri,
        TYPE_INT, err->code,
        TYPE_STR, err->message,
        NULL);

    return FALSE;
}

gboolean
focus_cb(GtkWidget* window, GdkEventFocus* event, void *ud) {
    (void) window;
    (void) event;
    (void) ud;

    send_event (event->in?FOCUS_GAINED:FOCUS_LOST, NULL, NULL);

    return FALSE;
}

gint
get_click_context() {
    WebKitHitTestResult *ht;
    guint context;

    if(!uzbl.state.last_button)
        return -1;

    ht = webkit_web_view_get_hit_test_result (uzbl.gui.web_view, uzbl.state.last_button);
    g_object_get (ht, "context", &context, NULL);
    g_object_unref (ht);

    return (gint)context;
}

gboolean
button_press_cb (GtkWidget* window, GdkEventButton* event) {
    (void) window;
    gint context;
    gboolean propagate = FALSE,
             sendev    = FALSE;

    // Save last button click for use in menu
    if(uzbl.state.last_button)
        gdk_event_free((GdkEvent *)uzbl.state.last_button);
    uzbl.state.last_button = (GdkEventButton *)gdk_event_copy((GdkEvent *)event);

    // Grab context from last click
    context = get_click_context();

    if(event->type == GDK_BUTTON_PRESS) {
        /* left click */
        if(event->button == 1) {
            if((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE))
                send_event(FORM_ACTIVE, NULL, TYPE_NAME, "button1", NULL);
            else if((context & WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT))
                send_event(ROOT_ACTIVE, NULL, TYPE_NAME, "button1", NULL);
            else {
                sendev    = TRUE;
                propagate = TRUE;
            }
        }
        else if(event->button == 2 && !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)) {
            sendev    = TRUE;
            propagate = TRUE;
        }
        else if(event->button > 3) {
            sendev    = TRUE;
            propagate = TRUE;
        }
    }

    if(event->type == GDK_2BUTTON_PRESS || event->type == GDK_3BUTTON_PRESS) {
        if(event->button == 1 && !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE) && (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT)) {
            sendev    = TRUE;
            propagate = uzbl.state.handle_multi_button;
        }
        else if(event->button == 2 && !(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE)) {
            sendev    = TRUE;
            propagate = uzbl.state.handle_multi_button;
        }
        else if(event->button >= 3) {
            sendev    = TRUE;
            propagate = uzbl.state.handle_multi_button;
        }
    }

    if(sendev) {
        button_to_event(event->button, event->state, event->type);
    }

    return propagate;
}

gboolean
button_release_cb (GtkWidget* window, GdkEventButton* event) {
    (void) window;
    gint context;
    gboolean propagate = FALSE,
             sendev    = FALSE;

    context = get_click_context();
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
            button_to_event(event->button, event->state, GDK_BUTTON_RELEASE);
        }
    }

    return propagate;
}

gboolean
navigation_decision_cb (WebKitWebView *web_view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action,
        WebKitWebPolicyDecision *policy_decision, gpointer user_data) {
    (void) web_view;
    (void) frame;
    (void) navigation_action;
    (void) user_data;

    const gchar* uri = webkit_network_request_get_uri (request);
    gboolean decision_made = FALSE;

    if (uzbl.state.verbose)
        printf("Navigation requested -> %s\n", uri);

    if (uzbl.behave.scheme_handler) {
        GString *result = g_string_new ("");
        GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
        const CommandInfo *c = parse_command_parts(uzbl.behave.scheme_handler, a);

        if(c) {
            g_array_append_val(a, uri);
            run_parsed_command(c, a, result);
        }
        g_array_free(a, TRUE);

        if(result->len > 0) {
            char *p = strchr(result->str, '\n' );
            if ( p != NULL ) *p = '\0';
            if (!strcmp(result->str, "USED")) {
                webkit_web_policy_decision_ignore(policy_decision);
                decision_made = TRUE;
            }
        }

        g_string_free(result, TRUE);
    }
    if (!decision_made)
        webkit_web_policy_decision_use(policy_decision);

    return TRUE;
}

void
close_web_view_cb(WebKitWebView *webview, gpointer user_data) {
    (void) webview; (void) user_data;
    send_event (CLOSE_WINDOW, NULL, NULL);
}

gboolean
mime_policy_cb (WebKitWebView *web_view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, gchar *mime_type,
        WebKitWebPolicyDecision *policy_decision, gpointer user_data) {
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

    const gchar *uri = webkit_network_request_get_uri (request);
    SoupMessage *message = webkit_network_request_get_message (request);

    if (message) {
        SoupURI *soup_uri = soup_uri_new (uri);
        soup_message_set_first_party (message, soup_uri);
    }

    if (uzbl.state.verbose)
        printf("Request starting -> %s\n", uri);
    send_event (REQUEST_STARTING, NULL, TYPE_STR, uri, NULL);

    if (uzbl.behave.request_handler) {
        GString *result = g_string_new ("");
        GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
        const CommandInfo *c = parse_command_parts(uzbl.behave.request_handler, a);

        if(c) {
            g_array_append_val(a, uri);
            run_parsed_command(c, a, result);
        }
        g_array_free(a, TRUE);

        if(result->len > 0) {
            char *p = strchr(result->str, '\n' );
            if ( p != NULL ) *p = '\0';
            webkit_network_request_set_uri(request, result->str);
        }

        g_string_free(result, TRUE);
    }
}

void
create_web_view_js_cb (WebKitWebView* web_view, GParamSpec param_spec) {
    (void) web_view;
    (void) param_spec;

    webkit_web_view_stop_loading(web_view);
    const gchar* uri = webkit_web_view_get_uri(web_view);

    if (strncmp(uri, "javascript:", strlen("javascript:")) == 0)
        eval_js(uzbl.gui.web_view, (gchar*) uri + strlen("javascript:"), NULL, "javascript:");
    else
        send_event(NEW_WINDOW, NULL, TYPE_STR, uri, NULL);

    gtk_widget_destroy(GTK_WIDGET(web_view));
}

/*@null@*/ WebKitWebView*
create_web_view_cb (WebKitWebView  *web_view, WebKitWebFrame *frame, gpointer user_data) {
    (void) web_view;
    (void) frame;
    (void) user_data;

    if (uzbl.state.verbose)
        printf("New web view -> javascript link...\n");

    WebKitWebView* new_view = WEBKIT_WEB_VIEW(webkit_web_view_new());

    g_object_connect (new_view, "signal::notify::uri",
                           G_CALLBACK(create_web_view_js_cb), NULL, NULL);
    return new_view;
}

void
download_progress_cb(WebKitDownload *download, GParamSpec *pspec, gpointer user_data) {
    (void) pspec; (void) user_data;

    gdouble progress;
    g_object_get(download, "progress", &progress, NULL);

    const gchar *dest_uri = webkit_download_get_destination_uri(download);
    const gchar *dest_path = dest_uri + strlen("file://");

    send_event(DOWNLOAD_PROGRESS, NULL,
        TYPE_STR, dest_path,
        TYPE_FLOAT, progress,
        NULL);
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
            send_event(DOWNLOAD_COMPLETE, NULL, TYPE_STR, dest_path, NULL);
        }
    }
}

gboolean
download_cb(WebKitWebView *web_view, WebKitDownload *download, gpointer user_data) {
    (void) web_view; (void) user_data;

    /* get the URI being downloaded */
    const gchar *uri = webkit_download_get_uri(download);

    /* get the destination path, if specified.
     * this is only intended to be set when this function is trigger by an
     * explicit download using uzbl's 'download' action. */
    const gchar *destination = user_data;

    if (uzbl.state.verbose)
        printf("Download requested -> %s\n", uri);

    if (!uzbl.behave.download_handler) {
        webkit_download_cancel(download);
        return FALSE; /* reject downloads when there's no download handler */
    }

    /* get a reasonable suggestion for a filename */
    const gchar *suggested_filename;
#ifdef USE_WEBKIT2
    WebKitURIResponse *response;
    g_object_get(download, "network-response", &response, NULL);
#if WEBKIT_CHECK_VERSION (1, 9, 90)
    g_object_get(response, "suggested-filename", &suggested_filename, NULL);
#else
    suggested_filename = webkit_uri_response_get_suggested_filename(respose);
#endif
#elif WEBKIT_CHECK_VERSION (1, 9, 6)
    WebKitNetworkResponse *response;
    g_object_get(download, "network-response", &response, NULL);
    g_object_get(response, "suggested-filename", &suggested_filename, NULL);
#else
    g_object_get(download, "suggested-filename", &suggested_filename, NULL);
#endif

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

    GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
    const CommandInfo *c = parse_command_parts(uzbl.behave.download_handler, a);
    if(!c) {
        webkit_download_cancel(download);
        g_array_free(a, TRUE);
        return FALSE;
    }

    g_array_append_val(a, uri);
    g_array_append_val(a, suggested_filename);
    g_array_append_val(a, content_type);
    gchar *total_size_s = g_strdup_printf("%d", total_size);
    g_array_append_val(a, total_size_s);

    if(destination)
        g_array_append_val(a, destination);

    GString *result = g_string_new ("");
    run_parsed_command(c, a, result);

    g_free(total_size_s);
    g_array_free(a, TRUE);

    /* no response, cancel the download */
    if(result->len == 0) {
        webkit_download_cancel(download);
        return FALSE;
    }

    /* we got a response, it's the path we should download the file to */
    gchar *destination_path = result->str;
    g_string_free(result, FALSE);

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

    send_event(DOWNLOAD_STARTED, NULL, TYPE_STR, destination_path, NULL);

    /* convert absolute path to file:// URI */
    gchar *destination_uri = g_strconcat("file://", destination_path, NULL);
    g_free(destination_path);

    webkit_download_set_destination_uri(download, destination_uri);
    g_free(destination_uri);

    return TRUE;
}

void
send_scroll_event(int type, GtkAdjustment *adjust) {
    gdouble value = gtk_adjustment_get_value(adjust);
    gdouble min = gtk_adjustment_get_lower(adjust);
    gdouble max = gtk_adjustment_get_upper(adjust);
    gdouble page = gtk_adjustment_get_page_size(adjust);

    send_event (type, NULL,
        TYPE_FLOAT, value,
        TYPE_FLOAT, min,
        TYPE_FLOAT, max,
        TYPE_FLOAT, page,
        NULL);
}

gboolean
scroll_vert_cb(GtkAdjustment *adjust, void *w) {
    (void) w;
    send_scroll_event(SCROLL_VERT, adjust);
    return (FALSE);
}

gboolean
scroll_horiz_cb(GtkAdjustment *adjust, void *w) {
    (void) w;
    send_scroll_event(SCROLL_HORIZ, adjust);
    return (FALSE);
}

void
run_menu_command(GtkWidget *menu, MenuItem *mi) {
    (void) menu;

    if (mi->context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE) {
        gchar* cmd = g_strdup_printf("%s %s", mi->cmd, mi->argument);

        parse_cmd_line(cmd, NULL);

        g_free(cmd);
        g_free(mi->argument);
    }
    else {
        parse_cmd_line(mi->cmd, NULL);
    }
}

gboolean
populate_context_menu (GtkWidget *default_menu, WebKitHitTestResult *hit_test_result, gint context) {
    guint i;

    /* find the user-defined menu items that are approprate for whatever was
     * clicked and append them to the default context menu. */
    for(i = 0; i < uzbl.gui.menu_items->len; i++) {
        MenuItem *mi = g_ptr_array_index(uzbl.gui.menu_items, i);
        GtkWidget *item;

        gboolean contexts_match = (context & mi->context);

        if(!contexts_match) {
          continue;
        }

        if (mi->context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE) {
            g_object_get(hit_test_result, "image-uri", &(mi->argument), NULL);
        }

        if(mi->issep) {
            item = gtk_separator_menu_item_new();
        } else {
            item = gtk_menu_item_new_with_label(mi->name);
            g_signal_connect(item, "activate",
                    G_CALLBACK(run_menu_command), mi);
        }

        gtk_menu_shell_append(GTK_MENU_SHELL(default_menu), item);
        gtk_widget_show(item);
    }

    return FALSE;
}

#if WEBKIT_CHECK_VERSION (1, 9, 0)
gboolean
context_menu_cb (WebKitWebView *v, GtkWidget *default_menu, WebKitHitTestResult *hit_test_result, gboolean triggered_with_keyboard, gpointer user_data) {
    (void) v; (void) triggered_with_keyboard; (void) user_data;
    gint context;

    if(!uzbl.gui.menu_items)
        return FALSE;

    /* check context */
    if((context = get_click_context()) == -1)
        return FALSE;

    /* display the default menu with our modifications. */
    return populate_context_menu(default_menu, hit_test_result, context);
}
#else
void
populate_popup_cb(WebKitWebView *v, GtkMenu *m, void *c) {
    (void) c;
    gint context;

    if(!uzbl.gui.menu_items)
        return;

    /* check context */
    if((context = get_click_context()) == -1)
        return;

    WebKitHitTestResult *hit_test_result;
    GdkEventButton ev;
    gint x, y;
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_window_get_device_position (gtk_widget_get_window(GTK_WIDGET(v)),
        gdk_device_manager_get_client_pointer (
            gdk_display_get_device_manager (
                gtk_widget_get_display (GTK_WIDGET (v)))),
        &x, &y, NULL);
#else
    gdk_window_get_pointer(gtk_widget_get_window(GTK_WIDGET(v)), &x, &y, NULL);
#endif
    ev.x = x;
    ev.y = y;
    hit_test_result = webkit_web_view_get_hit_test_result(v, &ev);

    populate_context_menu(m, hit_test_result, context);

    g_object_unref(hit_test_result);
}
#endif

void
window_object_cleared_cb(WebKitWebView *webview, WebKitWebFrame *frame,
        JSGlobalContextRef *context, JSObjectRef *object) {
    (void) frame; (void) context; (void) object;
#if WEBKIT_CHECK_VERSION (1, 3, 13)
    // Take this opportunity to set some callbacks on the DOM
    WebKitDOMDocument *document = webkit_web_view_get_dom_document (webview);
    webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (document),
        "focus", G_CALLBACK(dom_focus_cb), TRUE, NULL);
    webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (document),
        "blur", G_CALLBACK(dom_focus_cb), TRUE, NULL);
#else
	(void) webview;
#endif
}

#if WEBKIT_CHECK_VERSION (1, 3, 13)
void
dom_focus_cb(WebKitDOMEventTarget *target, WebKitDOMEvent *event, gpointer user_data) {
    (void) target; (void) user_data;
    WebKitDOMEventTarget *etarget = webkit_dom_event_get_target (event);
    gchar* name = webkit_dom_node_get_node_name (WEBKIT_DOM_NODE (etarget));
    send_event (FOCUS_ELEMENT, NULL, TYPE_STR, name, NULL);
}

void
dom_blur_cb(WebKitDOMEventTarget *target, WebKitDOMEvent *event, gpointer user_data) {
    (void) target; (void) user_data;
    WebKitDOMEventTarget *etarget = webkit_dom_event_get_target (event);
    gchar* name = webkit_dom_node_get_node_name (WEBKIT_DOM_NODE (etarget));
    send_event (BLUR_ELEMENT, NULL, TYPE_STR, name, NULL);
}
#endif

/* vi: set et ts=4: */
