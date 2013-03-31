#include "gui.h"

#include "commands.h"
#include "events.h"
#include "menu.h"
#include "status-bar.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"
#include "variables.h"

#include <string.h>

/* TODO: (WebKit2)
 *
 *   - Handle run-file-choose signal?
 *   - Handle script-dialog signal?
 *   - Handle submit-form signal?
 *   - Handle resource-load-started signal?
 *   - Handle print signal (turn hardcopy command into "js page string print()")?
 *   - Handle permission-request signal?
 *   - Handle leave-fullscreen signal?
 *   - Handle enter-fullscreen signal?
 *   - Handle insecure-content-detected signal?
 *   - Handle context-menu-dismissed signal?
 *   - Look into WebKitWindowProperties.
 *
 * (WebKit1)
 *
 *   - Handle resource-* signals?
 */

/* =========================== PUBLIC API =========================== */

static void
status_bar_init (void);
static void
web_view_init (void);
static void
vbox_init (void);
static void
window_init (void);
static void
plug_init (void);

void
uzbl_gui_init ()
{
    status_bar_init ();
    web_view_init ();
    vbox_init ();

    if (uzbl.state.plug_mode) {
        plug_init ();
    } else {
        window_init ();
    }
}

void
uzbl_gui_update_title ()
{
    const gchar *title_format = uzbl.behave.title_format_long;

    /* Update the status bar if shown. */
    if (uzbl_variables_get_int ("show_status")) {
        title_format = uzbl.behave.title_format_short;

        gchar *parsed = uzbl_variables_expand (uzbl.behave.status_format);
        uzbl_status_bar_update_left (uzbl.gui.status_bar, parsed);
        g_free(parsed);

        parsed = uzbl_variables_expand (uzbl.behave.status_format_right);
        uzbl_status_bar_update_right (uzbl.gui.status_bar, parsed);
        g_free (parsed);
    }

    /* Update window title. */
    /* If we're starting up or shutting down there might not be a window yet. */
    gboolean have_main_window = !uzbl.state.plug_mode && GTK_IS_WINDOW (uzbl.gui.main_window);
    if (title_format && have_main_window) {
        gchar *parsed = uzbl_variables_expand (title_format);
        const gchar *current_title = gtk_window_get_title (GTK_WINDOW (uzbl.gui.main_window));
        /* XMonad hogs CPU if the window title updates too frequently, so we
         * don't set it unless we need to. */
        if (!current_title || strcmp (current_title, parsed))
            gtk_window_set_title (GTK_WINDOW (uzbl.gui.main_window), parsed);
        g_free (parsed);
    }
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

static gboolean
key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean
key_release_cb (GtkWidget *widget, GdkEventKey *event, gpointer data);

void
status_bar_init (void)
{
    uzbl.gui.status_bar = uzbl_status_bar_new ();

    g_object_connect (G_OBJECT (uzbl.gui.status_bar),
        "signal::key-press-event",   G_CALLBACK (key_press_cb), NULL,
        "signal::key-release-event", G_CALLBACK (key_press_cb), NULL,
        NULL);

    /* TODO: What should be done with these?
    g_object_connect (G_OBJECT (UZBL_STATUS_BAR (uzbl.gui.status_bar)->label_left),
        "signal::key-press-event",   G_CALLBACK (key_press_cb),   NULL,
        "signal::key-release-event", G_CALLBACK (key_release_cb), NULL,
        NULL);

    g_object_connect (G_OBJECT (UZBL_STATUS_BAR (uzbl.gui.status_bar)->label_right),
        "signal::key-press-event",   G_CALLBACK (key_press_cb),   NULL,
        "signal::key-release-event", G_CALLBACK (key_release_cb), NULL,
        NULL);
      */
}

/* Mouse events */
static gboolean
button_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer data);
static gboolean
button_release_cb (GtkWidget *widget, GdkEventButton *event, gpointer data);
#ifdef USE_WEBKIT2
static void
mouse_target_cb (WebKitWebView *view, WebKitHitTestResult *hit_test, guint modifiers, gpointer data);
#else
static void
link_hover_cb (WebKitWebView *view, const gchar *title, const gchar *link, gpointer data);
#endif
/* Page metadata events */
static void
title_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data);
static void
progress_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data);
static void
uri_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data);
/* Navigation events */
#ifdef USE_WEBKIT2
static gboolean
decide_policy_cb (WebKitWebView *view, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data);
static void
load_changed_cb (WebKitWebView *view, WebKitLoadEvent event, gpointer data);
static gboolean
load_failed_cb (WebKitWebView *view, WebKitLoadEvent event, gchar *uri, gpointer web_err, gpointer data);
#else
static gboolean
navigation_decision_cb (WebKitWebView *view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action,
        WebKitWebPolicyDecision *policy_decision, gpointer data);
static gboolean
mime_policy_cb (WebKitWebView *view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, gchar *mime_type,
        WebKitWebPolicyDecision *policy_decision, gpointer data);
static void
request_starting_cb (WebKitWebView *view, WebKitWebFrame *frame, WebKitWebResource *resource,
        WebKitNetworkRequest *request, WebKitNetworkResponse *response, gpointer data);
static void
load_status_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data);
static gboolean
load_error_cb (WebKitWebView *view, WebKitWebFrame *frame, gchar *uri, gpointer web_err, gpointer data);
static void
window_object_cleared_cb (WebKitWebView *view, WebKitWebFrame *frame,
        JSGlobalContextRef *context, JSObjectRef *object, gpointer data);
#endif
#ifdef USE_WEBKIT2
static void
download_cb (WebKitWebContext *context, WebKitDownload *download, gpointer data);
#else
static gboolean
download_cb (WebKitWebView *view, WebKitDownload *download, gpointer data);
#endif
/* UI events */
#ifdef USE_WEBKIT2
static GtkWidget *
create_cb (WebKitWebView *view, gpointer data);
#else
static WebKitWebView *
create_web_view_cb (WebKitWebView *view, WebKitWebFrame *frame, gpointer data);
#endif
static void
close_web_view_cb (WebKitWebView *view, gpointer data);
static gboolean
focus_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data);
#if WEBKIT_CHECK_VERSION (1, 9, 0)
static gboolean
context_menu_cb (WebKitWebView *view, GtkMenu *menu, WebKitHitTestResult *hit_test_result,
        gboolean triggered_with_keyboard, gpointer data);
#else
static gboolean
populate_popup_cb (WebKitWebView *view, GtkMenu *menu, gpointer data);
#endif
#ifdef USE_WEBKIT2
static gboolean
web_process_crashed_cb (WebKitWebView *view, gpointer data);
#endif
/* Scrollbar events */
static gboolean
scroll_vert_cb (GtkAdjustment *adjust, gpointer data);
static gboolean
scroll_horiz_cb (GtkAdjustment *adjust, gpointer data);

