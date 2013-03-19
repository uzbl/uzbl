#include "variables.h"

#include "io.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"

/* A really generic function pointer. */
typedef void (*UzblFunction)(void);

typedef union {
    int                *i;
    float              *f;
    gchar             **s;
    unsigned long long *ull;
} UzblValue;

typedef struct {
    UzblType type;
    UzblValue value;
    int writeable;

    UzblFunction get;
    UzblFunction set;
} UzblVariable;

/* Abbreviations to help keep the table's width humane. */
#define UZBL_SETTING(typ, val, w, getter, setter) \
    { .type = TYPE_##typ, .value = val, .writeable = w, .get = (UzblFunction)getter, .set = (UzblFunction)setter }

#define UZBL_VARIABLE(typ, val, getter, setter) \
    UZBL_SETTING (typ, val, 1, getter, setter)
#define UZBL_CONSTANT(typ, val, getter) \
    UZBL_SETTING (typ, val, 0, getter, NULL)

/* Variables */
#define UZBL_V_STRING(val, set) UZBL_VARIABLE (STR,   { .s = &(val) },         NULL,      set)
#define UZBL_V_INT(val, set)    UZBL_VARIABLE (INT,   { .i = (int*)&(val) },   NULL,      set)
#define UZBL_V_LONG(val, set)   UZBL_VARIABLE (ULL,   { .ull = &(val) },       NULL,      set)
#define UZBL_V_FLOAT(val, set)  UZBL_VARIABLE (FLOAT, { .f = &(val) },         NULL,      set)
#define UZBL_V_FUNC(val, typ)   UZBL_VARIABLE (typ,   { .s = NULL },           get_##val, set_##val)

/* Constants */
#define UZBL_C_STRING(val)    UZBL_CONSTANT (STR,   { .s = &(val) },       NULL)
#define UZBL_C_INT(val)       UZBL_CONSTANT (INT,   { .i = (int*)&(val) }, NULL)
#define UZBL_C_LONG(val)      UZBL_CONSTANT (ULL,   { .ull = &(val) },     NULL)
#define UZBL_C_FLOAT(val)     UZBL_CONSTANT (FLOAT, { .f = &(val) },       NULL)
#define UZBL_C_FUNC(val, typ) UZBL_CONSTANT (typ,   { .s = NULL },         get_##val)

typedef struct {
    const char *name;
    UzblVariable var;
} UzblVariableEntry;

#define DECLARE_GETTER(type, name) \
    static type                    \
    get_##name ()
#define DECLARE_SETTER(type, name) \
    static void                    \
    set_##name (const type name)

#define DECLARE_GETSET(type, name) \
    DECLARE_GETTER (type, name);   \
    DECLARE_SETTER (type, name)

/* Communication variables */
DECLARE_SETTER (gchar *, fifo_dir);
DECLARE_SETTER (gchar *, socket_dir);

/* Handler variables */
DECLARE_GETSET (int, enable_builtin_auth);

/* Window variables */
/* TODO: This should not be public.
DECLARE_SETTER (gchar *, geometry);
*/
DECLARE_SETTER (gchar *, icon);
DECLARE_GETSET (gchar *, window_role);
DECLARE_GETSET (int, auto_resize_window);

/* UI variables */
DECLARE_GETSET (int, show_status);
DECLARE_SETTER (int, status_top);
DECLARE_SETTER (gchar *, status_background);

/* Customization */
DECLARE_GETSET (gchar *, stylesheet_uri);
#if !WEBKIT_CHECK_VERSION (1, 9, 0)
DECLARE_GETSET (int, default_context_menu);
#endif

/* Printing variables */
DECLARE_GETSET (int, print_backgrounds);

static const UzblVariableEntry
builtin_variable_table[] = {
    /* name                           entry                                                type/callback */
    /* Uzbl variables */
    { "verbose",                      UZBL_V_INT (uzbl.state.verbose,                      NULL)},
    { "print_events",                 UZBL_V_INT (uzbl.state.events_stdout,                NULL)},
    { "handle_multi_button",          UZBL_V_INT (uzbl.state.handle_multi_button,          NULL)},

    /* Communication variables */
    { "fifo_dir",                     UZBL_V_STRING (uzbl.behave.fifo_dir,                 set_fifo_dir)},
    { "socket_dir",                   UZBL_V_STRING (uzbl.behave.socket_dir,               set_socket_dir)},

    /* Handler variables */
    { "scheme_handler",               UZBL_V_STRING (uzbl.behave.scheme_handler,           NULL)},
    { "request_handler",              UZBL_V_STRING (uzbl.behave.request_handler,          NULL)},
    { "download_handler",             UZBL_V_STRING (uzbl.behave.download_handler,         NULL)},
    { "shell_cmd",                    UZBL_V_STRING (uzbl.behave.shell_cmd,                NULL)},
    { "enable_builtin_auth",          UZBL_V_FUNC (enable_builtin_auth,                    INT)},

    /* Window variables */
    { "geometry",                     UZBL_V_FUNC (geometry,                               STR)},
    { "icon",                         UZBL_V_STRING (uzbl.gui.icon,                        set_icon)},
    { "window_role",                  UZBL_V_FUNC (window_role,                            STR)},
    { "auto_resize_window",           UZBL_V_FUNC (auto_resize_window,                     INT)},

    /* UI variables */
    { "show_status",                  UZBL_V_FUNC (show_status,                            INT)},
    { "status_top",                   UZBL_V_INT (uzbl.behave.status_top,                  set_status_top)},
    { "status_format",                UZBL_V_STRING (uzbl.behave.status_format,            NULL)},
    { "status_format_right",          UZBL_V_STRING (uzbl.behave.status_format_right,      NULL)},
    { "status_background",            UZBL_V_STRING (uzbl.behave.status_background,        set_status_background)},
    { "title_format_long",            UZBL_V_STRING (uzbl.behave.title_format_long,        NULL)},
    { "title_format_short",           UZBL_V_STRING (uzbl.behave.title_format_short,       NULL)},

    /* Customization */
    { "stylesheet_uri",               UZBL_V_FUNC (stylesheet_uri,                         STR)},
    { "default_context_menu",
#if WEBKIT_CHECK_VERSION (1, 9, 0)
                                      UZBL_V_INT (uzbl.gui.custom_context_menu,            NULL)
#else
                                      UZBL_V_FUNC (default_context_menu,                   INT)
#endif
                                      },

    /* Printing variables */
    { "print_backgrounds",            UZBL_V_FUNC (print_backgrounds,                      INT)},

    /* Network variables */
    { "proxy_url",                    UZBL_V_STRING (uzbl.net.proxy_url,                   set_proxy_url)},
    { "max_conns",                    UZBL_V_INT (uzbl.net.max_conns,                      set_max_conns)},
    { "max_conns_host",               UZBL_V_INT (uzbl.net.max_conns_host,                 set_max_conns_host)},
    { "http_debug",                   UZBL_V_INT (uzbl.behave.http_debug,                  set_http_debug)},
    { "ssl_ca_file",                  UZBL_V_FUNC (ca_file,                                STR)},
    { "ssl_verify",                   UZBL_V_FUNC (verify_cert,                            INT)},
    { "cache_model",                  UZBL_V_FUNC (cache_model,                            STR)},

    /* Security variables */
    { "enable_private",               UZBL_V_FUNC (enable_private,                         INT)},
    { "enable_universal_file_access", UZBL_V_FUNC (enable_universal_file_access,           INT)},
    { "enable_cross_file_access",     UZBL_V_FUNC (enable_cross_file_access,               INT)},
    { "enable_hyperlink_auditing",    UZBL_V_FUNC (enable_hyperlink_auditing,              INT)},
    { "cookie_policy",                UZBL_V_FUNC (cookie_policy,                          INT)},
#if WEBKIT_CHECK_VERSION (1, 3, 13)
    { "enable_dns_prefetch",          UZBL_V_FUNC (enable_dns_prefetch,                    INT)},
#endif
#if WEBKIT_CHECK_VERSION (1, 11, 2)
    { "display_insecure_content",     UZBL_V_FUNC (display_insecure_content,               INT)},
    { "run_insecure_content",         UZBL_V_FUNC (run_insecure_content,                   INT)},
#endif
    { "maintain_history",             UZBL_V_FUNC (maintain_history,                       INT)},

    /* Inspector variables */
    { "profile_js",                   UZBL_V_FUNC (profile_js,                             INT)},
    { "profile_timeline",             UZBL_V_FUNC (profile_timeline,                       INT)},
#if WEBKIT_CHECK_VERSION (1, 3, 17)
    { "inspected_uri",                UZBL_C_FUNC (inspected_uri,                          STR)},
#endif

    /* Page variables */
    { "uri",                          UZBL_V_STRING (uzbl.state.uri,                       set_uri)},
    { "forward_keys",                 UZBL_V_INT (uzbl.behave.forward_keys,                NULL)},
    { "useragent",                    UZBL_V_STRING (uzbl.net.useragent,                   set_useragent)},
    { "accept_languages",             UZBL_V_STRING (uzbl.net.accept_languages,            set_accept_languages)},
    { "view_source",                  UZBL_V_INT (uzbl.behave.view_source,                 set_view_source)},
    { "zoom_level",                   UZBL_V_FUNC (zoom_level,                             FLOAT)},
    { "zoom_step",                    UZBL_V_FUNC (zoom_step,                              FLOAT)},
#ifndef USE_WEBKIT2
    { "zoom_type",                    UZBL_V_FUNC (zoom_type,                              INT)},
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 91)
    { "zoom_text_only",               UZBL_V_FUNC (zoom_text_only,                         INT)},
#endif
#endif
    { "caret_browsing",               UZBL_V_FUNC (caret_browsing,                         INT)},
#if WEBKIT_CHECK_VERSION (1, 3, 5)
    { "enable_frame_flattening",      UZBL_V_FUNC (enable_frame_flattening,                INT)},
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 0)
    { "enable_smooth_scrolling",      UZBL_V_FUNC (enable_smooth_scrolling,                INT)},
