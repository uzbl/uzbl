#include "variables.h"
#include "uzbl-core.h"
#include "callbacks.h"
#include "events.h"
#include "io.h"
#include "util.h"

#include <stdlib.h>

uzbl_cmdprop *
get_var_c(const gchar *name) {
    return g_hash_table_lookup(uzbl.behave.proto_var, name);
}

void
send_set_var_event(const char *name, const uzbl_cmdprop *c) {
    /* check for the variable type */
    switch(c->type) {
    case TYPE_STR:
    {
        gchar *v = get_var_value_string_c(c);
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "str",
            TYPE_STR, v,
            NULL);
        g_free(v);
        break;
    }
    case TYPE_INT:
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "int",
            TYPE_INT, get_var_value_int_c(c),
            NULL);
        break;
    case TYPE_FLOAT:
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "float",
            TYPE_FLOAT, get_var_value_float_c(c),
            NULL);
        break;
    default:
        g_assert_not_reached();
    }
}

void
expand_variable(GString *buf, const gchar *name) {
    uzbl_cmdprop* c = get_var_c(name);
    if(c) {
        if(c->type == TYPE_STR) {
            gchar *v = get_var_value_string_c(c);
            g_string_append(buf, v);
            g_free(v);
        } else if(c->type == TYPE_INT)
            g_string_append_printf(buf, "%d", get_var_value_int_c(c));
        else if(c->type == TYPE_FLOAT)
            g_string_append_printf(buf, "%f", get_var_value_float_c(c));
    }
}

void
set_var_value_string_c(uzbl_cmdprop *c, const gchar *val) {
    if(c->setter)
        ((void (*)(const gchar *))c->setter)(val);
    else {
        g_free(*(c->ptr.s));
        *(c->ptr.s) = g_strdup(val);
    }
}

void
set_var_value_int_c(uzbl_cmdprop *c, int i) {
    if(c->setter)
        ((void (*)(int))c->setter)(i);
    else
        *(c->ptr.i) = i;
}

void
set_var_value_float_c(uzbl_cmdprop *c, float f) {
    if(c->setter)
        ((void (*)(float))c->setter)(f);
    else
        *(c->ptr.f) = f;
}

gboolean
set_var_value(const gchar *name, gchar *val) {
    g_assert(val != NULL);

    uzbl_cmdprop *c = get_var_c(name);

    if(c) {
        if(!c->writeable) return FALSE;

        switch(c->type) {
        case TYPE_STR:
            set_var_value_string_c(c, val);
            break;
        case TYPE_INT:
        {
            int i = (int)strtoul(val, NULL, 10);
            set_var_value_int_c(c, i);
            break;
        }
        case TYPE_FLOAT:
        {
            float f = strtod(val, NULL);
            set_var_value_float_c(c, f);
            break;
        }
        default:
            g_assert_not_reached();
        }

        send_set_var_event(name, c);
    } else {
        /* a custom var that has not been set. */
        /* check whether name violates our naming scheme */
        if(!valid_name(name)) {
            if (uzbl.state.verbose)
                printf("Invalid variable name: %s\n", name);
            return FALSE;
        }

        /* create the cmdprop */
        c = g_malloc(sizeof(uzbl_cmdprop));
        c->type       = TYPE_STR;
        c->dump       = 0;
        c->getter     = NULL;
        c->setter     = NULL;
        c->writeable  = 1;

        c->ptr.s    = g_malloc(sizeof(gchar*));

        g_hash_table_insert(uzbl.behave.proto_var,
                g_strdup(name), (gpointer) c);

        /* set the value. */
        *(c->ptr.s) = g_strdup(val);

        send_set_var_event(name, c);
    }
    update_title();
    return TRUE;
}

gchar*
get_var_value_string_c(const uzbl_cmdprop *c) {
    if(!c) return NULL;

    gchar *result = NULL;

    if(c->getter) {
        result = ((gchar *(*)())c->getter)();
    } else if(c->ptr.s)
        result = g_strdup(*(c->ptr.s));

    return result ? result : g_strdup("");
}

gchar*
get_var_value_string(const gchar *name) {
    uzbl_cmdprop *c = get_var_c(name);
    return get_var_value_string_c(c);
}

int
get_var_value_int_c(const uzbl_cmdprop *c) {
    if(!c) return 0;

    if(c->getter) {
        return ((int (*)())c->getter)();
    } else if(c->ptr.i)
        return *(c->ptr.i);

    return 0;
}

int
get_var_value_int(const gchar *name) {
    uzbl_cmdprop *c = get_var_c(name);
    return get_var_value_int_c(c);
}

float
get_var_value_float_c(const uzbl_cmdprop *c) {
    if(!c) return 0;

    if(c->getter) {
        return ((float (*)())c->getter)();
    } else if(c->ptr.f)
        return *(c->ptr.f);

    return 0;
}