void
web_view_init (void)
{
    uzbl.gui.web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
    uzbl.gui.scrolled_win = gtk_scrolled_window_new (NULL, NULL);

    gtk_container_add (
        GTK_CONTAINER (uzbl.gui.scrolled_win),
        GTK_WIDGET (uzbl.gui.web_view));

#ifdef USE_WEBKIT2
    g_object_connect (G_OBJECT (webkit_web_view_get_context (uzbl.gui.web_view)),
        "signal::download-started",                     G_CALLBACK (download_cb),              NULL,
        NULL);
#endif

    g_object_connect (G_OBJECT (uzbl.gui.web_view),
        /* Keyboard events */
        "signal::key-press-event",                      G_CALLBACK (key_press_cb),             NULL,
        "signal::key-release-event",                    G_CALLBACK (key_release_cb),           NULL,
        /* Mouse events */
        "signal::button-press-event",                   G_CALLBACK (button_press_cb),          NULL,
        "signal::button-release-event",                 G_CALLBACK (button_release_cb),        NULL,
#ifdef USE_WEBKIT2
        "signal::mouse-target-changed",                 G_CALLBACK (mouse_target_cb),          NULL,
#else
        "signal::hovering-over-link",                   G_CALLBACK (link_hover_cb),            NULL,
#endif
        /* Page metadata events */
        "signal::notify::title",                        G_CALLBACK (title_change_cb),          NULL,
#ifdef USE_WEBKIT2
        "signal::notify::estimated-load-progress",
#else
        "signal::notify::progress",
#endif
                                                        G_CALLBACK (progress_change_cb),       NULL,
        "signal::notify::uri",                          G_CALLBACK (uri_change_cb),            NULL,
        /* Navigation events */
#ifdef USE_WEBKIT2
        "signal::decide-policy",                        G_CALLBACK (decide_policy_cb),         NULL,
        "signal::load-changed",                         G_CALLBACK (load_changed_cb),          NULL,
        "signal::load-failed",                          G_CALLBACK (load_failed_cb),           NULL,
#else
        "signal::navigation-policy-decision-requested", G_CALLBACK (navigation_decision_cb),   NULL,
        "signal::mime-type-policy-decision-requested",  G_CALLBACK (mime_policy_cb),           NULL,
        "signal::resource-request-starting",            G_CALLBACK (request_starting_cb),      NULL,
        "signal::notify::load-status",                  G_CALLBACK (load_status_change_cb),    NULL,
        "signal::load-error",                           G_CALLBACK (load_error_cb),            NULL,
        "signal::window-object-cleared",                G_CALLBACK (window_object_cleared_cb), NULL,
        "signal::download-requested",                   G_CALLBACK (download_cb),              NULL,
#endif
        /* UI events */
#ifdef USE_WEBKIT2
        "signal::create",                               G_CALLBACK (create_cb),                NULL,
#else
        "signal::create-web-view",                      G_CALLBACK (create_web_view_cb),       NULL,
#endif
#ifdef USE_WEBKIT2
        "signal::close",
#else
        "signal::close-web-view",
#endif
                                                        G_CALLBACK (close_web_view_cb),        NULL,
        "signal::focus-in-event",                       G_CALLBACK (focus_cb),                 NULL,
        "signal::focus-out-event",                      G_CALLBACK (focus_cb),                 NULL,
#if WEBKIT_CHECK_VERSION (1, 9, 0)
        "signal::context-menu",                         G_CALLBACK (context_menu_cb),          NULL,
#else
        "signal::populate-popup",                       G_CALLBACK (populate_popup_cb),        NULL,
#endif
#ifdef USE_WEBKIT2
        "signal::web-process-crashed",                  G_CALLBACK (web_process_crashed_cb),   NULL,
#endif
        NULL);

    uzbl.gui.bar_h = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (uzbl.gui.scrolled_win));
    uzbl.gui.bar_v = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (uzbl.gui.scrolled_win));

    g_object_connect (G_OBJECT (uzbl.gui.bar_v),
        "signal::value-changed", G_CALLBACK (scroll_vert_cb), NULL,
        "signal::changed",       G_CALLBACK (scroll_vert_cb), NULL,
        NULL);
    g_object_connect (G_OBJECT (uzbl.gui.bar_h),
        "signal::value-changed", G_CALLBACK (scroll_horiz_cb), NULL,
        "signal::changed",       G_CALLBACK (scroll_horiz_cb), NULL,
        NULL);

}

void
vbox_init (void)
{
#if GTK_CHECK_VERSION (3, 0, 0)
    uzbl.gui.vbox = gtk_box_new (FALSE, 0);
    gtk_orientable_set_orientation (GTK_ORIENTABLE (uzbl.gui.vbox), GTK_ORIENTATION_VERTICAL);
#else
    uzbl.gui.vbox = gtk_vbox_new (FALSE, 0);
#endif

    gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.status_bar, FALSE, TRUE, 0);
}

static void
destroy_cb (GtkWidget *widget, gpointer data);
static gboolean
configure_event_cb (GtkWidget *widget, GdkEventConfigure *event, gpointer data);

void
window_init (void)
{
    uzbl.gui.main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    /* TODO: Plumb through from command line. */
    gtk_window_set_default_size (GTK_WINDOW (uzbl.gui.main_window), 800, 600);
    gtk_window_set_title (GTK_WINDOW (uzbl.gui.main_window), "Uzbl");
    gtk_widget_set_name (GTK_WIDGET (uzbl.gui.main_window), "Uzbl");

#if GTK_CHECK_VERSION (3, 0, 0)
    /* TODO: Make into an option? */
    gtk_window_set_has_resize_grip (GTK_WINDOW (uzbl.gui.main_window), FALSE);
#endif

    /* Fill in the main window */
    gtk_container_add (GTK_CONTAINER (uzbl.gui.main_window), uzbl.gui.vbox);

    g_object_connect (G_OBJECT (uzbl.gui.main_window),
        "signal::destroy",         G_CALLBACK (destroy_cb),         NULL,
        "signal::configure-event", G_CALLBACK (configure_event_cb), NULL,
        NULL);
}

void
plug_init (void)
{
    uzbl.gui.plug = GTK_PLUG (gtk_plug_new (uzbl.state.socket_id));

    gtk_widget_set_name (GTK_WIDGET (uzbl.gui.plug), "Uzbl");

    gtk_container_add (GTK_CONTAINER (uzbl.gui.plug), uzbl.gui.vbox);

    g_object_connect (G_OBJECT (uzbl.gui.plug),
        /* FIXME: Should we really quit GTK if the plug is destroyed? */
        "signal::destroy",           G_CALLBACK (destroy_cb),   NULL,
        "signal::key-press-event",   G_CALLBACK (key_press_cb), NULL,
        "signal::key-release-event", G_CALLBACK (key_press_cb), NULL,
        NULL);
}

/* ==================== CALLBACK IMPLEMENTATIONS ==================== */

/* Status bar callbacks */

static void
send_keypress_event (guint keyval, guint state, guint is_modifier, gint mode);

gboolean
key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    UZBL_UNUSED (widget);
    UZBL_UNUSED (data);

    if (event->type == GDK_KEY_PRESS) {
        send_keypress_event (event->keyval, event->state, event->is_modifier, GDK_KEY_PRESS);
    }

    return (uzbl.behave.forward_keys ? FALSE : TRUE);
}

