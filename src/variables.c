#include "variables.h"

#include "gui.h"
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

/* Network variables */
DECLARE_SETTER (gchar *, proxy_url);
DECLARE_SETTER (int, max_conns);
DECLARE_SETTER (int, max_conns_host);
DECLARE_SETTER (int, http_debug);
DECLARE_GETSET (gchar *, ssl_ca_file);
DECLARE_GETSET (int, ssl_verify);
DECLARE_GETSET (gchar *, cache_model);

/* Security variables */
DECLARE_GETSET (int, enable_private);
DECLARE_GETSET (int, enable_universal_file_access);
DECLARE_GETSET (int, enable_cross_file_access);
DECLARE_GETSET (int, enable_hyperlink_auditing);
DECLARE_GETSET (int, cookie_policy);
#if WEBKIT_CHECK_VERSION (1, 3, 13)
DECLARE_GETSET (int, enable_dns_prefetch);
#endif
#if WEBKIT_CHECK_VERSION (1, 11, 2)
DECLARE_GETSET (int, display_insecure_content);
DECLARE_GETSET (int, run_insecure_content);
#endif
DECLARE_SETTER (int, maintain_history);

/* Inspector variables */
DECLARE_GETSET (int, profile_js);
DECLARE_GETSET (int, profile_timeline);
#if WEBKIT_CHECK_VERSION (1, 3, 17)
DECLARE_GETTER (gchar *, inspected_uri);
#endif

/* Page variables */
DECLARE_SETTER (gchar *, uri);
DECLARE_SETTER (gchar *, useragent);
DECLARE_SETTER (gchar *, accept_languages);
DECLARE_SETTER (int, view_source);
DECLARE_GETSET (float, zoom_level);
DECLARE_GETSET (float, zoom_step);
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, zoom_type);
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 91)
DECLARE_GETSET (int, zoom_text_only);
#endif
#endif
DECLARE_GETSET (int, caret_browsing);
#if WEBKIT_CHECK_VERSION (1, 3, 5)
DECLARE_GETSET (int, enable_frame_flattening);
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 0)
DECLARE_GETSET (int, enable_smooth_scrolling);
#endif
DECLARE_GETSET (int, transparent);
#if WEBKIT_CHECK_VERSION (1, 3, 4)
DECLARE_GETSET (gchar *, view_mode);
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 8)
DECLARE_GETSET (int, enable_fullscreen);
#endif
DECLARE_GETSET (int, editable);

/* Javascript variables */
DECLARE_GETSET (int, enable_scripts);
DECLARE_GETSET (int, javascript_windows);
DECLARE_GETSET (int, javascript_dom_paste);
#if WEBKIT_CHECK_VERSION (1, 3, 0)
DECLARE_GETSET (int, javascript_clipboard);
#endif

/* Image variables */
DECLARE_GETSET (int, autoload_images);
DECLARE_GETSET (int, autoshrink_images);

/* Spell checking variables */
DECLARE_GETSET (int, enable_spellcheck);
DECLARE_GETSET (gchar *, spellcheck_languages);

/* Form variables */
DECLARE_GETSET (int, resizable_text_areas);
DECLARE_GETSET (int, enable_spatial_navigation);
DECLARE_GETSET (int, editing_behavior);
DECLARE_GETSET (int, enable_tab_cycle);

/* Text variables */
DECLARE_GETSET (gchar *, default_encoding);
DECLARE_GETSET (gchar *, custom_encoding);
DECLARE_GETSET (int, enforce_96_dpi);
DECLARE_GETTER (gchar *, current_encoding);