float
get_var_value_float(const gchar *name) {
    uzbl_cmdprop *c = get_var_c(name);
    return get_var_value_float_c(c);
}

static void
dump_var_hash(gpointer k, gpointer v, gpointer ud) {
    (void) ud;
    uzbl_cmdprop *c = v;

    if(!c->dump)
        return;

    if(c->type == TYPE_STR) {
        gchar *v = get_var_value_string_c(c);
        printf("set %s = %s\n", (char *)k, v);
        g_free(v);
    } else if(c->type == TYPE_INT)
        printf("set %s = %d\n", (char *)k, get_var_value_int_c(c));
    else if(c->type == TYPE_FLOAT)
        printf("set %s = %f\n", (char *)k, get_var_value_float_c(c));
}

void
dump_config() {
    g_hash_table_foreach(uzbl.behave.proto_var, dump_var_hash, NULL);
}

static void
dump_var_hash_as_event(gpointer k, gpointer v, gpointer ud) {
    (void) ud;
    uzbl_cmdprop *c = v;

    if(c->dump)
        send_set_var_event(k, c);
}

void
dump_config_as_events() {
    g_hash_table_foreach(uzbl.behave.proto_var, dump_var_hash_as_event, NULL);
}

/* is the given string made up entirely of decimal digits? */
static gboolean
string_is_integer(const char *s) {
    return (strspn(s, "0123456789") == strlen(s));
}


static GObject *
cookie_jar() {
    return G_OBJECT(uzbl.net.soup_cookie_jar);
}

static GObject *
view_settings() {
    return G_OBJECT(webkit_web_view_get_settings(uzbl.gui.web_view));
}

static void
set_window_property(const gchar* prop, const gchar* value) {
    if(GTK_IS_WIDGET(uzbl.gui.main_window)) {
        gdk_property_change(
            gtk_widget_get_window (GTK_WIDGET (uzbl.gui.main_window)),
            gdk_atom_intern_static_string(prop),
            gdk_atom_intern_static_string("STRING"),
            8,
            GDK_PROP_MODE_REPLACE,
            (const guchar*)value,
            strlen(value));
    }
}

void
uri_change_cb (WebKitWebView *web_view, GParamSpec param_spec) {
    (void) param_spec;

    g_free (uzbl.state.uri);
    g_object_get (web_view, "uri", &uzbl.state.uri, NULL);

    g_setenv("UZBL_URI", uzbl.state.uri, TRUE);
    set_window_property("UZBL_URI", uzbl.state.uri);
}

static gchar *
make_uri_from_user_input(const gchar *uri) {
    gchar *result = NULL;

    SoupURI *soup_uri = soup_uri_new(uri);
    if (soup_uri) {
        /* this looks like a valid URI. */
        if(soup_uri->host == NULL && string_is_integer(soup_uri->path))
            /* the user probably typed in a host:port without a scheme. */
            result = g_strconcat("http://", uri, NULL);
        else
            result = g_strdup(uri);

        soup_uri_free(soup_uri);

        return result;
    }

    /* it's not a valid URI, maybe it's a path on the filesystem?
     * check to see if such a path exists. */
    if (file_exists(uri)) {
        if (g_path_is_absolute (uri))
            return g_strconcat("file://", uri, NULL);

        /* make it into an absolute path */
        gchar *wd = g_get_current_dir ();
        result = g_strconcat("file://", wd, "/", uri, NULL);
        g_free(wd);

        return result;
    }

    /* not a path on the filesystem, just assume it's an HTTP URL. */
    return g_strconcat("http://", uri, NULL);
}

static void
set_uri(const gchar *uri) {
    /* Strip leading whitespace */
    while (*uri && isspace(*uri))
        uri++;

    /* don't do anything when given a blank URL */
    if(uri[0] == 0)
        return;

    g_free(uzbl.state.uri);
    uzbl.state.uri = g_strdup(uri);

    /* evaluate javascript: URIs */
    if (!strncmp (uri, "javascript:", 11)) {
        eval_js(uzbl.gui.web_view, uri, NULL, "javascript:");
        return;
    }

    /* attempt to parse the URI */
    gchar *newuri = make_uri_from_user_input(uri);

    set_window_property("UZBL_URI", newuri);
    webkit_web_view_load_uri (uzbl.gui.web_view, newuri);

    g_free (newuri);
}

static void
set_max_conns(int max_conns) {
    uzbl.net.max_conns = max_conns;

    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS, uzbl.net.max_conns, NULL);
}

static void
set_max_conns_host(int max_conns_host) {
    uzbl.net.max_conns_host = max_conns_host;

    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS_PER_HOST, uzbl.net.max_conns_host, NULL);
}