gboolean
key_release_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    UZBL_UNUSED (widget);
    UZBL_UNUSED (data);

    if (event->type == GDK_KEY_RELEASE) {
        send_keypress_event (event->keyval, event->state, event->is_modifier, GDK_KEY_RELEASE);
    }

    return (uzbl.behave.forward_keys ? FALSE : TRUE);
}

/* Web view callbacks */

/* Mouse events */

#define NO_CLICK_CONTEXT -1
static gint
get_click_context (void);
static void
send_button_event (guint buttonval, guint state, gint mode);

gboolean
button_press_cb (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    UZBL_UNUSED (widget);
    UZBL_UNUSED (data);

    gint context;
    gboolean propagate = FALSE;
    gboolean sendev    = FALSE;
    gboolean is_editable = FALSE;
    gboolean is_document = FALSE;

    /* Save last button click for use in menu. */
    if (uzbl.state.last_button) {
        gdk_event_free ((GdkEvent *)uzbl.state.last_button);
    }
    uzbl.state.last_button = (GdkEventButton *)gdk_event_copy ((GdkEvent *)event);

    /* Grab context from last click. */
    context = get_click_context ();

    is_editable = (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE);
    is_document = (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT);

    if (event->type == GDK_BUTTON_PRESS) {
        if (event->button == 1) {
            /* Left click. */
            if (is_editable) {
                uzbl_events_send (FORM_ACTIVE, NULL,
                    TYPE_NAME, "button1",
                    NULL);
            } else if (is_document) {
                uzbl_events_send (ROOT_ACTIVE, NULL,
                    TYPE_NAME, "button1",
                    NULL);
            } else {
                sendev    = TRUE;
                propagate = TRUE;
            }
        } else if ((event->button == 2) && !is_editable) {
            sendev    = TRUE;
            propagate = TRUE;
        } else if (event->button == 3) {
            /* Ignore middle click. */
        } else if (event->button > 3) {
            sendev    = TRUE;
            propagate = TRUE;
        }
    }

    if ((event->type == GDK_2BUTTON_PRESS) || (event->type == GDK_3BUTTON_PRESS)) {
        if ((event->button == 1) && !is_editable && is_document) {
            sendev    = TRUE;
            propagate = uzbl.state.handle_multi_button;
        } else if ((event->button == 2) && !is_editable) {
            sendev    = TRUE;
            propagate = uzbl.state.handle_multi_button;
        } else if (event->button >= 3) {
            sendev    = TRUE;
            propagate = uzbl.state.handle_multi_button;
        }
    }

    if (sendev) {
        send_button_event (event->button, event->state, event->type);
    }

    return propagate;
}

gboolean
button_release_cb (GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    UZBL_UNUSED (widget);
    UZBL_UNUSED (data);

    gint context;
    gboolean propagate = FALSE;
    gboolean sendev    = FALSE;
    gboolean is_editable = FALSE;

    context = get_click_context ();

    is_editable = (context & WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE);

    if (event->type == GDK_BUTTON_RELEASE) {
        if ((event->button == 2) && !is_editable) {
            sendev    = TRUE;
            propagate = TRUE;
        } else if (event->button == 3) {
            /* Ignore middle click. */
        } else if (event->button > 3) {
            sendev    = TRUE;
            propagate = TRUE;
        }

        if (sendev) {
            send_button_event (event->button, event->state, GDK_BUTTON_RELEASE);
        }
    }

    return propagate;
}

static void
send_hover_event (const gchar *uri, const gchar *title);

#ifdef USE_WEBKIT2
void
mouse_target_cb (WebKitWebView *view, WebKitHitTestResult *hit_test, guint modifiers, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (modifiers);
    UZBL_UNUSED (data);

    /* TODO: Do something with modifiers? */

    /* TODO: Handle other cases? */
    if (!webkit_hit_test_result_context_is_link (hit_test)) {
        return;
    }

    const gchar *uri = webkit_hit_test_result_get_link_uri (hit_test);
    const gchar *title = webkit_hit_test_result_get_link_title (hit_test);

    send_hover_event (uri, title);
}
#else
void
link_hover_cb (WebKitWebView *view, const gchar *title, const gchar *link, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    send_hover_event (link, title);
}
#endif

/* Page metadata events */

void
title_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data)
{
    UZBL_UNUSED (param_spec);
    UZBL_UNUSED (data);

    const gchar *title = webkit_web_view_get_title (view);

    g_free (uzbl.gui.main_title);
    uzbl.gui.main_title = title ? g_strdup (title) : g_strdup ("(no title)");

    uzbl_gui_update_title ();

    uzbl_events_send (TITLE_CHANGED, NULL,
        TYPE_STR, uzbl.gui.main_title,
        NULL);
    /* TODO: Collect all environment settings into one place. */
    g_setenv ("UZBL_TITLE", uzbl.gui.main_title, TRUE);
}

void
progress_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data)
{
    UZBL_UNUSED (param_spec);
    UZBL_UNUSED (data);

    int progress = 100 *
#ifdef USE_WEBKIT2
        webkit_web_view_get_estimated_load_progress (view)
#else
        webkit_web_view_get_progress (view)
#endif
        ;

    uzbl_events_send (LOAD_PROGRESS, NULL,
        TYPE_INT, progress,
        NULL);
}

static void
set_window_property (const gchar *prop, const gchar *value);

void
uri_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data)
{
    UZBL_UNUSED (param_spec);
    UZBL_UNUSED (data);

    g_free (uzbl.state.uri);
    g_object_get (view,
        "uri", &uzbl.state.uri,
        NULL);

    /* TODO: Collect all environment settings into one place. */
    g_setenv ("UZBL_URI", uzbl.state.uri, TRUE);
    set_window_property ("UZBL_URI", uzbl.state.uri);
}

/* Navigation events */

#ifdef USE_WEBKIT2
typedef WebKitPolicyDecision WebKitWebPolicyDecision;
typedef WebKitLoadEvent WebKitLoadStatus;
#endif

static gboolean
navigation_decision (WebKitWebPolicyDecision *decision, const gchar *uri);
static gboolean
mime_decision (WebKitWebPolicyDecision *decision, const gchar *mime_type);
static void
send_load_status (WebKitLoadStatus status, const gchar *uri);
static gboolean
send_load_error (const gchar *uri, GError *error);

#ifdef USE_WEBKIT2
gboolean
decide_policy_cb (WebKitWebView *view, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    switch (type) {
        case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
        {
            WebKitNavigationPolicyDecision *nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
            WebKitURIRequest *request = webkit_navigation_policy_decision_get_request (nav_decision);
            const gchar *uri = webkit_uri_request_get_uri (request);

            return navigation_decision (decision, uri);
        }
        case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
            /* TODO: What to do here? */
            break;
        case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
        {
            WebKitResponsePolicyDecision *response_decision = WEBKIT_RESPONSE_POLICY_DECISION (decision);
            WebKitURIResponse *response = webkit_response_policy_decision_get_response (response_decision);
            const gchar *mime_type = webkit_uri_response_get_mime_type (response);

            return mime_decision (decision, mime_type);
        }
        default:
            uzbl_debug ("Unrecognized policy decision: %d\n", type);
            break;
    }

    return FALSE;
}

