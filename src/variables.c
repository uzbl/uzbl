#include "variables.h"
#include "uzbl-core.h"
#include "callbacks.h"
#include "events.h"
#include "io.h"
#include "util.h"
#include "type.h"

void
send_set_var_event(const char *name, const uzbl_cmdprop *c) {
    /* check for the variable type */
    switch(c->type) {
    case TYPE_STR:
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "str",
            TYPE_STR, *c->ptr.s ? *c->ptr.s : " ",
            NULL);
        break;
    case TYPE_INT:
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "int",
            TYPE_INT, *c->ptr.i,
            NULL);
        break;
    case TYPE_FLOAT:
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "float",
            TYPE_FLOAT, *c->ptr.f,
            NULL);
        break;
    default:
        g_assert_not_reached();
    }
}

void
expand_variable(GString *buf, const gchar *name) {
    uzbl_cmdprop* c;
    if((c = g_hash_table_lookup(uzbl.behave.proto_var, name))) {
        if(c->type == TYPE_STR && *c->ptr.s != NULL) {
            g_string_append(buf, (gchar *)*c->ptr.s);
        }
        else if(c->type == TYPE_INT) {
            g_string_append_printf(buf, "%d", *c->ptr.i);
        }
        else if(c->type == TYPE_FLOAT) {
            g_string_append_printf(buf, "%f", *c->ptr.f);
        }
    }
}

gboolean
set_var_value(const gchar *name, gchar *val) {
    uzbl_cmdprop *c = NULL;
    char *endp = NULL;
    char *buf = NULL;

    g_assert(val != NULL);

    if( (c = g_hash_table_lookup(uzbl.behave.proto_var, name)) ) {
        if(!c->writeable) return FALSE;

        switch(c->type) {
        case TYPE_STR:
            buf = g_strdup(val);
            g_free(*c->ptr.s);
            *c->ptr.s = buf;
            break;
        case TYPE_INT:
            *c->ptr.i = (int)strtoul(val, &endp, 10);
            break;
        case TYPE_FLOAT:
            *c->ptr.f = strtod(val, &endp);
            break;
        default:
            g_assert_not_reached();
        }

        send_set_var_event(name, c);

        /* invoke a command specific function */
        if(c->func) c->func();
    } else {
        /* check wether name violates our naming scheme */
        if(!valid_name(name)) {
            if (uzbl.state.verbose)
                printf("Invalid variable name: %s\n", name);
            return FALSE;
        }

        /* custom vars */
        c = g_malloc(sizeof(uzbl_cmdprop));
        c->type = TYPE_STR;
        c->dump = 0;
        c->func = NULL;
        c->writeable = 1;
        buf = g_strdup(val);
        c->ptr.s = g_malloc(sizeof(char *));
        *c->ptr.s = buf;
        g_hash_table_insert(uzbl.behave.proto_var,
                g_strdup(name), (gpointer) c);

        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "str",
            TYPE_STR, buf,
            NULL);
    }
    update_title();
    return TRUE;
}