/* Font variables */
DECLARE_GETSET (gchar *, default_font_family);
DECLARE_GETSET (gchar *, monospace_font_family);
DECLARE_GETSET (gchar *, sans_serif_font_family);
DECLARE_GETSET (gchar *, serif_font_family);
DECLARE_GETSET (gchar *, cursive_font_family);
DECLARE_GETSET (gchar *, fantasy_font_family);

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
    { "ssl_ca_file",                  UZBL_V_FUNC (ssl_ca_file,                            STR)},
    { "ssl_verify",                   UZBL_V_FUNC (ssl_verify,                             INT)},
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
    { "maintain_history",             UZBL_V_INT (uzbl.behave.maintain_history,            set_maintain_history)},

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

#define GOBJECT_GETTER(type, name, obj, prop) \
    IMPLEMENT_GETTER (type, name)             \
    {                                         \
        type name;                            \
                                              \
        g_object_get (G_OBJECT (obj),         \
            prop, &name,                      \
            NULL);                            \
                                              \
        return name;                          \
    }

#define GOBJECT_SETTER(type, name, obj, prop) \
    IMPLEMENT_SETTER (type, name)             \
    {                                         \
        g_object_set (G_OBJECT (obj),         \
            prop, name,                       \
            NULL);                            \
    }

#define GOBJECT_GETSET(type, name, obj, prop) \
    GOBJECT_GETTER (type, name, obj, prop)    \
    GOBJECT_SETTER (type, name, obj, prop)

#define ENUM_TO_STRING(val, str) \
    case val:                    \
        out = str;               \
        break;
#define STRING_TO_ENUM(val, str) \
    if (!g_strcmp0 (in, str)) {  \
        out = val;               \
    } else

#define CHOICE_GETSET(type, name, get, set) \
    IMPLEMENT_GETTER (gchar *, name)        \
    {                                       \
        type val = get ();                  \
        gchar *out = "unknown";             \
                                            \
        switch (val) {                      \
            name##_choices (ENUM_TO_STRING) \
            default:                        \
                break;                      \
        }                                   \
                                            \
        return g_strdup (out);              \
    }                                       \
                                            \
    IMPLEMENT_SETTER (gchar *, name)        \
    {                                       \
        type out;                           \
        const gchar *in = name;             \
                                            \
        name##_choices (STRING_TO_ENUM)     \
        {                                   \
            return;                         \
        }                                   \
                                            \
        set (out);                          \
    }

static GObject *
webkit_settings ();
static GObject *
soup_session ();
static GObject *
cookie_jar ();
static GObject *
inspector ();
static GObject *
webkit_view ();

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

/* Network variables */
IMPLEMENT_SETTER (gchar *, proxy_url)
{
    g_free (uzbl.net.proxy_url);
    uzbl.net.proxy_url = g_strdup (proxy_url);

    const gchar *url = uzbl.net.proxy_url;
    SoupSession *session  = uzbl.net.soup_session;
    SoupURI     *soup_uri = NULL;

    if (url && *url && *url != ' ') {
        soup_uri = soup_uri_new (url);
    }

    g_object_set (G_OBJECT (session),
        SOUP_SESSION_PROXY_URI, soup_uri,
        NULL);

    soup_uri_free (soup_uri);
}

IMPLEMENT_SETTER (int, max_conns)
{
    uzbl.net.max_conns = max_conns;

    g_object_set (G_OBJECT (uzbl.net.soup_session),
        SOUP_SESSION_MAX_CONNS, uzbl.net.max_conns,
        NULL);
}

IMPLEMENT_SETTER (int, max_conns_host)
{
    uzbl.net.max_conns_host = max_conns_host;

    g_object_set (G_OBJECT (uzbl.net.soup_session),
        SOUP_SESSION_MAX_CONNS_PER_HOST, uzbl.net.max_conns_host,
        NULL);
}

IMPLEMENT_SETTER (int, http_debug)
{
    uzbl.behave.http_debug = http_debug;

    if (uzbl.net.soup_logger) {
        soup_session_remove_feature (
            uzbl.net.soup_session, SOUP_SESSION_FEATURE (uzbl.net.soup_logger));
        g_object_unref (uzbl.net.soup_logger);
    }

    uzbl.net.soup_logger = soup_logger_new (uzbl.behave.http_debug, -1);
    soup_session_add_feature (
        uzbl.net.soup_session, SOUP_SESSION_FEATURE (uzbl.net.soup_logger));
}

