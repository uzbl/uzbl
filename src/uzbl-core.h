#ifndef UZBL_UZBL_CORE_H
#define UZBL_UZBL_CORE_H

#include "webkit.h"

#include "cookie-jar.h"

#include <sys/types.h>

#define uzbl_debug g_debug

/* GUI elements */
typedef struct {
    /* Window */
    GtkWidget     *main_window;
    GtkPlug       *plug;
    GtkWidget     *scrolled_win;
    GtkWidget     *vbox;

    /* Status bar */
    GtkWidget     *status_bar;

    /* Scrolling */
    GtkAdjustment *bar_v;     /* Information about document length */
    GtkAdjustment *bar_h;     /* and scrolling position */

    /* Web page */
    WebKitWebView *web_view;
    gchar         *main_title;

    /* Inspector */
    GtkWidget          *inspector_window;
    WebKitWebInspector *inspector;

    /* Context menu */
    GPtrArray     *menu_items;
} UzblGuiOld;

/* Internal state */
typedef struct {
    gchar          *uri;
    char           *instance_name;
    gchar          *selected_url;
    gboolean        embed;
    gchar          *last_result;
    gboolean        plug_mode;
    JSGlobalContextRef jscontext;

    gboolean        started;
    gboolean        gtk_started;
    gboolean        exit;

    /* Events */
    int             socket_id;
} UzblState;

/* Networking */
typedef struct {
    SoupSession    *soup_session;
    UzblCookieJar  *soup_cookie_jar;
    SoupLogger     *soup_logger;
} UzblNetwork;

struct _UzblCommands;
typedef struct _UzblCommands UzblCommands;

struct _UzblGui;
typedef struct _UzblGui UzblGui;

struct _UzblIO;
typedef struct _UzblIO UzblIO;

struct _UzblRequests;
typedef struct _UzblRequests UzblRequests;

struct _UzblVariables;
typedef struct _UzblVariables UzblVariables;

/* Main uzbl data structure */
typedef struct {
    UzblGuiOld        gui;
    UzblState         state;
    UzblNetwork       net;

    UzblCommands     *commands;
    UzblGui          *gui_;
    UzblIO           *io;
    UzblRequests     *requests;
    UzblVariables    *variables;
} UzblCore;

extern UzblCore uzbl; /* Main Uzbl object */

void
uzbl_init (int *argc, char ***argv);
void
uzbl_free ();

#endif