static void
set_http_debug(int debug) {
    uzbl.behave.http_debug = debug;

    if(uzbl.net.soup_logger) {
        soup_session_remove_feature
            (uzbl.net.soup_session, SOUP_SESSION_FEATURE(uzbl.net.soup_logger));
        g_object_unref (uzbl.net.soup_logger);
    }

    uzbl.net.soup_logger = soup_logger_new(uzbl.behave.http_debug, -1);
    soup_session_add_feature(uzbl.net.soup_session,
            SOUP_SESSION_FEATURE(uzbl.net.soup_logger));
}

static void
set_ca_file(gchar *path) {
    g_object_set (uzbl.net.soup_session, "ssl-ca-file", path, NULL);
}

static gchar *
get_ca_file() {
    gchar *path;
    g_object_get (uzbl.net.soup_session, "ssl-ca-file", &path, NULL);
    return path;
}

static void
set_verify_cert(int strict) {
    g_object_set (uzbl.net.soup_session, "ssl-strict", strict, NULL);
}

static int
get_verify_cert() {
    int strict;
    g_object_get (uzbl.net.soup_session, "ssl-strict", &strict, NULL);
    return strict;
}

#define EXPOSE_SOUP_COOKIE_JAR_SETTINGS(SYM, PROPERTY, TYPE) \
void set_##SYM(TYPE val) { \
  g_object_set(cookie_jar(), (PROPERTY), val, NULL); \
} \
TYPE get_##SYM() { \
  TYPE val; \
  g_object_get(cookie_jar(), (PROPERTY), &val, NULL); \
  return val; \
}

EXPOSE_SOUP_COOKIE_JAR_SETTINGS(cookie_policy,    "accept-policy",    int)

#undef EXPOSE_SOUP_COOKIE_JAR_SETTINGS


#define EXPOSE_WEBKIT_VIEW_SETTINGS(SYM, PROPERTY, TYPE) \
static void set_##SYM(TYPE val) { \
  g_object_set(view_settings(), (PROPERTY), val, NULL); \
} \
static TYPE get_##SYM() { \
  TYPE val; \
  g_object_get(view_settings(), (PROPERTY), &val, NULL); \
  return val; \
}

/* Font settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(default_font_family,          "default-font-family",                       gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(monospace_font_family,        "monospace-font-family",                     gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(sans_serif_font_family,       "sans_serif-font-family",                    gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(serif_font_family,            "serif-font-family",                         gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(cursive_font_family,          "cursive-font-family",                       gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(fantasy_font_family,          "fantasy-font-family",                       gchar *)

/* Font size settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(minimum_font_size,            "minimum-font-size",                         int)
EXPOSE_WEBKIT_VIEW_SETTINGS(font_size,                    "default-font-size",                         int)
EXPOSE_WEBKIT_VIEW_SETTINGS(monospace_size,               "default-monospace-font-size",               int)

/* Text settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(default_encoding,             "default-encoding",                          gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(enforce_96_dpi,               "enforce-96-dpi",                            int)

/* Feature settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_plugins,               "enable-plugins",                            int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_java_applet,           "enable-java-applet",                        int)
#if WEBKIT_CHECK_VERSION (1, 3, 14)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_webgl,                 "enable-webgl",                              int)
#endif
#if WEBKIT_CHECK_VERSION (1, 7, 5)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_webaudio,             "enable-webaudio",                           int)
#endif
#if WEBKIT_CHECK_VERSION (1, 7, 90) // Documentation says 1.7.5, but it's not there.
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_3d_acceleration,       "enable-accelerated-compositing",            int)
#endif

/* HTML5 Database settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_database,              "enable-html5-database",                     int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_local_storage,         "enable-html5-local-storage",                int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_pagecache,             "enable-page-cache",                         int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_offline_app_cache,     "enable-offline-web-application-cache",      int)
#if WEBKIT_CHECK_VERSION (1, 5, 2)
EXPOSE_WEBKIT_VIEW_SETTINGS(local_storage_path,           "html5-local-storage-database-path",         gchar *)
#endif

/* Security settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_private_webkit,        "enable-private-browsing",                   int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_universal_file_access, "enable-universal-access-from-file-uris",    int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_cross_file_access,     "enable-file-access-from-file-uris",         int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_hyperlink_auditing,    "enable-hyperlink-auditing",                 int)
#if WEBKIT_CHECK_VERSION (1, 3, 13)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_dns_prefetch,          "enable-dns-prefetching",                    int)
#endif

/* Display settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(zoom_step,                    "zoom-step",                                 float)
EXPOSE_WEBKIT_VIEW_SETTINGS(caret_browsing,               "enable-caret-browsing",                     int)
EXPOSE_WEBKIT_VIEW_SETTINGS(auto_resize_window,           "auto-resize-window",                        int)
#if WEBKIT_CHECK_VERSION (1, 3, 5)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_frame_flattenting,     "enable-frame-flattening",                   int)
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 8)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_fullscreen,            "enable-fullscreen",                         int)
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 91)
EXPOSE_WEBKIT_VIEW_SETTINGS(zoom_text_only,               "zoom-text-only",                            int)
#endif
#endif

/* Javascript settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_scripts,               "enable-scripts",                            int)
EXPOSE_WEBKIT_VIEW_SETTINGS(javascript_windows,           "javascript-can-open-windows-automatically", int)
EXPOSE_WEBKIT_VIEW_SETTINGS(javascript_dom_paste,         "enable-dom-paste",                          int)
#if WEBKIT_CHECK_VERSION (1, 3, 0)
EXPOSE_WEBKIT_VIEW_SETTINGS(javascript_clipboard,         "javascript-can-access-clipboard",           int)
#endif

/* Image settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(autoload_images,              "auto-load-images",                          int)
EXPOSE_WEBKIT_VIEW_SETTINGS(autoshrink_images,            "auto-shrink-images",                        int)

/* Spell checking settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_spellcheck,            "enable-spell-checking",                     int)
EXPOSE_WEBKIT_VIEW_SETTINGS(spellcheck_languages,         "spell-checking-languages",                  gchar *)

/* Form settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(resizable_text_areas,         "resizable-text-areas",                      int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_spatial_navigation,    "enable-spatial-navigation",                 int)
EXPOSE_WEBKIT_VIEW_SETTINGS(editing_behavior,             "editing-behavior",                          int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_tab_cycle,             "tab-key-cycles-through-elements",           int)

/* Customization */
EXPOSE_WEBKIT_VIEW_SETTINGS(stylesheet_uri,               "user-stylesheet-uri",                       gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(default_context_menu,         "enable-default-context-menu",               int)

/* Hacks */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_site_workarounds,      "enable-site-specific-quirks",               int)

