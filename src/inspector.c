/*
 ** WebInspector
 ** (c) 2009 by Robert Manea
*/

#include "uzbl-core.h"
#include "events.h"
#include "callbacks.h"


void
hide_window_cb(GtkWidget *widget, gpointer data) {
    (void) data;

    gtk_widget_hide(widget);
}

WebKitWebView*
create_inspector_cb (WebKitWebInspector* web_inspector, WebKitWebView* page, gpointer data){
    (void) data;
    (void) page;
    (void) web_inspector;
    GtkWidget* scrolled_window;
    GtkWidget* new_web_view;
    GUI *g = &uzbl.gui;

    g->inspector_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(g->inspector_window), "delete-event",
            G_CALLBACK(hide_window_cb), NULL);

    gtk_window_set_title(GTK_WINDOW(g->inspector_window), "Uzbl WebInspector");
    gtk_window_set_default_size(GTK_WINDOW(g->inspector_window), 400, 300);
    gtk_widget_show(g->inspector_window);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(g->inspector_window), scrolled_window);
    gtk_widget_show(scrolled_window);

    new_web_view = webkit_web_view_new();
    gtk_container_add(GTK_CONTAINER(scrolled_window), new_web_view);

    return WEBKIT_WEB_VIEW(new_web_view);
}

gboolean
inspector_show_window_cb (WebKitWebInspector* inspector){
    (void) inspector;
    gtk_widget_show(uzbl.gui.inspector_window);

    send_event(WEBINSPECTOR, NULL, TYPE_NAME, "open", NULL);
    return TRUE;
}

/* TODO: Add variables and code to make use of these functions */
gboolean
inspector_close_window_cb (WebKitWebInspector* inspector){
    (void) inspector;
    send_event(WEBINSPECTOR, NULL, TYPE_NAME, "close", NULL);
    return TRUE;
}

gboolean
inspector_attach_window_cb (WebKitWebInspector* inspector){
    (void) inspector;
    return FALSE;
}

gboolean
inspector_detach_window_cb (WebKitWebInspector* inspector){
    (void) inspector;
    return FALSE;
}

gboolean
inspector_uri_changed_cb (WebKitWebInspector* inspector){
    (void) inspector;
    return FALSE;
}

gboolean
inspector_inspector_destroyed_cb (WebKitWebInspector* inspector){
    (void) inspector;
    return FALSE;
}

void
set_up_inspector() {
    GUI *g = &uzbl.gui;
    WebKitWebSettings *settings = view_settings();
    g_object_set(G_OBJECT(settings), "enable-developer-extras", TRUE, NULL);

    uzbl.gui.inspector = webkit_web_view_get_inspector(uzbl.gui.web_view);
    g_signal_connect (G_OBJECT (g->inspector), "inspect-web-view", G_CALLBACK (create_inspector_cb), NULL);
    g_signal_connect (G_OBJECT (g->inspector), "show-window", G_CALLBACK (inspector_show_window_cb), NULL);
    g_signal_connect (G_OBJECT (g->inspector), "close-window", G_CALLBACK (inspector_close_window_cb), NULL);
    g_signal_connect (G_OBJECT (g->inspector), "attach-window", G_CALLBACK (inspector_attach_window_cb), NULL);
    g_signal_connect (G_OBJECT (g->inspector), "detach-window", G_CALLBACK (inspector_detach_window_cb), NULL);
    g_signal_connect (G_OBJECT (g->inspector), "finished", G_CALLBACK (inspector_inspector_destroyed_cb), NULL);

    g_signal_connect (G_OBJECT (g->inspector), "notify::inspected-uri", G_CALLBACK (inspector_uri_changed_cb), NULL);
}
