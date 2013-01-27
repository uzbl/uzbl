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

gboolean
focus_cb(GtkWidget* window, GdkEventFocus* event, void *ud) {
    (void) window;
    (void) event;
    (void) ud;

    send_event (event->in?FOCUS_GAINED:FOCUS_LOST, NULL, NULL);

    return FALSE;
}

void
close_web_view_cb(WebKitWebView *webview, gpointer user_data) {
    (void) webview; (void) user_data;
    send_event (CLOSE_WINDOW, NULL, NULL);
}

void
create_web_view_js_cb (WebKitWebView* web_view, GParamSpec param_spec) {
    (void) web_view;
    (void) param_spec;

    webkit_web_view_stop_loading(web_view);
    const gchar* uri = webkit_web_view_get_uri(web_view);

    if (strncmp(uri, "javascript:", strlen("javascript:")) == 0) {
        eval_js(uzbl.gui.web_view, (gchar*) uri + strlen("javascript:"), NULL, "javascript:");
        gtk_widget_destroy(GTK_WIDGET(web_view));
    }
    else
        send_event(NEW_WINDOW, NULL, TYPE_STR, uri, NULL);
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

/* vi: set et ts=4: */