/* Printing settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(print_bg,                     "print-backgrounds",                         int)

#undef EXPOSE_WEBKIT_VIEW_SETTINGS

static void
set_enable_private (int private) {
    const char *priv_envvar = "UZBL_PRIVATE";

    if (private)
        setenv (priv_envvar, "true", 1);
    else
        unsetenv (priv_envvar);

    set_enable_private_webkit (private);
}

static int
get_enable_private () {
    return get_enable_private_webkit ();
}

static void
set_proxy_url(const gchar *proxy_url) {
    g_free(uzbl.net.proxy_url);
    uzbl.net.proxy_url = g_strdup(proxy_url);

    const gchar *url = uzbl.net.proxy_url;
    SoupSession *session  = uzbl.net.soup_session;
    SoupURI     *soup_uri = NULL;

    if (url != NULL || *url != 0 || *url != ' ')
        soup_uri = soup_uri_new(url);

    g_object_set(G_OBJECT(session), SOUP_SESSION_PROXY_URI, soup_uri, NULL);

    if(soup_uri)
        soup_uri_free(soup_uri);
}

static void
set_authentication_handler(const gchar *handler) {
    /* Check if WEBKIT_TYPE_SOUP_AUTH_DIALOG feature is set */
    GSList *flist = soup_session_get_features (uzbl.net.soup_session, (GType) WEBKIT_TYPE_SOUP_AUTH_DIALOG);
    guint feature_is_set = g_slist_length(flist);
    g_slist_free(flist);

    g_free(uzbl.behave.authentication_handler);
    uzbl.behave.authentication_handler = g_strdup(handler);

    if (uzbl.behave.authentication_handler == NULL || *uzbl.behave.authentication_handler == 0) {
        if (!feature_is_set)
            soup_session_add_feature_by_type
                (uzbl.net.soup_session, (GType) WEBKIT_TYPE_SOUP_AUTH_DIALOG);
    } else {
        if (feature_is_set)
            soup_session_remove_feature_by_type
                (uzbl.net.soup_session, (GType) WEBKIT_TYPE_SOUP_AUTH_DIALOG);
    }
}

static void
set_status_background(const gchar *background) {
    /* labels and hboxes do not draw their own background. applying this
     * on the vbox/main_window is ok as the statusbar is the only affected
     * widget. (if not, we could also use GtkEventBox) */
    GtkWidget* widget = uzbl.gui.main_window ? uzbl.gui.main_window : GTK_WIDGET (uzbl.gui.plug);

    g_free(uzbl.behave.status_background);
    uzbl.behave.status_background = g_strdup(background);

#if GTK_CHECK_VERSION(2,91,0)
    GdkRGBA color;
    gdk_rgba_parse (&color, uzbl.behave.status_background);
    gtk_widget_override_background_color (widget, GTK_STATE_NORMAL, &color);
#else
    GdkColor color;
    gdk_color_parse (uzbl.behave.status_background, &color);
    gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &color);
#endif
}

