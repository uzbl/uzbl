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

#include <gtk/gtkimcontextsimple.h>

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#include <gnutls/x509.h>

#include <string.h>

/* TODO: (WebKit2)
 *
 *   - Handle run-file-choose signal?
 *   - Handle script-dialog signal?
 *   - Handle submit-form signal?
 *   - Handle resource-load-started signal?
 *   - Handle print signal (turn hardcopy command into "js page string print()")?
 *   - Handle leave-fullscreen signal?
 *   - Handle enter-fullscreen signal?
 *   - Handle context-menu-dismissed signal?
 *   - Look into WebKitWindowProperties.
 */

struct _UzblGui {
    gchar *last_geometry;
    gchar *last_selected_url;

    GtkIMContext *im_context;
    guint current_key_state;

    GdkEventButton *last_button;

    gboolean load_failed;
#if WEBKIT_CHECK_VERSION (2, 5, 1)
    WebKitUserContentManager *user_manager;
#endif

    WebKitWebView *tmp_web_view;
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

static void
uzbl_input_commit_cb (GtkIMContext *context, const gchar *str, gpointer data);

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

    uzbl.gui_->im_context = gtk_im_context_simple_new ();
    gtk_im_context_reset (uzbl.gui_->im_context);
    g_signal_connect (uzbl.gui_->im_context, "commit",
        G_CALLBACK (uzbl_input_commit_cb), uzbl.gui_);
}

void
uzbl_gui_free ()
{
    g_free (uzbl.gui_->last_geometry);
    g_free (uzbl.gui_->last_selected_url);

    if (uzbl.gui_->last_button) {
        gdk_event_free ((GdkEvent *)uzbl.gui_->last_button);
    }

    if (uzbl.gui_->im_context) {
        g_object_unref (uzbl.gui_->im_context);
    }

#if WEBKIT_CHECK_VERSION (2, 5, 1)
    if (uzbl.gui_->user_manager) {
        g_object_unref (uzbl.gui_->user_manager);
    }
#endif

    if (uzbl.gui_->tmp_web_view) {
        g_object_unref (uzbl.gui_->tmp_web_view);
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
static void
mouse_target_cb (WebKitWebView *view, WebKitHitTestResult *hit_test, guint modifiers, gpointer data);
/* Page metadata events */
static void
title_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data);
static void
progress_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data);
static void
uri_change_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data);
/* Navigation events */
static gboolean
decide_policy_cb (WebKitWebView *view, WebKitPolicyDecision *decision, WebKitPolicyDecisionType type, gpointer data);
static void
load_changed_cb (WebKitWebView *view, WebKitLoadEvent event, gpointer data);
static gboolean
load_failed_cb (WebKitWebView *view, WebKitLoadEvent event, gchar *uri, gpointer web_err, gpointer data);
static gboolean
authenticate_cb (WebKitWebView *view, WebKitAuthenticationRequest *request, gpointer data);
static void
insecure_content_cb (WebKitWebView *view, WebKitInsecureContentEvent type, gpointer data);
static void
download_cb (WebKitWebContext *context, WebKitDownload *download, gpointer data);
static void
extension_cb (WebKitWebContext *context, gpointer data);
static gboolean
permission_cb (WebKitWebView *view, WebKitPermissionRequest *request, gpointer data);
#if WEBKIT_CHECK_VERSION (2, 5, 90)
static gboolean
tls_error_cb (WebKitWebView *view, gchar *uri, GTlsCertificate *cert, GTlsCertificateFlags flags, gpointer data);
#elif WEBKIT_CHECK_VERSION (2, 5, 1)
static gboolean
tls_error_cb (WebKitWebView *view, GTlsCertificate *cert, GTlsCertificateFlags flags, const gchar *host, gpointer data);
static gboolean
tls_error_cb (WebKitWebView *view, WebKitCertificateInfo *info, const gchar *host, gpointer data);
/* UI events */
static GtkWidget *
create_cb (WebKitWebView *view, gpointer data);
static void
close_web_view_cb (WebKitWebView *view, gpointer data);
static gboolean
focus_cb (GtkWidget *widget, GdkEventFocus *event, gpointer data);
static gboolean
context_menu_cb (WebKitWebView *view, GtkMenu *menu, WebKitHitTestResult *hit_test_result,
        gboolean triggered_with_keyboard, gpointer data);
static gboolean
web_process_crashed_cb (WebKitWebView *view, gpointer data);
#if WEBKIT_CHECK_VERSION (2, 7, 3)
static gboolean
show_notification_cb (WebKitWebView *view, WebKitNotification *notification, gpointer data);
#if !WEBKIT_CHECK_VERSION (2, 7, 90)
static gboolean
close_notification_cb (WebKitWebView *view, WebKitNotification *notification, gpointer data);
#endif
#if WEBKIT_CHECK_VERSION (2, 7, 90)
static gboolean
run_color_chooser_cb (WebKitWebView *view, WebKitColorChooserRequest *request, gpointer data);
#endif
#endif
static gboolean
run_file_chooser_cb (WebKitWebView *view, WebKitFileChooserRequest *request, gpointer data);
/* Scrollbar events */
static gboolean
scroll_vert_cb (GtkAdjustment *adjust, gpointer data);
static gboolean
scroll_horiz_cb (GtkAdjustment *adjust, gpointer data);