GOBJECT_GETSET (gchar *, ssl_ca_file,
                soup_session (), "ssl-ca-file")

GOBJECT_GETSET (int, ssl_verify,
                soup_session (), "ssl-strict")

#define cache_model_choices(call)                                 \
    call (WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER , "document_viewer") \
    call (WEBKIT_CACHE_MODEL_WEB_BROWSER,      "web_browser")     \
    call (WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER, "document_browser")

CHOICE_GETSET (WebKitCacheModel, cache_model,
               webkit_get_cache_model, webkit_set_cache_model)

#undef cache_model_choices

/* Security variables */
DECLARE_GETSET (int, enable_private_webkit);

IMPLEMENT_GETTER (int, enable_private)
{
    return get_enable_private_webkit ();
}

IMPLEMENT_SETTER (int, enable_private)
{
    static const char *priv_envvar = "UZBL_PRIVATE";

    if (enable_private) {
        setenv (priv_envvar, "true", 1);
    } else {
        unsetenv (priv_envvar);
    }

    set_enable_private_webkit (enable_private);
}

GOBJECT_GETSET (int, enable_private_webkit,
                webkit_settings (), "enable-private-browsing")

GOBJECT_GETSET (int, enable_universal_file_access,
                webkit_settings (), "enable-universal-access-from-file-urls")

GOBJECT_GETSET (int, enable_cross_file_access,
                webkit_settings (), "enable-file-access-from-file-urls")

GOBJECT_GETSET (int, enable_hyperlink_auditing,
                webkit_settings (), "enable-hyperlink-auditing")

GOBJECT_GETSET (int, cookie_policy,
                cookie_jar (), "accept-policy")

#if WEBKIT_CHECK_VERSION (1, 3, 13)
GOBJECT_GETSET (int, enable_dns_prefetch,
                webkit_settings (), "enable-dns-prefetching")
#endif

#if WEBKIT_CHECK_VERSION (1, 11, 2)
GOBJECT_GETSET (int, display_insecure_content,
                webkit_settings (), "enable-display-of-insecure-content")

GOBJECT_GETSET (int, run_insecure_content,
                webkit_settings (), "enable-running-of-insecure-content")
#endif

IMPLEMENT_SETTER (int, maintain_history)
{
    uzbl.behave.maintain_history = maintain_history;

    webkit_web_view_set_maintains_back_forward_list (uzbl.gui.web_view, maintain_history);
}

/* Inspector variables */
GOBJECT_GETSET (int, profile_js,
                inspector (), "javascript-profiling-enabled")

GOBJECT_GETSET (int, profile_timeline,
                inspector (), "timeline-profiling-enabled")

#if WEBKIT_CHECK_VERSION (1, 3, 17)
IMPLEMENT_GETTER (gchar *, inspected_uri)
{
    return g_strdup (webkit_web_inspector_get_inspected_uri (uzbl.gui.inspector));
}
#endif

/* Page variables */
static gchar *
make_uri_from_user_input (const gchar *uri);

IMPLEMENT_SETTER (gchar *, uri)
{
    /* Strip leading whitespace. */
    while (*uri && isspace (*uri)) {
        ++uri;
    }

    /* Don't do anything when given a blank URL. */
    if (!uri[0]) {
        return;
    }

    g_free (uzbl.state.uri);
    uzbl.state.uri = g_strdup (uri);

    /* Evaluate javascript: URIs. */
    /* TODO: Use strprefix. */
    if (!strncmp (uri, "javascript:", 11)) {
        eval_js (uzbl.gui.web_view, uri, NULL, "javascript:");
        return;
    }

    /* Attempt to parse the URI. */
    gchar *newuri = make_uri_from_user_input (uri);

    /* TODO: Collect all environment settings into one place. */
    set_window_property ("UZBL_URI", newuri);
    webkit_web_view_load_uri (uzbl.gui.web_view, newuri);

    g_free (newuri);
}