#endif
    { "transparent",                  UZBL_V_FUNC (transparent,                            INT)},
#if WEBKIT_CHECK_VERSION (1, 3, 4)
    { "view_mode",                    UZBL_V_FUNC (view_mode,                              STR)},
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 8)
    { "enable_fullscreen",            UZBL_V_FUNC (enable_fullscreen,                      INT)},
#endif
    { "editable",                     UZBL_V_FUNC (editable,                               INT)},

    /* Javascript variables */
    { "enable_scripts",               UZBL_V_FUNC (enable_scripts,                         INT)},
    { "javascript_windows",           UZBL_V_FUNC (javascript_windows,                     INT)},
    { "javascript_dom_paste",         UZBL_V_FUNC (javascript_dom_paste,                   INT)},
#if WEBKIT_CHECK_VERSION (1, 3, 0)
    { "javascript_clipboard",         UZBL_V_FUNC (javascript_clipboard,                   INT)},
#endif

    /* Image variables */
    { "autoload_images",              UZBL_V_FUNC (autoload_images,                        INT)},
    { "autoshrink_images",            UZBL_V_FUNC (autoshrink_images,                      INT)},

    /* Spell checking variables */
    { "enable_spellcheck",            UZBL_V_FUNC (enable_spellcheck,                      INT)},
    { "spellcheck_languages",         UZBL_V_FUNC (spellcheck_languages,                   STR)},

    /* Form variables */
    { "resizable_text_areas",         UZBL_V_FUNC (resizable_text_areas,                   INT)},
    { "enable_spatial_navigation",    UZBL_V_FUNC (enable_spatial_navigation,              INT)},
    { "editing_behavior",             UZBL_V_FUNC (editing_behavior,                       INT)},
    { "enable_tab_cycle",             UZBL_V_FUNC (enable_tab_cycle,                       INT)},

    /* Text variables */
    { "default_encoding",             UZBL_V_FUNC (default_encoding,                       STR)},
    { "custom_encoding",              UZBL_V_FUNC (custom_encoding,                        STR)},
    { "enforce_96_dpi",               UZBL_V_FUNC (enforce_96_dpi,                         INT)},
    { "current_encoding",             UZBL_C_FUNC (current_encoding,                       STR)},

    /* Font variables */
    { "default_font_family",          UZBL_V_FUNC (default_font_family,                    STR)},
    { "monospace_font_family",        UZBL_V_FUNC (monospace_font_family,                  STR)},
    { "sans_serif_font_family",       UZBL_V_FUNC (sans_serif_font_family,                 STR)},
    { "serif_font_family",            UZBL_V_FUNC (serif_font_family,                      STR)},
    { "cursive_font_family",          UZBL_V_FUNC (cursive_font_family,                    STR)},
    { "fantasy_font_family",          UZBL_V_FUNC (fantasy_font_family,                    STR)},

    /* Font size variables */
    { "minimum_font_size",            UZBL_V_FUNC (minimum_font_size,                      INT)},
    { "font_size",                    UZBL_V_FUNC (font_size,                              INT)},
    { "monospace_size",               UZBL_V_FUNC (monospace_size,                         INT)},

    /* Feature variables */
    { "enable_plugins",               UZBL_V_FUNC (enable_plugins,                         INT)},
    { "enable_java_applet",           UZBL_V_FUNC (enable_java_applet,                     INT)},