void
web_view_init ()
{
#if WEBKIT_CHECK_VERSION (2, 5, 1)
    uzbl.gui_->user_manager = webkit_user_content_manager_new ();
    uzbl.gui.web_view = WEBKIT_WEB_VIEW (webkit_web_view_new_with_user_content_manager(uzbl.gui_->user_manager));
#else
    uzbl.gui.web_view = WEBKIT_WEB_VIEW (webkit_web_view_new ());
#endif
    uzbl.gui.scrolled_win = gtk_scrolled_window_new (NULL, NULL);

    gtk_container_add (
        GTK_CONTAINER (uzbl.gui.scrolled_win),
        GTK_WIDGET (uzbl.gui.web_view));

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    g_object_connect (G_OBJECT (context),
        "signal::download-started",                     G_CALLBACK (download_cb),              NULL,
        "signal::initialize-web-extensions",            G_CALLBACK (extension_cb),             NULL,
        NULL);

    g_object_connect (G_OBJECT (uzbl.gui.web_view),
        /* Keyboard events */
        "signal::key-press-event",                      G_CALLBACK (key_press_cb),             NULL,
        "signal::key-release-event",                    G_CALLBACK (key_release_cb),           NULL,
        /* Mouse events */
        "signal::button-press-event",                   G_CALLBACK (button_press_cb),          NULL,
        "signal::button-release-event",                 G_CALLBACK (button_release_cb),        NULL,
        "signal::mouse-target-changed",                 G_CALLBACK (mouse_target_cb),          NULL,
        /* Page metadata events */
        "signal::notify::title",                        G_CALLBACK (title_change_cb),          NULL,
        "signal::notify::estimated-load-progress",      G_CALLBACK (progress_change_cb),       NULL,
        "signal::notify::uri",                          G_CALLBACK (uri_change_cb),            NULL,
        /* Navigation events */
        "signal::decide-policy",                        G_CALLBACK (decide_policy_cb),         NULL,
        "signal::load-changed",                         G_CALLBACK (load_changed_cb),          NULL,
        "signal::load-failed",                          G_CALLBACK (load_failed_cb),           NULL,
        "signal::authenticate",                         G_CALLBACK (authenticate_cb),          NULL,
        "signal::insecure-content-detected",            G_CALLBACK (insecure_content_cb),      NULL,
        "signal::permission-request",                   G_CALLBACK (permission_cb),            NULL,
        "signal::load-failed-with-tls-errors",          G_CALLBACK (tls_error_cb),             NULL,
        /* UI events */
        "signal::create",                               G_CALLBACK (create_cb),                NULL,
        "signal::close",                                G_CALLBACK (close_web_view_cb),        NULL,
        "signal::focus-in-event",                       G_CALLBACK (focus_cb),                 NULL,
        "signal::focus-out-event",                      G_CALLBACK (focus_cb),                 NULL,
        "signal::context-menu",                         G_CALLBACK (context_menu_cb),          NULL,
        "signal::web-process-crashed",                  G_CALLBACK (web_process_crashed_cb),   NULL,
#if WEBKIT_CHECK_VERSION (2, 7, 3)
        "signal::show-notification",                    G_CALLBACK (show_notification_cb),     NULL,
#if !WEBKIT_CHECK_VERSION (2, 7, 90)
        "signal::close-notification",                   G_CALLBACK (close_notification_cb),    NULL,
#endif
#if WEBKIT_CHECK_VERSION (2, 7, 90)
        "signal::run-color-chooser",                    G_CALLBACK (run_color_chooser_cb),     NULL,
#endif
#endif
        "signal::run-file-chooser",                     G_CALLBACK (run_file_chooser_cb),      NULL,
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
    uzbl.gui.vbox = gtk_box_new (FALSE, 0);
    gtk_orientable_set_orientation (GTK_ORIENTABLE (uzbl.gui.vbox), GTK_ORIENTATION_VERTICAL);

    gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.status_bar, FALSE, TRUE, 0);
}

static void
destroy_cb (GtkWidget *widget, gpointer data);
static gboolean
configure_event_cb (GtkWidget *widget, GdkEventConfigure *event, gpointer data);
static void
set_window_property (const gchar *prop, const gchar *value);

void
window_init ()
{
    uzbl.gui.main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    /* TODO: Plumb through from command line. */
    gtk_window_set_default_size (GTK_WINDOW (uzbl.gui.main_window), 800, 600);
    gtk_window_set_title (GTK_WINDOW (uzbl.gui.main_window), "Uzbl");
    gtk_widget_set_name (uzbl.gui.main_window, "Uzbl");

#if !GTK_CHECK_VERSION (3, 13, 5)
    /* TODO: Make into an option? Nope...it doesn't exist in 3.14 anymore
     * because there are no resize grips whatsoever. */
    gtk_window_set_has_resize_grip (GTK_WINDOW (uzbl.gui.main_window), FALSE);
#endif

    /* Fill in the main window */
    gtk_container_add (GTK_CONTAINER (uzbl.gui.main_window), uzbl.gui.vbox);

    g_object_connect (G_OBJECT (uzbl.gui.main_window),
        "signal::destroy",         G_CALLBACK (destroy_cb),         NULL,
        "signal::configure-event", G_CALLBACK (configure_event_cb), NULL,
        NULL);

    uzbl_gui_update_title ();
    if (uzbl.state.uri) {
        set_window_property ("UZBL_URI", uzbl.state.uri);
    }
}

void
plug_init ()
{
    uzbl.gui.plug = GTK_PLUG (gtk_plug_new (uzbl.state.xembed_socket_id));

    gtk_widget_set_name (GTK_WIDGET (uzbl.gui.plug), "Uzbl");

    gtk_container_add (GTK_CONTAINER (uzbl.gui.plug), uzbl.gui.vbox);

    g_object_connect (G_OBJECT (uzbl.gui.plug),
        /* FIXME: Should we really quit GTK if the plug is destroyed? */
        "signal::destroy",           G_CALLBACK (destroy_cb),   NULL,
        "signal::key-press-event",   G_CALLBACK (key_press_cb), NULL,
        "signal::key-release-event", G_CALLBACK (key_press_cb), NULL,
        NULL);
}

static guint
key_to_modifier (guint keyval);
static gchar *
get_modifier_mask (guint state);

static void
uzbl_input_commit_cb (GtkIMContext *context, const gchar *str, gpointer data)
{
    UZBL_UNUSED (context);

    UzblGui *gui = (UzblGui *)data;

    gchar *modifiers = get_modifier_mask (gui->current_key_state);
    uzbl_events_send (KEY_PRESS, NULL,
        TYPE_STR, modifiers,
        TYPE_STR, str,
        NULL);
    g_free (modifiers);
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

    int progress = 100 * webkit_web_view_get_estimated_load_progress (view);

    uzbl_events_send (LOAD_PROGRESS, NULL,
        TYPE_INT, progress,
        NULL);
}

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

#define make_policy(decision, policy) \
    webkit_policy_decision_##policy (WEBKIT_POLICY_DECISION (decision))