static gboolean
string_is_integer (const char *s);

gchar *
make_uri_from_user_input (const gchar *uri)
{
    gchar *result = NULL;

    SoupURI *soup_uri = soup_uri_new (uri);
    if (soup_uri) {
        /* This looks like a valid URI. */
        if (!soup_uri->host && string_is_integer (soup_uri->path)) {
            /* The user probably typed in a host:port without a scheme. */
            /* TODO: Add an option to default to https? */
            result = g_strconcat("http://", uri, NULL);
        } else {
            result = g_strdup (uri);
        }

        soup_uri_free (soup_uri);

        return result;
    }

    /* It's not a valid URI, maybe it's a path on the filesystem? Check to see
     * if such a path exists. */
    if (file_exists (uri)) {
        if (g_path_is_absolute (uri)) {
            return g_strconcat ("file://", uri, NULL);
        }

        /* Make it into an absolute path */
        gchar *wd = g_get_current_dir ();
        result = g_strconcat ("file://", wd, "/", uri, NULL);
        g_free (wd);

        return result;
    }

    /* Not a path on the filesystem, just assume it's an HTTP URL. */
    return g_strconcat ("http://", uri, NULL);
}

gboolean
string_is_integer (const char *s)
{
    /* Is the given string made up entirely of decimal digits? */
    return (strspn (s, "0123456789") == strlen (s));
}

IMPLEMENT_SETTER (gchar *, useragent)
{
    g_free (uzbl.net.useragent);

    if (!useragent || !*useragent) {
        uzbl.net.useragent = NULL;
    } else {
        uzbl.net.useragent = g_strdup (useragent);

        g_object_set (G_OBJECT (uzbl.net.soup_session),
            SOUP_SESSION_USER_AGENT, uzbl.net.useragent,
            NULL);
        g_object_set (webkit_settings (),
            "user-agent", uzbl.net.useragent,
            NULL);
    }
}

IMPLEMENT_SETTER (gchar *, accept_languages)
{
    g_free (uzbl.net.accept_languages);

    if (!*accept_languages || *accept_languages == ' ') {
        uzbl.net.accept_languages = NULL;
    } else {
        uzbl.net.accept_languages = g_strdup (accept_languages);

        g_object_set (G_OBJECT (uzbl.net.soup_session),
            SOUP_SESSION_ACCEPT_LANGUAGE, uzbl.net.accept_languages,
            NULL);
    }
}

IMPLEMENT_SETTER (int, view_source)
{
    uzbl.behave.view_source = view_source;

    webkit_web_view_set_view_source_mode (uzbl.gui.web_view, uzbl.behave.view_source);
}

IMPLEMENT_GETTER (float, zoom_level)
{
    return webkit_web_view_get_zoom_level (uzbl.gui.web_view);
}

IMPLEMENT_SETTER (float, zoom_level)
{
    webkit_web_view_set_zoom_level (uzbl.gui.web_view, zoom_level);
}

GOBJECT_GETSET (float, zoom_step,
                webkit_settings (), "zoom-step")

#ifndef USE_WEBKIT2
IMPLEMENT_GETTER (int, zoom_type)
{
    return webkit_web_view_get_full_content_zoom (uzbl.gui.web_view);
}

IMPLEMENT_SETTER (int, zoom_type)
{
    webkit_web_view_set_full_content_zoom (uzbl.gui.web_view, zoom_type);
}
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 91)
GOBJECT_GETSET (int, zoom_text_only,
                webkit_settings (), "zoom-text-only")
#endif
#endif

GOBJECT_GETSET (int, caret_browsing,
                webkit_settings (), "enable-caret-browsing")

