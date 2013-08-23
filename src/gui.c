#include "gui.h"

#include "commands.h"
#include "events.h"
#include "io.h"
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
 *   - Handle context-menu-dismissed signal?
 *   - Look into WebKitWindowProperties.
 *
 * (WebKit1)
 *
 *   - Handle resource-* signals?
 */

struct _UzblGui {
    gchar *last_geometry;
    gchar *last_selected_url;

    GdkEventButton *last_button;

#ifdef USE_WEBKIT2
    gboolean load_failed;
#endif
};

/* =========================== PUBLIC API =========================== */

static void
status_bar_init ();
static void
web_view_init ();
static void
vbox_init ();
static void
window_init ();
static void
plug_init ();

void
uzbl_gui_init ()
{
    uzbl.gui_ = g_malloc0 (sizeof (UzblGui));

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
uzbl_gui_free ()
{
    g_free (uzbl.gui_->last_geometry);
    g_free (uzbl.gui_->last_selected_url);

    if (uzbl.gui_->last_button) {
        gdk_event_free ((GdkEvent *)uzbl.gui_->last_button);
    }

    g_free (uzbl.gui_);
    uzbl.gui_ = NULL;
}

void
uzbl_gui_update_title ()
{
    const gchar *format = NULL;

    /* Update the status bar if shown. */
    if (uzbl_variables_get_int ("show_status")) {
        format = "title_format_short";

        gchar *status_format;
        gchar *parsed;

        status_format = uzbl_variables_get_string ("status_format");
        parsed = uzbl_variables_expand (status_format);
        uzbl_status_bar_update_left (uzbl.gui.status_bar, parsed);
        g_free (status_format);
        g_free (parsed);

        status_format = uzbl_variables_get_string ("status_format_right");
        parsed = uzbl_variables_expand (status_format);
        uzbl_status_bar_update_right (uzbl.gui.status_bar, parsed);
        g_free (status_format);
        g_free (parsed);
    } else {
        format = "title_format_long";
    }

    gchar *title_format = uzbl_variables_get_string (format);

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

    g_free (title_format);
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

static gboolean
key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data);
static gboolean
key_release_cb (GtkWidget *widget, GdkEventKey *event, gpointer data);

void
status_bar_init ()
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
#if WEBKIT_CHECK_VERSION (2, 1, 4)
static gboolean
authenticate_cb (WebKitWebView *view, WebKitAuthenticationRequest *request, gpointer data);
#endif
static void
insecure_content_cb (WebKitWebView *view, WebKitInsecureContentEvent type, gpointer data);
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
web_view_init ()
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
#if WEBKIT_CHECK_VERSION (2, 1, 4)
        "signal::authenticate",                         G_CALLBACK (authenticate_cb),          NULL,
#endif
#if WEBKIT_CHECK_VERSION (1, 11, 4)
        "signal::insecure-content-detected",            G_CALLBACK (insecure_content_cb),      NULL,
#endif
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
vbox_init ()
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
window_init ()
{
    uzbl.gui.main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    /* TODO: Plumb through from command line. */
    gtk_window_set_default_size (GTK_WINDOW (uzbl.gui.main_window), 800, 600);
    gtk_window_set_title (GTK_WINDOW (uzbl.gui.main_window), "Uzbl");
    gtk_widget_set_name (uzbl.gui.main_window, "Uzbl");

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
plug_init ()
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
send_keypress_event (GdkEventKey *event);

gboolean
key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    UZBL_UNUSED (widget);
    UZBL_UNUSED (data);

    if (event->type == GDK_KEY_PRESS) {
        send_keypress_event (event);
    }

    return !uzbl_variables_get_int ("forward_keys");
}

gboolean
key_release_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    UZBL_UNUSED (widget);
    UZBL_UNUSED (data);

    if (event->type == GDK_KEY_RELEASE) {
        send_keypress_event (event);
    }

    return !uzbl_variables_get_int ("forward_keys");
}

/* Web view callbacks */