void
load_changed_cb (WebKitWebView *view, WebKitLoadEvent event, gpointer data)
{
    UZBL_UNUSED (data);

    const gchar *uri = webkit_web_view_get_uri (view);

    send_load_status (event, uri);
}

gboolean
load_failed_cb (WebKitWebView *view, WebKitLoadEvent event, gchar *uri, gpointer web_err, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (event);
    UZBL_UNUSED (data);

    uzbl.state.load_failed = TRUE;

    /* TODO: Use event for a better message? */

    GError *err = (GError *)web_err;

    return send_load_error (uri, err);
}

#else

gboolean
navigation_decision_cb (WebKitWebView *view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, WebKitWebNavigationAction *navigation_action,
        WebKitWebPolicyDecision *policy_decision, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (frame);
    UZBL_UNUSED (navigation_action);
    UZBL_UNUSED (data);

    const gchar *uri = webkit_network_request_get_uri (request);

    return navigation_decision (policy_decision, uri);
}

gboolean
mime_policy_cb (WebKitWebView *view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, gchar *mime_type,
        WebKitWebPolicyDecision *policy_decision, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (frame);
    UZBL_UNUSED (request);
    UZBL_UNUSED (data);

    return mime_decision (WEBKIT_WEB_POLICY_DECISION (policy_decision), mime_type);
}

void
request_starting_cb (WebKitWebView *view, WebKitWebFrame *frame, WebKitWebResource *resource,
        WebKitNetworkRequest *request, WebKitNetworkResponse *response, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (frame);
    UZBL_UNUSED (resource);
    UZBL_UNUSED (response);
    UZBL_UNUSED (data);

    const gchar *uri = webkit_network_request_get_uri (request);
    SoupMessage *message = webkit_network_request_get_message (request);

    uzbl_debug ("Request starting -> %s\n", uri);

    if (message) {
        SoupURI *soup_uri = soup_uri_new (uri);
        soup_message_set_first_party (message, soup_uri);
    }

    uzbl_events_send (REQUEST_STARTING, NULL,
        TYPE_STR, uri,
        NULL);

    if (uzbl.behave.request_handler) {
        GString *result = g_string_new ("");
        GArray *args = uzbl_commands_args_new ();
        const UzblCommand *request_command = uzbl_commands_parse (uzbl.behave.request_handler, args);

        if (request_command) {
            uzbl_commands_args_append (args, g_strdup (uri));
            uzbl_commands_run_parsed (request_command, args, result);
        }
        uzbl_commands_args_free (args);

        if (result->len > 0) {
            remove_trailing_newline (result->str);

            webkit_network_request_set_uri (request, result->str);
        }

        g_string_free (result, TRUE);
    }
}

void
load_status_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data)
{
    UZBL_UNUSED (param_spec);
    UZBL_UNUSED (data);

    WebKitLoadStatus status = webkit_web_view_get_load_status (view);

    send_load_status (status, uzbl.state.uri);
}

gboolean
load_error_cb (WebKitWebView *view, WebKitWebFrame *frame, gchar *uri, gpointer web_err, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (frame);
    UZBL_UNUSED (data);

    GError *err = (GError *)web_err;

    return send_load_error (uri, err);
}

#if WEBKIT_CHECK_VERSION (1, 3, 13)
static void
dom_focus_cb (WebKitDOMEventTarget *target, WebKitDOMEvent *event, gpointer data);
static void
dom_blur_cb (WebKitDOMEventTarget *target, WebKitDOMEvent *event, gpointer data);
#endif

void
window_object_cleared_cb (WebKitWebView *view, WebKitWebFrame *frame,
        JSGlobalContextRef *context, JSObjectRef *object, gpointer data)
{
    UZBL_UNUSED (frame);
    UZBL_UNUSED (context);
    UZBL_UNUSED (object);
    UZBL_UNUSED (data);

#if WEBKIT_CHECK_VERSION (1, 3, 13)
    /* Take this opportunity to set some callbacks on the DOM. */
    WebKitDOMDocument *document = webkit_web_view_get_dom_document (view);
    webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (document),
        "focus", G_CALLBACK (dom_focus_cb), TRUE, NULL);
    webkit_dom_event_target_add_event_listener (WEBKIT_DOM_EVENT_TARGET (document),
        "blur",  G_CALLBACK (dom_blur_cb), TRUE, NULL);
#else
    UZBL_UNUSED (view);
#endif
}
#endif

#ifdef USE_WEBKIT2
static void
handle_download (WebKitDownload *download, const gchar *suggested_destination);
#endif

#ifdef USE_WEBKIT2
void
download_cb (WebKitWebContext *context, WebKitDownload *download, gpointer data)
{
    UZBL_UNUSED (context);
    UZBL_UNUSED (download);
    UZBL_UNUSED (data);

    handle_download (download, NULL);
}
#else
gboolean
download_cb (WebKitWebView *view, WebKitDownload *download, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    handle_download (download, NULL);

    return TRUE;
}
#endif

/* UI events */

static WebKitWebView *
create_view ();

#ifdef USE_WEBKIT2
GtkWidget *
create_cb (WebKitWebView *view, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    return GTK_WIDGET (create_view ());
}
#else
WebKitWebView *
create_web_view_cb (WebKitWebView *view, WebKitWebFrame *frame, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (frame);
    UZBL_UNUSED (data);

    return create_view ();
}
#endif

void
close_web_view_cb (WebKitWebView *view, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    uzbl_events_send (CLOSE_WINDOW, NULL,
        NULL);
}

gboolean
focus_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data)
{
    UZBL_UNUSED (widget);
    UZBL_UNUSED (event);
    UZBL_UNUSED (data);

    uzbl_events_send (event->in ? FOCUS_GAINED : FOCUS_LOST, NULL,
        NULL);

    return FALSE;
}

static gboolean
populate_context_menu (GtkMenu *menu, WebKitHitTestResult *hit_test_result, gint context);

#if WEBKIT_CHECK_VERSION (1, 9, 0)
gboolean
context_menu_cb (WebKitWebView *view, GtkMenu *menu, WebKitHitTestResult *hit_test_result,
        gboolean triggered_with_keyboard, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (triggered_with_keyboard);
    UZBL_UNUSED (data);

    gint context;

    if (!uzbl.gui.menu_items) {
        return FALSE;
    }

    context = get_click_context ();

    /* Check context. */
    if (context == NO_CLICK_CONTEXT) {
        return FALSE;
    }

    /* Display the default menu with our modifications. */
    return populate_context_menu (menu, hit_test_result, context);
}
#else
gboolean
populate_popup_cb (WebKitWebView *view, GtkMenu *menu, gpointer data)
{
    UZBL_UNUSED (data);

    gint context;

    if (!uzbl.gui.menu_items) {
        return FALSE;
    }

    context = get_click_context ();

    /* Check context. */
    if (context == NO_CLICK_CONTEXT) {
        return FALSE;
    }

    WebKitHitTestResult *hit_test_result;
    GdkEventButton event;
    gint x;
    gint y;
#if GTK_CHECK_VERSION (3, 0, 0)
    gdk_window_get_device_position (gtk_widget_get_window (GTK_WIDGET (view)),
        gdk_device_manager_get_client_pointer (
            gdk_display_get_device_manager (
                gtk_widget_get_display (GTK_WIDGET (view)))),
        &x, &y, NULL);
#else
    gdk_window_get_pointer (gtk_widget_get_window (GTK_WIDGET (view)), &x, &y, NULL);
#endif
    event.x = x;
    event.y = y;
    hit_test_result = webkit_web_view_get_hit_test_result (view, &event);

    gboolean ret = populate_context_menu (menu, hit_test_result, context);

    g_object_unref (hit_test_result);

    return ret;
}
#endif