static void
set_icon(const gchar *icon) {
    if(file_exists(icon) && uzbl.gui.main_window) {
        g_free(uzbl.gui.icon);
        uzbl.gui.icon = g_strdup(icon);

        gtk_window_set_icon_from_file (GTK_WINDOW (uzbl.gui.main_window), uzbl.gui.icon, NULL);
    } else {
        g_printerr ("Icon \"%s\" not found. ignoring.\n", icon);
    }
}

static void
set_window_role(const gchar *role) {
    if (!uzbl.gui.main_window)
        return;

    gtk_window_set_role(GTK_WINDOW (uzbl.gui.main_window), role);
}

static gchar *
get_window_role() {
    if (!uzbl.gui.main_window)
        return NULL;

    const gchar* role = gtk_window_get_role(GTK_WINDOW (uzbl.gui.main_window));
    return g_strdup(role);
}

gchar *
get_geometry() {
    int w, h, x, y;
    GString *buf = g_string_new("");

    if(uzbl.gui.main_window) {
      gtk_window_get_size(GTK_WINDOW(uzbl.gui.main_window), &w, &h);
      gtk_window_get_position(GTK_WINDOW(uzbl.gui.main_window), &x, &y);

      g_string_printf(buf, "%dx%d+%d+%d", w, h, x, y);
    }

    return g_string_free(buf, FALSE);
}

void
set_geometry(const gchar *geometry) {
    if(!geometry)
        return;

    if(geometry[0] == 'm') { /* m/maximize/maximized */
        gtk_window_maximize((GtkWindow *)(uzbl.gui.main_window));
    } else {
        int x=0, y=0;
        unsigned int w=0, h=0;

        /* we used to use gtk_window_parse_geometry() but that didn't work
         * how it was supposed to. */
        int ret = XParseGeometry(uzbl.gui.geometry, &x, &y, &w, &h);

        if(ret & XValue)
            gtk_window_move((GtkWindow *)uzbl.gui.main_window, x, y);

        if(ret & WidthValue)
            gtk_window_resize((GtkWindow *)uzbl.gui.main_window, w, h);
    }

    /* get the actual geometry (which might be different from what was
     * specified) and store it (since the GEOMETRY_CHANGED event needs to
     * know what it changed from) */
    g_free(uzbl.gui.geometry);
    uzbl.gui.geometry = get_geometry();
}

void
set_show_status(int show_status) {
    gtk_widget_set_visible(uzbl.gui.status_bar, show_status);
    update_title();
}

int
get_show_status() {
  return gtk_widget_get_visible(uzbl.gui.status_bar);
}

static void
set_status_top(int status_top) {
    if (!uzbl.gui.scrolled_win && !uzbl.gui.status_bar)
        return;

    uzbl.behave.status_top = status_top;

    g_object_ref(uzbl.gui.scrolled_win);
    g_object_ref(uzbl.gui.status_bar);
    gtk_container_remove(GTK_CONTAINER(uzbl.gui.vbox), uzbl.gui.scrolled_win);
    gtk_container_remove(GTK_CONTAINER(uzbl.gui.vbox), uzbl.gui.status_bar);

    if(uzbl.behave.status_top) {
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.status_bar,   FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE,  TRUE, 0);
    } else {
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE,  TRUE, 0);
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.status_bar,   FALSE, TRUE, 0);
    }

    g_object_unref(uzbl.gui.scrolled_win);
    g_object_unref(uzbl.gui.status_bar);

    if (!uzbl.state.plug_mode)
        gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
}

static void
set_current_encoding(const gchar *encoding) {
    if(strlen(encoding) == 0)
        encoding = NULL;

    webkit_web_view_set_custom_encoding(uzbl.gui.web_view, encoding);
}

static gchar *
get_current_encoding() {
    const gchar *encoding = webkit_web_view_get_custom_encoding(uzbl.gui.web_view);
    return g_strdup(encoding);
}

static void
set_fifo_dir(const gchar *fifo_dir) {
    g_free(uzbl.behave.fifo_dir);

    if(init_fifo(fifo_dir))
      uzbl.behave.fifo_dir = g_strdup(fifo_dir);
    else
      uzbl.behave.fifo_dir = NULL;
}

static void
set_socket_dir(const gchar *socket_dir) {
    g_free(uzbl.behave.socket_dir);

    if(init_socket(socket_dir))
      uzbl.behave.socket_dir = g_strdup(socket_dir);
    else
      uzbl.behave.socket_dir = NULL;
}

static void
set_inject_html(const gchar *html) {
    webkit_web_view_load_html_string (uzbl.gui.web_view, html, NULL);
}

static void
set_useragent(const gchar *useragent) {
    g_free(uzbl.net.useragent);

    if (!useragent || !*useragent) {
        uzbl.net.useragent = NULL;
    } else {
        uzbl.net.useragent = g_strdup(useragent);

        g_object_set(G_OBJECT(uzbl.net.soup_session), SOUP_SESSION_USER_AGENT,
            uzbl.net.useragent, NULL);
        g_object_set(view_settings(), "user-agent", uzbl.net.useragent, NULL);
    }
}