#if WEBKIT_CHECK_VERSION (1, 3, 5)
GOBJECT_GETSET (int, enable_frame_flattening,
                webkit_settings (), "enable-frame-flattening")
#endif

#if WEBKIT_CHECK_VERSION (1, 9, 0)
GOBJECT_GETSET (int, enable_smooth_scrolling,
                webkit_settings (), "enable-smooth-scrolling")
#endif

GOBJECT_GETSET (int, transparent,
                webkit_view (), "transparent")

#if WEBKIT_CHECK_VERSION (1, 3, 4)
#define view_mode_choices(call)                             \
    call (WEBKIT_WEB_VIEW_VIEW_MODE_WINDOWED, "windowed")   \
    call (WEBKIT_WEB_VIEW_VIEW_MODE_FLOATING, "floating")   \
    call (WEBKIT_WEB_VIEW_VIEW_MODE_MAXIMIZED, "maximized") \
    call (WEBKIT_WEB_VIEW_VIEW_MODE_MINIMIZED, "minimized")

#define _webkit_web_view_get_view_mode() \
    webkit_web_view_get_view_mode (uzbl.gui.web_view)
#define _webkit_web_view_set_view_mode(val) \
    webkit_web_view_set_view_mode (uzbl.gui.web_view, val)

CHOICE_GETSET (WebKitWebViewViewMode, view_mode,
               _webkit_web_view_get_view_mode, _webkit_web_view_set_view_mode)

#undef _webkit_web_view_get_view_mode
#undef _webkit_web_view_set_view_mode
#undef view_mode_choices
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 8)
GOBJECT_GETSET (int, enable_fullscreen,
                webkit_settings (), "enable-fullscreen")
#endif

GOBJECT_GETSET (int, editable,
                webkit_view (), "editable")

/* Javascript variables */
GOBJECT_GETSET (int, enable_scripts,
                webkit_settings (), "enable-scripts")

GOBJECT_GETSET (int, javascript_windows,
                webkit_settings (), "javascript-can-open-windows-automatically")

GOBJECT_GETSET (int, javascript_dom_paste,
                webkit_settings (), "enable-dom-paste")

#if WEBKIT_CHECK_VERSION (1, 3, 0)
GOBJECT_GETSET (int, javascript_clipboard,
                webkit_settings (), "javascript-can-access-clipboard")
#endif

/* Image variables */
GOBJECT_GETSET (int, autoload_images,
                webkit_settings (), "auto-load-images")

GOBJECT_GETSET (int, autoshrink_images,
                webkit_settings (), "auto-shrink-images")

/* Spell checking variables */
GOBJECT_GETSET (int, enable_spellcheck,
                webkit_settings (), "enable-spell-checking")

GOBJECT_GETTER (gchar *, spellcheck_languages,
                webkit_settings (), "spell-checking-languages")

IMPLEMENT_SETTER (gchar *, spellcheck_languages)
{
  GObject *obj = webkit_get_text_checker ();

  if (!obj) {
      return;
  }
  if (!WEBKIT_IS_SPELL_CHECKER (obj)) {
      return;
  }

  WebKitSpellChecker *checker = WEBKIT_SPELL_CHECKER (obj);

  webkit_spell_checker_update_spell_checking_languages (checker, spellcheck_languages);
  g_object_set (webkit_settings (),
    "spell-checking-languages", spellcheck_languages,
    NULL);
}

/* Form variables */
GOBJECT_GETSET (int, resizable_text_areas,
                webkit_settings (), "resizable-text-areas")

GOBJECT_GETSET (int, enable_spatial_navigation,
                webkit_settings (), "enable-spatial-navigation")

GOBJECT_GETSET (int, editing_behavior,
                webkit_settings (), "editing-behavior")

GOBJECT_GETSET (int, enable_tab_cycle,
                webkit_settings (), "tab-key-cycles-through-elements")