#ifdef USE_WEBKIT2
gboolean
web_process_crashed_cb (WebKitWebView *view, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    uzbl_events_send (WEB_PROCESS_CRASHED, NULL,
        NULL);

    return FALSE;
}
#endif

/* Scrollbar events */

static void
send_scroll_event (int type, GtkAdjustment *adjust);

gboolean
scroll_vert_cb (GtkAdjustment *adjust, gpointer data)
{
    UZBL_UNUSED (data);

    send_scroll_event (SCROLL_VERT, adjust);

    return FALSE;
}

gboolean
scroll_horiz_cb (GtkAdjustment *adjust, gpointer data)
{
    UZBL_UNUSED (data);

    send_scroll_event (SCROLL_HORIZ, adjust);

    return FALSE;
}

/* Window callbacks */

void
destroy_cb (GtkWidget *widget, gpointer data)
{
    UZBL_UNUSED (widget);
    UZBL_UNUSED (data);

    /* No need to exit; we're on our way down anyways. */
    if (!uzbl.state.exit) {
        uzbl_commands_run ("exit", NULL);
    }
}

gboolean
configure_event_cb (GtkWidget *widget, GdkEventConfigure *event, gpointer data)
{
    UZBL_UNUSED (widget);
    UZBL_UNUSED (event);
    UZBL_UNUSED (data);

    gchar *last_geo    = uzbl.gui.geometry;
    /* TODO: We should set the geometry instead. */
    gchar *current_geo = uzbl_variables_get_string ("geometry");

    if (!last_geo || g_strcmp0 (last_geo, current_geo)) {
        uzbl_events_send (GEOMETRY_CHANGED, NULL,
            TYPE_STR, current_geo,
            NULL);
    }

    g_free (current_geo);

    return FALSE;
}

static guint
key_to_modifier (guint keyval);
static gchar *
get_modifier_mask (guint state);

void
send_keypress_event (guint keyval, guint state, guint is_modifier, gint mode)
{
    gchar ucs[7];
    gint ulen;
    gchar *keyname;
    guint32 ukval = gdk_keyval_to_unicode (keyval);
    gchar *modifiers = NULL;
    guint mod = key_to_modifier (keyval);

    /* Get modifier state including this key press/release. */
    modifiers = get_modifier_mask ((mode == GDK_KEY_PRESS) ? (state | mod) : (state & ~mod));

    if (is_modifier && mod) {
        gchar *newmods = get_modifier_mask (mod);

        uzbl_events_send ((mode == GDK_KEY_PRESS) ? MOD_PRESS : MOD_RELEASE, NULL,
            TYPE_STR, modifiers,
            TYPE_NAME, newmods,
            NULL);

        g_free (newmods);
    } else if (g_unichar_isgraph (ukval)) {
        /* Check for printable unicode char. */
        /* TODO: Pass the keyvals through a GtkIMContext so that we also get
         * combining chars right. */
        ulen = g_unichar_to_utf8 (ukval, ucs);
        ucs[ulen] = 0;

        uzbl_events_send ((mode == GDK_KEY_PRESS) ? KEY_PRESS : KEY_RELEASE, NULL,
            TYPE_STR, modifiers,
            TYPE_STR, ucs,
            NULL);
    } else if ((keyname = gdk_keyval_name (keyval))) {
        /* Send keysym for non-printable chars. */
        uzbl_events_send ((mode == GDK_KEY_PRESS) ? KEY_PRESS : KEY_RELEASE, NULL,
            TYPE_STR, modifiers,
            TYPE_NAME, keyname,
            NULL);
    }

    g_free (modifiers);
}

gint
get_click_context ()
{
    guint context = NO_CLICK_CONTEXT;

    if (!uzbl.state.last_button) {
        return NO_CLICK_CONTEXT;
    }

#ifndef USE_WEBKIT2 /* TODO: No API for this? :( */
    WebKitHitTestResult *ht = webkit_web_view_get_hit_test_result (uzbl.gui.web_view, uzbl.state.last_button);
    g_object_get (ht,
        "context", &context,
        NULL);
    g_object_unref (ht);
#endif

    return (gint)context;
}

static guint
button_to_modifier (guint buttonval);

void
send_button_event (guint buttonval, guint state, gint mode)
{
    gchar *details;
    const char *reps;
    gchar *modifiers = NULL;
    guint mod = button_to_modifier (buttonval);

    /* Get modifier state including this button press/release. */
    modifiers = get_modifier_mask ((mode != GDK_BUTTON_RELEASE) ? (state | mod) : (state & ~mod));

    switch (mode) {
        case GDK_2BUTTON_PRESS:
            reps = "2";
            break;
        case GDK_3BUTTON_PRESS:
            reps = "3";
            break;
        default:
            reps = "";
            break;
    }

    details = g_strdup_printf ("%sButton%d", reps, buttonval);

    uzbl_events_send ((mode == GDK_BUTTON_PRESS) ? KEY_PRESS : KEY_RELEASE, NULL,
        TYPE_STR, modifiers,
        TYPE_FORMATTEDSTR, details,
        NULL);

    g_free (details);
    g_free (modifiers);
}

void
send_hover_event (const gchar *uri, const gchar *title)
{
    g_free (uzbl.state.last_selected_url);

    if (uzbl.state.selected_url) {
        uzbl.state.last_selected_url = g_strdup (uzbl.state.selected_url);
        g_free (uzbl.state.selected_url);
        uzbl.state.selected_url = NULL;
    } else {
        uzbl.state.last_selected_url = NULL;
    }

    if (uzbl.state.last_selected_url && g_strcmp0 (uri, uzbl.state.last_selected_url)) {
        uzbl_events_send (LINK_UNHOVER, NULL,
            TYPE_STR, uzbl.state.last_selected_url,
            NULL);
    }

    if (uri) {
        uzbl.state.selected_url = g_strdup (uri);
        uzbl_events_send (LINK_HOVER, NULL,
            TYPE_STR, uzbl.state.selected_url,
            TYPE_STR, title ? title : "",
            NULL);
    }

    uzbl_gui_update_title ();
}

