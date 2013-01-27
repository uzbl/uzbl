/*
 ** GUI code
 ** (c) 2009-2013 by Robert Manea et al.
*/

#include "gui.h"

#include "uzbl-core.h"
#include "callbacks.h"
#include "variables.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <glib.h>

static void
uzbl_status_bar_init (void);
static void
uzbl_web_view_init (void);
static void
uzbl_vbox_init (void);
static void
uzbl_window_init (void);
static void
uzbl_plug_init (void);

void
uzbl_gui_init (gboolean plugmode)
{
    uzbl_status_bar_init ();
    uzbl_web_view_init ();
    uzbl_vbox_init ();

    if (plugmode) {
        uzbl_plug_init ();
    } else {
        uzbl_window_init ();
    }
}

void
uzbl_status_bar_init (void)
{
    uzbl.gui.status_bar = uzbl_status_bar_new ();

    g_object_connect (G_OBJECT (uzbl.gui.status_bar),
        "signal::key-press-event",   (GCallback)key_press_cb, NULL,
        "signal::key-release-event", (GCallback)key_press_cb, NULL,
        NULL);

    /*
    g_object_connect (G_OBJECT (UZBL_STATUS_BAR (uzbl.gui.status_bar)->label_left),
      "signal::key-press-event",   (GCallback)key_press_cb,   NULL,
      "signal::key-release-event", (GCallback)key_release_cb, NULL,
      NULL);

    g_object_connect (G_OBJECT (UZBL_STATUS_BAR (uzbl.gui.status_bar)->label_right),
      "signal::key-press-event",   (GCallback)key_press_cb,   NULL,
      "signal::key-release-event", (GCallback)key_release_cb, NULL,
      NULL);
      */
}

void
uzbl_web_view_init (void)
{
    uzbl.gui.web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    uzbl.gui.scrolled_win = gtk_scrolled_window_new(NULL, NULL);

    gtk_container_add(
        GTK_CONTAINER(uzbl.gui.scrolled_win),
        GTK_WIDGET(uzbl.gui.web_view)
    );

    g_object_connect((GObject*)uzbl.gui.web_view,
      "signal::key-press-event",                      (GCallback)key_press_cb,             NULL,
      "signal::key-release-event",                    (GCallback)key_release_cb,           NULL,
      "signal::button-press-event",                   (GCallback)button_press_cb,          NULL,
      "signal::button-release-event",                 (GCallback)button_release_cb,        NULL,
      "signal::notify::title",                        (GCallback)title_change_cb,          NULL,
      "signal::notify::progress",                     (GCallback)progress_change_cb,       NULL,
      "signal::notify::load-status",                  (GCallback)load_status_change_cb,    NULL,
      "signal::notify::uri",                          (GCallback)uri_change_cb,            NULL,
      "signal::load-error",                           (GCallback)load_error_cb,            NULL,
      "signal::hovering-over-link",                   (GCallback)link_hover_cb,            NULL,
      "signal::navigation-policy-decision-requested", (GCallback)navigation_decision_cb,   NULL,
      "signal::close-web-view",                       (GCallback)close_web_view_cb,        NULL,
      "signal::download-requested",                   (GCallback)download_cb,              NULL,
      "signal::create-web-view",                      (GCallback)create_web_view_cb,       NULL,
      "signal::mime-type-policy-decision-requested",  (GCallback)mime_policy_cb,           NULL,
      "signal::resource-request-starting",            (GCallback)request_starting_cb,      NULL,
#if WEBKIT_CHECK_VERSION (1, 9, 0)
      "signal::context-menu",                         (GCallback)context_menu_cb,          NULL,
#else
      "signal::populate-popup",                       (GCallback)populate_popup_cb,        NULL,
#endif
      "signal::focus-in-event",                       (GCallback)focus_cb,                 NULL,
      "signal::focus-out-event",                      (GCallback)focus_cb,                 NULL,
      "signal::window-object-cleared",                (GCallback)window_object_cleared_cb, NULL,
      NULL);

    uzbl.gui.bar_h = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (uzbl.gui.scrolled_win));
    uzbl.gui.bar_v = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (uzbl.gui.scrolled_win));

    g_object_connect(G_OBJECT (uzbl.gui.bar_v),
        "signal::value-changed", (GCallback)scroll_vert_cb, NULL,
        "signal::changed",       (GCallback)scroll_vert_cb, NULL,
        NULL);
    g_object_connect(G_OBJECT (uzbl.gui.bar_h),
        "signal::value-changed", (GCallback)scroll_horiz_cb, NULL,
        "signal::changed",       (GCallback)scroll_horiz_cb, NULL,
        NULL);

}

void
uzbl_vbox_init (void)
{
#if GTK_CHECK_VERSION(3,0,0)
    uzbl.gui.vbox = gtk_box_new(FALSE, 0);
    gtk_orientable_set_orientation(GTK_ORIENTABLE(uzbl.gui.vbox), GTK_ORIENTATION_VERTICAL);
#else
    uzbl.gui.vbox = gtk_vbox_new(FALSE, 0);
#endif

    gtk_box_pack_start(GTK_BOX(uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(uzbl.gui.vbox), uzbl.gui.status_bar, FALSE, TRUE, 0);
}

void
uzbl_window_init (void)
{
    uzbl.gui.main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    gtk_window_set_default_size (GTK_WINDOW (uzbl.gui.main_window), 800, 600);
    gtk_window_set_title(GTK_WINDOW(uzbl.gui.main_window), "Uzbl");
    gtk_widget_set_name (GTK_WIDGET (uzbl.gui.main_window), "Uzbl");

#if GTK_CHECK_VERSION (3, 0, 0)
    gtk_window_set_has_resize_grip (GTK_WINDOW (uzbl.gui.main_window), FALSE);
#endif

    /* Fill in the main window */
    gtk_container_add (GTK_CONTAINER (uzbl.gui.main_window), uzbl.gui.vbox);

    g_signal_connect (G_OBJECT (uzbl.gui.main_window), "destroy",         G_CALLBACK (destroy_cb),         NULL);
    g_signal_connect (G_OBJECT (uzbl.gui.main_window), "configure-event", G_CALLBACK (configure_event_cb), NULL);
}

void
uzbl_plug_init (void)
{
    uzbl.gui.plug = GTK_PLUG (gtk_plug_new (uzbl.state.socket_id));

    gtk_widget_set_name (GTK_WIDGET(uzbl.gui.plug), "Uzbl");

    gtk_container_add (GTK_CONTAINER (uzbl.gui.plug), uzbl.gui.vbox);

    g_signal_connect (G_OBJECT (uzbl.gui.plug), "destroy",           G_CALLBACK (destroy_cb),   NULL);
    g_signal_connect (G_OBJECT (uzbl.gui.plug), "key-press-event",   G_CALLBACK (key_press_cb), NULL);
    g_signal_connect (G_OBJECT (uzbl.gui.plug), "key-release-event", G_CALLBACK (key_press_cb), NULL);
}