#if WEBKIT_CHECK_VERSION (1, 3, 8)
    { "plugin_list",                  UZBL_C_FUNC (plugin_list,                            STR)},
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 14)
    { "enable_webgl",                 UZBL_V_FUNC (enable_webgl,                           INT)},
#endif
#if WEBKIT_CHECK_VERSION (1, 7, 5)
    { "enable_webaudio",              UZBL_V_FUNC (enable_webaudio,                        INT)},
#endif
#if WEBKIT_CHECK_VERSION (1, 7, 90) /* Documentation says 1.7.5, but it's not there. */
    { "enable_3d_acceleration",       UZBL_V_FUNC (enable_3d_acceleration,                 INT)},
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 3)
    { "enable_inline_media",          UZBL_V_FUNC (enable_inline_media,                    INT)},
    { "require_click_to_play",        UZBL_V_FUNC (require_click_to_play,                  INT)},
#endif
#if WEBKIT_CHECK_VERSION (1, 11, 1)
    { "enable_css_shaders",           UZBL_V_FUNC (enable_css_shaders,                     INT)},
    { "enable_media_stream",          UZBL_V_FUNC (enable_media_stream,                    INT)},
#endif

    /* HTML5 Database variables */
    { "enable_database",              UZBL_V_FUNC (enable_database,                        INT)},
    { "enable_local_storage",         UZBL_V_FUNC (enable_local_storage,                   INT)},
    { "enable_pagecache",             UZBL_V_FUNC (enable_pagecache,                       INT)},
    { "enable_offline_app_cache",     UZBL_V_FUNC (enable_offline_app_cache,               INT)},
#if WEBKIT_CHECK_VERSION (1, 3, 13)
    { "app_cache_size",               UZBL_V_FUNC (app_cache_size,                         ULL)},
#endif
    { "web_database_directory",       UZBL_V_FUNC (web_database_directory,                 STR)},
    { "web_database_quota",           UZBL_V_FUNC (web_database_quota,                     ULL)},
#if WEBKIT_CHECK_VERSION (1, 5, 2)
    { "local_storage_path",           UZBL_V_FUNC (local_storage_path,                     STR)},
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 13)
    { "app_cache_directory",          UZBL_V_FUNC (app_cache_directory,                    STR)},