/* Mouse events */

#define NO_CLICK_CONTEXT -1
static gint
get_click_context ();
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
    if (uzbl.gui_->last_button) {
        gdk_event_free ((GdkEvent *)uzbl.gui_->last_button);
    }
    uzbl.gui_->last_button = (GdkEventButton *)gdk_event_copy ((GdkEvent *)event);

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
        gboolean handle_multi_button = uzbl_variables_get_int ("handle_multi_button");

        if ((event->button == 1) && !is_editable && is_document) {
            sendev    = TRUE;
            propagate = handle_multi_button;
        } else if ((event->button == 2) && !is_editable) {
            sendev    = TRUE;
            propagate = handle_multi_button;
        } else if (event->button >= 3) {
            sendev    = TRUE;
            propagate = handle_multi_button;
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

    WebKitHitTestResultContext context;

    g_object_get (G_OBJECT (hit_test),
        "context", &context,
        NULL);

    /* TODO: Handle other cases? */
    if (!(context & WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK)) {
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
    uzbl.gui.main_title = g_strdup (title ? title : "(no title)");

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

#ifdef USE_WEBKIT2
#define make_policy(decision, policy) \
    webkit_policy_decision_##policy (WEBKIT_POLICY_DECISION (decision))
#else
#define make_policy(decision, policy) \
    webkit_web_policy_decision_##policy (decision)
#endif

static gboolean
navigation_decision (WebKitWebPolicyDecision *decision, const gchar *uri,
        const gchar *frame, const gchar *type, guint button, guint modifiers);
static gboolean
request_decision (const gchar *uri, gpointer data);
static void
rewrite_request (GString *result, gpointer data);
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

    if (uzbl_variables_get_int ("frozen")) {
        make_policy (decision, ignore);
        return TRUE;
    }

    switch (type) {
    case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
    case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
    {
        WebKitNavigationPolicyDecision *nav_decision = WEBKIT_NAVIGATION_POLICY_DECISION (decision);
        WebKitURIRequest *request = webkit_navigation_policy_decision_get_request (nav_decision);

        const gchar *uri = webkit_uri_request_get_uri (request);
        const gchar *frame = webkit_navigation_policy_decision_get_frame_name (nav_decision);
        WebKitNavigationType type = webkit_navigation_policy_decision_get_navigation_type (nav_decision);
        guint button = webkit_navigation_policy_decision_get_mouse_button (nav_decision);
        guint modifiers = webkit_navigation_policy_decision_get_modifiers (nav_decision);

#define navigation_type_choices(call)                                  \
    call(WEBKIT_NAVIGATION_TYPE_LINK_CLICKED, "link")                  \
    call(WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED, "form_submission")     \
    call(WEBKIT_NAVIGATION_TYPE_BACK_FORWARD, "back_forward")          \
    call(WEBKIT_NAVIGATION_TYPE_RELOAD, "reload")                      \
    call(WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED, "form_resubmission") \
    call(WEBKIT_NAVIGATION_TYPE_OTHER, "other")

#define ENUM_TO_STRING(val, str) \
    case val:                    \
        scheme_str = str;        \
        break;

        const gchar *type_str = "unknown";
        switch (type) {
        navigation_type_choices (ENUM_TO_STRING)
        default:
            break;
        }

#undef ENUM_TO_STRING
#undef navigation_type_choices

        return navigation_decision (decision, uri, frame, type_str, button, modifiers);
    }
    case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
    {
        WebKitResponsePolicyDecision *response_decision = WEBKIT_RESPONSE_POLICY_DECISION (decision);
        WebKitURIRequest *request = webkit_response_policy_decision_get_request (response_decision);
        const gchar *uri = webkit_uri_request_get_uri (request);

        g_object_ref (decision);
        return request_decision (uri, decision);
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

    uzbl.gui_->load_failed = TRUE;

    /* TODO: Use event for a better message? */

    GError *err = (GError *)web_err;

    return send_load_error (uri, err);
}

#if WEBKIT_CHECK_VERSION (2, 1, 4)
typedef struct {
    WebKitAuthenticationRequest *request;
} UzblAuthenticateData;

static void
authenticate (GString *result, gpointer data);

gboolean
authenticate_cb (WebKitWebView *view, WebKitAuthenticationRequest *request, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    gchar *handler = uzbl_variables_get_string ("authentication_handler");

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *authentication_command = uzbl_commands_parse (handler, args);
    g_free (handler);

    if (!authentication_command) {
        uzbl_commands_args_free (args);
        return FALSE;
    }

    const gchar *host = webkit_authentication_request_get_host (request);
    gint port = webkit_authentication_request_get_port (request);
    const gchar *realm = webkit_authentication_request_get_realm (request);
    gboolean retrying = webkit_authentication_request_is_retry (request);
    const gchar *retry = retrying ? "retrying" : "initial";
    WebKitAuthenticationScheme scheme = webkit_authentication_request_get_scheme (request);

#define authentication_scheme_choices(call)                                                         \
    call(WEBKIT_AUTHENTICATION_SCHEME_DEFAULT, "default")                                           \
    call(WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC, "http_basic")                                     \
    call(WEBKIT_AUTHENTICATION_SCHEME_HTTP_DIGEST, "http_digest")                                   \
    call(WEBKIT_AUTHENTICATION_SCHEME_HTML_FORM, "html_form")                                       \
    call(WEBKIT_AUTHENTICATION_SCHEME_NTLM, "ntlm")                                                 \
    call(WEBKIT_AUTHENTICATION_SCHEME_NEGOTIATE, "negotiate")                                       \
    call(WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_REQUESTED, "client_certificate_requested") \
    call(WEBKIT_AUTHENTICATION_SCHEME_SERVER_TRUST_EVALUATION_REQUESTED, "server_trust_evaluation_requested")

#define ENUM_TO_STRING(val, str) \
    case val:                    \
        scheme_str = str;        \
        break;

    const gchar *scheme_str = "unknown";
    switch (scheme) {
    authentication_scheme_choices (ENUM_TO_STRING)
    case WEBKIT_AUTHENTICATION_SCHEME_UNKNOWN:
    default:
        break;
    }

#undef ENUM_TO_STRING
#undef authentication_scheme_choices

    gboolean is_proxy = webkit_authentication_request_is_for_proxy (request);
    const gchar *proxy = is_proxy ? "proxy" : "origin";
    gchar *port_str = g_strdup_printf ("%d", port);
    gboolean can_save = webkit_authentication_request_can_save_credentials (request);
    const gchar *save_str = can_save ? "can_save" : "cant_save";

    uzbl_commands_args_append (args, g_strdup (host));
    uzbl_commands_args_append (args, g_strdup (realm));
    uzbl_commands_args_append (args, g_strdup (retry));
    uzbl_commands_args_append (args, g_strdup (scheme_str));
    uzbl_commands_args_append (args, g_strdup (proxy));
    uzbl_commands_args_append (args, g_strdup (port_str));
    uzbl_commands_args_append (args, g_strdup (save_str));

    g_free (port_str);

    UzblAuthenticateData *auth_data = g_malloc (sizeof (UzblAuthenticateData));
    auth_data->request = request;

    uzbl_io_schedule_command (authentication_command, args, authenticate, auth_data);

    g_object_ref (request);

    return TRUE;
}
#endif

#if WEBKIT_CHECK_VERSION (1, 11, 4)
void
insecure_content_cb (WebKitWebView *view, WebKitInsecureContentEvent type, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    const char *why = NULL;

    switch (type) {
    case WEBKIT_INSECURE_CONTENT_RUN:
        why = "run";
        break;
    case WEBKIT_INSECURE_CONTENT_DISPLAYED:
        why = "displayed";
        break;
    default:
        why = "unknown";
        break;
    }

    uzbl_events_send (INSECURE_CONTENT, NULL,
        TYPE_STR, why,
        NULL);
}
#endif

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

    return navigation_decision (policy_decision, uri, "", "unknown", 0, 0);
}

static gboolean
mime_decision (WebKitWebPolicyDecision *decision, const gchar *mime_type, const gchar *disposition);

gboolean
mime_policy_cb (WebKitWebView *view, WebKitWebFrame *frame,
        WebKitNetworkRequest *request, gchar *mime_type,
        WebKitWebPolicyDecision *policy_decision, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (frame);
    UZBL_UNUSED (data);

    SoupMessage *soup_message = webkit_network_request_get_message (request);
    SoupMessageHeaders *headers = NULL;

    g_object_get (G_OBJECT (soup_message),
        "response-headers", &headers,
        NULL);

    char *disposition = NULL;

    if (headers) {
        soup_message_headers_get_content_disposition (headers, &disposition, NULL);

        soup_message_headers_free (headers);
    }

    gboolean res = mime_decision (WEBKIT_WEB_POLICY_DECISION (policy_decision), mime_type, disposition);

    g_free (disposition);

    return res;
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

    if (message) {
        SoupURI *soup_uri = soup_uri_new (uri);
        soup_message_set_first_party (message, soup_uri);
    }

    g_object_ref (request);
    request_decision (uri, request);
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

    if (!uzbl.gui.menu_items) {
        return FALSE;
    }

    gint context = get_click_context ();

    /* Check context. */
    if (context == NO_CLICK_CONTEXT) {
        return FALSE;
    }

    if (uzbl_variables_get_int ("default_context_menu")) {
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

    if (!uzbl.gui.menu_items) {
        return FALSE;
    }

    gint context = get_click_context ();

    /* Check context. */
    if (context == NO_CLICK_CONTEXT) {
        return FALSE;
    }

    if (uzbl_variables_get_int ("default_context_menu")) {
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

    gchar *current_geo = uzbl_variables_get_string ("geometry");

    if (!uzbl.gui_->last_geometry || g_strcmp0 (uzbl.gui_->last_geometry, current_geo)) {
        uzbl_events_send (GEOMETRY_CHANGED, NULL,
            TYPE_STR, current_geo,
            NULL);
    }

    g_free (uzbl.gui_->last_geometry);
    uzbl.gui_->last_geometry = current_geo;

    return FALSE;
}

static guint
key_to_modifier (guint keyval);
static gchar *
get_modifier_mask (guint state);

void
send_keypress_event (GdkEventKey *event)
{
    gchar ucs[7];
    gint ulen;
    gchar *keyname;
    guint32 ukval = gdk_keyval_to_unicode(event->keyval);
    gchar *modifiers = NULL;
    guint mod = key_to_modifier (event->keyval);
    guint state;
    GdkModifierType consumed;
    GdkKeymap *keymap = gdk_keymap_get_default ();

    gdk_keymap_translate_keyboard_state (keymap, event->hardware_keycode,
        event->state, event->group,
        NULL, NULL, NULL, &consumed);
    state = event->state & ~consumed;

    /* Get modifier state including this key press/release. */
    modifiers = get_modifier_mask ((event->type == GDK_KEY_PRESS) ? (state | mod) : (state & ~mod));

    if (event->is_modifier && mod) {
        gchar *newmods = get_modifier_mask (mod);

        uzbl_events_send ((event->type == GDK_KEY_PRESS) ? MOD_PRESS : MOD_RELEASE, NULL,
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

        uzbl_events_send ((event->type == GDK_KEY_PRESS) ? KEY_PRESS : KEY_RELEASE, NULL,
            TYPE_STR, modifiers,
            TYPE_STR, ucs,
            NULL);
    } else if ((keyname = gdk_keyval_name (event->keyval))) {
        /* Send keysym for non-printable chars. */
        uzbl_events_send ((event->type == GDK_KEY_PRESS) ? KEY_PRESS : KEY_RELEASE, NULL,
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

    if (!uzbl.gui_->last_button) {
        return NO_CLICK_CONTEXT;
    }

#ifndef USE_WEBKIT2 /* TODO: No API for this? :( */
    WebKitHitTestResult *ht = webkit_web_view_get_hit_test_result (uzbl.gui.web_view, uzbl.gui_->last_button);
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
    g_free (uzbl.gui_->last_selected_url);

    uzbl.gui_->last_selected_url = g_strdup (uzbl.state.selected_url);

    g_free (uzbl.state.selected_url);
    uzbl.state.selected_url = NULL;

    if (uzbl.gui_->last_selected_url && g_strcmp0 (uri, uzbl.gui_->last_selected_url)) {
        uzbl_events_send (LINK_UNHOVER, NULL,
            TYPE_STR, uzbl.gui_->last_selected_url,
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
            gtk_widget_get_window (uzbl.gui.main_window),
            gdk_atom_intern_static_string (prop),
            gdk_atom_intern_static_string ("STRING"),
            CHAR_BIT * sizeof (value[0]),
            GDK_PROP_MODE_REPLACE,
            (const guchar *)value,
            strlen (value));
    }
}

static void
decide_navigation (GString *result, gpointer data);

gboolean
navigation_decision (WebKitWebPolicyDecision *decision, const gchar *uri,
        const gchar *frame, const gchar *type, guint button, guint modifiers)
{
    if (uzbl_variables_get_int ("frozen")) {
        make_policy (decision, ignore);
        return TRUE;
    }

    uzbl_debug ("Navigation requested -> %s\n", uri);

    gchar *handler = uzbl_variables_get_string ("scheme_handler");

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *scheme_command = uzbl_commands_parse (handler, args);

    if (scheme_command) {
        uzbl_commands_args_append (args, g_strdup (uri));
        uzbl_commands_args_append (args, g_strdup (frame));
        uzbl_commands_args_append (args, g_strdup (type));
        uzbl_commands_args_append (args, g_strdup_printf ("%d", button));
        uzbl_commands_args_append (args, get_modifier_mask (modifiers));
        g_object_ref (decision);
        uzbl_io_schedule_command (scheme_command, args, decide_navigation, decision);
    } else {
        make_policy (decision, use);
        uzbl_commands_args_free (args);
    }

    g_free (handler);

    return TRUE;
}

gboolean
request_decision (const gchar *uri, gpointer data)
{
    uzbl_debug ("Request starting -> %s\n", uri);

    uzbl_events_send (REQUEST_STARTING, NULL,
        TYPE_STR, uri,
        NULL);

    gchar *handler = uzbl_variables_get_string ("request_handler");

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *request_command = uzbl_commands_parse (handler, args);

    if (request_command) {
        uzbl_commands_args_append (args, g_strdup (uri));
        uzbl_io_schedule_command (request_command, args, rewrite_request, data);
    } else {
        uzbl_commands_args_free (args);
    }

    g_free (handler);

    return (request_command != NULL);
}

void
send_load_status (WebKitLoadStatus status, const gchar *uri)
{
    UzblEventType event = LAST_EVENT;

    switch (status) {
#ifdef USE_WEBKIT2
    case WEBKIT_LOAD_STARTED:
#else
    case WEBKIT_LOAD_PROVISIONAL:
#endif
        event = LOAD_START;
        break;
#ifdef USE_WEBKIT2
    case WEBKIT_LOAD_REDIRECTED:
        event = LOAD_REDIRECTED;
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
        event = LOAD_COMMIT;
        break;
    case WEBKIT_LOAD_FINISHED:
#ifdef USE_WEBKIT2
        if (uzbl.gui_->load_failed) {
            uzbl.gui_->load_failed = FALSE;
            break;
        }
#endif
        event = LOAD_FINISH;
        break;
    default:
        uzbl_debug ("Unrecognized load status: %d\n", status);
        break;
    }

    if (event != LAST_EVENT) {
        uzbl_events_send (event, NULL,
            TYPE_STR, uri ? uri : "",
            NULL);
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

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 1, 4)
void
authenticate (GString *result, gpointer data)
{
    UzblAuthenticateData *auth = (UzblAuthenticateData *)data;

    gchar **tokens = g_strsplit (result->str, "\n", 0);

    const gchar *action = tokens[0];
    const gchar *username = tokens[1];
    const gchar *password = tokens[2];
    const gchar *persistence = tokens[3];

    WebKitCredential *credential = NULL;
    gboolean cancel = FALSE;

    if (!action) {
        /* Use the default credential. */
        WebKitCredential *webkit_credential = webkit_authentication_request_get_proposed_credential (auth->request);

        if (webkit_credential) {
            credential = webkit_credential_copy (webkit_credential);
        }
    } else if (!g_strcmp0 (action, "IGNORE")) {
        credential = NULL;
    } else if (!g_strcmp0 (action, "CANCEL")) {
        cancel = TRUE;
        credential = NULL;
    } else if (!g_strcmp0 (action, "AUTH") && username && password) {
        WebKitCredentialPersistence persist = WEBKIT_CREDENTIAL_PERSISTENCE_NONE;

        if (persistence) {
            if (!g_strcmp0 (persistence, "none")) {
                persist = WEBKIT_CREDENTIAL_PERSISTENCE_NONE;
            } else if (!g_strcmp0 (persistence, "session")) {
                persist = WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION;
            } else if (!g_strcmp0 (persistence, "permanent")) {
                persist = WEBKIT_CREDENTIAL_PERSISTENCE_PERMANENT;
            }
        }

        webkit_credential_free (credential);
        credential = webkit_credential_new (username, password, persist);
    }

    if (cancel) {
        webkit_authentication_request_cancel (auth->request);
    } else {
        webkit_authentication_request_authenticate (auth->request, credential);
    }

    if (credential) {
        webkit_credential_free (credential);
    }

    g_object_unref (auth->request);

    g_free (auth);
}
#endif
#endif

#ifndef USE_WEBKIT2
gboolean
mime_decision (WebKitWebPolicyDecision *decision, const gchar *mime_type, const gchar *disposition)
{
    if (uzbl_variables_get_int ("frozen")) {
        make_policy (decision, ignore);
        return FALSE;
    }

    gchar *handler = uzbl_variables_get_string ("mime_handler");

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *mime_command = uzbl_commands_parse (handler, args);

    if (mime_command) {
        uzbl_commands_args_append (args, g_strdup (mime_type));
        uzbl_commands_args_append (args, g_strdup (disposition ? disposition : ""));
        g_object_ref (decision);
        uzbl_io_schedule_command (mime_command, args, decide_navigation, decision);
    } else {
        gboolean can_show = webkit_web_view_can_show_mime_type (uzbl.gui.web_view, mime_type);

        if (can_show) {
            /* If we can display it, let's display it... */
            make_policy (decision, use);
        } else {
            /* ...everything we can't display is downloaded. */
            make_policy (decision, download);
        }

        uzbl_commands_args_free (args);
    }

    return TRUE;
}
#endif

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
        TYPE_DOUBLE, value,
        TYPE_DOUBLE, min,
        TYPE_DOUBLE, max,
        TYPE_DOUBLE, page,
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

void
decide_navigation (GString *result, gpointer data)
{
    WebKitWebPolicyDecision *decision = (WebKitWebPolicyDecision *)data;

    if (!g_strcmp0 (result->str, "IGNORE") ||
        !g_strcmp0 (result->str, "USED")) { /* XXX: Deprecated */
        make_policy (decision, ignore);
    } else if (!g_strcmp0 (result->str, "DOWNLOAD")) {
        make_policy (decision, download);
    } else if (!g_strcmp0 (result->str, "USE")) {
        make_policy (decision, use);
    } else {
        make_policy (decision, use);
    }

    g_object_unref (decision);
}

void
rewrite_request (GString *result, gpointer data)
{
#ifdef USE_WEBKIT2
    WebKitResponsePolicyDecision *decision = (WebKitResponsePolicyDecision *)data;
    WebKitURIRequest *request = webkit_response_policy_decision_get_request (decision);
#else
    WebKitNetworkRequest *request = (WebKitNetworkRequest *)data;
#endif

    if (result->len > 0) {
        uzbl_debug ("Request rewritten -> %s\n", result->str);

#ifdef USE_WEBKIT2
        if (!g_strcmp0 (result->str, "IGNORE")) {
            make_policy (decision, ignore);
        } else if (!g_strcmp0 (result->str, "DOWNLOAD")) {
            make_policy (decision, download);
        } else {
            webkit_uri_request_set_uri (request, result->str);
        }
#else
        webkit_network_request_set_uri (request, result->str);
#endif
    }

#ifdef USE_WEBKIT2
    g_object_unref (decision);
#else
    g_object_unref (request);
#endif
}

static void
download_destination (GString *result, gpointer data);

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

    gchar *handler = uzbl_variables_get_string ("download_handler");

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *download_command = uzbl_commands_parse (handler, args);
    g_free (handler);
    if (!download_command) {
        webkit_download_cancel (download);
        uzbl_commands_args_free (args);
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

    uzbl_commands_args_append (args, g_strdup (uri));
    uzbl_commands_args_append (args, g_strdup (suggested_filename));
    uzbl_commands_args_append (args, g_strdup (content_type));
    gchar *total_size_s = g_strdup_printf ("%" G_GUINT64_FORMAT, total_size);
    uzbl_commands_args_append (args, total_size_s);
    uzbl_commands_args_append (args, g_strdup (destination ? destination : ""));

    uzbl_io_schedule_command (download_command, args, download_destination, download);

    return FALSE;
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
send_download_error (const gchar *destination, WebKitDownloadError err, const gchar *message);

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
    UZBL_UNUSED (data);

    const gchar *destination = webkit_download_get_destination (download);

    GError *err = (GError *)error;

    send_download_error (destination, err->code, err->message);
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
    UZBL_UNUSED (error_code);
    UZBL_UNUSED (data);

    const gchar *destination = webkit_download_get_destination_uri (download);

    send_download_error (destination, error_detail, reason);
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
download_destination (GString *result, gpointer data)
{
    WebKitDownload *download = (WebKitDownload *)data;

    /* No response, cancel the download. */
    if (!result->len) {
        webkit_download_cancel (download);
        g_object_unref (download);
        return;
    }

    gboolean is_file_uri = !strprefix (result->str, "file:///");

    gchar *destination_uri;
    /* Convert relative path to absolute path. */
    if ((*result->str != '/') && !is_file_uri) {
        gchar *cwd = g_get_current_dir ();
        destination_uri = g_strconcat ("file://", cwd, "/", result->str, NULL);
        g_free (cwd);
    } else {
        destination_uri = g_strdup (result->str);
    }

    uzbl_events_send (DOWNLOAD_STARTED, NULL,
        TYPE_STR, destination_uri,
        NULL);

#ifdef USE_WEBKIT2
    webkit_download_set_destination (download, destination_uri);
#else
    webkit_download_set_destination_uri (download, destination_uri);
#endif
    g_free (destination_uri);

    g_object_unref (download);
}

void
download_update (WebKitDownload *download)
{
    gdouble progress;
    const gchar *property =
#ifdef USE_WEBKIT2
        "estimated-progress"
#else
        "progress"
#endif
        ;
    g_object_get (download,
        property, &progress,
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
        TYPE_DOUBLE, progress,
        NULL);
}

void
send_download_error (const gchar *destination, WebKitDownloadError err, const gchar *message)
{
    const gchar *str;

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
        TYPE_STR, destination,
        TYPE_STR, str,
        TYPE_INT, err,
        TYPE_STR, message,
        NULL);

}
