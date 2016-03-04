#include "inspector.h"

#include "events.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"

struct _UzblInspector {
    GtkWidget *window;
};

/* =========================== PUBLIC API =========================== */

static gboolean
inspector_create_cb (WebKitWebInspector *inspector, WebKitWebView *view, gpointer data);
static gboolean
inspector_show_window_cb (WebKitWebInspector *inspector, gpointer data);
static gboolean
inspector_close_window_cb (WebKitWebInspector *inspector, gpointer data);
static gboolean
inspector_attach_window_cb (WebKitWebInspector *inspector, gpointer data);
static gboolean
inspector_detach_window_cb (WebKitWebInspector *inspector, gpointer data);
static gboolean
inspector_uri_changed_cb (WebKitWebInspector *inspector, gpointer data);

void
uzbl_inspector_init ()
{
    uzbl.inspector = g_malloc0 (sizeof (UzblInspector));

    WebKitSettings *settings = webkit_web_view_get_settings (uzbl.gui.web_view);
    g_object_set (G_OBJECT (settings),
        "enable-developer-extras", TRUE,
        NULL);

    uzbl.gui.inspector = webkit_web_view_get_inspector (uzbl.gui.web_view);

    g_object_connect (G_OBJECT (uzbl.gui.inspector),
        "signal::bring-to-front",        G_CALLBACK (inspector_create_cb),              NULL,
        "signal::closed",                G_CALLBACK (inspector_close_window_cb),        NULL,
        "signal::attach",                G_CALLBACK (inspector_attach_window_cb),       NULL,
        "signal::detach",                G_CALLBACK (inspector_detach_window_cb),       NULL,
        "signal::notify::inspected-uri", G_CALLBACK (inspector_uri_changed_cb),         NULL,
        NULL);
}

void
uzbl_inspector_free ()
{
    g_free (uzbl.inspector);
    uzbl.inspector = NULL;
}

/* ==================== CALLBACK IMPLEMENTATIONS ==================== */

gboolean
inspector_create_cb (WebKitWebInspector *inspector, WebKitWebView *view, gpointer data)
{
    UZBL_UNUSED (view);
    UZBL_UNUSED (data);

    inspector_show_window_cb (inspector, NULL);

    return TRUE;
}

gboolean
inspector_show_window_cb (WebKitWebInspector *inspector, gpointer data)
{
    UZBL_UNUSED (inspector);
    UZBL_UNUSED (data);

    uzbl_events_send (WEBINSPECTOR, NULL,
        TYPE_NAME, "open",
        NULL);

    return TRUE;
}

gboolean
inspector_close_window_cb (WebKitWebInspector *inspector, gpointer data)
{
    UZBL_UNUSED (inspector);
    UZBL_UNUSED (data);

    uzbl_events_send (WEBINSPECTOR, NULL,
        TYPE_NAME, "close",
        NULL);

    return TRUE;
}

/* TODO: Add variables and code to make use of these functions. */
gboolean
inspector_attach_window_cb (WebKitWebInspector *inspector, gpointer data)
{
    UZBL_UNUSED (inspector);
    UZBL_UNUSED (data);

    return FALSE;
}

gboolean
inspector_detach_window_cb (WebKitWebInspector *inspector, gpointer data)
{
    UZBL_UNUSED (inspector);
    UZBL_UNUSED (data);

    return FALSE;
}

gboolean
inspector_uri_changed_cb (WebKitWebInspector *inspector, gpointer data)
{
    UZBL_UNUSED (inspector);
    UZBL_UNUSED (data);

    return FALSE;
}