void
set_window_property (const gchar *prop, const gchar *value)
{
    if (uzbl.gui.main_window && GTK_IS_WIDGET (uzbl.gui.main_window))
    {
        gdk_property_change (
            gtk_widget_get_window (GTK_WIDGET (uzbl.gui.main_window)),
            gdk_atom_intern_static_string (prop),
            gdk_atom_intern_static_string ("STRING"),
            CHAR_BIT * sizeof (value[0]),
            GDK_PROP_MODE_REPLACE,
            (const guchar *)value,
            strlen (value));
    }
}

gboolean
navigation_decision (WebKitWebPolicyDecision *decision, const gchar *uri)
{
    gboolean decision_made = FALSE;

    uzbl_debug ("Navigation requested -> %s\n", uri);

    if (uzbl.behave.scheme_handler) {
        GString *result = g_string_new ("");
        GArray *args = uzbl_commands_args_new ();
        const UzblCommand *scheme_command = uzbl_commands_parse (uzbl.behave.scheme_handler, args);

        if (scheme_command) {
            uzbl_commands_args_append (args, g_strdup (uri));
            uzbl_commands_run_parsed (scheme_command, args, result);
        }

        uzbl_commands_args_free (args);

        if (result->len > 0) {
            remove_trailing_newline (result->str);

            if (!g_strcmp0 (result->str, "USED")) {
#ifdef USE_WEBKIT2
                webkit_policy_decision_ignore (decision);
#else
                webkit_web_policy_decision_ignore (decision);
#endif
                decision_made = TRUE;
            }
        }

        g_string_free (result, TRUE);
    }

    if (!decision_made) {
#ifdef USE_WEBKIT2
        webkit_policy_decision_use (decision);
#else
        webkit_web_policy_decision_use (decision);
#endif
    }

    return TRUE;
}

gboolean
mime_decision (WebKitWebPolicyDecision *decision, const gchar *mime_type)
{
    /* TODO: Ignore based on external filter program? */

    /* If we can display it, let's display it... */
    if (webkit_web_view_can_show_mime_type (uzbl.gui.web_view, mime_type)) {
#ifdef USE_WEBKIT2
        webkit_policy_decision_use (decision);
#else
        webkit_web_policy_decision_use (decision);
#endif
    } else {
        /* ...everything we can't display is downloaded. */
#ifdef USE_WEBKIT2
        /* TODO: Ignore the request and use our download logic. */
        webkit_policy_decision_download (decision);
#else
        webkit_web_policy_decision_download (decision);
#endif
    }

    return TRUE;
}

void
send_load_status (WebKitLoadStatus status, const gchar *uri)
{
    switch (status) {
#ifdef USE_WEBKIT2
        case WEBKIT_LOAD_STARTED:
#else
        case WEBKIT_LOAD_PROVISIONAL:
#endif
            uzbl_events_send (LOAD_START, NULL,
                TYPE_STR, uri ? uri : "",
                NULL);
            break;
#ifdef USE_WEBKIT2
        case WEBKIT_LOAD_REDIRECTED:
            uzbl_events_send (LOAD_REDIRECTED, NULL,
                TYPE_STR, uri,
                NULL);
            break;
#else
        case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
            /* TODO: Implement. */
            break;
        case WEBKIT_LOAD_FAILED:
            /* Handled by load_error_cb. */
            break;
#endif
        case WEBKIT_LOAD_COMMITTED:
            uzbl_events_send (LOAD_COMMIT, NULL,
                TYPE_STR, uri,
                NULL);
            break;
        case WEBKIT_LOAD_FINISHED:
#ifdef USE_WEBKIT2
            if (uzbl.state.load_failed) {
                uzbl.state.load_failed = FALSE;
                break;
            }
#endif
            uzbl_events_send (LOAD_FINISH, NULL,
                TYPE_STR, uri,
                NULL);
            break;
        default:
            uzbl_debug ("Unrecognized load status: %d\n", status);
            break;
    }
}

gboolean
send_load_error (const gchar *uri, GError *error)
{
    uzbl_events_send (LOAD_ERROR, NULL,
        TYPE_STR, uri,
        TYPE_INT, error->code,
        TYPE_STR, error->message,
        NULL);

    return FALSE;
}

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 3, 13)
void
dom_focus_cb (WebKitDOMEventTarget *target, WebKitDOMEvent *event, gpointer data)
{
    UZBL_UNUSED (target);
    UZBL_UNUSED (data);

    WebKitDOMEventTarget *etarget = webkit_dom_event_get_target (event);
    gchar *name = webkit_dom_node_get_node_name (WEBKIT_DOM_NODE (etarget));

    uzbl_events_send (FOCUS_ELEMENT, NULL,
        TYPE_STR, name,
        NULL);
}

void
dom_blur_cb (WebKitDOMEventTarget *target, WebKitDOMEvent *event, gpointer data)
{
    UZBL_UNUSED (target);
    UZBL_UNUSED (data);

    WebKitDOMEventTarget *etarget = webkit_dom_event_get_target (event);
    gchar *name = webkit_dom_node_get_node_name (WEBKIT_DOM_NODE (etarget));

    uzbl_events_send (BLUR_ELEMENT, NULL,
        TYPE_STR, name,
        NULL);
}
#endif
#endif

static gboolean
decide_destination_cb (WebKitDownload *download, const gchar *suggested_filename, gpointer data);
static void
download_finished_cb (WebKitDownload *download, gpointer data);
#ifdef USE_WEBKIT2
static void
download_receive_cb (WebKitDownload *download, guint64 length, gpointer data);
static void
download_failed_cb (WebKitDownload *download, gpointer error, gpointer data);
#else
static void
download_size_cb (WebKitDownload *download, GParamSpec *param_spec, gpointer data);
static void
download_status_cb (WebKitDownload *download, GParamSpec *param_spec, gpointer data);
static void
download_error_cb (WebKitDownload *download, gint error_code, gint error_detail, gchar *reason, gpointer data);
#endif

void
handle_download (WebKitDownload *download, const gchar *suggested_destination)
{
#ifdef USE_WEBKIT2
    g_object_connect (G_OBJECT (download),
        "signal::decide-destination", G_CALLBACK (decide_destination_cb), suggested_destination,
        "signal::finished",           G_CALLBACK (download_finished_cb),  NULL,
        "signal::received-data",      G_CALLBACK (download_receive_cb),   NULL,
        "signal::failed",             G_CALLBACK (download_failed_cb),    NULL,
        NULL);
#else
    const gchar *download_suggestion = webkit_download_get_suggested_filename (download);

    if (!decide_destination_cb (download, download_suggestion, (gpointer)suggested_destination)) {
        return;
    }

    g_object_connect (G_OBJECT (download),
        "signal::notify::current-size", G_CALLBACK (download_size_cb),      NULL,
        "signal::notify::status",       G_CALLBACK (download_status_cb),    NULL,
        "signal::error",                G_CALLBACK (download_error_cb),     NULL,
        NULL);

    if (webkit_download_get_destination_uri (download)) {
        webkit_download_start (download);
    }
#endif
}

static void
create_web_view_js_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data);