static gboolean
navigation_decision (WebKitPolicyDecision *decision, const gchar *uri, const gchar *src_frame,
        const gchar *dest_frame, const gchar *type, guint button, guint modifiers, gboolean is_gesture);
static gboolean
request_decision (const gchar *uri, gpointer data);
static void
send_load_status (WebKitLoadEvent status, const gchar *uri);
static gboolean
send_load_error (const gchar *uri, GError *err);

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
#if WEBKIT_CHECK_VERSION (2, 5, 2)
        WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action (nav_decision);
        WebKitURIRequest *request = webkit_navigation_action_get_request (action);
#define read_navigation_data(name) webkit_navigation_action_get_##name (action)
#else
        WebKitURIRequest *request = webkit_navigation_policy_decision_get_request (nav_decision);
#define read_navigation_data(name) webkit_navigation_policy_decision_get_##name (nav_decision)
#endif

        const gchar *uri = webkit_uri_request_get_uri (request);
        const gchar *dest_frame = webkit_navigation_policy_decision_get_frame_name (nav_decision);
        WebKitNavigationType type = read_navigation_data (navigation_type);
        guint button = read_navigation_data (mouse_button);
        guint modifiers = read_navigation_data (modifiers);
        gboolean is_gesture =
#if WEBKIT_CHECK_VERSION (2, 5, 2)
            webkit_navigation_action_is_user_gesture (action)
#else
            FALSE
#endif
            ;

#define navigation_type_choices(call)                                   \
    call (WEBKIT_NAVIGATION_TYPE_LINK_CLICKED, "link")                  \
    call (WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED, "form_submission")     \
    call (WEBKIT_NAVIGATION_TYPE_BACK_FORWARD, "back_forward")          \
    call (WEBKIT_NAVIGATION_TYPE_RELOAD, "reload")                      \
    call (WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED, "form_resubmission") \
    call (WEBKIT_NAVIGATION_TYPE_OTHER, "other")

#define ENUM_TO_STRING(val, str) \
    case val:                    \
        type_str = str;          \
        break;

        const gchar *type_str = "unknown";
        switch (type) {
        navigation_type_choices (ENUM_TO_STRING)
        default:
            break;
        }

#undef ENUM_TO_STRING
#undef navigation_type_choices

        return navigation_decision (decision, uri, "", dest_frame, type_str, button, modifiers, is_gesture);
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

#define authentication_scheme_choices(call)                                                          \
    call (WEBKIT_AUTHENTICATION_SCHEME_DEFAULT, "default")                                           \
    call (WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC, "http_basic")                                     \
    call (WEBKIT_AUTHENTICATION_SCHEME_HTTP_DIGEST, "http_digest")                                   \
    call (WEBKIT_AUTHENTICATION_SCHEME_HTML_FORM, "html_form")                                       \
    call (WEBKIT_AUTHENTICATION_SCHEME_NTLM, "ntlm")                                                 \
    call (WEBKIT_AUTHENTICATION_SCHEME_NEGOTIATE, "negotiate")                                       \
    call (WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_REQUESTED, "client_certificate_requested") \
    call (WEBKIT_AUTHENTICATION_SCHEME_SERVER_TRUST_EVALUATION_REQUESTED, "server_trust_evaluation_requested")

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

static void
handle_download (WebKitDownload *download, const gchar *suggested_destination);

void
download_cb (WebKitWebContext *context, WebKitDownload *download, gpointer data)
{
    UZBL_UNUSED (context);
    UZBL_UNUSED (download);
    UZBL_UNUSED (data);

#if WEBKIT_CHECK_VERSION (2, 5, 90)
    /* TODO: Set allow-overwrite property? */
#endif

    handle_download (download, NULL);
}

void
extension_cb (WebKitWebContext *context, gpointer data)
{
    UZBL_UNUSED (context);
    UZBL_UNUSED (data);

    /* TODO: Look at using webkit_web_context_set_web_extensions_directory and
     * webkit_web_context_set_web_extensions_initialization_user_data.
     */
    uzbl_events_send (WEB_PROCESS_STARTED, NULL,
        NULL);
}

static gboolean
request_permission (const gchar *uri, const gchar *type, const gchar *desc, GObject *obj);

gboolean
permission_cb (WebKitWebView *view, WebKitPermissionRequest *request, gpointer data)
{
    UZBL_UNUSED (data);

    const gchar *uri = webkit_web_view_get_uri (view);
    const gchar *type = "unknown";
    const gchar *desc = "";

    if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST (request)) {
        type = "geolocation";
#if WEBKIT_CHECK_VERSION (2, 7, 3)
    } else if (WEBKIT_IS_NOTIFICATION_PERMISSION_REQUEST (request)) {
        type = "notification";
    } else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST (request)) {
        WebKitUserMediaPermissionRequest *user_media_request = (WebKitUserMediaPermissionRequest *)request;
        gboolean is_for_audio = webkit_user_media_permission_is_for_audio_device (user_media_request);
        gboolean is_for_video = webkit_user_media_permission_is_for_video_device (user_media_request);

        if (is_for_audio && is_for_video) {
            type = "media:audio,video";
        } else if (is_for_audio) {
            type = "media:audio";
        } else if (is_for_video) {
            type = "media:video";
        } else {
            type = "media:unknown";
        }
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 90)
    } else if (WEBKIT_IS_INSTALL_MISSING_MEDIA_PLUGINS_PERMISSION_REQUEST (request)) {
        WebKitInstallMissingMediaPluginsPermissionRequest *media_plugin_request = (WebKitInstallMissingMediaPluginsPermissionRequest *)request;

        type = "install_media_plugin";
        desc = webkit_install_missing_media_plugins_permission_request_get_description (media_plugin_request);
#endif
    }

    return request_permission (uri, type, desc, G_OBJECT (request));
}

static gboolean
make_tls_error_uri (const gchar *uri, GTlsCertificate *cert, GTlsCertificateFlags flags);
static gboolean
make_tls_error (const gchar *host, GTlsCertificate *cert, GTlsCertificateFlags flags, void *webkit_2_3_1_hack);