void
dump_var_hash(gpointer k, gpointer v, gpointer ud) {
    (void) ud;
    uzbl_cmdprop *c = v;

    if(!c->dump)
        return;

    if(c->type == TYPE_STR)
        printf("set %s = %s\n", (char *)k, *c->ptr.s ? *c->ptr.s : " ");
    else if(c->type == TYPE_INT)
        printf("set %s = %d\n", (char *)k, *c->ptr.i);
    else if(c->type == TYPE_FLOAT)
        printf("set %s = %f\n", (char *)k, *c->ptr.f);
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
uri_change_cb (WebKitWebView *web_view, GParamSpec param_spec) {
    (void) param_spec;

    g_free (uzbl.state.uri);
    g_object_get (web_view, "uri", &uzbl.state.uri, NULL);
    g_setenv("UZBL_URI", uzbl.state.uri, TRUE);

    if(GTK_IS_WIDGET(uzbl.gui.main_window)) {
        gdk_property_change(
            gtk_widget_get_window (GTK_WIDGET (uzbl.gui.main_window)),
            gdk_atom_intern_static_string("UZBL_URI"),
            gdk_atom_intern_static_string("STRING"),
            8,
            GDK_PROP_MODE_REPLACE,
            (unsigned char *)uzbl.state.uri,
            strlen(uzbl.state.uri));
    }
}

void
cmd_load_uri() {
    const gchar *uri = uzbl.state.uri;

    gchar   *newuri;
    SoupURI *soup_uri;

    /* Strip leading whitespaces */
    while (*uri && isspace(*uri))
        uri++;

    /* evaluate javascript: URIs */
    if (!strncmp (uri, "javascript:", 11)) {
        eval_js(uzbl.gui.web_view, uri, NULL, "javascript:");
        return;
    }

    /* attempt to parse the URI */
    soup_uri = soup_uri_new(uri);

    if (!soup_uri) {
        /* it's not a valid URI, maybe it's a path on the filesystem. */
        const gchar *fullpath;
        if (g_path_is_absolute (uri))
            fullpath = uri;
        else {
            gchar *wd = g_get_current_dir ();
            fullpath = g_build_filename (wd, uri, NULL);
            g_free(wd);
        }

        struct stat stat_result;
        if (! g_stat(fullpath, &stat_result))
            newuri = g_strconcat("file://", fullpath, NULL);
        else
            newuri = g_strconcat("http://", uri, NULL);
    } else {
        if(soup_uri->host == NULL && string_is_integer(soup_uri->path))
            /* the user probably typed in a host:port without a scheme */
            newuri = g_strconcat("http://", uri, NULL);
        else
            newuri = g_strdup(uri);

        soup_uri_free(soup_uri);
    }

    if(GTK_IS_WIDGET(uzbl.gui.main_window)) {
        gdk_property_change(
            gtk_widget_get_window (GTK_WIDGET (uzbl.gui.main_window)),
            gdk_atom_intern_static_string("UZBL_URI"),
            gdk_atom_intern_static_string("STRING"),
            8,
            GDK_PROP_MODE_REPLACE,
            (unsigned char *)newuri,
            strlen(newuri));
    }

    webkit_web_view_load_uri (uzbl.gui.web_view, newuri);
    g_free (newuri);
}

void
cmd_max_conns() {
    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS, uzbl.net.max_conns, NULL);
}

void
cmd_max_conns_host() {
    g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_MAX_CONNS_PER_HOST, uzbl.net.max_conns_host, NULL);
}

void
cmd_http_debug() {
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
cmd_font_size() {
    GObject *ws = view_settings();
    if (uzbl.behave.font_size > 0) {
        g_object_set (ws, "default-font-size", uzbl.behave.font_size, NULL);
    }

    if (uzbl.behave.monospace_size > 0) {
        g_object_set (ws, "default-monospace-font-size",
                      uzbl.behave.monospace_size, NULL);
    } else {
        g_object_set (ws, "default-monospace-font-size",
                      uzbl.behave.font_size, NULL);
    }
}

void
cmd_zoom_level() {
    webkit_web_view_set_zoom_level (uzbl.gui.web_view, uzbl.behave.zoom_level);
}

#define EXPOSE_WEBKIT_VIEW_SETTINGS(SYM, STORAGE, PROPERTY) void cmd_##SYM() { \
  g_object_set(view_settings(), (PROPERTY), (STORAGE), NULL); \
}

EXPOSE_WEBKIT_VIEW_SETTINGS(default_font_family,    uzbl.behave.default_font_family,    "default-font-family")
EXPOSE_WEBKIT_VIEW_SETTINGS(monospace_font_family,  uzbl.behave.monospace_font_family,  "monospace-font-family")
EXPOSE_WEBKIT_VIEW_SETTINGS(sans_serif_font_family, uzbl.behave.sans_serif_font_family, "sans_serif-font-family")
EXPOSE_WEBKIT_VIEW_SETTINGS(serif_font_family,      uzbl.behave.serif_font_family,      "serif-font-family")
EXPOSE_WEBKIT_VIEW_SETTINGS(cursive_font_family,    uzbl.behave.cursive_font_family,    "cursive-font-family")
EXPOSE_WEBKIT_VIEW_SETTINGS(fantasy_font_family,    uzbl.behave.fantasy_font_family,    "fantasy-font-family")