/* Text variables */
GOBJECT_GETSET (gchar *, default_encoding,
                webkit_settings (), "default_encoding")

IMPLEMENT_GETTER (gchar *, custom_encoding)
{
    const gchar *encoding = webkit_web_view_get_custom_encoding (uzbl.gui.web_view);
    return g_strdup (encoding);
}

IMPLEMENT_SETTER (gchar *, custom_encoding)
{
    if (!*custom_encoding) {
        custom_encoding = NULL;
    }

    webkit_web_view_set_custom_encoding (uzbl.gui.web_view, custom_encoding);
}

GOBJECT_GETSET (int, enforce_96_dpi,
                webkit_settings (), "enforce-96-dpi")

IMPLEMENT_GETTER (gchar *, current_encoding)
{
    const gchar *encoding = webkit_web_view_get_encoding (uzbl.gui.web_view);
    return g_strdup (encoding);
}

/* Font variables */
GOBJECT_GETSET (gchar *, default_font_family,
                webkit_settings (), "default-font-family")

GOBJECT_GETSET (gchar *, monospace_font_family,
                webkit_settings (), "monospace-font-family")

GOBJECT_GETSET (gchar *, sans_serif_font_family,
                webkit_settings (), "sans_serif-font-family")

GOBJECT_GETSET (gchar *, serif_font_family,
                webkit_settings (), "serif-font-family")

GOBJECT_GETSET (gchar *, cursive_font_family,
                webkit_settings (), "cursive-font-family")

GOBJECT_GETSET (gchar *, fantasy_font_family,
                webkit_settings (), "fantasy-font-family")

GObject *
webkit_settings ()
{
    return G_OBJECT (webkit_web_view_get_settings (uzbl.gui.web_view));
}

GObject *
soup_session ()
{
    return G_OBJECT (uzbl.net.soup_session);
}

GObject *
cookie_jar ()
{
    return G_OBJECT (uzbl.net.soup_cookie_jar);
}

GObject *
inspector ()
{
    return G_OBJECT (uzbl.gui.inspector);
}

GObject *
webkit_view ()
{
    return G_OBJECT (uzbl.gui.web_view);
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

#define EXPOSE_WEBKIT_VIEW_SETTINGS(SYM, PROPERTY, TYPE) \
static void set_##SYM(TYPE val) { \
  g_object_set(view_settings(), (PROPERTY), val, NULL); \
} \
static TYPE get_##SYM() { \
  TYPE val; \
  g_object_get(view_settings(), (PROPERTY), &val, NULL); \
  return val; \
}

/* Font size settings */
EXPOSE_WEBKIT_VIEW_SETTINGS(minimum_font_size,            "minimum-font-size",                         int)
EXPOSE_WEBKIT_VIEW_SETTINGS(font_size,                    "default-font-size",                         int)
EXPOSE_WEBKIT_VIEW_SETTINGS(monospace_size,               "default-monospace-font-size",               int)

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

/* Media settings */
#if WEBKIT_CHECK_VERSION (1, 9, 3)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_inline_media,          "media-playback-allows-inline",              int)
EXPOSE_WEBKIT_VIEW_SETTINGS(require_click_to_play,        "media-playback-requires-user-gesture",      int)
#endif
#if WEBKIT_CHECK_VERSION (1, 11, 1)
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_media_stream,          "enable-media-stream",                       int)
#endif

/* Hacks */
EXPOSE_WEBKIT_VIEW_SETTINGS(enable_site_workarounds,      "enable-site-specific-quirks",               int)

#undef EXPOSE_WEBKIT_VIEW_SETTINGS

static void
set_inject_html(const gchar *html) {
#ifdef USE_WEBKIT2
    webkit_web_view_load_html (uzbl.gui.web_view, html, NULL);
#else
    webkit_web_view_load_html_string (uzbl.gui.web_view, html, NULL);
#endif
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