#endif

    /* Hacks */
    { "enable_site_workarounds",      UZBL_V_FUNC (enable_site_workarounds,                INT)},

    /* Magic/special variables */
    /* FIXME: These are probably better off as commands. */
#ifdef USE_WEBKIT2
    { "inject_text",                  UZBL_VARIABLE (STR, { .s = NULL }, NULL,             set_inject_text)},
#endif
    { "inject_html",                  UZBL_VARIABLE (STR, { .s = NULL }, NULL,             set_inject_html)},

    /* Constants */
    { "WEBKIT_MAJOR",                 UZBL_C_INT (uzbl.info.webkit_major)},
    { "WEBKIT_MINOR",                 UZBL_C_INT (uzbl.info.webkit_minor)},
    { "WEBKIT_MICRO",                 UZBL_C_INT (uzbl.info.webkit_micro)},
    { "HAS_WEBKIT2",                  UZBL_C_INT (uzbl.info.webkit2)},
    { "ARCH_UZBL",                    UZBL_C_STRING (uzbl.info.arch)},
    { "COMMIT",                       UZBL_C_STRING (uzbl.info.commit)},
    { "TITLE",                        UZBL_C_STRING (uzbl.gui.main_title)},
    { "SELECTED_URI",                 UZBL_C_STRING (uzbl.state.selected_url)},
    { "NAME",                         UZBL_C_STRING (uzbl.state.instance_name)},
    { "PID",                          UZBL_C_STRING (uzbl.info.pid_str)},
    { "_",                            UZBL_C_STRING (uzbl.state.last_result)},

    /* Add a terminator entry. */
    { NULL,                           UZBL_SETTING (INT, { .i = NULL }, 0, NULL, NULL)}
};

/* Construct a hash table from the var_name_to_ptr array for quick access. */
void
uzbl_variables_init ()
{
    const UzblVariableEntry *entry = builtin_variable_table;
    uzbl.behave.proto_var = g_hash_table_new (g_str_hash, g_str_equal);
    while (entry->name) {
        g_hash_table_insert (uzbl.behave.proto_var,
            (gpointer)entry->name,
            (gpointer)&entry->var);
        ++entry;
    }
}

#define IMPLEMENT_GETTER(type, name) \
    type                             \
    get_##name ()

#define IMPLEMENT_SETTER(type, name) \
    void                             \
    set_##name (const type name)

#define GOBJECT_GETSET(type, name, obj, prop) \
    IMPLEMENT_GETTER (type, name)             \
    {                                         \
        type name;                            \
                                              \
        g_object_get (G_OBJECT (obj),         \
            prop, &name,                      \
            NULL);                            \
                                              \
        return name;                          \
    }                                         \
                                              \
    IMPLEMENT_SETTER (type, name)             \
    {                                         \
        g_object_set (G_OBJECT (obj),         \
            prop, name,                       \
            NULL);                            \
    }

static GObject *
webkit_settings ();

/* Communication variables */
IMPLEMENT_SETTER (gchar *, fifo_dir)
{
    g_free (uzbl.behave.fifo_dir);

    if (uzbl_io_init_fifo (fifo_dir)) {
        uzbl.behave.fifo_dir = g_strdup (fifo_dir);
    } else {
        uzbl.behave.fifo_dir = NULL;
    }
}

IMPLEMENT_SETTER (gchar *, socket_dir)
{
    g_free (uzbl.behave.socket_dir);

    if (uzbl_io_init_socket (socket_dir)) {
        uzbl.behave.socket_dir = g_strdup (socket_dir);
    } else {
        uzbl.behave.socket_dir = NULL;
    }
}

/* Handler variables */
IMPLEMENT_GETTER (int, enable_builtin_auth)
{
    SoupSessionFeature *auth = soup_session_get_feature (
        uzbl.net.soup_session, (GType)WEBKIT_TYPE_SOUP_AUTH_DIALOG);

    return (auth != NULL);
}

IMPLEMENT_SETTER (int, enable_builtin_auth)
{
    SoupSessionFeature *auth = soup_session_get_feature (
        uzbl.net.soup_session, (GType)WEBKIT_TYPE_SOUP_AUTH_DIALOG);

    if (enable_builtin_auth > 0) {
        if (!auth) {
            soup_session_add_feature_by_type (
                uzbl.net.soup_session, (GType)WEBKIT_TYPE_SOUP_AUTH_DIALOG);
        }
    } else {
        if (auth) {
            soup_session_remove_feature (uzbl.net.soup_session, auth);
        }
    }
}

/* Window variables */
IMPLEMENT_GETTER (gchar *, geometry)
{
    int w;
    int h;
    int x;
    int y;
    GString *buf = g_string_new ("");

    if (uzbl.gui.main_window) {
        gtk_window_get_size (GTK_WINDOW (uzbl.gui.main_window), &w, &h);
        gtk_window_get_position (GTK_WINDOW (uzbl.gui.main_window), &x, &y);

        g_string_printf (buf, "%dx%d+%d+%d", w, h, x, y);
    }

    return g_string_free (buf, FALSE);
}