WebKitWebView *
create_view ()
{
    WebKitWebView *new_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());

    uzbl_debug ("New web view -> javascript link...\n");

    g_object_connect (G_OBJECT (new_view),
        "signal::notify::uri", G_CALLBACK (create_web_view_js_cb), NULL,
        NULL);

    return new_view;
}

static void
run_menu_command (GtkMenuItem *menu_item, gpointer data);

gboolean
populate_context_menu (GtkMenu *menu, WebKitHitTestResult *hit_test_result, gint context)
{
    guint i;

    /* Find the user-defined menu items that are approprate for whatever was
     * clicked and append them to the default context menu. */
    for (i = 0; i < uzbl.gui.menu_items->len; ++i) {
        UzblMenuItem *menu_item = g_ptr_array_index (uzbl.gui.menu_items, i);
        GtkWidget *item;

        gboolean contexts_match = (context & menu_item->context);

        if (!contexts_match) {
            continue;
        }

        gboolean is_image = (menu_item->context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);

        if (is_image) {
            g_object_get (hit_test_result,
                "image-uri", &menu_item->argument,
                NULL);
        }

        if (menu_item->issep) {
            item = gtk_separator_menu_item_new ();
        } else {
            item = gtk_menu_item_new_with_label (menu_item->name);
            g_object_connect (G_OBJECT (item),
                "signal::activate", G_CALLBACK (run_menu_command), menu_item,
                NULL);
        }

        gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
        gtk_widget_show (item);
    }

    return FALSE;
}

void
send_scroll_event (int type, GtkAdjustment *adjust)
{
    gdouble value = gtk_adjustment_get_value (adjust);
    gdouble min = gtk_adjustment_get_lower (adjust);
    gdouble max = gtk_adjustment_get_upper (adjust);
    gdouble page = gtk_adjustment_get_page_size (adjust);

    uzbl_events_send (type, NULL,
        TYPE_FLOAT, value,
        TYPE_FLOAT, min,
        TYPE_FLOAT, max,
        TYPE_FLOAT, page,
        NULL);
}

guint
key_to_modifier (guint keyval)
{
/* Backwards compatibility. */
#if !GTK_CHECK_VERSION (2, 22, 0)
#define GDK_KEY_Shift_L GDK_Shift_L
#define GDK_KEY_Shift_R GDK_Shift_R
#define GDK_KEY_Control_L GDK_Control_L
#define GDK_KEY_Control_R GDK_Control_R
#define GDK_KEY_Alt_L GDK_Alt_L
#define GDK_KEY_Alt_R GDK_Alt_R
#define GDK_KEY_Super_L GDK_Super_L
#define GDK_KEY_Super_R GDK_Super_R
#define GDK_KEY_ISO_Level3_Shift GDK_ISO_Level3_Shift
#endif

    /* FIXME: Should really use XGetModifierMapping and/or Xkb to get actual
     * modifier keys. */
    switch (keyval) {
        case GDK_KEY_Shift_L:
        case GDK_KEY_Shift_R:
            return GDK_SHIFT_MASK;
        case GDK_KEY_Control_L:
        case GDK_KEY_Control_R:
            return GDK_CONTROL_MASK;
        case GDK_KEY_Alt_L:
        case GDK_KEY_Alt_R:
            return GDK_MOD1_MASK;
        case GDK_KEY_Super_L:
        case GDK_KEY_Super_R:
            return GDK_MOD4_MASK;
        case GDK_KEY_ISO_Level3_Shift:
            return GDK_MOD5_MASK;
        default:
            return 0;
    }
}

gchar *
get_modifier_mask (guint state)
{
    GString *modifiers = g_string_new ("");

    if (state & GDK_MODIFIER_MASK) {
#define CHECK_MODIFIER(mask, modifier)                 \
    do {                                               \
        if (state & GDK_##mask##_MASK) {               \
            g_string_append (modifiers, modifier "|"); \
        }                                              \
    } while (0)

        CHECK_MODIFIER (SHIFT,   "Shift");
        CHECK_MODIFIER (LOCK,    "ScrollLock");
        CHECK_MODIFIER (CONTROL, "Ctrl");
        CHECK_MODIFIER (MOD1,    "Mod1");
        /* Mod2 is usually NumLock. Ignore it since NumLock shouldn't be used
         * in bindings.
        CHECK_MODIFIER (MOD2,    "Mod2");
         */
        CHECK_MODIFIER (MOD3,    "Mod3");
        CHECK_MODIFIER (MOD4,    "Mod4");
        CHECK_MODIFIER (MOD5,    "Mod5");
        CHECK_MODIFIER (BUTTON1, "Button1");
        CHECK_MODIFIER (BUTTON2, "Button2");
        CHECK_MODIFIER (BUTTON3, "Button3");
        CHECK_MODIFIER (BUTTON4, "Button4");
        CHECK_MODIFIER (BUTTON5, "Button5");

#undef CHECK_MODIFIER

        if (modifiers->len) {
            gsize end = modifiers->len - 1;

            if (modifiers->str[end] == '|') {
                g_string_truncate (modifiers, end);
            }
        }
    }

    return g_string_free (modifiers, FALSE);
}

guint
button_to_modifier (guint buttonval)
{
    if (buttonval <= 5) {
        /* TODO: Where does this come from? */
        return (1 << (7 + buttonval));
    }
    return 0;
}