#if WEBKIT_CHECK_VERSION (2, 5, 90)
gboolean
tls_error_cb (WebKitWebView *view, gchar *uri, GTlsCertificate *cert, GTlsCertificateFlags flags, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    return make_tls_error_uri (uri, cert, flags);
}
#elif WEBKIT_CHECK_VERSION (2, 5, 1)
gboolean
tls_error_cb (WebKitWebView *view, GTlsCertificate *cert, GTlsCertificateFlags flags, const gchar *host, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    return make_tls_error (host, cert, flags, NULL /* webkit 2.3.1 hack */);
}
#else
gboolean
tls_error_cb (WebKitWebView *view, WebKitCertificateInfo *info, const gchar *host, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    GTlsCertificate *cert = webkit_certificate_info_get_tls_certificate (info);
    GTlsCertificateFlags flags = webkit_certificate_info_get_tls_errors (info);

    WebKitCertificateInfo *info_copy = webkit_certificate_info_copy (info);
    return make_tls_error (host, cert, flags, info_copy);
}
#endif

typedef struct {
#if WEBKIT_CHECK_VERSION (2, 5, 1)
    GTlsCertificate *cert;
#else
    WebKitCertificateInfo *info;
#endif
    gchar *host;
} UzblTlsErrorInfo;

static void
decide_tls_error_policy (GString *result, gpointer data);
static gchar *
get_certificate_info (GTlsCertificate *cert);

gboolean
make_tls_error_uri (const gchar *uri, GTlsCertificate *cert, GTlsCertificateFlags flags)
{
    SoupURI *soup_uri = soup_uri_new (uri);

    if (!soup_uri) {
        uzbl_debug ("Failed to parse URI for TLS failure: %s\n", uri);
        return TRUE;
    }

    const gchar *host = soup_uri_get_host (soup_uri);
    gboolean ret = make_tls_error (host, cert, flags, NULL /* webkit 2.3.1 hack */);
    soup_uri_free (soup_uri);

    return ret;
}

gboolean
make_tls_error (const gchar *host, GTlsCertificate *cert, GTlsCertificateFlags flags, void *info)
{
#if WEBKIT_CHECK_VERSION (2, 5, 1)
    UZBL_UNUSED (info);
#endif

#define tls_error_flags(call)                               \
    call (G_TLS_CERTIFICATE_UNKNOWN_CA, "unknown_ca")       \
    call (G_TLS_CERTIFICATE_BAD_IDENTITY, "bad_identity")   \
    call (G_TLS_CERTIFICATE_NOT_ACTIVATED, "not_activated") \
    call (G_TLS_CERTIFICATE_EXPIRED, "expired")             \
    call (G_TLS_CERTIFICATE_REVOKED, "revoked")             \
    call (G_TLS_CERTIFICATE_INSECURE, "insecure")           \
    call (G_TLS_CERTIFICATE_GENERIC_ERROR, "error")

    GString *flags_str = NULL;

#define CHECK_TLS_FLAG(val, str)                \
    if (flags & val) {                          \
        if (flags_str) {                        \
            g_string_append_c (flags_str, ','); \
            g_string_append (flags_str, str);   \
        } else {                                \
            flags_str = g_string_new (str);     \
        }                                       \
    }

    tls_error_flags (CHECK_TLS_FLAG)

#undef CHECK_TLS_FLAG
#undef tls_error_flags

    if (!flags_str) {
        flags_str = g_string_new ("unknown");
    }

    gchar *cert_info = get_certificate_info (cert);

    uzbl_debug ("TLS Error -> %s\n", host);

    gchar *handler = uzbl_variables_get_string ("tls_error_handler");

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *tls_error_command = uzbl_commands_parse (handler, args);

    g_free (handler);

    if (tls_error_command) {
        uzbl_commands_args_append (args, g_strdup (host));
        uzbl_commands_args_append (args, g_strdup (flags_str->str));
        uzbl_commands_args_append (args, g_strdup (cert_info));
        UzblTlsErrorInfo *error_info = g_malloc (sizeof (UzblTlsErrorInfo));
#if WEBKIT_CHECK_VERSION (2, 5, 1)
        g_object_ref (cert);
        error_info->cert = cert;
#else
        error_info->info = (WebKitCertificateInfo *)info;
#endif
        error_info->host = g_strdup (host);
        uzbl_io_schedule_command (tls_error_command, args, decide_tls_error_policy, error_info);
    } else {
        uzbl_commands_args_free (args);

        uzbl_events_send (TLS_ERROR, NULL,
            TYPE_STR, host,
            TYPE_STR, flags_str->str,
            TYPE_STR, cert_info,
            NULL);
    }

    g_free (cert_info);
    g_string_free (flags_str, TRUE);

    return TRUE;
}

/* UI events */

static WebKitWebView *
create_view (WebKitWebView *view);

GtkWidget *
create_cb (WebKitWebView *view, gpointer data)
{
    UZBL_UNUSED (data);

    return GTK_WIDGET (create_view (view));
}

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

gboolean
web_process_crashed_cb (WebKitWebView *view, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    uzbl_events_send (WEB_PROCESS_CRASHED, NULL,
        NULL);

    return FALSE;
}

#if WEBKIT_CHECK_VERSION (2, 7, 3)
static void
notification_closed_cb (WebKitNotification *notification, gpointer data);

gboolean
show_notification_cb (WebKitWebView *view, WebKitNotification *notification, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    if (!uzbl_variables_get_int ("custom_notifications")) {
        return FALSE;
    }

    guint64 id = webkit_notification_get_id (notification);
    const gchar *title = webkit_notification_get_title (notification);
    const gchar *body = webkit_notification_get_body (notification);

#if WEBKIT_CHECK_VERSION (2, 7, 90)
    g_object_connect (G_OBJECT (notification),
        "signal::closed", G_CALLBACK (notification_closed_cb), NULL,
        NULL);
#endif

    uzbl_events_send (SHOW_NOTIFICATION, NULL,
        TYPE_ULL, id,
        TYPE_STR, title,
        TYPE_STR, body,
        NULL);

    return TRUE;
}

static void
request_close_notification (WebKitNotification *notification);

#if !WEBKIT_CHECK_VERSION (2, 7, 90)
gboolean
close_notification_cb (WebKitWebView *view, WebKitNotification *notification, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    if (!uzbl_variables_get_int ("custom_notifications")) {
        return FALSE;
    }

    request_close_notification (notification);

    return TRUE;
}
#endif