IMPLEMENT_SETTER (gchar *, geometry)
{
    if (!geometry) {
        return;
    }

    if (geometry[0] == 'm') { /* m/maximize/maximized */
        gtk_window_maximize (GTK_WINDOW (uzbl.gui.main_window));
    } else {
        int x = 0;
        int y = 0;
        unsigned w = 0;
        unsigned h=0;

        /* We used to use gtk_window_parse_geometry() but that didn't work how
         * it was supposed to. */
        int ret = XParseGeometry (uzbl.gui.geometry, &x, &y, &w, &h);

        if (ret & XValue) {
            gtk_window_move (GTK_WINDOW (uzbl.gui.main_window), x, y);
        }

        if (ret & WidthValue) {
            gtk_window_resize (GTK_WINDOW (uzbl.gui.main_window), w, h);
        }
    }

    /* Get the actual geometry (which might be different from what was
     * specified) and store it (since the GEOMETRY_CHANGED event needs to know
     * what it changed from) */
    g_free (uzbl.gui.geometry);
    uzbl.gui.geometry = get_geometry ();
}

IMPLEMENT_SETTER (gchar *, icon)
{
    if (!uzbl.gui.main_window) {
        return;
    }

    /* Clear icon_name. */
    g_free (uzbl.gui.icon_name);
    uzbl.gui.icon_name = NULL;

    if (file_exists (icon)) {
        g_free (uzbl.gui.icon);
        uzbl.gui.icon = g_strdup (icon);

        gtk_window_set_icon_from_file (GTK_WINDOW (uzbl.gui.main_window), uzbl.gui.icon, NULL);
    } else {
        g_printerr ("Icon \"%s\" not found. ignoring.\n", icon);
    }
}

IMPLEMENT_GETTER (gchar *, window_role)
{
    if (!uzbl.gui.main_window) {
        return NULL;
    }

    const gchar* role = gtk_window_get_role (GTK_WINDOW (uzbl.gui.main_window));

    return g_strdup (role);
}

IMPLEMENT_SETTER (gchar *, window_role)
{
    if (!uzbl.gui.main_window) {
        return;
    }

    gtk_window_set_role (GTK_WINDOW (uzbl.gui.main_window), window_role);
}

GOBJECT_GETSET (int, auto_resize_window,
                webkit_settings (), "auto-resize-window")

/* UI variables */
IMPLEMENT_GETTER (int, show_status)
{
    return gtk_widget_get_visible (uzbl.gui.status_bar);
}

IMPLEMENT_SETTER (int, show_status)
{
    gtk_widget_set_visible (uzbl.gui.status_bar, show_status);
    update_title ();
}

IMPLEMENT_SETTER (int, status_top)
{
    if (!uzbl.gui.scrolled_win || !uzbl.gui.status_bar) {
        return;
    }

    uzbl.behave.status_top = status_top;

    g_object_ref (uzbl.gui.scrolled_win);
    g_object_ref (uzbl.gui.status_bar);
    gtk_container_remove (GTK_CONTAINER (uzbl.gui.vbox), uzbl.gui.scrolled_win);
    gtk_container_remove (GTK_CONTAINER (uzbl.gui.vbox), uzbl.gui.status_bar);

    if (uzbl.behave.status_top) {
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.status_bar,   FALSE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE,  TRUE, 0);
    } else {
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.scrolled_win, TRUE,  TRUE, 0);
        gtk_box_pack_start (GTK_BOX (uzbl.gui.vbox), uzbl.gui.status_bar,   FALSE, TRUE, 0);
    }

    g_object_unref (uzbl.gui.scrolled_win);
    g_object_unref (uzbl.gui.status_bar);

    if (!uzbl.state.plug_mode) {
        gtk_widget_grab_focus (GTK_WIDGET (uzbl.gui.web_view));
    }
}

IMPLEMENT_SETTER (char *, status_background)
{
    /* Labels and hboxes do not draw their own background. Applying this on the
     * vbox/main_window is ok as the statusbar is the only affected widget. If
     * not, we could also use GtkEventBox. */
    GtkWidget *widget = uzbl.gui.main_window ? uzbl.gui.main_window : GTK_WIDGET (uzbl.gui.plug);

    g_free (uzbl.behave.status_background);
    uzbl.behave.status_background = g_strdup (status_background);

#if GTK_CHECK_VERSION (2, 91, 0)
    GdkRGBA color;
    gdk_rgba_parse (&color, uzbl.behave.status_background);
    gtk_widget_override_background_color (widget, GTK_STATE_NORMAL, &color);
#else
    GdkColor color;
    gdk_color_parse (uzbl.behave.status_background, &color);
    gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &color);
#endif
}

/* Customization */
GOBJECT_GETSET (gchar *, stylesheet_uri,
                webkit_settings (), "user-stylesheet-uri")

#if !WEBKIT_CHECK_VERSION (1, 9, 0)
GOBJECT_GETSET (int, default_context_menu,
                webkit_settings (), "enable-default-context-menu")
#endif

/* Printing variables */
GOBJECT_GETSET (int, print_backgrounds,
                webkit_settings (), "print-backgrounds")

