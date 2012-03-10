#include "variables.h"
#include "uzbl-core.h"
#include "callbacks.h"
#include "events.h"
#include "io.h"
#include "util.h"

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

void
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

void
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
gboolean
string_is_integer(const char *s) {
    return (strspn(s, "0123456789") == strlen(s));
}


GObject*
view_settings() {
    return G_OBJECT(webkit_web_view_get_settings(uzbl.gui.web_view));
}

void
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

gchar *
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

void
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

void
set_max_conns(int max_conns) {
    uzbl.net.max_conns = max_conns;

    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS, uzbl.net.max_conns, NULL);
}

void
set_max_conns_host(int max_conns_host) {
    uzbl.net.max_conns_host = max_conns_host;

    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS_PER_HOST, uzbl.net.max_conns_host, NULL);
}

void
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

void
set_ca_file(gchar *path) {
    g_object_set (uzbl.net.soup_session, "ssl-ca-file", path, NULL);
}

gchar *
get_ca_file() {
    gchar *path;
    g_object_get (uzbl.net.soup_session, "ssl-ca-file", &path, NULL);
    return path;
}

void
set_verify_cert(int strict) {
    g_object_set (uzbl.net.soup_session, "ssl-strict", strict, NULL);
}

int
get_verify_cert() {
    int strict;
    g_object_get (uzbl.net.soup_session, "ssl-strict", &strict, NULL);
    return strict;
}

#define EXPOSE_WEBKIT_VIEW_SETTINGS(SYM, PROPERTY, TYPE) \
void set_##SYM(TYPE val) { \
  g_object_set(view_settings(), (PROPERTY), val, NULL); \
} \
TYPE get_##SYM() { \
  TYPE val; \
  g_object_get(view_settings(), (PROPERTY), &val, NULL); \
  return val; \
}