#if WEBKIT_CHECK_VERSION (2, 7, 90)
static void
choose_color (GString *result, gpointer data);

static gboolean
run_color_chooser_cb (WebKitWebView *view, WebKitColorChooserRequest *request, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    gchar *handler = uzbl_variables_get_string ("color_chooser_handler");

    if (!handler || !*handler) {
        return FALSE;
    }

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *color_chooser_command = uzbl_commands_parse (handler, args);

    if (color_chooser_command) {
        GdkRectangle rect;
        char *bounds;

        webkit_color_chooser_request_get_element_rectangle (request, &rect);

        bounds = g_strdup_printf ("%dx%d+%d+%d",
            rect.width, rect.height,
            rect.x, rect.y);
        uzbl_commands_args_append (args, bounds);

        g_object_ref (request);
        uzbl_io_schedule_command (color_chooser_command, args, choose_color, request);
    }

    g_free (handler);

    return (color_chooser_command != NULL);
}
#endif
#endif

static void
choose_file (GString *result, gpointer data);

static gboolean
run_file_chooser_cb (WebKitWebView *view, WebKitFileChooserRequest *request, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    gchar *handler = uzbl_variables_get_string ("file_chooser_handler");

    if (!handler || !*handler) {
        return FALSE;
    }

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *file_chooser_command = uzbl_commands_parse (handler, args);

    if (file_chooser_command) {
        gboolean multiple = webkit_file_chooser_request_get_select_multiple (request);
        const gchar * const * mime_types = webkit_file_chooser_request_get_mime_types (request);
        gchar *mime_types_str;

        if (mime_types) {
            mime_types_str = g_strjoinv (",", (gchar **)mime_types);
        } else {
            mime_types_str = g_strdup ("");
        }

        uzbl_commands_args_append (args, g_strdup (multiple ? "multiple" : "single"));
        uzbl_commands_args_append (args, mime_types_str);

        g_object_ref (request);
        uzbl_io_schedule_command (file_chooser_command, args, choose_file, request);
    }

    g_free (handler);

    return (file_chooser_command != NULL);
}

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

void
set_window_property (const gchar *prop, const gchar *value)
{
    if (uzbl.gui.main_window && GTK_IS_WIDGET (uzbl.gui.main_window))
    {
        GdkWindow *window = gtk_widget_get_window (uzbl.gui.main_window);
        if (!window) {
            return;
        }

        gdk_property_change (
            window,
            gdk_atom_intern_static_string (prop),
            gdk_atom_intern_static_string ("STRING"),
            CHAR_BIT * sizeof (value[0]),
            GDK_PROP_MODE_REPLACE,
            (const guchar *)value,
            strlen (value));
    }
}

guint
key_to_modifier (guint keyval)
{
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
    guint old_state;
    GdkModifierType consumed;
    GdkKeymap *keymap = gdk_keymap_get_default ();

    gdk_keymap_translate_keyboard_state (keymap, event->hardware_keycode,
        event->state, event->group,
        NULL, NULL, NULL, &consumed);
    state = event->state & ~consumed;

    /* Get modifier state including this key press/release. */
    uzbl.gui_->current_key_state = (event->type == GDK_KEY_PRESS) ? (state | mod) : (state & ~mod);
    modifiers = get_modifier_mask (uzbl.gui_->current_key_state);

    /* Set the state to 0 for the input method to process it correctly. The
     * modifiers are taken into account by the previous code anyway */
    old_state = event->state;
    event->state = 0;
    if(gtk_im_context_filter_keypress (uzbl.gui_->im_context, event)) {
        /* If we enter here, that means that the input method already handled
         * the key event. There is nothing else to do then. */
    } else if (event->is_modifier && mod) {
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
    /* Put back the state to its initial value to not disturb further processing
     * of the event */
    event->state = old_state;

    g_free (modifiers);
}

gint
get_click_context ()
{
    guint context = NO_CLICK_CONTEXT;

    if (!uzbl.gui_->last_button) {
        return NO_CLICK_CONTEXT;
    }

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

static void
decide_navigation (GString *result, gpointer data);

gboolean
navigation_decision (WebKitPolicyDecision *decision, const gchar *uri, const gchar *src_frame,
        const gchar *dest_frame, const gchar *type, guint button, guint modifiers, gboolean is_gesture)
{
    if (uzbl_variables_get_int ("frozen")) {
        make_policy (decision, ignore);
        return TRUE;
    }

    uzbl_debug ("Navigation requested -> %s\n", uri);

    uzbl_events_send (NAVIGATION_STARTING, NULL,
        TYPE_STR, uri,
        TYPE_STR, src_frame ? src_frame : "",
        TYPE_STR, dest_frame ? dest_frame : "",
        TYPE_STR, type,
        NULL);

    gchar *handler = uzbl_variables_get_string ("navigation_handler");

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *scheme_command = uzbl_commands_parse (handler, args);

    if (scheme_command) {
        uzbl_commands_args_append (args, g_strdup (uri));
        uzbl_commands_args_append (args, g_strdup (src_frame ? src_frame : ""));
        uzbl_commands_args_append (args, g_strdup (dest_frame ? dest_frame : ""));
        uzbl_commands_args_append (args, g_strdup (type));
        uzbl_commands_args_append (args, g_strdup_printf ("%d", button));
        uzbl_commands_args_append (args, get_modifier_mask (modifiers));
        uzbl_commands_args_append (args, g_strdup (is_gesture ? "true" : "false"));
        g_object_ref (decision);
        uzbl_io_schedule_command (scheme_command, args, decide_navigation, decision);
    } else {
        make_policy (decision, use);
        uzbl_commands_args_free (args);
    }

    g_free (handler);

    return TRUE;
}

static void
rewrite_request (GString *result, gpointer data);

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
        const gchar *can_display = "unknown";

        WebKitResponsePolicyDecision *request = (WebKitResponsePolicyDecision *)data;
        if (webkit_response_policy_decision_is_mime_type_supported (request)) {
            can_display = "can_display";
        } else {
            can_display = "cant_display";
        }

        uzbl_commands_args_append (args, g_strdup (uri));
        uzbl_commands_args_append (args, g_strdup (can_display));

        uzbl_commands_args_append (args, g_strdup ("")); /* frame name */
        uzbl_commands_args_append (args, g_strdup ("unknown")); /* redirect */

        uzbl_io_schedule_command (request_command, args, rewrite_request, data);
    } else {
        uzbl_commands_args_free (args);
    }

    g_free (handler);

    return (request_command != NULL);
}