static void
set_accept_languages(const gchar *accept_languages) {
    g_free(uzbl.net.accept_languages);

    if (*accept_languages == ' ') {
        uzbl.net.accept_languages = NULL;
    } else {
        uzbl.net.accept_languages = g_strdup(accept_languages);

        g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_ACCEPT_LANGUAGE, uzbl.net.accept_languages, NULL);
    }
}

static void
set_view_source(int view_source) {
    uzbl.behave.view_source = view_source;

    webkit_web_view_set_view_source_mode(uzbl.gui.web_view,
            (gboolean) uzbl.behave.view_source);
}

#ifndef USE_WEBKIT2
void
set_zoom_type (int type) {
    webkit_web_view_set_full_content_zoom (uzbl.gui.web_view, type);
}

int
get_zoom_type () {
    return webkit_web_view_get_full_content_zoom (uzbl.gui.web_view);
}
#endif

static void
set_zoom_level(float zoom_level) {
    webkit_web_view_set_zoom_level (uzbl.gui.web_view, zoom_level);
}

static float
get_zoom_level() {
    return webkit_web_view_get_zoom_level (uzbl.gui.web_view);
}

/* abbreviations to help keep the table's width humane */

/* variables */
#define PTR_V_STR(var, d, set)   { .ptr = { .s = &(var) },       .type = TYPE_STR,   .dump = d, .writeable = 1, .getter = NULL, .setter = (uzbl_fp)set }
#define PTR_V_INT(var, d, set)   { .ptr = { .i = (int*)&(var) }, .type = TYPE_INT,   .dump = d, .writeable = 1, .getter = NULL, .setter = (uzbl_fp)set }
#define PTR_V_FLOAT(var, d, set) { .ptr = { .f = &(var) },       .type = TYPE_FLOAT, .dump = d, .writeable = 1, .getter = NULL, .setter = (uzbl_fp)set }

#define PTR_V_STR_GETSET(var)    { .type = TYPE_STR,   .dump = 1, .writeable = 1, .getter = (uzbl_fp) get_##var, .setter = (uzbl_fp)set_##var }
#define PTR_V_INT_GETSET(var)    { .type = TYPE_INT,   .dump = 1, .writeable = 1, .getter = (uzbl_fp) get_##var, .setter = (uzbl_fp)set_##var }
#define PTR_V_FLOAT_GETSET(var)  { .type = TYPE_FLOAT, .dump = 1, .writeable = 1, .getter = (uzbl_fp) get_##var, .setter = (uzbl_fp)set_##var }

/* constants */
#define PTR_C_STR(var)           { .ptr = { .s = &(var) },       .type = TYPE_STR,   .dump = 0, .writeable = 0, .getter = NULL, .setter = NULL }
#define PTR_C_INT(var)           { .ptr = { .i = (int*)&(var) }, .type = TYPE_INT,   .dump = 0, .writeable = 0, .getter = NULL, .setter = NULL }
#define PTR_C_FLOAT(var)         { .ptr = { .f = &(var) },       .type = TYPE_FLOAT, .dump = 0, .writeable = 0, .getter = NULL, .setter = NULL }