GObject *
webkit_settings ()
{
    return G_OBJECT (webkit_web_view_get_settings (uzbl.gui.web_view));
}

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
        uzbl_events_send (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "str",
            TYPE_STR, v,
            NULL);
        g_free(v);
        break;
    }
    case TYPE_INT:
        uzbl_events_send (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "int",
            TYPE_INT, get_var_value_int_c(c),
            NULL);
        break;
    case TYPE_ULL:
        send_event (VARIABLE_SET, NULL,
            TYPE_NAME, name,
            TYPE_NAME, "ull",
            TYPE_ULL, get_var_value_ull_c(c),
            NULL);
        break;
    case TYPE_FLOAT:
        uzbl_events_send (VARIABLE_SET, NULL,
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
set_var_value_ull_c(uzbl_cmdprop *c, unsigned long long ull) {
    if(c->setter)
        ((void (*)(unsigned long long))c->setter)(ull);
    else
        *(c->ptr.ull) = ull;
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
            int i = (int)strtol(val, NULL, 10);
            set_var_value_int_c(c, i);
            break;
        }
        case TYPE_ULL:
        {
            unsigned long long ull = strtoull(val, NULL, 10);
            set_var_value_ull_c(c, ull);
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
        c = g_malloc(sizeof (uzbl_cmdprop));
        c->type       = TYPE_STR;
        c->dump       = 0;
        c->getter     = NULL;
        c->setter     = NULL;
        c->writeable  = 1;

        c->ptr.s    = g_malloc(sizeof (gchar*));

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

unsigned long long
get_var_value_ull_c(const uzbl_cmdprop *c) {
    if(!c) return 0;

    if(c->getter) {
        return ((unsigned long long (*)())c->getter)();
    } else if(c->ptr.ull)
        return *(c->ptr.ull);

    return 0;
}

unsigned long long
get_var_value_ull(const gchar *name) {
    uzbl_cmdprop *c = get_var_c(name);
    return get_var_value_ull_c(c);
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
    } else if(c->type == TYPE_INT) {
        printf("set %s = %d\n", (char *)k, get_var_value_int_c(c));
    } else if(c->type == TYPE_ULL) {
        printf("set %s = %llu\n", (char *)k, get_var_value_ull_c(c));
    } else if(c->type == TYPE_FLOAT) {
        printf("set %s = %f\n", (char *)k, get_var_value_float_c(c));
    }
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

#define EXPOSE_WEB_INSPECTOR_SETTINGS(SYM, PROPERTY, TYPE) \
static void set_##SYM(TYPE val) { \
  g_object_set(uzbl.gui.inspector, (PROPERTY), val, NULL); \
} \
static TYPE get_##SYM() { \
  TYPE val; \
  g_object_get(uzbl.gui.inspector, (PROPERTY), &val, NULL); \
  return val; \
}

EXPOSE_WEB_INSPECTOR_SETTINGS(profile_js,         "javascript-profiling-enabled",       int)
EXPOSE_WEB_INSPECTOR_SETTINGS(profile_timeline,   "timeline-profiling-enabled",         gchar *)

#undef EXPOSE_WEB_INSPECTOR_SETTINGS

#define EXPOSE_SOUP_SESSION_SETTINGS(SYM, PROPERTY, TYPE) \
static void set_##SYM(TYPE val) { \
  g_object_set(uzbl.net.soup_session, (PROPERTY), val, NULL); \
} \
static TYPE get_##SYM() { \
  TYPE val; \
  g_object_get(uzbl.net.soup_session, (PROPERTY), &val, NULL); \
  return val; \
}

EXPOSE_SOUP_SESSION_SETTINGS(verify_cert,      "ssl-strict",       int)

#undef EXPOSE_SOUP_SESSION_SETTINGS

#define EXPOSE_SOUP_COOKIE_JAR_SETTINGS(SYM, PROPERTY, TYPE) \
static void set_##SYM(TYPE val) { \
  g_object_set(cookie_jar(), (PROPERTY), val, NULL); \
} \
static TYPE get_##SYM() { \
  TYPE val; \
  g_object_get(cookie_jar(), (PROPERTY), &val, NULL); \
  return val; \
}

EXPOSE_SOUP_COOKIE_JAR_SETTINGS(cookie_policy,    "accept-policy",    int)

#undef EXPOSE_SOUP_COOKIE_JAR_SETTINGS

#define EXPOSE_WEBKIT_VIEW_VIEW_SETTINGS(SYM, PROPERTY, TYPE) \
static void set_##SYM(TYPE val) { \
  g_object_set(uzbl.gui.web_view, (PROPERTY), val, NULL); \
} \
static TYPE get_##SYM() { \
  TYPE val; \
  g_object_get(uzbl.gui.web_view, (PROPERTY), &val, NULL); \
  return val; \
}

EXPOSE_WEBKIT_VIEW_VIEW_SETTINGS(editable,                "editable",                                  int)
EXPOSE_WEBKIT_VIEW_VIEW_SETTINGS(transparent,             "transparent",                               int)

#undef EXPOSE_WEBKIT_VIEW_SETTINGS

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
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_webaudio,              "enable-webaudio",                           int)
#endif
#if WEBKIT_CHECK_VERSION (1, 7, 90) // Documentation says 1.7.5, but it's not there.
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_3d_acceleration,       "enable-accelerated-compositing",            int)
#endif
#if WEBKIT_CHECK_VERSION (1, 11, 1)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_css_shaders,           "enable-css-shaders",                        int)
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
#if WEBKIT_CHECK_VERSION (1, 11, 2)
EXPOSE_WEBKIT_VIEW_SETTINGS(display_insecure_content,     "enable-display-of-insecure-content",        int)
EXPOSE_WEBKIT_VIEW_SETTINGS(run_insecure_content,         "enable-running-of-insecure-content",        int)
#endif