void
send_load_status (WebKitLoadEvent status, const gchar *uri)
{
    UzblEventType event = LAST_EVENT;

    switch (status) {
    case WEBKIT_LOAD_STARTED:
        uzbl_events_send (LOAD_START, NULL,
            NULL);
        break;
    case WEBKIT_LOAD_REDIRECTED:
        event = LOAD_REDIRECTED;
        break;
    case WEBKIT_LOAD_COMMITTED:
        event = LOAD_COMMIT;
        break;
    case WEBKIT_LOAD_FINISHED:
        if (uzbl.gui_->load_failed) {
            uzbl.gui_->load_failed = FALSE;
            break;
        }
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
send_load_error (const gchar *uri, GError *err)
{
    if (err->code == WEBKIT_NETWORK_ERROR_CANCELLED) {
        uzbl_events_send (LOAD_CANCELLED, NULL,
            TYPE_STR, uri,
            NULL);
    } else {
        uzbl_events_send (LOAD_ERROR, NULL,
            TYPE_STR, uri,
            TYPE_INT, err->code,
            TYPE_STR, err->message,
            NULL);
    }

    return FALSE;
}

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

#if WEBKIT_CHECK_VERSION (2, 7, 2)
void
resource_tls_error_cb (WebKitWebResource *resource, GTlsCertificate *certificate, GTlsCertificateFlags flags, gpointer data)
{
    UZBL_UNUSED (data);

    const gchar *uri = webkit_web_resource_get_uri (resource);
    make_tls_error_uri (uri, certificate, flags);
}
#endif

static gboolean
decide_destination_cb (WebKitDownload *download, const gchar *suggested_filename, gpointer data);
static void
download_finished_cb (WebKitDownload *download, gpointer data);
static void
download_receive_cb (WebKitDownload *download, guint64 length, gpointer data);
static void
download_failed_cb (WebKitDownload *download, gpointer error, gpointer data);

void
handle_download (WebKitDownload *download, const gchar *suggested_destination)
{
    g_object_connect (G_OBJECT (download),
        "signal::decide-destination", G_CALLBACK (decide_destination_cb), suggested_destination,
        "signal::finished",           G_CALLBACK (download_finished_cb),  NULL,
        "signal::received-data",      G_CALLBACK (download_receive_cb),   NULL,
        "signal::failed",             G_CALLBACK (download_failed_cb),    NULL,
        NULL);
}

#define permission_requests(call)                                  \
    call (WEBKIT_IS_PERMISSION_REQUEST, WEBKIT_PERMISSION_REQUEST, \
        webkit_permission_request_allow, webkit_permission_request_deny)
#define allow_request(check, cast, allow, deny) \
    } else if (check (obj)) {                   \
        allow (cast (obj));
#define deny_request(check, cast, allow, deny) \
    } else if (check (obj)) {                  \
        deny (cast (obj));

static void
decide_permission (GString *result, gpointer data);

gboolean
request_permission (const gchar *uri, const gchar *type, const gchar *desc, GObject *obj)
{
    if (uzbl_variables_get_int ("frozen")) {
        if (false) {
        permission_requests (deny_request)
        }
        return TRUE;
    }

    uzbl_debug ("Permission requested -> %s\n", uri);

    gchar *handler = uzbl_variables_get_string ("permission_handler");

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *permission_command = uzbl_commands_parse (handler, args);

    g_free (handler);

    if (permission_command) {
        uzbl_commands_args_append (args, g_strdup (uri));
        uzbl_commands_args_append (args, g_strdup (type));
        uzbl_commands_args_append (args, g_strdup (desc));
        g_object_ref (obj);
        uzbl_io_schedule_command (permission_command, args, decide_permission, obj);
    } else {
        uzbl_commands_args_free (args);

        gboolean allow = !uzbl_variables_get_int ("enable_private") &&
                         uzbl_variables_get_int ("permissive");

        if (allow) {
            if (FALSE) {
            permission_requests (allow_request)
            }
        } else {
            if (FALSE) {
            permission_requests (deny_request)
            }
        }
    }

    return TRUE;
}

void
decide_tls_error_policy (GString *result, gpointer data)
{
    UzblTlsErrorInfo *info = (UzblTlsErrorInfo *)data;

    if (!g_strcmp0 (result->str, "ALLOW")) {
        WebKitWebContext *ctx = webkit_web_view_get_context (uzbl.gui.web_view);
#if WEBKIT_CHECK_VERSION (2, 5, 1)
        webkit_web_context_allow_tls_certificate_for_host (ctx, info->cert, info->host);
#else
        webkit_web_context_allow_tls_certificate_for_host (ctx, info->info, info->host);
#endif
    }

    g_free (info->host);
#if WEBKIT_CHECK_VERSION (2, 5, 1)
    g_object_unref (info->cert);
#else
    webkit_certificate_info_free (info->info);
#endif
    g_free (info);
}

gchar *
get_certificate_info (GTlsCertificate *cert)
{
    gnutls_x509_crt_t tls_cert = NULL;
    GString *info = g_string_new ("{ \"uzbl_tls_info_version\": 0");

    /* Parse out the certificate data. */
    {
        GByteArray *der_info = NULL;
        gnutls_datum_t datum;

        g_object_get (G_OBJECT (cert),
            "certificate", &der_info,
            NULL);
        datum.data = der_info->data;
        datum.size = der_info->len;

        /* Import the certificate into gnutls data structure. */
        gnutls_x509_crt_init (&tls_cert);
        gnutls_x509_crt_import (tls_cert, &datum, GNUTLS_X509_FMT_DER);
        g_byte_array_unref (der_info);
    }

    /* Get the certificate version. */
    {
        g_string_append_printf (info, ", \"version\": %d",
            gnutls_x509_crt_get_version (tls_cert));
    }

    /* Get domain name information. */
    {
        gchar *dn_info = NULL;
        size_t dn_size;
        gnutls_x509_crt_get_dn (tls_cert, NULL, &dn_size);
        dn_info = g_malloc (dn_size);
        gnutls_x509_crt_get_dn (tls_cert, dn_info, &dn_size);
        g_string_append_printf (info, ", domain: \"%s\"", dn_info);
        g_free (dn_info);
    }

    /* Get the algorithm information. */
    {
        int algo = gnutls_x509_crt_get_signature_algorithm (tls_cert);
        const gchar *algo_str;
        gboolean secure = FALSE;

#define tls_sign_algorithm(call)                    \
    call (GNUTLS_SIGN_RSA_SHA1, "rsa_sha1")         \
    call (GNUTLS_SIGN_DSA_SHA1, "dsa_sha1")         \
    call (GNUTLS_SIGN_RSA_MD5, "rsa_md5")           \
    call (GNUTLS_SIGN_RSA_MD2, "rsa_md2")           \
    call (GNUTLS_SIGN_RSA_RMD160, "rsa_rmd160")     \
    call (GNUTLS_SIGN_RSA_SHA256, "rsa_sha256")     \
    call (GNUTLS_SIGN_RSA_SHA384, "rsa_sha384")     \
    call (GNUTLS_SIGN_RSA_SHA512, "rsa_sha512")     \
    call (GNUTLS_SIGN_RSA_SHA224, "rsa_sha224")     \
    call (GNUTLS_SIGN_DSA_SHA224, "dsa_sha224")     \
    call (GNUTLS_SIGN_DSA_SHA256, "dsa_sha256")     \
    call (GNUTLS_SIGN_ECDSA_SHA1, "ecdsa_sha1")     \
    call (GNUTLS_SIGN_ECDSA_SHA224, "ecdsa_sha224") \
    call (GNUTLS_SIGN_ECDSA_SHA256, "ecdsa_sha256") \
    call (GNUTLS_SIGN_ECDSA_SHA384, "ecdsa_sha384") \
    call (GNUTLS_SIGN_ECDSA_SHA512, "ecdsa_sha512")

#define CHECK_ALGO(val, str)                   \
    } else if (algo == val) {                  \
        algo_str = str;                        \
        secure = gnutls_sign_is_secure (algo);

        if (algo < 0) {
            algo_str = "error";
        tls_sign_algorithm (CHECK_ALGO);
        } else {
            algo_str = "unknown";
        }

#undef CHECK_ALGO
#undef tls_sign_algorithm

        g_string_append_printf (info,
            ", \"algorithm\": \"%s\", \"secure\": %s",
            algo_str, secure ? "true" : "false");
    }

    /* Get the certificate fingerprint. */
    {
        const gnutls_digest_algorithm_t algo = GNUTLS_DIG_SHA512;
        const gchar *algo_name = "sha512";
        size_t size = gnutls_hash_get_len (algo);
        gchar *fingerprint = g_malloc (size);
        gnutls_x509_crt_get_fingerprint (tls_cert, algo, fingerprint, &size);
        g_string_append_printf (info,
            "\"fingerprint_digest\": \"%s\", \"fingerprint\": ",
            algo_name);
        for (size_t i = 0; i < size; ++i) {
            g_string_append_printf (info, "%c%x",
                i ? ':' : '\"', fingerprint[i]);
        }
    }

    /* Free the certificate. */
    gnutls_x509_crt_deinit (tls_cert);

    g_string_append_c (info, '}');

    GTlsCertificate *issuer_cert = g_tls_certificate_get_issuer (cert);
    if (issuer_cert) {
        gchar *issuer_info = get_certificate_info (cert);
        g_string_append_printf (info, ", \"issuer\": %s", issuer_info);
        g_free (issuer_info);
    }

    return g_string_free (info, FALSE);
}

static void
create_web_view_uri_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data);

WebKitWebView *
create_view (WebKitWebView *view)
{
#if !WEBKIT_CHECK_VERSION (2, 3, 90)
    UZBL_UNUSED (view);
#endif

    if (!uzbl.gui_->tmp_web_view) {
        /*
         * The URL is not known at this point and destroying the view in the
         * uri notification callback causes segfaults. Instead, create a dummy
         * view which is reused between calls to handle this. It is allocated
         * here to avoid carrying around a full view even if it isn't needed.
         *
         * The new-window-policy-decision-requested signal can't be used
         * because it doesn't fire when Javascript requests a new window with
         * window.open().
         */
        uzbl.gui_->tmp_web_view =
#if WEBKIT_CHECK_VERSION (2, 3, 90)
            WEBKIT_WEB_VIEW (webkit_web_view_new_with_related_view (view))
#else
            WEBKIT_WEB_VIEW (webkit_web_view_new ())
#endif
            ;

        g_object_connect (G_OBJECT (uzbl.gui_->tmp_web_view),
            "signal::notify::uri", G_CALLBACK (create_web_view_uri_cb), NULL,
            NULL);

        g_object_ref_sink (G_OBJECT (uzbl.gui_->tmp_web_view));
    }

    return uzbl.gui_->tmp_web_view;
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

#if WEBKIT_CHECK_VERSION (2, 7, 3)
#if WEBKIT_CHECK_VERSION (2, 7, 90)
void
notification_closed_cb (WebKitNotification *notification, gpointer data)
{
    UZBL_UNUSED (data);

    request_close_notification (notification);
}
#endif

void
request_close_notification (WebKitNotification *notification)
{
    guint64 id = webkit_notification_get_id (notification);

    uzbl_events_send (CLOSE_NOTIFICATION, NULL,
        TYPE_ULL, id,
        NULL);
}
#endif

#if WEBKIT_CHECK_VERSION (2, 7, 90)
void
choose_color (GString *result, gpointer data)
{
    WebKitColorChooserRequest *request = (WebKitColorChooserRequest *)data;
    GdkRGBA color;

    if (gdk_rgba_parse (&color, result->str)) {
        webkit_color_chooser_request_set_rgba (request, &color);
        webkit_color_chooser_request_finish (request);
    } else {
        uzbl_debug ("Failed to parse color: %s\n", result->str);
        webkit_color_chooser_request_cancel (request);
    }

    g_object_unref (request);
}
#endif

void
choose_file (GString *result, gpointer data)
{
    WebKitFileChooserRequest *request = (WebKitFileChooserRequest *)data;
    gboolean multiple = webkit_file_chooser_request_get_select_multiple (request);
    gchar **files = g_strsplit (result->str, "\n", -1);

    if (!result->len || !files || !*files) {
        webkit_file_chooser_request_cancel (request);
    } else if (multiple) {
        webkit_file_chooser_request_select_files (request, (const gchar * const *)files);
    } else {
        const gchar *single_file[] = { files[0], NULL };
        webkit_file_chooser_request_select_files (request, single_file);
    }

    g_strfreev (files);

    g_object_unref (request);
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
    WebKitPolicyDecision *decision = (WebKitPolicyDecision *)data;

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
    WebKitResponsePolicyDecision *decision = (WebKitResponsePolicyDecision *)data;
    WebKitURIRequest *request = webkit_response_policy_decision_get_request (decision);

    if (result->len > 0) {
        uzbl_debug ("Request rewritten -> %s\n", result->str);

        if (!g_strcmp0 (result->str, "IGNORE")) {
            make_policy (decision, ignore);
        } else if (!g_strcmp0 (result->str, "DOWNLOAD")) {
            make_policy (decision, download);
        } else {
            webkit_uri_request_set_uri (request, result->str);
        }
    }

    g_object_unref (decision);
}

static void
download_destination (GString *result, gpointer data);

gboolean
decide_destination_cb (WebKitDownload *download, const gchar *suggested_filename, gpointer data)
{
    /* Get the URI being downloaded. */
    const gchar *uri = webkit_download_get_destination (download);
    const gchar *user_destination = (const gchar *)data;

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
    WebKitURIResponse *response = webkit_download_get_response (download);
    content_type = webkit_uri_response_get_mime_type (response);
    total_size = webkit_uri_response_get_content_length (response);
#if WEBKIT_CHECK_VERSION (2, 5, 90)
    /* TODO: Use response headers? */
#endif

    if (!content_type) {
        content_type = "application/octet-stream";
    }

    uzbl_commands_args_append (args, g_strdup (uri));
    uzbl_commands_args_append (args, g_strdup (suggested_filename));
    uzbl_commands_args_append (args, g_strdup (content_type));
    gchar *total_size_s = g_strdup_printf ("%" G_GUINT64_FORMAT, total_size);
    uzbl_commands_args_append (args, total_size_s);
    uzbl_commands_args_append (args, g_strdup (user_destination ? user_destination : ""));

    uzbl_io_schedule_command (download_command, args, download_destination, download);

    return FALSE;
}

void
download_finished_cb (WebKitDownload *download, gpointer data)
{
    UZBL_UNUSED (data);

    const gchar *dest_uri = webkit_download_get_destination (download);
    const gchar *dest_path = dest_uri + strlen ("file://");

    uzbl_events_send (DOWNLOAD_COMPLETE, NULL,
        TYPE_STR, dest_path,
        NULL);
}

static void
download_update (WebKitDownload *download);
static void
send_download_error (const gchar *destination, WebKitDownloadError err, const gchar *message);

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

void
decide_permission (GString *result, gpointer data)
{
    GObject *obj = (GObject *)data;
    gboolean allow;

    if (!g_strcmp0 (result->str, "ALLOW")) {
        allow = TRUE;
    } else if (!g_strcmp0 (result->str, "DENY")) {
        allow = FALSE;
    } else {
        allow = !uzbl_variables_get_int ("enable_private") &&
                uzbl_variables_get_int ("permissive");
    }

    if (allow) {
        if (FALSE) {
        permission_requests (allow_request)
        }
    } else {
        if (FALSE) {
        permission_requests (deny_request)
        }
    }

    g_object_unref (obj);
}

void
create_web_view_uri_cb (WebKitWebView *view, GParamSpec param_spec, gpointer data)
{
    UZBL_UNUSED (param_spec);
    UZBL_UNUSED (data);

    webkit_web_view_stop_loading (view);
    const gchar *uri = webkit_web_view_get_uri (view);

    static const char *js_protocol = "javascript:";

    if (g_str_has_prefix (uri, js_protocol)) {
        GArray *args = uzbl_commands_args_new ();
        const gchar *js_code = uri + strlen (js_protocol);
        uzbl_commands_args_append (args, g_strdup ("page"));
        uzbl_commands_args_append (args, g_strdup ("string"));
        uzbl_commands_args_append (args, g_strdup (js_code));
        uzbl_commands_run_argv ("js", args, NULL);
        uzbl_commands_args_free (args);
    } else {
        uzbl_events_send (REQ_NEW_WINDOW, NULL,
            TYPE_STR, uri,
            NULL);
    }
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
        return;
    }

    gboolean is_file_uri = g_str_has_prefix (result->str, "file:///");

    gchar *destination_uri;
    /* Convert relative path to absolute path. */
    if (!is_file_uri) {
        if (*result->str == '/') {
            destination_uri = g_strconcat ("file://", result->str, NULL);
        } else {
            gchar *cwd = g_get_current_dir ();
            destination_uri = g_strconcat ("file://", cwd, "/", result->str, NULL);
            g_free (cwd);
        }
    } else {
        destination_uri = g_strdup (result->str);
    }


    int len = strlen (destination_uri);
    if (destination_uri[len - 1] == '\n') {
        destination_uri[len - 1] = '\0';
    }

    uzbl_events_send (DOWNLOAD_STARTED, NULL,
        TYPE_STR, destination_uri + strlen ("file://"),
        NULL);

    webkit_download_set_destination (download, destination_uri);
    g_free (destination_uri);
}

void
download_update (WebKitDownload *download)
{
    gdouble progress;
    g_object_get (download,
        "estimated-progress", &progress,
        NULL);

    const gchar *dest_uri = webkit_download_get_destination (download);
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
        TYPE_STR, destination ? destination : "",
        TYPE_STR, str,
        TYPE_INT, err,
        TYPE_STR, message,
        NULL);
}