EXPOSE_WEBKIT_VIEW_SETTINGS(default_font_family,    "default-font-family",    gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(monospace_font_family,  "monospace-font-family",  gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(sans_serif_font_family, "sans_serif-font-family", gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(serif_font_family,      "serif-font-family",      gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(cursive_font_family,    "cursive-font-family",    gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(fantasy_font_family,    "fantasy-font-family",    gchar *)

EXPOSE_WEBKIT_VIEW_SETTINGS(minimum_font_size,      "minimum-font-size",            int)
EXPOSE_WEBKIT_VIEW_SETTINGS(font_size,              "default-font-size",            int)
EXPOSE_WEBKIT_VIEW_SETTINGS(monospace_size,         "default-monospace-font-size",  int)

EXPOSE_WEBKIT_VIEW_SETTINGS(enable_plugins, "enable-plugins", int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_scripts, "enable-scripts", int)

EXPOSE_WEBKIT_VIEW_SETTINGS(javascript_windows, "javascript-can-open-windows-automatically", int)

EXPOSE_WEBKIT_VIEW_SETTINGS(autoload_images,       "auto-load-images",   int)
EXPOSE_WEBKIT_VIEW_SETTINGS(autoshrink_images,     "auto-shrink-images", int)

EXPOSE_WEBKIT_VIEW_SETTINGS(enable_pagecache,   "enable-page-cache",       int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_private,     "enable-private-browsing", int)

EXPOSE_WEBKIT_VIEW_SETTINGS(enable_spellcheck,     "enable-spell-checking",    int)
EXPOSE_WEBKIT_VIEW_SETTINGS(spellcheck_languages,  "spell-checking-languages", gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(resizable_text_areas,  "resizable-text-areas",     int)

EXPOSE_WEBKIT_VIEW_SETTINGS(stylesheet_uri,   "user-stylesheet-uri", gchar *)
EXPOSE_WEBKIT_VIEW_SETTINGS(print_bg,         "print-backgrounds",   int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enforce_96_dpi,   "enforce-96-dpi",      int)

EXPOSE_WEBKIT_VIEW_SETTINGS(caret_browsing,   "enable-caret-browsing", int)

EXPOSE_WEBKIT_VIEW_SETTINGS(enable_cross_file_access, "enable-file-access-from-file-uris", int)

EXPOSE_WEBKIT_VIEW_SETTINGS(default_encoding, "default-encoding", gchar *)

void
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

void
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

void
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

void
set_icon(const gchar *icon) {
    if(file_exists(icon) && uzbl.gui.main_window) {
        g_free(uzbl.gui.icon);
        uzbl.gui.icon = g_strdup(icon);

        gtk_window_set_icon_from_file (GTK_WINDOW (uzbl.gui.main_window), uzbl.gui.icon, NULL);
    } else {
        g_printerr ("Icon \"%s\" not found. ignoring.\n", icon);
    }
}

void
set_window_role(const gchar *role) {
    if (!uzbl.gui.main_window)
        return;

    gtk_window_set_role(GTK_WINDOW (uzbl.gui.main_window), role);
}

gchar *
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

void
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

void
set_current_encoding(const gchar *encoding) {
    if(strlen(encoding) == 0)
        encoding = NULL;

    webkit_web_view_set_custom_encoding(uzbl.gui.web_view, encoding);
}

gchar *
get_current_encoding() {
    const gchar *encoding = webkit_web_view_get_custom_encoding(uzbl.gui.web_view);
    return g_strdup(encoding);
}

void
set_fifo_dir(const gchar *fifo_dir) {
    g_free(uzbl.behave.fifo_dir);

    if(init_fifo(fifo_dir))
      uzbl.behave.fifo_dir = g_strdup(fifo_dir);
    else
      uzbl.behave.fifo_dir = NULL;
}

void
set_socket_dir(const gchar *socket_dir) {
    g_free(uzbl.behave.socket_dir);

    if(init_socket(socket_dir))
      uzbl.behave.socket_dir = g_strdup(socket_dir);
    else
      uzbl.behave.socket_dir = NULL;
}

void
set_inject_html(const gchar *html) {
    webkit_web_view_load_html_string (uzbl.gui.web_view, html, NULL);
}

void
set_useragent(const gchar *useragent) {
    g_free(uzbl.net.useragent);

    if (*useragent == ' ') {
        uzbl.net.useragent = NULL;
    } else {
        uzbl.net.useragent = g_strdup(useragent);

        g_object_set(G_OBJECT(uzbl.net.soup_session), SOUP_SESSION_USER_AGENT,
            uzbl.net.useragent, NULL);
    }
}

void
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

/* requires webkit >=1.1.14 */
void
set_view_source(int view_source) {
    uzbl.behave.view_source = view_source;

    webkit_web_view_set_view_source_mode(uzbl.gui.web_view,
            (gboolean) uzbl.behave.view_source);
}

void
set_zoom_type (int type) {
    webkit_web_view_set_full_content_zoom (uzbl.gui.web_view, type);
}

int
get_zoom_type () {
    return webkit_web_view_get_full_content_zoom (uzbl.gui.web_view);
}

void
set_zoom_level(float zoom_level) {
    webkit_web_view_set_zoom_level (uzbl.gui.web_view, zoom_level);
}

float
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
    { "javascript_windows",     PTR_V_INT_GETSET(javascript_windows)},
    { "zoom_level",             PTR_V_FLOAT_GETSET(zoom_level)},
    { "zoom_type",              PTR_V_INT_GETSET(zoom_type)},

    { "default_font_family",    PTR_V_STR_GETSET(default_font_family)},
    { "monospace_font_family",  PTR_V_STR_GETSET(monospace_font_family)},
    { "cursive_font_family",    PTR_V_STR_GETSET(cursive_font_family)},
    { "sans_serif_font_family", PTR_V_STR_GETSET(sans_serif_font_family)},
    { "serif_font_family",      PTR_V_STR_GETSET(serif_font_family)},
    { "fantasy_font_family",    PTR_V_STR_GETSET(fantasy_font_family)},

    { "monospace_size",         PTR_V_INT_GETSET(monospace_size)},
    { "font_size",              PTR_V_INT_GETSET(font_size)},
    { "minimum_font_size",      PTR_V_INT_GETSET(minimum_font_size)},

    { "enable_pagecache",       PTR_V_INT_GETSET(enable_pagecache)},
    { "enable_plugins",         PTR_V_INT_GETSET(enable_plugins)},
    { "enable_scripts",         PTR_V_INT_GETSET(enable_scripts)},
    { "autoload_images",        PTR_V_INT_GETSET(autoload_images)},
    { "autoshrink_images",      PTR_V_INT_GETSET(autoshrink_images)},
    { "enable_spellcheck",      PTR_V_INT_GETSET(enable_spellcheck)},
    { "spellcheck_languages",   PTR_V_STR_GETSET(spellcheck_languages)},
    { "enable_private",         PTR_V_INT_GETSET(enable_private)},
    { "print_backgrounds",      PTR_V_INT_GETSET(print_bg)},
    { "stylesheet_uri",         PTR_V_STR_GETSET(stylesheet_uri)},
    { "resizable_text_areas",   PTR_V_INT_GETSET(resizable_text_areas)},
    { "default_encoding",       PTR_V_STR_GETSET(default_encoding)},
    { "current_encoding",       PTR_V_STR_GETSET(current_encoding)},
    { "enforce_96_dpi",         PTR_V_INT_GETSET(enforce_96_dpi)},
    { "caret_browsing",         PTR_V_INT_GETSET(caret_browsing)},
    { "enable_cross_file_access", PTR_V_INT_GETSET(enable_cross_file_access)},

    { "inject_html",            { .type = TYPE_STR, .dump = 0, .writeable = 1, .getter = NULL, .setter = (uzbl_fp) set_inject_html }},

    /* constants (not dumpable or writeable) */
    { "WEBKIT_MAJOR",           PTR_C_INT(uzbl.info.webkit_major)},
    { "WEBKIT_MINOR",           PTR_C_INT(uzbl.info.webkit_minor)},
    { "WEBKIT_MICRO",           PTR_C_INT(uzbl.info.webkit_micro)},
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