/* Display settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(zoom_step,                    "zoom-step",                                 float)
EXPOSE_WEBKIT_VIEW_SETTINGS(caret_browsing,               "enable-caret-browsing",                     int)
#if WEBKIT_CHECK_VERSION (1, 3, 5)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_frame_flattening,      "enable-frame-flattening",                   int)
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 8)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_fullscreen,            "enable-fullscreen",                         int)
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 91)
EXPOSE_WEBKIT_VIEW_SETTINGS(zoom_text_only,               "zoom-text-only",                            int)
#endif
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 0)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_smooth_scrolling,      "enable-smooth-scrolling",                   int)
#endif

/* Javascript settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_scripts,               "enable-scripts",                            int)
EXPOSE_WEBKIT_VIEW_SETTINGS(javascript_windows,           "javascript-can-open-windows-automatically", int)
EXPOSE_WEBKIT_VIEW_SETTINGS(javascript_dom_paste,         "enable-dom-paste",                          int)
#if WEBKIT_CHECK_VERSION (1, 3, 0)
EXPOSE_WEBKIT_VIEW_SETTINGS(javascript_clipboard,         "javascript-can-access-clipboard",           int)
#endif

/* Media settings */
#if WEBKIT_CHECK_VERSION (1, 9, 3)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_inline_media,          "media-playback-allows-inline",              int)
EXPOSE_WEBKIT_VIEW_SETTINGS(require_click_to_play,        "media-playback-requires-user-gesture",      int)
#endif
#if WEBKIT_CHECK_VERSION (1, 11, 1)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_media_stream,          "enable-media-stream",                       int)
#endif

/* Image settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(autoload_images,              "auto-load-images",                          int)
EXPOSE_WEBKIT_VIEW_SETTINGS(autoshrink_images,            "auto-shrink-images",                        int)

/* Spell checking settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_spellcheck,            "enable-spell-checking",                     int)

/* Form settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(resizable_text_areas,         "resizable-text-areas",                      int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_spatial_navigation,    "enable-spatial-navigation",                 int)
EXPOSE_WEBKIT_VIEW_SETTINGS(editing_behavior,             "editing-behavior",                          int)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_tab_cycle,             "tab-key-cycles-through-elements",           int)

/* Hacks */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_site_workarounds,      "enable-site-specific-quirks",               int)

#undef EXPOSE_WEBKIT_VIEW_SETTINGS

static void
set_maintain_history (int maintain) {
    uzbl.behave.maintain_history = maintain;

    webkit_web_view_set_maintains_back_forward_list (uzbl.gui.web_view, maintain);
}

static int
get_maintain_history () {
    return uzbl.behave.maintain_history;
}

static void
set_spellcheck_languages(const gchar *languages) {
  GObject *obj = webkit_get_text_checker ();

  if (!obj) {
      return;
  }
  if (!WEBKIT_IS_SPELL_CHECKER (obj)) {
      return;
  }

  WebKitSpellChecker *checker = WEBKIT_SPELL_CHECKER (obj);

  webkit_spell_checker_update_spell_checking_languages (checker, languages);
  g_object_set(view_settings(), "spell-checking-languages", languages, NULL);
}

static gchar *
get_spellcheck_languages() {
  gchar *val;
  g_object_get(view_settings(), "spell-checking-languages", &val, NULL);
  return val;
}

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
set_custom_encoding(const gchar *encoding) {
    if(strlen(encoding) == 0)
        encoding = NULL;

    webkit_web_view_set_custom_encoding(uzbl.gui.web_view, encoding);
}

static gchar *
get_custom_encoding() {
    const gchar *encoding = webkit_web_view_get_custom_encoding(uzbl.gui.web_view);
    return g_strdup(encoding);
}

static gchar *
get_current_encoding() {
    const gchar *encoding = webkit_web_view_get_encoding (uzbl.gui.web_view);
    return g_strdup(encoding);
}