EXPOSE_WEBKIT_VIEW_SETTINGS(minimum_font_size,      uzbl.behave.minimum_font_size,      "minimum_font_size")

EXPOSE_WEBKIT_VIEW_SETTINGS(disable_plugins, !uzbl.behave.disable_plugins, "enable-plugins")
EXPOSE_WEBKIT_VIEW_SETTINGS(disable_scripts, !uzbl.behave.disable_scripts, "enable-scripts")

EXPOSE_WEBKIT_VIEW_SETTINGS(javascript_windows, uzbl.behave.javascript_windows, "javascript-can-open-windows-automatically")

EXPOSE_WEBKIT_VIEW_SETTINGS(autoload_img,       uzbl.behave.autoload_img,       "auto-load-images")
EXPOSE_WEBKIT_VIEW_SETTINGS(autoshrink_img,     uzbl.behave.autoshrink_img,     "auto-shrink-images")

EXPOSE_WEBKIT_VIEW_SETTINGS(enable_pagecache,   uzbl.behave.enable_pagecache,   "enable-page-cache")
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_private,     uzbl.behave.enable_private,     "enable-private-browsing")

EXPOSE_WEBKIT_VIEW_SETTINGS(enable_spellcheck,     uzbl.behave.enable_spellcheck,    "enable-spell-checking")
EXPOSE_WEBKIT_VIEW_SETTINGS(spellcheck_languages,  uzbl.behave.spellcheck_languages, "spell-checking-languages")
EXPOSE_WEBKIT_VIEW_SETTINGS(resizable_txt,         uzbl.behave.resizable_txt,        "resizable-text-areas")

EXPOSE_WEBKIT_VIEW_SETTINGS(style_uri,        uzbl.behave.style_uri,         "user-stylesheet-uri")
EXPOSE_WEBKIT_VIEW_SETTINGS(print_bg,         uzbl.behave.print_bg,          "print-backgrounds")
EXPOSE_WEBKIT_VIEW_SETTINGS(enforce_96dpi,    uzbl.behave.enforce_96dpi,     "enforce-96-dpi")

EXPOSE_WEBKIT_VIEW_SETTINGS(caret_browsing,   uzbl.behave.caret_browsing,    "enable-caret-browsing")

EXPOSE_WEBKIT_VIEW_SETTINGS(default_encoding, uzbl.behave.default_encoding,  "default-encoding")