gboolean
decide_destination_cb (WebKitDownload *download, const gchar *suggested_filename, gpointer data)
{
    /* Get the URI being downloaded. */
    const gchar *uri =
#ifdef USE_WEBKIT2
        webkit_download_get_destination (download)
#else
        webkit_download_get_uri (download)
#endif
        ;
    const gchar *user_destination = (const gchar *)data;
    const gchar *destination = suggested_filename;

    if (user_destination) {
        destination = user_destination;
    }

    uzbl_debug ("Download requested -> %s\n", uri);

    if (!uzbl.behave.download_handler) {
        /* Reject downloads when there's no download handler. */
        uzbl_debug ("No download handler set; ignoring download\n");
        webkit_download_cancel (download);
        return FALSE;
    }

    /* Get the mimetype of the download. */
    const gchar *content_type = NULL;
    guint64 total_size = 0;
#ifdef USE_WEBKIT2
    WebKitURIResponse *response = webkit_download_get_response (download);
    content_type = webkit_uri_response_get_mime_type (response);
    total_size = webkit_uri_response_get_content_length (response);
#else
    WebKitNetworkResponse *response = webkit_download_get_network_response (download);
    /* Downloads can be initiated from the context menu, in that case there is
     * no network response yet and trying to get one would crash. */
    if (WEBKIT_IS_NETWORK_RESPONSE (response)) {
        SoupMessage        *message = webkit_network_response_get_message (response);
        SoupMessageHeaders *headers = NULL;
        g_object_get (G_OBJECT (message),
            "response-headers", &headers,
            NULL);
        /* Some versions of libsoup don't have "response-headers" here. */
        if (headers) {
            content_type = soup_message_headers_get_one (headers, "Content-Type");
        }
    }

    /* Get the filesize of the download, as given by the server. It may be
     * inaccurate, but there's nothing we can do about that. */
    total_size = webkit_download_get_total_size (download);
#endif

    if (!content_type) {
        content_type = "application/octet-stream";
    }

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *download_command = uzbl_commands_parse (uzbl.behave.download_handler, args);
    if (!download_command) {
        webkit_download_cancel (download);
        uzbl_commands_args_free (args);
        return FALSE;
    }

    uzbl_commands_args_append (args, g_strdup (uri));
    uzbl_commands_args_append (args, g_strdup (suggested_filename));
    uzbl_commands_args_append (args, g_strdup (content_type));
    gchar *total_size_s = g_strdup_printf ("%" G_GUINT64_FORMAT, total_size);
    uzbl_commands_args_append (args, total_size_s);

    if (destination) {
        uzbl_commands_args_append (args, g_strdup (destination));
    }

    GString *result = g_string_new ("");
    uzbl_commands_run_parsed (download_command, args, result);

    uzbl_commands_args_free (args);

    /* No response, cancel the download. */
    if (!result->len) {
        webkit_download_cancel (download);
        return FALSE;
    }

    /* We got a response, it's the path we should download the file to. */
    gchar *destination_path = result->str;
    g_string_free (result, FALSE);

    /* Presumably people don't need newlines in their filenames. */
    remove_trailing_newline (destination_path);

    /* Convert relative path to absolute path. */
    if (destination_path[0] != '/') {
        /* TODO: Detect file:// URI from the handler. */
        gchar *rel_path = destination_path;
        gchar *cwd = g_get_current_dir ();
        destination_path = g_strconcat (cwd, "/", destination_path, NULL);
        g_free (cwd);
        g_free (rel_path);
    }

    uzbl_events_send (DOWNLOAD_STARTED, NULL,
        TYPE_STR, destination_path,
        NULL);

    /* Convert absolute path to file:// URI. */
    gchar *destination_uri = g_strconcat ("file://", destination_path, NULL);
    g_free (destination_path);

#ifdef USE_WEBKIT2
    webkit_download_set_destination (download, destination_uri);
#else
    webkit_download_set_destination_uri (download, destination_uri);
#endif
    g_free (destination_uri);

    return TRUE;
}

void
download_finished_cb (WebKitDownload *download, gpointer data)
{
    UZBL_UNUSED (data);

    const gchar *dest_uri =
#ifdef USE_WEBKIT2
        webkit_download_get_destination (download)
#else
        webkit_download_get_destination_uri (download)
#endif
        ;
    const gchar *dest_path = dest_uri + strlen ("file://");

    uzbl_events_send (DOWNLOAD_COMPLETE, NULL,
        TYPE_STR, dest_path,
        NULL);
}

static void
download_update (WebKitDownload *download);
static void
send_download_error (WebKitDownloadError err, const gchar *message);

#ifdef USE_WEBKIT2
void
download_receive_cb (WebKitDownload *download, guint64 length, gpointer data)
{
    UZBL_UNUSED (length);
    UZBL_UNUSED (data);

    download_update (download);
}

void
download_failed_cb (WebKitDownload *download, gpointer error, gpointer data)
{
    UZBL_UNUSED (download);
    UZBL_UNUSED (data);

    GError *err = (GError *)error;

    send_download_error (err->code, err->message);
}

#else

void
download_size_cb (WebKitDownload *download, GParamSpec *param_spec, gpointer data)
{
    UZBL_UNUSED (param_spec);
    UZBL_UNUSED (data);

    download_update (download);
}

void
download_status_cb (WebKitDownload *download, GParamSpec *param_spec, gpointer data)
{
    UZBL_UNUSED (param_spec);

    WebKitDownloadStatus status = webkit_download_get_status (download);

    switch (status) {
        case WEBKIT_DOWNLOAD_STATUS_CREATED:
        case WEBKIT_DOWNLOAD_STATUS_STARTED:
        case WEBKIT_DOWNLOAD_STATUS_CANCELLED:
        case WEBKIT_DOWNLOAD_STATUS_ERROR:
            /* Handled elsewhere. */
            break;
        case WEBKIT_DOWNLOAD_STATUS_FINISHED:
            download_finished_cb (download, data);
            break;
        default:
            uzbl_debug ("Unknown download status: %d\n", status);
            break;
    }
}

void
download_error_cb (WebKitDownload *download, gint error_code, gint error_detail, gchar *reason, gpointer data)
{
    UZBL_UNUSED (download);
    UZBL_UNUSED (error_code);
    UZBL_UNUSED (data);

    send_download_error (error_detail, reason);
}
#endif

void
create_web_view_js_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data)
{
    UZBL_UNUSED (param_spec);
    UZBL_UNUSED (data);

    webkit_web_view_stop_loading (view);
    const gchar *uri = webkit_web_view_get_uri (view);

    static const char *js_protocol = "javascript:";

    if (!strprefix (uri, js_protocol)) {
        GArray *args = uzbl_commands_args_new ();
        const gchar *js_code = uri + strlen (js_protocol);
        uzbl_commands_args_append (args, g_strdup ("page"));
        uzbl_commands_args_append (args, g_strdup ("string"));
        uzbl_commands_args_append (args, g_strdup (js_code));
        uzbl_commands_run_argv ("js", args, NULL);
        uzbl_commands_args_free (args);
    } else {
        uzbl_events_send (NEW_WINDOW, NULL,
            TYPE_STR, uri,
            NULL);
    }

    gtk_widget_destroy (GTK_WIDGET (view));
}

void
run_menu_command (GtkMenuItem *menu_item, gpointer data)
{
    UZBL_UNUSED (menu_item);

    UzblMenuItem *item = (UzblMenuItem *)data;

    gboolean is_image = (item->context & WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE);

    if (is_image) {
        gchar *cmd = g_strdup_printf ("%s %s", item->cmd, item->argument);

        uzbl_commands_run (cmd, NULL);

        g_free (cmd);
        g_free (item->argument);
    } else {
        uzbl_commands_run (item->cmd, NULL);
    }
}

void
download_update (WebKitDownload *download)
{
    gdouble progress;
    g_object_get (download,
        "progress", &progress,
        NULL);

    const gchar *dest_uri =
#ifdef USE_WEBKIT2
        webkit_download_get_destination (download)
#else
        webkit_download_get_destination_uri (download)
#endif
        ;
    const gchar *dest_path = dest_uri + strlen ("file://");

    uzbl_events_send (DOWNLOAD_PROGRESS, NULL,
        TYPE_STR, dest_path,
        TYPE_FLOAT, progress,
        NULL);
}

void
send_download_error (WebKitDownloadError err, const gchar *message)
{
    const gchar *str = NULL;

    switch (err) {
        case WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER:
            str = "cancelled";
            break;
        case WEBKIT_DOWNLOAD_ERROR_DESTINATION:
            str = "destination";
            break;
        case WEBKIT_DOWNLOAD_ERROR_NETWORK:
            str = "network";
            break;
        default:
            str = "unknown";
            break;
    }

    uzbl_events_send (DOWNLOAD_ERROR, NULL,
        TYPE_INT, err,
        TYPE_STR, str ? str : "",
        TYPE_STR, message,
        NULL);

}