const struct var_name_to_ptr_t {
    const char *name;
    uzbl_cmdprop cp;
} var_name_to_ptr[] = {
/*    variable name            pointer to variable in code                     dump callback function    */
/*  ---------------------------------------------------------------------------------------------- */
    { "uri",                    PTR_V_STR(uzbl.state.uri,                       1,   set_uri)},

    { "verbose",                PTR_V_INT(uzbl.state.verbose,                   1,   NULL)},
    { "print_events",           PTR_V_INT(uzbl.state.events_stdout,             1,   NULL)},

    { "handle_multi_button",    PTR_V_INT(uzbl.state.handle_multi_button,        1,   NULL)},

    { "show_status",            PTR_V_INT_GETSET(show_status)},
    { "status_top",             PTR_V_INT(uzbl.behave.status_top,               1,   set_status_top)},
    { "status_format",          PTR_V_STR(uzbl.behave.status_format,            1,   NULL)},
    { "status_format_right",    PTR_V_STR(uzbl.behave.status_format_right,      1,   NULL)},
    { "status_background",      PTR_V_STR(uzbl.behave.status_background,        1,   set_status_background)},
    { "title_format_long",      PTR_V_STR(uzbl.behave.title_format_long,        1,   NULL)},
    { "title_format_short",     PTR_V_STR(uzbl.behave.title_format_short,       1,   NULL)},

    { "geometry",               PTR_V_STR_GETSET(geometry)},
    { "icon",                   PTR_V_STR(uzbl.gui.icon,                        1,   set_icon)},
    { "window_role",            PTR_V_STR_GETSET(window_role)},

    { "forward_keys",           PTR_V_INT(uzbl.behave.forward_keys,             1,   NULL)},

    { "authentication_handler", PTR_V_STR(uzbl.behave.authentication_handler,   1,   set_authentication_handler)},
    { "scheme_handler",         PTR_V_STR(uzbl.behave.scheme_handler,           1,   NULL)},
    { "request_handler",        PTR_V_STR(uzbl.behave.request_handler,          1,   NULL)},
    { "download_handler",       PTR_V_STR(uzbl.behave.download_handler,         1,   NULL)},

    { "fifo_dir",               PTR_V_STR(uzbl.behave.fifo_dir,                 1,   set_fifo_dir)},
    { "socket_dir",             PTR_V_STR(uzbl.behave.socket_dir,               1,   set_socket_dir)},

    { "shell_cmd",              PTR_V_STR(uzbl.behave.shell_cmd,                1,   NULL)},

    { "http_debug",             PTR_V_INT(uzbl.behave.http_debug,               1,   set_http_debug)},
    { "proxy_url",              PTR_V_STR(uzbl.net.proxy_url,                   1,   set_proxy_url)},
    { "max_conns",              PTR_V_INT(uzbl.net.max_conns,                   1,   set_max_conns)},
    { "max_conns_host",         PTR_V_INT(uzbl.net.max_conns_host,              1,   set_max_conns_host)},
    { "useragent",              PTR_V_STR(uzbl.net.useragent,                   1,   set_useragent)},
    { "accept_languages",       PTR_V_STR(uzbl.net.accept_languages,            1,   set_accept_languages)},

    { "view_source",            PTR_V_INT(uzbl.behave.view_source,              0,   set_view_source)},

    { "ssl_ca_file",            PTR_V_STR_GETSET(ca_file)},
    { "ssl_verify",             PTR_V_INT_GETSET(verify_cert)},

    /* exported WebKitWebSettings properties */
    /* Font settings */
    { "default_font_family",    PTR_V_STR_GETSET(default_font_family)},
    { "monospace_font_family",  PTR_V_STR_GETSET(monospace_font_family)},
    { "sans_serif_font_family", PTR_V_STR_GETSET(sans_serif_font_family)},
    { "serif_font_family",      PTR_V_STR_GETSET(serif_font_family)},
    { "cursive_font_family",    PTR_V_STR_GETSET(cursive_font_family)},
    { "fantasy_font_family",    PTR_V_STR_GETSET(fantasy_font_family)},
    /* Font size settings */
    { "minimum_font_size",      PTR_V_INT_GETSET(minimum_font_size)},
    { "font_size",              PTR_V_INT_GETSET(font_size)},
    { "monospace_size",         PTR_V_INT_GETSET(monospace_size)},
    /* Text settings */
    { "default_encoding",       PTR_V_STR_GETSET(default_encoding)},
    { "current_encoding",       PTR_V_STR_GETSET(current_encoding)},
    { "enforce_96_dpi",         PTR_V_INT_GETSET(enforce_96_dpi)},
    /* Feature settings */
    { "enable_plugins",         PTR_V_INT_GETSET(enable_plugins)},
    { "enable_java_applet",     PTR_V_INT_GETSET(enable_java_applet)},
#if WEBKIT_CHECK_VERSION (1, 3, 14)
    { "enable_webgl",           PTR_V_INT_GETSET(enable_webgl)},
#endif
#if WEBKIT_CHECK_VERSION (1, 7, 5)
    { "enable_webaudio",           PTR_V_INT_GETSET(enable_webaudio)},
#endif
#if WEBKIT_CHECK_VERSION (1, 7, 90) // Documentation says 1.7.5, but it's not there.
    { "enable_3d_acceleration", PTR_V_INT_GETSET(enable_3d_acceleration)},
#endif
    /* HTML5 Database settings */
    { "enable_database",        PTR_V_INT_GETSET(enable_database)},
    { "enable_local_storage",   PTR_V_INT_GETSET(enable_local_storage)},
    { "enable_pagecache",       PTR_V_INT_GETSET(enable_pagecache)},
    { "enable_offline_app_cache", PTR_V_INT_GETSET(enable_offline_app_cache)},
#if WEBKIT_CHECK_VERSION (1, 5, 2)
    { "local_storage_path",     PTR_V_STR_GETSET(local_storage_path)},
#endif
    /* Security settings */
    { "enable_private",         PTR_V_INT_GETSET(enable_private)},
    { "enable_universal_file_access", PTR_V_INT_GETSET(enable_universal_file_access)},
    { "enable_cross_file_access", PTR_V_INT_GETSET(enable_cross_file_access)},
    { "enable_hyperlink_auditing", PTR_V_INT_GETSET(enable_hyperlink_auditing)},
    { "cookie_policy",          PTR_V_INT_GETSET(cookie_policy)},
#if WEBKIT_CHECK_VERSION (1, 3, 13)
    { "enable_dns_prefetch",    PTR_V_INT_GETSET(enable_dns_prefetch)},
#endif
    /* Display settings */
    { "zoom_level",             PTR_V_FLOAT_GETSET(zoom_level)},
    { "zoom_step",              PTR_V_FLOAT_GETSET(zoom_step)},
#ifndef USE_WEBKIT2
    { "zoom_type",              PTR_V_INT_GETSET(zoom_type)},
#endif
    { "caret_browsing",         PTR_V_INT_GETSET(caret_browsing)},
    { "auto_resize_window",     PTR_V_INT_GETSET(auto_resize_window)},
#if WEBKIT_CHECK_VERSION (1, 3, 5)
    { "enable_frame_flattenting", PTR_V_INT_GETSET(enable_frame_flattenting)},
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 8)
    { "enable_fullscreen",      PTR_V_INT_GETSET(enable_fullscreen)},
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 91)
    { "zoom_text_only",         PTR_V_INT_GETSET(zoom_text_only)},