void
set_proxy_url() {
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
set_authentication_handler() {
    /* Check if WEBKIT_TYPE_SOUP_AUTH_DIALOG feature is set */
    GSList *flist = soup_session_get_features (uzbl.net.soup_session, (GType) WEBKIT_TYPE_SOUP_AUTH_DIALOG);
    guint feature_is_set = g_slist_length(flist);
    g_slist_free(flist);

    if (uzbl.behave.authentication_handler == NULL || *uzbl.behave.authentication_handler == 0) {
        if (!feature_is_set)
            soup_session_add_feature_by_type
                (uzbl.net.soup_session, (GType) WEBKIT_TYPE_SOUP_AUTH_DIALOG);
    } else {
        if (feature_is_set)
            soup_session_remove_feature_by_type
                (uzbl.net.soup_session, (GType) WEBKIT_TYPE_SOUP_AUTH_DIALOG);
    }
    return;
}

void
set_status_background() {
    /* labels and hboxes do not draw their own background. applying this
     * on the vbox/main_window is ok as the statusbar is the only affected
     * widget. (if not, we could also use GtkEventBox) */
    GtkWidget* widget = uzbl.gui.main_window ? uzbl.gui.main_window : GTK_WIDGET (uzbl.gui.plug);

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
set_icon() {
    if(file_exists(uzbl.gui.icon)) {
        if (uzbl.gui.main_window)
            gtk_window_set_icon_from_file (GTK_WINDOW (uzbl.gui.main_window), uzbl.gui.icon, NULL);
    } else {
        g_printerr ("Icon \"%s\" not found. ignoring.\n", uzbl.gui.icon);
    }
}

void
set_window_role() {
    if (uzbl.gui.main_window)
        gtk_window_set_role(GTK_WINDOW (uzbl.gui.main_window), uzbl.gui.window_role);
}

void
set_geometry() {
    int ret=0, x=0, y=0;
    unsigned int w=0, h=0;
    if(uzbl.gui.geometry) {
        if(uzbl.gui.geometry[0] == 'm') { /* m/maximize/maximized */
            gtk_window_maximize((GtkWindow *)(uzbl.gui.main_window));
        } else {
            /* we used to use gtk_window_parse_geometry() but that didn't work how it was supposed to */
            ret = XParseGeometry(uzbl.gui.geometry, &x, &y, &w, &h);
            if(ret & XValue)
                gtk_window_move((GtkWindow *)uzbl.gui.main_window, x, y);
            if(ret & WidthValue)
                gtk_window_resize((GtkWindow *)uzbl.gui.main_window, w, h);
        }
    }

    /* update geometry var with the actual geometry
       this is necessary as some WMs don't seem to honour
       the above setting and we don't want to end up with
       wrong geometry information
    */
    retrieve_geometry();
}

void
set_show_status() {
    if (!uzbl.behave.show_status)
        gtk_widget_hide(uzbl.gui.status_bar);
    else
        gtk_widget_show(uzbl.gui.status_bar);

    update_title();
}

void
set_status_top() {
    if (!uzbl.gui.scrolled_win && !uzbl.gui.status_bar)
        return;

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
set_current_encoding() {
    gchar *encoding = uzbl.behave.current_encoding;
    if(strlen(encoding) == 0)
        encoding = NULL;

    webkit_web_view_set_custom_encoding(uzbl.gui.web_view, encoding);
}

void
cmd_fifo_dir() {
    uzbl.behave.fifo_dir = init_fifo(uzbl.behave.fifo_dir);
}

void
cmd_socket_dir() {
    uzbl.behave.socket_dir = init_socket(uzbl.behave.socket_dir);
}

void
cmd_inject_html() {
    if(uzbl.behave.inject_html) {
        webkit_web_view_load_html_string (uzbl.gui.web_view,
                uzbl.behave.inject_html, NULL);
    }
}

void
cmd_useragent() {
    if (*uzbl.net.useragent == ' ') {
        g_free (uzbl.net.useragent);
        uzbl.net.useragent = NULL;
    } else {
        g_object_set(G_OBJECT(uzbl.net.soup_session), SOUP_SESSION_USER_AGENT,
            uzbl.net.useragent, NULL);
    }
}

void
set_accept_languages() {
    if (*uzbl.net.accept_languages == ' ') {
        g_free (uzbl.net.accept_languages);
        uzbl.net.accept_languages = NULL;
    } else {
        g_object_set(G_OBJECT(uzbl.net.soup_session),
            SOUP_SESSION_ACCEPT_LANGUAGE, uzbl.net.accept_languages, NULL);
    }
}

/* requires webkit >=1.1.14 */
void
cmd_view_source() {
    webkit_web_view_set_view_source_mode(uzbl.gui.web_view,
            (gboolean) uzbl.behave.view_source);
}

void
cmd_set_zoom_type () {
    if(uzbl.behave.zoom_type)
        webkit_web_view_set_full_content_zoom (uzbl.gui.web_view, TRUE);
    else
        webkit_web_view_set_full_content_zoom (uzbl.gui.web_view, FALSE);
}

/* abbreviations to help keep the table's width humane */
#define PTR_V_STR(var, d, fun) { .ptr.s = &(var), .type = TYPE_STR, .dump = d, .writeable = 1, .func = fun }
#define PTR_V_INT(var, d, fun) { .ptr.i = (int*)&(var), .type = TYPE_INT, .dump = d, .writeable = 1, .func = fun }
#define PTR_V_FLOAT(var, d, fun) { .ptr.f = &(var), .type = TYPE_FLOAT, .dump = d, .writeable = 1, .func = fun }
#define PTR_C_STR(var,    fun) { .ptr.s = &(var), .type = TYPE_STR, .dump = 0, .writeable = 0, .func = fun }
#define PTR_C_INT(var,    fun) { .ptr.i = (int*)&(var), .type = TYPE_INT, .dump = 0, .writeable = 0, .func = fun }
#define PTR_C_FLOAT(var,  fun) { .ptr.f = &(var), .type = TYPE_FLOAT, .dump = 0, .writeable = 0, .func = fun }

const struct var_name_to_ptr_t {
    const char *name;
    uzbl_cmdprop cp;
} var_name_to_ptr[] = {
/*    variable name            pointer to variable in code                     dump callback function    */
/*  ---------------------------------------------------------------------------------------------- */
    { "uri",                    PTR_V_STR(uzbl.state.uri,                       1,   cmd_load_uri)},
    { "verbose",                PTR_V_INT(uzbl.state.verbose,                   1,   NULL)},
    { "print_events",           PTR_V_INT(uzbl.state.events_stdout,             1,   NULL)},
    { "inject_html",            PTR_V_STR(uzbl.behave.inject_html,              0,   cmd_inject_html)},
    { "geometry",               PTR_V_STR(uzbl.gui.geometry,                    1,   set_geometry)},
    { "show_status",            PTR_V_INT(uzbl.behave.show_status,              1,   set_show_status)},
    { "status_top",             PTR_V_INT(uzbl.behave.status_top,               1,   set_status_top)},
    { "status_format",          PTR_V_STR(uzbl.behave.status_format,            1,   NULL)},
    { "status_format_right",    PTR_V_STR(uzbl.behave.status_format_right,      1,   NULL)},
    { "status_background",      PTR_V_STR(uzbl.behave.status_background,        1,   set_status_background)},
    { "title_format_long",      PTR_V_STR(uzbl.behave.title_format_long,        1,   NULL)},
    { "title_format_short",     PTR_V_STR(uzbl.behave.title_format_short,       1,   NULL)},
    { "icon",                   PTR_V_STR(uzbl.gui.icon,                        1,   set_icon)},
    { "window_role",            PTR_V_STR(uzbl.gui.window_role,                 1,   set_window_role)},
    { "forward_keys",           PTR_V_INT(uzbl.behave.forward_keys,             1,   NULL)},
    { "authentication_handler", PTR_V_STR(uzbl.behave.authentication_handler,   1,   set_authentication_handler)},
    { "scheme_handler",         PTR_V_STR(uzbl.behave.scheme_handler,           1,   NULL)},
    { "download_handler",       PTR_V_STR(uzbl.behave.download_handler,         1,   NULL)},
    { "fifo_dir",               PTR_V_STR(uzbl.behave.fifo_dir,                 1,   cmd_fifo_dir)},
    { "socket_dir",             PTR_V_STR(uzbl.behave.socket_dir,               1,   cmd_socket_dir)},
    { "http_debug",             PTR_V_INT(uzbl.behave.http_debug,               1,   cmd_http_debug)},
    { "shell_cmd",              PTR_V_STR(uzbl.behave.shell_cmd,                1,   NULL)},
    { "proxy_url",              PTR_V_STR(uzbl.net.proxy_url,                   1,   set_proxy_url)},
    { "max_conns",              PTR_V_INT(uzbl.net.max_conns,                   1,   cmd_max_conns)},
    { "max_conns_host",         PTR_V_INT(uzbl.net.max_conns_host,              1,   cmd_max_conns_host)},
    { "useragent",              PTR_V_STR(uzbl.net.useragent,                   1,   cmd_useragent)},
    { "accept_languages",       PTR_V_STR(uzbl.net.accept_languages,            1,   set_accept_languages)},
    { "javascript_windows",     PTR_V_INT(uzbl.behave.javascript_windows,       1,   cmd_javascript_windows)},
    /* requires webkit >=1.1.14 */
    { "view_source",            PTR_V_INT(uzbl.behave.view_source,              0,   cmd_view_source)},

    /* exported WebKitWebSettings properties */
    { "zoom_level",             PTR_V_FLOAT(uzbl.behave.zoom_level,             1,   cmd_zoom_level)},
    { "zoom_type",              PTR_V_INT(uzbl.behave.zoom_type,                1,   cmd_set_zoom_type)},
    { "font_size",              PTR_V_INT(uzbl.behave.font_size,                1,   cmd_font_size)},
    { "default_font_family",    PTR_V_STR(uzbl.behave.default_font_family,      1,   cmd_default_font_family)},
    { "monospace_font_family",  PTR_V_STR(uzbl.behave.monospace_font_family,    1,   cmd_monospace_font_family)},
    { "cursive_font_family",    PTR_V_STR(uzbl.behave.cursive_font_family,      1,   cmd_cursive_font_family)},
    { "sans_serif_font_family", PTR_V_STR(uzbl.behave.sans_serif_font_family,   1,   cmd_sans_serif_font_family)},
    { "serif_font_family",      PTR_V_STR(uzbl.behave.serif_font_family,        1,   cmd_serif_font_family)},
    { "fantasy_font_family",    PTR_V_STR(uzbl.behave.fantasy_font_family,      1,   cmd_fantasy_font_family)},
    { "monospace_size",         PTR_V_INT(uzbl.behave.monospace_size,           1,   cmd_font_size)},
    { "minimum_font_size",      PTR_V_INT(uzbl.behave.minimum_font_size,        1,   cmd_minimum_font_size)},
    { "enable_pagecache",       PTR_V_INT(uzbl.behave.enable_pagecache,         1,   cmd_enable_pagecache)},
    { "disable_plugins",        PTR_V_INT(uzbl.behave.disable_plugins,          1,   cmd_disable_plugins)},
    { "disable_scripts",        PTR_V_INT(uzbl.behave.disable_scripts,          1,   cmd_disable_scripts)},
    { "autoload_images",        PTR_V_INT(uzbl.behave.autoload_img,             1,   cmd_autoload_img)},
    { "autoshrink_images",      PTR_V_INT(uzbl.behave.autoshrink_img,           1,   cmd_autoshrink_img)},
    { "enable_spellcheck",      PTR_V_INT(uzbl.behave.enable_spellcheck,        1,   cmd_enable_spellcheck)},
    { "spellcheck_languages",   PTR_V_STR(uzbl.behave.spellcheck_languages,     1,   cmd_spellcheck_languages)},
    { "enable_private",         PTR_V_INT(uzbl.behave.enable_private,           1,   cmd_enable_private)},
    { "print_backgrounds",      PTR_V_INT(uzbl.behave.print_bg,                 1,   cmd_print_bg)},
    { "stylesheet_uri",         PTR_V_STR(uzbl.behave.style_uri,                1,   cmd_style_uri)},
    { "resizable_text_areas",   PTR_V_INT(uzbl.behave.resizable_txt,            1,   cmd_resizable_txt)},
    { "default_encoding",       PTR_V_STR(uzbl.behave.default_encoding,         1,   cmd_default_encoding)},
    { "current_encoding",       PTR_V_STR(uzbl.behave.current_encoding,         1,   set_current_encoding)},
    { "enforce_96_dpi",         PTR_V_INT(uzbl.behave.enforce_96dpi,            1,   cmd_enforce_96dpi)},
    { "caret_browsing",         PTR_V_INT(uzbl.behave.caret_browsing,           1,   cmd_caret_browsing)},

    /* constants (not dumpable or writeable) */
    { "WEBKIT_MAJOR",           PTR_C_INT(uzbl.info.webkit_major,                    NULL)},
    { "WEBKIT_MINOR",           PTR_C_INT(uzbl.info.webkit_minor,                    NULL)},
    { "WEBKIT_MICRO",           PTR_C_INT(uzbl.info.webkit_micro,                    NULL)},
    { "ARCH_UZBL",              PTR_C_STR(uzbl.info.arch,                            NULL)},
    { "COMMIT",                 PTR_C_STR(uzbl.info.commit,                          NULL)},
    { "TITLE",                  PTR_C_STR(uzbl.gui.main_title,                       NULL)},
    { "SELECTED_URI",           PTR_C_STR(uzbl.state.selected_url,                   NULL)},
    { "NAME",                   PTR_C_STR(uzbl.state.instance_name,                  NULL)},
    { "PID",                    PTR_C_STR(uzbl.info.pid_str,                         NULL)},
    { "_",                      PTR_C_STR(uzbl.state.last_result,                    NULL)},

    { NULL,                     {.ptr.s = NULL, .type = TYPE_INT, .dump = 0, .writeable = 0, .func = NULL}}
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