static void
set_inject_html(const gchar *html) {
#ifdef USE_WEBKIT2
    webkit_web_view_load_html (uzbl.gui.web_view, html, NULL);
#else
    webkit_web_view_load_html_string (uzbl.gui.web_view, html, NULL);
#endif
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

static gchar *
get_cache_model() {
    WebKitCacheModel model = webkit_get_cache_model ();

    switch (model) {
    case WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER:
        return g_strdup("document_viewer");
    case WEBKIT_CACHE_MODEL_WEB_BROWSER:
        return g_strdup("web_browser");
    case WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER:
        return g_strdup("document_browser");
    default:
        return g_strdup("unknown");
    }
}

static void
set_cache_model(const gchar *model) {
    if (!g_strcmp0 (model, "default")) {
        webkit_set_cache_model (WEBKIT_CACHE_MODEL_DEFAULT);
    } else if (!g_strcmp0 (model, "document_viewer")) {
        webkit_set_cache_model (WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER);
    } else if (!g_strcmp0 (model, "web_browser")) {
        webkit_set_cache_model (WEBKIT_CACHE_MODEL_WEB_BROWSER);
    } else if (!g_strcmp0 (model, "document_browser")) {
        webkit_set_cache_model (WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER);
    }
}

static gchar *
get_web_database_directory() {
    return g_strdup (webkit_get_web_database_directory_path ());
}

static unsigned long long
get_web_database_quota () {
    return webkit_get_default_web_database_quota ();
}

static void
set_web_database_quota (unsigned long long quota) {
    webkit_set_default_web_database_quota (quota);
}

static void
set_web_database_directory(const gchar *path) {
    webkit_set_web_database_directory_path (path);
}

#if WEBKIT_CHECK_VERSION (1, 3, 4)
static gchar *
get_view_mode() {
    WebKitWebViewViewMode mode = webkit_web_view_get_view_mode (uzbl.gui.web_view);

    switch (mode) {
    case WEBKIT_WEB_VIEW_VIEW_MODE_WINDOWED:
        return g_strdup("windowed");
    case WEBKIT_WEB_VIEW_VIEW_MODE_FLOATING:
        return g_strdup("floating");
    case WEBKIT_WEB_VIEW_VIEW_MODE_FULLSCREEN:
        return g_strdup("fullscreen");
    case WEBKIT_WEB_VIEW_VIEW_MODE_MAXIMIZED:
        return g_strdup("maximized");
    case WEBKIT_WEB_VIEW_VIEW_MODE_MINIMIZED:
        return g_strdup("minimized");
    default:
        return g_strdup("unknown");
    }
}

static void
set_view_mode(const gchar *mode) {
    if (!g_strcmp0 (mode, "windowed")) {
        webkit_web_view_set_view_mode (uzbl.gui.web_view, WEBKIT_WEB_VIEW_VIEW_MODE_WINDOWED);
    } else if (!g_strcmp0 (mode, "floating")) {
        webkit_web_view_set_view_mode (uzbl.gui.web_view, WEBKIT_WEB_VIEW_VIEW_MODE_FLOATING);
    } else if (!g_strcmp0 (mode, "fullscreen")) {
        webkit_web_view_set_view_mode (uzbl.gui.web_view, WEBKIT_WEB_VIEW_VIEW_MODE_FULLSCREEN);
    } else if (!g_strcmp0 (mode, "maximized")) {
        webkit_web_view_set_view_mode (uzbl.gui.web_view, WEBKIT_WEB_VIEW_VIEW_MODE_MAXIMIZED);
    } else if (!g_strcmp0 (mode, "minimized")) {
        webkit_web_view_set_view_mode (uzbl.gui.web_view, WEBKIT_WEB_VIEW_VIEW_MODE_MINIMIZED);
    }
}
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 17)
static gchar *
get_inspected_uri() {
    return g_strdup (webkit_web_inspector_get_inspected_uri (uzbl.gui.inspector));
}
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 13)
static gchar *
get_app_cache_directory() {
    return g_strdup (webkit_application_cache_get_database_directory_path ());
}

static unsigned long long
get_app_cache_size() {
    return webkit_application_cache_get_maximum_size ();
}

static void
set_app_cache_size(unsigned long long size) {
    webkit_application_cache_set_maximum_size (size);
}
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 8)
static void
mimetype_list_append(WebKitWebPluginMIMEType *mimetype, GString *list) {
    if (*list->str != '[') {
        g_string_append_c (list, ',');
    }

    /* Write out a JSON representation of the information */
    g_string_append_printf (list,
            "{\"name\": \"%s\","
            "\"description\": \"%s\","
            "\"extensions\": [", /* Open array for the extensions */
            mimetype->name,
            mimetype->description);

    char **extension = mimetype->extensions;
    gboolean first = TRUE;

    while (extension) {
        if (first) {
            first = FALSE;
        } else {
            g_string_append_c (list, ',');
        }
        g_string_append (list, *extension);

        ++extension;
    }

    g_string_append_c (list, '}');
}

static void
plugin_list_append(WebKitWebPlugin *plugin, GString *list) {
    if (*list->str != '[') {
        g_string_append_c (list, ',');
    }

    const gchar *desc = webkit_web_plugin_get_description (plugin);
    gboolean enabled = webkit_web_plugin_get_enabled (plugin);
    GSList *mimetypes = webkit_web_plugin_get_mimetypes (plugin);
    const gchar *name = webkit_web_plugin_get_name (plugin);
    const gchar *path = webkit_web_plugin_get_path (plugin);

    /* Write out a JSON representation of the information */
    g_string_append_printf (list,
            "{\"name\": \"%s\","
            "\"description\": \"%s\","
            "\"enabled\": %s,"
            "\"path\": \"%s\","
            "\"mimetypes\": [", /* Open array for the mimetypes */
            name,
            desc,
            enabled ? "true" : "false",
            path);

    g_slist_foreach (mimetypes, (GFunc)mimetype_list_append, list);

    /* Close the array and the object */
    g_string_append (list, "]}");
}

static gchar *
get_plugin_list() {
    WebKitWebPluginDatabase *db = webkit_get_web_plugin_database ();
    GSList *plugins = webkit_web_plugin_database_get_plugins (db);

    GString *list = g_string_new ("[");

    g_slist_foreach (plugins, (GFunc)plugin_list_append, list);

    g_string_append_c (list, ']');

    webkit_web_plugin_database_plugins_list_free (plugins);

    return g_string_free (list, FALSE);
}
#endif