#endif
#endif
    /* Javascript settings */
    { "enable_scripts",         PTR_V_INT_GETSET(enable_scripts)},
    { "javascript_windows",     PTR_V_INT_GETSET(javascript_windows)},
    { "javascript_dom_paste",   PTR_V_INT_GETSET(javascript_dom_paste)},
#if WEBKIT_CHECK_VERSION (1, 3, 0)
    { "javascript_clipboard",   PTR_V_INT_GETSET(javascript_clipboard)},
#endif
    /* Image settings */
    { "autoload_images",        PTR_V_INT_GETSET(autoload_images)},
    { "autoshrink_images",      PTR_V_INT_GETSET(autoshrink_images)},
    /* Spell checking settings */
    { "enable_spellcheck",      PTR_V_INT_GETSET(enable_spellcheck)},
    { "spellcheck_languages",   PTR_V_STR_GETSET(spellcheck_languages)},
    /* Form settings */
    { "resizable_text_areas",   PTR_V_INT_GETSET(resizable_text_areas)},
    { "enable_spatial_navigation", PTR_V_INT_GETSET(enable_spatial_navigation)},
    { "editing_behavior",       PTR_V_INT_GETSET(editing_behavior)},
    { "enable_tab_cycle",       PTR_V_INT_GETSET(enable_tab_cycle)},
    /* Customization */
    { "stylesheet_uri",         PTR_V_STR_GETSET(stylesheet_uri)},
    { "default_context_menu",   PTR_V_INT_GETSET(default_context_menu)},
    /* Hacks */
    { "enable_site_workarounds", PTR_V_INT_GETSET(enable_site_workarounds)},
    /* Printing settings */
    { "print_backgrounds",      PTR_V_INT_GETSET(print_bg)},

    { "inject_html",            { .type = TYPE_STR, .dump = 0, .writeable = 1, .getter = NULL, .setter = (uzbl_fp) set_inject_html }},

    /* constants (not dumpable or writeable) */
    { "WEBKIT_MAJOR",           PTR_C_INT(uzbl.info.webkit_major)},
    { "WEBKIT_MINOR",           PTR_C_INT(uzbl.info.webkit_minor)},
    { "WEBKIT_MICRO",           PTR_C_INT(uzbl.info.webkit_micro)},
    { "HAS_WEBKIT2",            PTR_C_INT(uzbl.info.webkit2)},
    { "ARCH_UZBL",              PTR_C_STR(uzbl.info.arch)},
    { "COMMIT",                 PTR_C_STR(uzbl.info.commit)},
    { "TITLE",                  PTR_C_STR(uzbl.gui.main_title)},
    { "SELECTED_URI",           PTR_C_STR(uzbl.state.selected_url)},
    { "NAME",                   PTR_C_STR(uzbl.state.instance_name)},
    { "PID",                    PTR_C_STR(uzbl.info.pid_str)},
    { "_",                      PTR_C_STR(uzbl.state.last_result)},

    /* and we terminate the whole thing with the closest thing we have to NULL.
     * it's important that dump = 0. */
    { NULL,                     {.ptr = { .i = NULL }, .type = TYPE_INT, .dump = 0, .writeable = 0}}
};

/* construct a hash from the var_name_to_ptr array for quick access */
void
variables_hash() {
    const struct var_name_to_ptr_t *n2v_p = var_name_to_ptr;
    uzbl.behave.proto_var = g_hash_table_new(g_str_hash, g_str_equal);
    while(n2v_p->name) {
        g_hash_table_insert(uzbl.behave.proto_var,
                (gpointer) n2v_p->name,
                (gpointer) &n2v_p->cp);
        n2v_p++;
    }
}
