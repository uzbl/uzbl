#include "variables.h"

#include "commands.h"
#include "events.h"
#include "gui.h"
#include "io.h"
#include "js.h"
#include "sync.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"

#include <JavaScriptCore/JavaScript.h>

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* TODO: (WebKit2)
 *
 *   - Add variables for cookies.
 *   - Add variables for back/forward lists (also WK1).
 *   - Add variables for favicons (also WK1).
 *   - Expose WebView's is-loading property.
 *   - Expose information from webkit_web_view_get_tls_info.
 *
 * (WebKit1)
 *
 *   - Expose WebKitViewportAttributes values.
 *   - Expose list of frames (ro).
 */

/* ======================== VARIABLES TABLE ========================= */

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 4)
#define HAVE_PLUGIN_API
#endif
#else
#if WEBKIT_CHECK_VERSION (1, 3, 8)
#define HAVE_PLUGIN_API
#endif
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 91)
#define HAVE_ZOOM_TEXT_API
#endif
#else
#define HAVE_ZOOM_TEXT_API
#endif

/* A really generic function pointer. */
typedef void (*UzblFunction) ();

typedef union {
    int                *i;
    float              *f;
    gchar             **s;
    unsigned long long *ull;
} UzblValue;

typedef struct {
    UzblType type;
    UzblValue value;
    gboolean writeable;
    gboolean builtin;

    UzblFunction get;
    UzblFunction set;
} UzblVariable;

/* Abbreviations to help keep the table's width humane. */
#define UZBL_SETTING(typ, val, w, getter, setter) \
    { .type = TYPE_##typ, .value = val, .writeable = w, .builtin = TRUE, .get = (UzblFunction)getter, .set = (UzblFunction)setter }

#define UZBL_VARIABLE(typ, val, getter, setter) \
    UZBL_SETTING (typ, val, TRUE, getter, setter)
#define UZBL_CONSTANT(typ, val, getter) \
    UZBL_SETTING (typ, val, FALSE, getter, NULL)

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
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, enable_builtin_auth);
#endif

/* Window variables */
DECLARE_GETSET (gchar *, geometry);
DECLARE_SETTER (gchar *, icon);
DECLARE_SETTER (gchar *, icon_name);
DECLARE_GETSET (gchar *, window_role);
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, auto_resize_window);
#endif

/* UI variables */
DECLARE_GETSET (int, show_status);
DECLARE_SETTER (int, status_top);
DECLARE_SETTER (gchar *, status_background);
#ifdef USE_WEBKIT2
DECLARE_GETSET (int, enable_compositing_debugging);
#endif

/* Customization */
#if !WEBKIT_CHECK_VERSION (1, 9, 0)
DECLARE_GETSET (int, default_context_menu);
#endif

/* Printing variables */
DECLARE_GETSET (int, print_backgrounds);

/* Network variables */
#ifndef USE_WEBKIT2
DECLARE_SETTER (gchar *, proxy_url);
DECLARE_SETTER (int, max_conns);
DECLARE_SETTER (int, max_conns_host);
DECLARE_SETTER (gchar *, http_debug);
DECLARE_GETSET (gchar *, ssl_ca_file);
#endif
DECLARE_GETSET (gchar *, ssl_policy);
DECLARE_GETSET (gchar *, cache_model);

/* Security variables */
DECLARE_GETSET (int, enable_private);
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, enable_universal_file_access);
DECLARE_GETSET (int, enable_cross_file_access);
#endif
DECLARE_GETSET (int, enable_hyperlink_auditing);
#ifdef USE_WEBKIT2
DECLARE_GETSET (int, enable_xss_auditing);
/*DECLARE_GETSET (gchar *, cookie_location);*/
/*DECLARE_GETSET (gchar *, cookie_store);*/
#endif
DECLARE_GETSET (gchar *, cookie_policy);
#if WEBKIT_CHECK_VERSION (1, 3, 13)
DECLARE_GETSET (int, enable_dns_prefetch);
#endif
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 2)
DECLARE_GETSET (int, display_insecure_content);
DECLARE_GETSET (int, run_insecure_content);
#endif
/* TODO: For WebKit2, we'll have to manage the BackForwardList manually. */
DECLARE_SETTER (int, maintain_history);
#endif

/* Inspector variables */
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, profile_js);
DECLARE_GETSET (int, profile_timeline);
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 17)
DECLARE_GETTER (gchar *, inspected_uri);
#endif

/* Page variables */
DECLARE_SETTER (gchar *, uri);
DECLARE_SETTER (gchar *, useragent);
DECLARE_SETTER (gchar *, accept_languages);
DECLARE_SETTER (int, view_source);
DECLARE_GETSET (float, zoom_level);
#ifdef HAVE_ZOOM_TEXT_API
DECLARE_GETSET (int, zoom_text_only);
#endif
DECLARE_GETSET (int, caret_browsing);
#if WEBKIT_CHECK_VERSION (1, 3, 5)
DECLARE_GETSET (int, enable_frame_flattening);
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 0)
DECLARE_GETSET (int, enable_smooth_scrolling);
#endif
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, transparent);
#if WEBKIT_CHECK_VERSION (1, 3, 4)
DECLARE_GETSET (gchar *, window_view_mode);
#endif
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 8)
DECLARE_GETSET (int, enable_fullscreen);
#endif
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, editable);
#endif

/* Javascript variables */
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, enable_scripts);
#endif
DECLARE_GETSET (int, javascript_windows);
#ifdef USE_WEBKIT2
DECLARE_GETSET (int, javascript_modal_dialogs);
#endif
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, javascript_dom_paste);
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 0)
DECLARE_GETSET (int, javascript_clipboard);
#endif

/* Image variables */
DECLARE_GETSET (int, autoload_images);
#ifdef USE_WEBKIT2
DECLARE_GETSET (int, always_load_icons);
#endif
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, autoshrink_images);
DECLARE_GETSET (int, use_image_orientation);
#endif

/* Spell checking variables */
DECLARE_GETSET (int, enable_spellcheck);
DECLARE_GETSET (gchar *, spellcheck_languages);

/* Form variables */
DECLARE_GETSET (int, resizable_text_areas);
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, enable_spatial_navigation);
DECLARE_GETSET (gchar *, editing_behavior);
#endif
DECLARE_GETSET (int, enable_tab_cycle);

/* Text variables */
DECLARE_GETSET (gchar *, default_encoding);
DECLARE_GETSET (gchar *, custom_encoding);
#ifndef USE_WEBKIT2
DECLARE_GETTER (gchar *, current_encoding);
DECLARE_GETSET (int, enforce_96_dpi);
#endif

/* Font variables */
DECLARE_GETSET (gchar *, default_font_family);
DECLARE_GETSET (gchar *, monospace_font_family);
DECLARE_GETSET (gchar *, sans_serif_font_family);
DECLARE_GETSET (gchar *, serif_font_family);
DECLARE_GETSET (gchar *, cursive_font_family);
DECLARE_GETSET (gchar *, fantasy_font_family);
#ifdef USE_WEBKIT2
DECLARE_GETSET (gchar *, pictograph_font_family);
#endif

/* Font size variables */
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, minimum_font_size);
DECLARE_GETSET (int, minimum_logical_font_size);
#endif
DECLARE_GETSET (int, font_size);
DECLARE_GETSET (int, monospace_size);

/* Feature variables */
DECLARE_GETSET (int, enable_plugins);
DECLARE_GETSET (int, enable_java_applet);
#ifdef HAVE_PLUGIN_API
DECLARE_GETTER (gchar *, plugin_list);
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 14)
DECLARE_GETSET (int, enable_webgl);
#endif
#if WEBKIT_CHECK_VERSION (1, 7, 5)
DECLARE_GETSET (int, enable_webaudio);
#endif
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 90) /* Documentation says 1.7.5, but it's not there. */
DECLARE_GETSET (int, enable_3d_acceleration);
#endif
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 3)
DECLARE_GETSET (int, enable_inline_media);
DECLARE_GETSET (int, require_click_to_play);
#endif
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 1)
DECLARE_GETSET (int, enable_css_shaders);
DECLARE_GETSET (int, enable_media_stream);
#endif
#endif

/* HTML5 Database variables */
DECLARE_GETSET (int, enable_database);
DECLARE_GETSET (int, enable_local_storage);
DECLARE_GETSET (int, enable_pagecache);
DECLARE_GETSET (int, enable_offline_app_cache);
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 3, 13)
DECLARE_GETSET (unsigned long long, app_cache_size);
/* DECLARE_GETTER (gchar *, app_cache_directory); */
#endif
DECLARE_GETSET (gchar *, web_database_directory);
DECLARE_GETSET (unsigned long long, web_database_quota);
#if WEBKIT_CHECK_VERSION (1, 5, 2)
DECLARE_GETSET (gchar *, local_storage_path);
#endif
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 92)
DECLARE_SETTER (gchar *, disk_cache_directory);
#endif
DECLARE_SETTER (gchar *, web_extensions_directory);
#endif

/* Hacks */
DECLARE_GETSET (int, enable_site_workarounds);

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
#ifndef USE_WEBKIT2
    { "enable_builtin_auth",          UZBL_V_FUNC (enable_builtin_auth,                    INT)},
#endif

    /* Window variables */
    { "geometry",                     UZBL_V_FUNC (geometry,                               STR)},
    { "icon",                         UZBL_V_STRING (uzbl.gui.icon,                        set_icon)},
    { "icon_name",                    UZBL_V_STRING (uzbl.gui.icon_name,                   set_icon_name)},
    { "window_role",                  UZBL_V_FUNC (window_role,                            STR)},
#ifndef USE_WEBKIT2
    { "auto_resize_window",           UZBL_V_FUNC (auto_resize_window,                     INT)},
#endif

    /* UI variables */
    { "show_status",                  UZBL_V_FUNC (show_status,                            INT)},
    { "status_top",                   UZBL_V_INT (uzbl.behave.status_top,                  set_status_top)},
    { "status_format",                UZBL_V_STRING (uzbl.behave.status_format,            NULL)},
    { "status_format_right",          UZBL_V_STRING (uzbl.behave.status_format_right,      NULL)},
    { "status_background",            UZBL_V_STRING (uzbl.behave.status_background,        set_status_background)},
    { "title_format_long",            UZBL_V_STRING (uzbl.behave.title_format_long,        NULL)},
    { "title_format_short",           UZBL_V_STRING (uzbl.behave.title_format_short,       NULL)},
#ifdef USE_WEBKIT2
    { "enable_compositing_debugging", UZBL_V_FUNC (enable_compositing_debugging,           INT)},
#endif

    /* Customization */
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
#ifndef USE_WEBKIT2
    { "proxy_url",                    UZBL_V_STRING (uzbl.net.proxy_url,                   set_proxy_url)},
    { "max_conns",                    UZBL_V_INT (uzbl.net.max_conns,                      set_max_conns)},
    { "max_conns_host",               UZBL_V_INT (uzbl.net.max_conns_host,                 set_max_conns_host)},
    { "http_debug",                   UZBL_V_STRING (uzbl.behave.http_debug,               set_http_debug)},
    { "ssl_ca_file",                  UZBL_V_FUNC (ssl_ca_file,                            STR)},
#endif
    { "ssl_policy",                   UZBL_V_FUNC (ssl_policy,                             STR)
                                                                                               },
    { "cache_model",                  UZBL_V_FUNC (cache_model,                            STR)},

    /* Security variables */
    { "enable_private",               UZBL_V_FUNC (enable_private,                         INT)},
#ifndef USE_WEBKIT2
    { "enable_universal_file_access", UZBL_V_FUNC (enable_universal_file_access,           INT)},
    { "enable_cross_file_access",     UZBL_V_FUNC (enable_cross_file_access,               INT)},
#endif
    { "enable_hyperlink_auditing",    UZBL_V_FUNC (enable_hyperlink_auditing,              INT)},
#ifdef USE_WEBKIT2
    { "enable_xss_auditing",          UZBL_V_FUNC (enable_xss_auditing,                    INT)},
#endif
    { "cookie_policy",                UZBL_V_FUNC (cookie_policy,                          STR)},
#if WEBKIT_CHECK_VERSION (1, 3, 13)
    { "enable_dns_prefetch",          UZBL_V_FUNC (enable_dns_prefetch,                    INT)},
#endif
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 2)
    { "display_insecure_content",     UZBL_V_FUNC (display_insecure_content,               INT)},
    { "run_insecure_content",         UZBL_V_FUNC (run_insecure_content,                   INT)},
#endif
    { "maintain_history",             UZBL_V_INT (uzbl.behave.maintain_history,            set_maintain_history)},
#endif

    /* Inspector variables */
#ifndef USE_WEBKIT2
    { "profile_js",                   UZBL_V_FUNC (profile_js,                             INT)},
    { "profile_timeline",             UZBL_V_FUNC (profile_timeline,                       INT)},
#endif
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
    { "zoom_step",                    UZBL_V_FLOAT (uzbl.behave.zoom_step,                 NULL)},
#ifdef HAVE_ZOOM_TEXT_API
    { "zoom_text_only",               UZBL_V_FUNC (zoom_text_only,                         INT)},
#endif
    { "caret_browsing",               UZBL_V_FUNC (caret_browsing,                         INT)},
#if WEBKIT_CHECK_VERSION (1, 3, 5)
    { "enable_frame_flattening",      UZBL_V_FUNC (enable_frame_flattening,                INT)},
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 0)
    { "enable_smooth_scrolling",      UZBL_V_FUNC (enable_smooth_scrolling,                INT)},
#endif
#ifndef USE_WEBKIT2
    { "transparent",                  UZBL_V_FUNC (transparent,                            INT)},
#if WEBKIT_CHECK_VERSION (1, 3, 4)
    { "window_view_mode",             UZBL_V_FUNC (window_view_mode,                       STR)},
#endif
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 8)
    { "enable_fullscreen",            UZBL_V_FUNC (enable_fullscreen,                      INT)},
#endif
#ifndef USE_WEBKIT2
    { "editable",                     UZBL_V_FUNC (editable,                               INT)},
#endif

    /* Javascript variables */
#ifndef USE_WEBKIT2
    { "enable_scripts",               UZBL_V_FUNC (enable_scripts,                         INT)},
#endif
    { "javascript_windows",           UZBL_V_FUNC (javascript_windows,                     INT)},
#ifdef USE_WEBKIT2
    { "javascript_modal_dialogs",     UZBL_V_FUNC (javascript_modal_dialogs,               INT)},
#endif
#ifndef USE_WEBKIT2
    { "javascript_dom_paste",         UZBL_V_FUNC (javascript_dom_paste,                   INT)},
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 0)
    { "javascript_clipboard",         UZBL_V_FUNC (javascript_clipboard,                   INT)},
#endif

    /* Image variables */
    { "autoload_images",              UZBL_V_FUNC (autoload_images,                        INT)},
#ifdef USE_WEBKIT2
    { "always_load_icons",            UZBL_V_FUNC (always_load_icons,                      INT)},
#endif
#ifndef USE_WEBKIT2
    { "autoshrink_images",            UZBL_V_FUNC (autoshrink_images,                      INT)},
    { "use_image_orientation",        UZBL_V_FUNC (use_image_orientation,                  INT)},
#endif

    /* Spell checking variables */
    { "enable_spellcheck",            UZBL_V_FUNC (enable_spellcheck,                      INT)},
    { "spellcheck_languages",         UZBL_V_FUNC (spellcheck_languages,                   STR)},

    /* Form variables */
    { "resizable_text_areas",         UZBL_V_FUNC (resizable_text_areas,                   INT)},
#ifndef USE_WEBKIT2
    { "enable_spatial_navigation",    UZBL_V_FUNC (enable_spatial_navigation,              INT)},
    { "editing_behavior",             UZBL_V_FUNC (editing_behavior,                       STR)},
#endif
    { "enable_tab_cycle",             UZBL_V_FUNC (enable_tab_cycle,                       INT)},

    /* Text variables */
    { "default_encoding",             UZBL_V_FUNC (default_encoding,                       STR)},
    { "custom_encoding",              UZBL_V_FUNC (custom_encoding,                        STR)},
#ifndef USE_WEBKIT2
    { "current_encoding",             UZBL_C_FUNC (current_encoding,                       STR)},
    { "enforce_96_dpi",               UZBL_V_FUNC (enforce_96_dpi,                         INT)},
#endif

    /* Font variables */
    { "default_font_family",          UZBL_V_FUNC (default_font_family,                    STR)},
    { "monospace_font_family",        UZBL_V_FUNC (monospace_font_family,                  STR)},
    { "sans_serif_font_family",       UZBL_V_FUNC (sans_serif_font_family,                 STR)},
    { "serif_font_family",            UZBL_V_FUNC (serif_font_family,                      STR)},
    { "cursive_font_family",          UZBL_V_FUNC (cursive_font_family,                    STR)},
    { "fantasy_font_family",          UZBL_V_FUNC (fantasy_font_family,                    STR)},
#ifdef USE_WEBKIT2
    { "pictograph_font_family",       UZBL_V_FUNC (pictograph_font_family,                 STR)},
#endif

    /* Font size variables */
#ifndef USE_WEBKIT2
    { "minimum_font_size",            UZBL_V_FUNC (minimum_font_size,                      INT)},
    { "minimum_logical_font_size",    UZBL_V_FUNC (minimum_logical_font_size,              INT)},
#endif
    { "font_size",                    UZBL_V_FUNC (font_size,                              INT)},
    { "monospace_size",               UZBL_V_FUNC (monospace_size,                         INT)},

    /* Feature variables */
    { "enable_plugins",               UZBL_V_FUNC (enable_plugins,                         INT)},
    { "enable_java_applet",           UZBL_V_FUNC (enable_java_applet,                     INT)},
#ifdef HAVE_PLUGIN_API
    { "plugin_list",                  UZBL_C_FUNC (plugin_list,                            STR)},
#endif
#if WEBKIT_CHECK_VERSION (1, 3, 14)
    { "enable_webgl",                 UZBL_V_FUNC (enable_webgl,                           INT)},
#endif
#if WEBKIT_CHECK_VERSION (1, 7, 5)
    { "enable_webaudio",              UZBL_V_FUNC (enable_webaudio,                        INT)},
#endif
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 90) /* Documentation says 1.7.5, but it's not there. */
    { "enable_3d_acceleration",       UZBL_V_FUNC (enable_3d_acceleration,                 INT)},
#endif
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 3)
    { "enable_inline_media",          UZBL_V_FUNC (enable_inline_media,                    INT)},
    { "require_click_to_play",        UZBL_V_FUNC (require_click_to_play,                  INT)},
#endif
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 1)
    { "enable_css_shaders",           UZBL_V_FUNC (enable_css_shaders,                     INT)},
    { "enable_media_stream",          UZBL_V_FUNC (enable_media_stream,                    INT)},
#endif
#endif

    /* HTML5 Database variables */
    { "enable_database",              UZBL_V_FUNC (enable_database,                        INT)},
    { "enable_local_storage",         UZBL_V_FUNC (enable_local_storage,                   INT)},
    { "enable_pagecache",             UZBL_V_FUNC (enable_pagecache,                       INT)},
    { "enable_offline_app_cache",     UZBL_V_FUNC (enable_offline_app_cache,               INT)},
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 3, 13)
    { "app_cache_size",               UZBL_V_FUNC (app_cache_size,                         ULL)},
    /* { "app_cache_directory",          UZBL_C_FUNC (app_cache_directory,                    STR)}, */
#endif
    { "web_database_directory",       UZBL_V_FUNC (web_database_directory,                 STR)},
    { "web_database_quota",           UZBL_V_FUNC (web_database_quota,                     ULL)},
#if WEBKIT_CHECK_VERSION (1, 5, 2)
    { "local_storage_path",           UZBL_V_FUNC (local_storage_path,                     STR)},
#endif
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 92)
    { "disk_cache_directory",         UZBL_V_STRING (uzbl.state.disk_cache_directory,      set_disk_cache_directory)},
#endif
    { "web_extensions_directory",     UZBL_V_STRING (uzbl.state.web_extensions_directory,  set_web_extensions_directory)},
#endif

    /* Hacks */
    { "enable_site_workarounds",      UZBL_V_FUNC (enable_site_workarounds,                INT)},

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

/* =========================== PUBLIC API =========================== */

static void
init_js_variables_api ();

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

    init_js_variables_api ();
}

static void
variable_free (gpointer key, gpointer value, gpointer data);

void
uzbl_variables_free ()
{
    g_hash_table_foreach (uzbl.behave.proto_var, variable_free, NULL);

    g_hash_table_destroy (uzbl.behave.proto_var);
    uzbl.behave.proto_var = NULL;
}

static const char *valid_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.";

gboolean
uzbl_variables_is_valid (const gchar *name)
{
    if (!name || !*name) {
        return FALSE;
    }

    if (isdigit (*name)) {
        return FALSE;
    }

    size_t loc = strspn (name, valid_chars);

    return (name[loc] == '\0');
}

static UzblVariable *
get_variable (const gchar *name);
static void
set_variable_string (UzblVariable *var, const gchar *val);
static void
set_variable_int (UzblVariable *var, int i);
static void
set_variable_ull (UzblVariable *var, unsigned long long ull);
static void
set_variable_float (UzblVariable *var, float f);
static void
send_variable_event (const gchar *name, const UzblVariable *var);

gboolean
uzbl_variables_set (const gchar *name, gchar *val)
{
    if (!val) {
        return FALSE;
    }

    UzblVariable *var = get_variable (name);

    if (var) {
        if (!var->writeable) {
            return FALSE;
        }

        switch (var->type) {
            case TYPE_STR:
                set_variable_string (var, val);
                break;
            case TYPE_INT:
            {
                int i = (int)strtol (val, NULL, 10);
                set_variable_int (var, i);
                break;
            }
            case TYPE_ULL:
            {
                unsigned long long ull = strtoull (val, NULL, 10);
                set_variable_ull (var, ull);
                break;
            }
            case TYPE_FLOAT:
            {
                float f = strtod (val, NULL);
                set_variable_float (var, f);
                break;
            }
            default:
                g_assert_not_reached ();
        }
    } else {
        /* A custom var that has not been set. Check whether name violates our
         * naming scheme. */
        if (!uzbl_variables_is_valid (name)) {
            uzbl_debug ("Invalid variable name: %s\n", name);
            return FALSE;
        }

        /* Create the variable. */
        var = g_malloc (sizeof (UzblVariable));
        var->type      = TYPE_STR;
        var->get       = NULL;
        var->set       = NULL;
        var->writeable = TRUE;
        var->builtin   = FALSE;

        var->value.s = g_malloc (sizeof (gchar *));

        g_hash_table_insert (uzbl.behave.proto_var,
            g_strdup (name), (gpointer)var);

        /* Set the value. */
        *(var->value.s) = g_strdup (val);
    }

    send_variable_event (name, var);

    uzbl_gui_update_title ();
    return TRUE;
}

static gchar *
get_variable_string (const UzblVariable *var);
static int
get_variable_int (const UzblVariable *var);
static unsigned long long
get_variable_ull (const UzblVariable *var);
static float
get_variable_float (const UzblVariable *var);

void
uzbl_variables_toggle (const gchar *name, GArray *values)
{
    UzblVariable *var = get_variable (name);

    if (!var) {
        if (values && values->len) {
            uzbl_variables_set (name, argv_idx (values, 1));
        } else {
            uzbl_variables_set (name, "1");
        }

        return;
    }

    switch (var->type) {
        case TYPE_STR:
        {
            const gchar *next;

            if (values && values->len) {
                gchar *current = get_variable_string (var);

                guint i = 0;
                const gchar *first   = argv_idx (values, 0);
                const gchar *this    = first;
                             next    = argv_idx (values, 1);

                while (next && strcmp (current, this)) {
                    this = next;
                    next = argv_idx (values, ++i);
                }

                if (!next) {
                    next = first;
                }

                g_free (current);
            } else {
                next = "";
            }

            set_variable_string (var, next);
            break;
        }
        case TYPE_INT:
        {
            int current = get_variable_int (var);
            int next;

            if (values && values->len) {
                guint i = 0;

                int first = strtol (argv_idx (values, 0), NULL, 0);
                int  this = first;

                const gchar *next_s = argv_idx (values, 1);

                while (next_s && (this != current)) {
                    this   = strtol (next_s, NULL, 0);
                    next_s = argv_idx (values, ++i);
                }

                if (next_s) {
                    next = strtol (next_s, NULL, 0);
                } else {
                    next = first;
                }
            } else {
                next = !current;
            }

            set_variable_int (var, next);
            break;
        }
        case TYPE_ULL:
        {
            unsigned long long current = get_variable_ull (var);
            unsigned long long next;

            if (values && values->len) {
                guint i = 0;

                unsigned long long first = strtoull (argv_idx (values, 0), NULL, 0);
                unsigned long long  this = first;

                const gchar *next_s = argv_idx (values, 1);

                while (next_s && this != current) {
                    this   = strtoull (next_s, NULL, 0);
                    next_s = argv_idx (values, ++i);
                }

                if (next_s) {
                    next = strtoull (next_s, NULL, 0);
                } else {
                    next = first;
                }
            } else {
                next = !current;
            }

            set_variable_ull (var, next);
            break;
        }
        case TYPE_FLOAT:
        {
            float current = get_variable_float (var);
            float next;

            if (values && values->len) {
                guint i = 0;

                float first = strtod (argv_idx (values, 0), NULL);
                float  this = first;

                const gchar *next_s = argv_idx (values, 1);

                while (next_s && (this != current)) {
                    this   = strtod (next_s, NULL);
                    next_s = argv_idx (values, ++i);
                }

                if (next_s) {
                    next = strtod (next_s, NULL);
                } else {
                    next = first;
                }
            } else {
                next = !current;
            }

            set_variable_float (var, next);
            break;
        }
        default:
            g_assert_not_reached ();
    }

    send_variable_event (name, var);
}

typedef enum {
    EXPAND_INITIAL,
    EXPAND_IGNORE_SHELL,
    EXPAND_IGNORE_JS,
    EXPAND_IGNORE_UZBL_JS,
    EXPAND_IGNORE_UZBL
} UzblExpandStage;

static gchar *
expand_impl (const gchar *str, UzblExpandStage stage);

gchar *
uzbl_variables_expand (const gchar *str)
{
    return expand_impl (str, EXPAND_INITIAL);
}

#define VAR_GETTER(type, name)                     \
    type                                           \
    uzbl_variables_get_##name (const gchar *name_) \
    {                                              \
        UzblVariable *var = get_variable (name_);  \
                                                   \
        return get_variable_##name (var);          \
    }

VAR_GETTER (gchar *, string)
VAR_GETTER (int, int)
VAR_GETTER (unsigned long long, ull)
VAR_GETTER (float, float)

static void
dump_variable (gpointer key, gpointer value, gpointer data);

void
uzbl_variables_dump ()
{
    g_hash_table_foreach (uzbl.behave.proto_var, dump_variable, NULL);
}

static void
dump_variable_event (gpointer key, gpointer value, gpointer data);

void
uzbl_variables_dump_events ()
{
    g_hash_table_foreach (uzbl.behave.proto_var, dump_variable_event, NULL);
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

static bool
js_has_variable (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName);
static JSValueRef
js_get_variable (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);
static bool
js_set_variable (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception);
static bool
js_delete_variable (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);

void
init_js_variables_api ()
{
    JSObjectRef uzbl_obj = uzbl_js_object (uzbl.state.jscontext, "uzbl");

    static JSClassDefinition
    variables_class_def = {
        0,                     // version
        kJSClassAttributeNone, // attributes
        "UzblVariables",       // class name
        NULL,                  // parent class
        NULL,                  // static values
        NULL,                  // static functions
        NULL,                  // initialize
        NULL,                  // finalize
        js_has_variable,       // has property
        js_get_variable,       // get property
        js_set_variable,       // set property
        js_delete_variable,    // delete property
        NULL,                  // get property names
        NULL,                  // call as function
        NULL,                  // call as contructor
        NULL,                  // has instance
        NULL                   // convert to type
    };

    JSClassRef variables_class = JSClassCreate (&variables_class_def);

    JSObjectRef variables_obj = JSObjectMake (uzbl.state.jscontext, variables_class, NULL);

    uzbl_js_set (uzbl.state.jscontext,
        uzbl_obj, "variables", variables_obj,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete);

    JSClassRelease (variables_class);
}

void
variable_free (gpointer key, gpointer value, gpointer data)
{
    UZBL_UNUSED (data);

    UzblVariable *var = (UzblVariable *)value;

    if (var->builtin) {
        return;
    }

    if (var->type == TYPE_STR) {
        g_free (*var->value.s);
        g_free (var->value.s);
    }

    gchar *name = (gchar *)key;

    g_free (name);
    g_free (var);
}

UzblVariable *
get_variable (const gchar *name)
{
    return g_hash_table_lookup (uzbl.behave.proto_var, name);
}

void
set_variable_string (UzblVariable *var, const gchar *val)
{
    typedef void (*setter_t) (const gchar *);

    if (var->set) {
        ((setter_t)var->set) (val);
    } else {
        g_free (*(var->value.s));
        *(var->value.s) = g_strdup (val);
    }
}

#define TYPE_SETTER(type, name, member)               \
    void                                              \
    set_variable_##name (UzblVariable *var, type val) \
    {                                                 \
        typedef void (*setter_t) (type);              \
                                                      \
        if (var->set) {                               \
            ((setter_t)var->set) (val);               \
        } else {                                      \
            *var->value.member = val;                 \
        }                                             \
    }

TYPE_SETTER (int, int, i)
TYPE_SETTER (unsigned long long, ull, ull)
TYPE_SETTER (float, float, f)

static void
variable_expand (const UzblVariable *var, GString *buf);

void
send_variable_event (const gchar *name, const UzblVariable *var)
{
    GString *str = g_string_new ("");

    variable_expand (var, str);

    const gchar *type = NULL;

    /* Check for the variable type. */
    switch (var->type) {
        case TYPE_STR:
            type = "str";
            break;
        case TYPE_INT:
            type = "int";
            break;
        case TYPE_ULL:
            type = "ull";
            break;
        case TYPE_FLOAT:
            type = "float";
            break;
        default:
            g_assert_not_reached ();
    }

    uzbl_events_send (VARIABLE_SET, NULL,
        TYPE_NAME, name,
        TYPE_NAME, type,
        TYPE_STR, str->str,
        NULL);

    g_string_free (str, TRUE);
}

gchar *
get_variable_string (const UzblVariable *var)
{
    if (!var) {
        return g_strdup ("");
    }

    gchar *result = NULL;

    typedef gchar *(*getter_t) ();

    if (var->get) {
        result = ((getter_t)var->get) ();
    } else if (var->value.s) {
        result = g_strdup (*(var->value.s));
    }

    return result ? result : g_strdup ("");
}

#define TYPE_GETTER(type, name, member)           \
    type                                          \
    get_variable_##name (const UzblVariable *var) \
    {                                             \
        if (!var) {                               \
            return (type)0;                       \
        }                                         \
                                                  \
        typedef type (*getter_t) ();              \
                                                  \
        if (var->get) {                           \
            return ((getter_t)var->get) ();       \
        } else {                                  \
            return *var->value.member;            \
        }                                         \
    }

TYPE_GETTER (int, int, i)
TYPE_GETTER (unsigned long long, ull, ull)
TYPE_GETTER (float, float, f)

typedef enum {
    EXPAND_SHELL,
    EXPAND_JS,
    EXPAND_ESCAPE,
    EXPAND_UZBL,
    EXPAND_UZBL_JS,
    EXPAND_VAR,
    EXPAND_VAR_BRACE
} UzblExpandType;

static UzblExpandType
expand_type (const gchar *str);

gchar *
expand_impl (const gchar *str, UzblExpandStage stage)
{
    GString *buf = g_string_new ("");

    if (!str) {
        return g_string_free (buf, FALSE);
    }

    const gchar *p = str;

    while (*p) {
        switch (*p) {
            case '@':
            {
                UzblExpandType etype = expand_type (p);
                const gchar *vend = NULL;
                gchar *ret = NULL;
                ++p;

                switch (etype) {
                    case EXPAND_VAR:
                    {
                        size_t sz = strspn (p, valid_chars);
                        vend = p + sz;
                        break;
                    }
                    case EXPAND_VAR_BRACE:
                        ++p;
                        vend = strchr (p, '}');
                        if (!vend) {
                            vend = strchr (p, '\0');
                        }
                        break;
                    case EXPAND_SHELL:
                        ++p;
                        vend = strstr (p, ")@");
                        if (!vend) {
                            vend = strchr (p, '\0');
                        }
                        break;
                    case EXPAND_UZBL:
                        ++p;
                        vend = strstr (p, "/@");
                        if (!vend) {
                            vend = strchr (p, '\0');
                        }
                        break;
                    case EXPAND_UZBL_JS:
                        ++p;
                        vend = strstr (p, "*@");
                        if (!vend) {
                            vend = strchr (p, '\0');
                        }
                        break;
                    case EXPAND_JS:
                        ++p;
                        vend = strstr (p, ">@");
                        if (!vend) {
                            vend = strchr (p, '\0');
                        }
                        break;
                    case EXPAND_ESCAPE:
                        ++p;
                        vend = strstr (p, "]@");
                        if (!vend) {
                            vend = strchr (p, '\0');
                        }
                        break;
                }
                assert (vend);

                ret = g_strndup (p, vend - p);

                switch (etype) {
                    case EXPAND_VAR_BRACE:
                        /* Skip the end brace. */
                        ++vend;
                        /* FALLTHROUGH */
                    case EXPAND_VAR:
                        variable_expand (get_variable (ret), buf);

                        p = vend;
                        break;
                    case EXPAND_SHELL:
                    {
                        GError *err = NULL;
                        gchar *cmd_stdout = NULL;

                        if (stage == EXPAND_IGNORE_SHELL) {
                            break;
                        }

                        if (*ret == '+') {
                            /* Execute program directly. */
                            gchar *mycmd = expand_impl (ret + 1, EXPAND_IGNORE_SHELL);
                            g_spawn_command_line_sync (mycmd, &cmd_stdout, NULL, NULL, &err);
                            g_free (mycmd);
                        } else {
                            /* Execute program through shell, quote it first. */
                            gchar *mycmd = expand_impl (ret, EXPAND_IGNORE_SHELL);
                            gchar *quoted = g_shell_quote (mycmd);
                            gchar *tmp = g_strdup_printf ("%s %s",
                                uzbl.behave.shell_cmd ? uzbl.behave.shell_cmd : "/bin/sh -c",
                                quoted);
                            g_spawn_command_line_sync (tmp, &cmd_stdout, NULL, NULL, &err);
                            g_free (mycmd);
                            g_free (quoted);
                            g_free (tmp);
                        }

                        if (err) {
                            g_printerr ("error on running command: %s\n", err->message);
                            g_error_free (err);
                        } else if (*cmd_stdout) {
                            remove_trailing_newline (cmd_stdout);

                            g_string_append (buf, cmd_stdout);
                            g_free (cmd_stdout);
                        }
                        p = vend + 2;

                        break;
                    }
                    case EXPAND_UZBL:
                    {
                        if (stage == EXPAND_IGNORE_UZBL) {
                            break;
                        }

                        GString *uzbl_ret = g_string_new ("");

                        GArray *tmp = uzbl_commands_args_new ();

                        if (*ret == '+') {
                            /* Read JS from file. */
                            gchar *mycmd = expand_impl (ret + 1, EXPAND_IGNORE_UZBL);
                            g_array_append_val (tmp, mycmd);

                            uzbl_commands_run_argv ("include", tmp, uzbl_ret);
                        } else {
                            /* JS from string. */
                            gchar *mycmd = expand_impl (ret, EXPAND_IGNORE_UZBL);

                            uzbl_commands_run (mycmd, uzbl_ret);
                        }

                        uzbl_commands_args_free (tmp);

                        if (uzbl_ret->str) {
                            g_string_append (buf, uzbl_ret->str);
                            g_string_free (uzbl_ret, TRUE);
                        }
                        p = vend + 2;

                        break;
                    }
                    case EXPAND_UZBL_JS:
                    {
                        if (stage == EXPAND_IGNORE_UZBL_JS) {
                            break;
                        }

                        GString *uzbl_js_ret = g_string_new ("");

                        GArray *tmp = uzbl_commands_args_new ();
                        uzbl_commands_args_append (tmp, g_strdup ("uzbl"));

                        if (*ret == '+') {
                            /* Read JS from file. */
                            uzbl_commands_args_append (tmp, g_strdup ("file"));
                            gchar *mycmd = expand_impl (ret + 1, EXPAND_IGNORE_UZBL_JS);
                            g_array_append_val (tmp, mycmd);

                            uzbl_commands_run_argv ("js", tmp, uzbl_js_ret);
                        } else {
                            /* JS from string. */
                            uzbl_commands_args_append (tmp, g_strdup ("string"));
                            gchar *mycmd = expand_impl (ret, EXPAND_IGNORE_UZBL_JS);
                            g_array_append_val (tmp, mycmd);

                            uzbl_commands_run_argv ("js", tmp, uzbl_js_ret);
                        }

                        uzbl_commands_args_free (tmp);

                        if (uzbl_js_ret->str) {
                            g_string_append (buf, uzbl_js_ret->str);
                            g_string_free (uzbl_js_ret, TRUE);
                        }
                        p = vend + 2;

                        break;
                    }
                    case EXPAND_JS:
                    {
                        if (stage == EXPAND_IGNORE_JS) {
                            break;
                        }

                        GString *js_ret = g_string_new ("");

                        GArray *tmp = uzbl_commands_args_new ();
                        uzbl_commands_args_append (tmp, g_strdup ("page"));

                        if (*ret == '+') {
                            /* Read JS from file. */
                            uzbl_commands_args_append (tmp, g_strdup ("file"));
                            gchar *mycmd = expand_impl (ret + 1, EXPAND_IGNORE_JS);
                            g_array_append_val (tmp, mycmd);

                            uzbl_commands_run_argv ("js", tmp, js_ret);
                        } else {
                            /* JS from string. */
                            uzbl_commands_args_append (tmp, g_strdup ("string"));
                            gchar *mycmd = expand_impl (ret, EXPAND_IGNORE_JS);
                            g_array_append_val (tmp, mycmd);

                            uzbl_commands_run_argv ("js", tmp, js_ret);
                        }

                        uzbl_commands_args_free (tmp);

                        if (js_ret->str) {
                            g_string_append (buf, js_ret->str);
                            g_string_free (js_ret, TRUE);
                        }
                        p = vend + 2;

                        break;
                    }
                    case EXPAND_ESCAPE:
                    {
                        gchar *mycmd = expand_impl (ret, EXPAND_INITIAL);
                        gchar *escaped = g_markup_escape_text (mycmd, strlen (mycmd));

                        g_string_append (buf, escaped);

                        g_free (escaped);
                        g_free (mycmd);
                        p = vend + 2;
                    }
                }

                g_free (ret);
                break;
            }
            case '\\':
                ++p;
                if (!*p) {
                    break;
                }
                /* FALLTHROUGH */
            default:
                g_string_append_c (buf, *p);
                ++p;
                break;
        }
    }

    return g_string_free (buf, FALSE);
}

void
dump_variable (gpointer key, gpointer value, gpointer data)
{
    UZBL_UNUSED (data);

    const gchar *name = (const gchar *)key;
    UzblVariable *var = (UzblVariable *)value;

    if (!var->writeable) {
        printf ("# ");
    }

    GString *buf = g_string_new ("");

    variable_expand (var, buf);

    printf ("set %s = %s\n", name, buf->str);

    g_string_free (buf, TRUE);
}

void
dump_variable_event (gpointer key, gpointer value, gpointer data)
{
    UZBL_UNUSED (data);

    const gchar *name = (const gchar *)key;
    UzblVariable *var = (UzblVariable *)value;

    send_variable_event (name, var);
}

bool
js_has_variable (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName)
{
    UZBL_UNUSED (ctx);
    UZBL_UNUSED (object);

    size_t max_size = JSStringGetMaximumUTF8CStringSize (propertyName);
    gchar *var = (gchar *)malloc (max_size * sizeof (gchar));
    JSStringGetUTF8CString (propertyName, var, max_size);

    UzblVariable *uzbl_var = get_variable (var);

    g_free (var);

    return uzbl_var;
}

JSValueRef
js_get_variable (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UZBL_UNUSED (object);
    UZBL_UNUSED (exception);

    size_t max_size = JSStringGetMaximumUTF8CStringSize (propertyName);
    gchar *var = (gchar *)malloc (max_size * sizeof (gchar));
    JSStringGetUTF8CString (propertyName, var, max_size);

    UzblVariable *uzbl_var = get_variable (var);

    g_free (var);

    if (!uzbl_var) {
        return JSValueMakeUndefined (ctx);
    }

    JSValueRef js_value = NULL;

    switch (uzbl_var->type) {
        case TYPE_STR:
        {
            gchar *val = get_variable_string (uzbl_var);
            JSStringRef js_str = JSStringCreateWithUTF8CString (val);
            g_free (val);

            js_value = JSValueMakeString (ctx, js_str);

            JSStringRelease (js_str);
            break;
        }
        case TYPE_INT:
        {
            int val = get_variable_int (uzbl_var);
            js_value = JSValueMakeNumber (ctx, val);
            break;
        }
        case TYPE_ULL:
        {
            unsigned long long val = get_variable_ull (uzbl_var);
            js_value = JSValueMakeNumber (ctx, val);
            break;
        }
        case TYPE_FLOAT:
        {
            float val = get_variable_float (uzbl_var);
            js_value = JSValueMakeNumber (ctx, val);
            break;
        }
        default:
            g_assert_not_reached ();
    }

    if (!js_value) {
        js_value = JSValueMakeUndefined (ctx);
    }

    return js_value;
}

bool
js_set_variable (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    UZBL_UNUSED (object);
    UZBL_UNUSED (exception);

    size_t max_size = JSStringGetMaximumUTF8CStringSize (propertyName);
    gchar *var = (gchar *)malloc (max_size * sizeof (gchar));
    JSStringGetUTF8CString (propertyName, var, max_size);

    gchar *val = uzbl_js_to_string (ctx, value);

    gboolean was_set = uzbl_variables_set (var, val);

    g_free (var);
    g_free (val);

    return (was_set ? true : false);
}

bool
js_delete_variable (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UZBL_UNUSED (ctx);
    UZBL_UNUSED (object);
    UZBL_UNUSED (propertyName);
    UZBL_UNUSED (exception);

    /* Variables cannot be deleted from uzbl. */

    return false;
}

void
variable_expand (const UzblVariable *var, GString *buf)
{
    if (!var) {
        return;
    }

    switch (var->type) {
        case TYPE_STR:
        {
            gchar *v = get_variable_string (var);
            g_string_append (buf, v);
            g_free (v);
            break;
        }
        case TYPE_INT:
            g_string_append_printf (buf, "%d", get_variable_int (var));
            break;
        case TYPE_ULL:
            g_string_append_printf (buf, "%llu", get_variable_ull (var));
            break;
        case TYPE_FLOAT:
            g_string_append_printf (buf, "%f", get_variable_float (var));
            break;
        default:
            break;
    }
}

UzblExpandType
expand_type (const gchar *str)
{
    switch (*(str + 1)) {
        case '(':
            return EXPAND_SHELL;
        case '{':
            return EXPAND_VAR_BRACE;
        case '/':
            return EXPAND_UZBL;
        case '*':
            return EXPAND_UZBL_JS;
        case '<':
            return EXPAND_JS;
        case '[':
            return EXPAND_ESCAPE;
        default:
            return EXPAND_VAR;
  }
}

/* =================== VARIABLES IMPLEMENTATIONS ==================== */

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
            uzbl_debug ("Unrecognized "     \
                        "value for " #name  \
                        ": %s\n", name);    \
            return;                         \
        }                                   \
                                            \
        set (out);                          \
    }

static GObject *
webkit_settings ();
#ifndef USE_WEBKIT2
static GObject *
soup_session ();
static GObject *
inspector ();
static GObject *
webkit_view ();
static int
object_get (GObject *obj, const gchar *prop);
#endif

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
#ifndef USE_WEBKIT2
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
#endif

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

        /* We used to use gtk_window_parse_geometry () but that didn't work how
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

IMPLEMENT_SETTER (gchar *, icon_name)
{
    if (!uzbl.gui.main_window) {
        return;
    }

    /* Clear icon path. */
    g_free (uzbl.gui.icon);
    uzbl.gui.icon = NULL;

    g_free (uzbl.gui.icon_name);
    uzbl.gui.icon_name = g_strdup (icon_name);

    gtk_window_set_icon_name (GTK_WINDOW (uzbl.gui.main_window), uzbl.gui.icon_name);
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

#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, auto_resize_window,
                webkit_settings (), "auto-resize-window")
#endif

/* UI variables */
IMPLEMENT_GETTER (int, show_status)
{
    if (!uzbl.gui.status_bar) {
        return FALSE;
    }

    return gtk_widget_get_visible (GTK_WIDGET (uzbl.gui.status_bar));
}

IMPLEMENT_SETTER (int, show_status)
{
    if (!uzbl.gui.status_bar) {
        return;
    }

    gtk_widget_set_visible (uzbl.gui.status_bar, show_status);
    uzbl_gui_update_title ();
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

#ifdef USE_WEBKIT2
GOBJECT_GETSET (int, enable_compositing_debugging,
                webkit_settings (), "draw-compositing-indicators")
#endif

/* Customization */
#if !WEBKIT_CHECK_VERSION (1, 9, 0)
GOBJECT_GETSET (int, default_context_menu,
                webkit_settings (), "enable-default-context-menu")
#endif

/* Printing variables */
GOBJECT_GETSET (int, print_backgrounds,
                webkit_settings (), "print-backgrounds")

/* Network variables */
#ifndef USE_WEBKIT2
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

IMPLEMENT_SETTER (gchar *, http_debug)
{
    SoupLoggerLogLevel out;
    const gchar *in = http_debug;

#define http_debug_choices(call)              \
    call (SOUP_LOGGER_LOG_NONE, "none")       \
    call (SOUP_LOGGER_LOG_MINIMAL, "minimal") \
    call (SOUP_LOGGER_LOG_HEADERS, "headers") \
    call (SOUP_LOGGER_LOG_BODY, "body")

    http_debug_choices (STRING_TO_ENUM)
    {
        uzbl_debug ("Unrecognized value for http_debug: %s\n", http_debug);
        return;
    }

#undef http_debug_choices

    g_free (uzbl.behave.http_debug);
    uzbl.behave.http_debug = g_strdup (http_debug);

    if (uzbl.net.soup_logger) {
        soup_session_remove_feature (
            uzbl.net.soup_session, SOUP_SESSION_FEATURE (uzbl.net.soup_logger));
        g_object_unref (uzbl.net.soup_logger);
    }

    uzbl.net.soup_logger = soup_logger_new (out, -1);
    soup_session_add_feature (
        uzbl.net.soup_session, SOUP_SESSION_FEATURE (uzbl.net.soup_logger));
}

GOBJECT_GETSET (gchar *, ssl_ca_file,
                soup_session (), "ssl-ca-file")
#endif

#define ssl_policy_choices(call)                     \
    call (WEBKIT_TLS_ERRORS_POLICY_IGNORE, "ignore") \
    call (WEBKIT_TLS_ERRORS_POLICY_FAIL, "fail")

#ifdef USE_WEBKIT2
#define _webkit_web_context_get_tls_errors_policy() \
    webkit_web_context_get_tls_errors_policy (webkit_web_view_get_context (uzbl.gui.web_view))
#define _webkit_web_context_set_tls_errors_policy(val) \
    webkit_web_context_set_tls_errors_policy (webkit_web_view_get_context (uzbl.gui.web_view), val)

CHOICE_GETSET (UzblSslPolicy, ssl_policy,
               _webkit_web_context_get_tls_errors_policy, _webkit_web_context_set_tls_errors_policy)

#undef _webkit_web_context_get_tls_errors_policy
#undef _webkit_web_context_set_tls_errors_policy
#else
#define _soup_session_get_ssl_strict() \
    object_get (soup_session (), "ssl-strict")
#define _soup_session_set_ssl_strict(val) \
    g_object_set (soup_session(),         \
        "ssl-strict", val,                \
        NULL);

CHOICE_GETSET (UzblSslPolicy, ssl_policy,
               _soup_session_get_ssl_strict, _soup_session_set_ssl_strict)

#undef _soup_session_get_ssl_strict
#undef _soup_session_set_ssl_strict
#endif

#undef ssl_policy_choices

#define cache_model_choices(call)                                 \
    call (WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER , "document_viewer") \
    call (WEBKIT_CACHE_MODEL_WEB_BROWSER, "web_browser")          \
    call (WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER, "document_browser")

#ifdef USE_WEBKIT2
#define _webkit_web_context_get_cache_model() \
    webkit_web_context_get_cache_model (webkit_web_view_get_context (uzbl.gui.web_view))
#define _webkit_web_context_set_cache_model(val) \
    webkit_web_context_set_cache_model (webkit_web_view_get_context (uzbl.gui.web_view), val)

CHOICE_GETSET (WebKitCacheModel, cache_model,
               _webkit_web_context_get_cache_model, _webkit_web_context_set_cache_model)

#undef _webkit_web_context_get_cache_model
#undef _webkit_web_context_set_cache_model
#else
CHOICE_GETSET (WebKitCacheModel, cache_model,
               webkit_get_cache_model, webkit_set_cache_model)
#endif

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

#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, enable_universal_file_access,
                webkit_settings (), "enable-universal-access-from-file-uris")

GOBJECT_GETSET (int, enable_cross_file_access,
                webkit_settings (), "enable-file-access-from-file-uris")
#endif

GOBJECT_GETSET (int, enable_hyperlink_auditing,
                webkit_settings (), "enable-hyperlink-auditing")

#ifdef USE_WEBKIT2
GOBJECT_GETSET (int, enable_xss_auditing,
                webkit_settings (), "enable-xss-auditor")
#endif

#ifdef USE_WEBKIT2
#define cookie_policy_choices(call)                     \
    call (WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS, "always") \
    call (WEBKIT_COOKIE_POLICY_ACCEPT_NEVER, "never")   \
    call (WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY, "first_party")

static WebKitCookieAcceptPolicy
cookie_policy ();

#define _webkit_cookie_manager_set_accept_policy(val) \
    webkit_cookie_manager_set_accept_policy (         \
        webkit_web_context_get_cookie_manager (       \
            webkit_web_view_get_context (uzbl.gui.web_view)), val)

CHOICE_GETSET (WebKitCookieAcceptPolicy, cookie_policy,
               cookie_policy, _webkit_cookie_manager_set_accept_policy)

#undef _webkit_cookie_manager_set_accept_policy
#else
#define cookie_policy_choices(call)                \
    call (SOUP_COOKIE_JAR_ACCEPT_ALWAYS, "always") \
    call (SOUP_COOKIE_JAR_ACCEPT_NEVER, "never")   \
    call (SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY, "first_party")

#define _soup_cookie_jar_get_accept_policy() \
    soup_cookie_jar_get_accept_policy (SOUP_COOKIE_JAR (uzbl.net.soup_cookie_jar))
#define _soup_cookie_jar_set_accept_policy(val) \
    soup_cookie_jar_set_accept_policy (SOUP_COOKIE_JAR (uzbl.net.soup_cookie_jar), val)

CHOICE_GETSET (SoupCookieJarAcceptPolicy, cookie_policy,
               _soup_cookie_jar_get_accept_policy, _soup_cookie_jar_set_accept_policy)

#undef _soup_cookie_jar_get_accept_policy
#undef _soup_cookie_jar_set_accept_policy
#endif

#undef cookie_policy_choices

#if WEBKIT_CHECK_VERSION (1, 3, 13)
GOBJECT_GETSET (int, enable_dns_prefetch,
                webkit_settings (), "enable-dns-prefetching")
#endif

#ifndef USE_WEBKIT2
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
#endif

/* Inspector variables */
#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, profile_js,
                inspector (), "javascript-profiling-enabled")

GOBJECT_GETSET (int, profile_timeline,
                inspector (), "timeline-profiling-enabled")
#endif

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
        GArray *argv = g_array_new (TRUE, FALSE, sizeof (gchar *));
        g_array_append_val (argv, uri);
        uzbl_commands_run_argv ("js", argv, NULL);
        g_array_free (argv, FALSE);
        return;
    }

    /* Attempt to parse the URI. */
    gchar *newuri = make_uri_from_user_input (uri);

    webkit_web_view_load_uri (uzbl.gui.web_view, newuri);

    g_free (newuri);
}

IMPLEMENT_SETTER (gchar *, useragent)
{
    g_free (uzbl.net.useragent);

    if (!useragent || !*useragent) {
        uzbl.net.useragent = NULL;
    } else {
        uzbl.net.useragent = g_strdup (useragent);

#ifndef USE_WEBKIT2
        g_object_set (G_OBJECT (uzbl.net.soup_session),
            SOUP_SESSION_USER_AGENT, uzbl.net.useragent,
            NULL);
#endif
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

#ifdef USE_WEBKIT2
        WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);

        gchar **languages = g_strsplit (uzbl.net.accept_languages, ",", 0);

        webkit_web_context_set_preferred_languages (context, (const gchar * const *)languages);

        g_strfreev (languages);
#else
        g_object_set (G_OBJECT (uzbl.net.soup_session),
            SOUP_SESSION_ACCEPT_LANGUAGE, uzbl.net.accept_languages,
            NULL);
#endif
    }
}

IMPLEMENT_SETTER (int, view_source)
{
    uzbl.behave.view_source = view_source;

#ifdef USE_WEBKIT2
    WebKitViewMode mode = uzbl.behave.view_source ? WEBKIT_VIEW_MODE_SOURCE : WEBKIT_VIEW_MODE_WEB;
    webkit_web_view_set_view_mode (uzbl.gui.web_view, mode);
#else
    webkit_web_view_set_view_source_mode (uzbl.gui.web_view, uzbl.behave.view_source);
#endif
}

IMPLEMENT_GETTER (float, zoom_level)
{
    return webkit_web_view_get_zoom_level (uzbl.gui.web_view);
}

IMPLEMENT_SETTER (float, zoom_level)
{
    webkit_web_view_set_zoom_level (uzbl.gui.web_view, zoom_level);
}

#ifdef HAVE_ZOOM_TEXT_API
#ifdef USE_WEBKIT2
GOBJECT_GETSET (int, zoom_text_only,
                webkit_settings (), "zoom-text-only")

#else
IMPLEMENT_GETTER (int, zoom_text_only)
{
    return !webkit_web_view_get_full_content_zoom (uzbl.gui.web_view);
}

IMPLEMENT_SETTER (int, zoom_text_only)
{
    webkit_web_view_set_full_content_zoom (uzbl.gui.web_view, !zoom_text_only);
}
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

#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, transparent,
                webkit_view (), "transparent")

#if WEBKIT_CHECK_VERSION (1, 3, 4)
#define window_view_mode_choices(call)                        \
    call (WEBKIT_WEB_VIEW_VIEW_MODE_WINDOWED, "windowed")     \
    call (WEBKIT_WEB_VIEW_VIEW_MODE_FLOATING, "floating")     \
    call (WEBKIT_WEB_VIEW_VIEW_MODE_FULLSCREEN, "fullscreen") \
    call (WEBKIT_WEB_VIEW_VIEW_MODE_MAXIMIZED, "maximized")   \
    call (WEBKIT_WEB_VIEW_VIEW_MODE_MINIMIZED, "minimized")

#define _webkit_web_view_get_window_view_mode() \
    webkit_web_view_get_view_mode (uzbl.gui.web_view)
#define _webkit_web_view_set_window_view_mode(val) \
    webkit_web_view_set_view_mode (uzbl.gui.web_view, val)

CHOICE_GETSET (WebKitWebViewViewMode, window_view_mode,
               _webkit_web_view_get_window_view_mode, _webkit_web_view_set_window_view_mode)

#undef _webkit_web_view_get_window_view_mode
#undef _webkit_web_view_set_window_view_mode

#undef window_view_mode_choices
#endif
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 8)
GOBJECT_GETSET (int, enable_fullscreen,
                webkit_settings (), "enable-fullscreen")
#endif

#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, editable,
                webkit_view (), "editable")
#endif

/* Javascript variables */
#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, enable_scripts,
                webkit_settings (), "enable-scripts")
#endif

GOBJECT_GETSET (int, javascript_windows,
                webkit_settings (), "javascript-can-open-windows-automatically")

#ifdef USE_WEBKIT2
GOBJECT_GETSET (int, javascript_modal_dialogs,
                webkit_settings (), "allow-modal-dialogs")
#endif

#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, javascript_dom_paste,
                webkit_settings (), "enable-dom-paste")
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 0)
GOBJECT_GETSET (int, javascript_clipboard,
                webkit_settings (), "javascript-can-access-clipboard")
#endif

/* Image variables */
GOBJECT_GETSET (int, autoload_images,
                webkit_settings (), "auto-load-images")

#ifdef USE_WEBKIT2
GOBJECT_GETSET (int, always_load_icons,
                webkit_settings (), "load-icons-ignoring-image-load-setting")
#endif

#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, autoshrink_images,
                webkit_settings (), "auto-shrink-images")

GOBJECT_GETSET (int, use_image_orientation,
                webkit_settings (), "respect-image-orientation")
#endif

/* Spell checking variables */
#ifdef USE_WEBKIT2
IMPLEMENT_GETTER (int, enable_spellcheck)
{
    return webkit_web_context_get_spell_checking_enabled (webkit_web_view_get_context (uzbl.gui.web_view));
}

IMPLEMENT_SETTER (int, enable_spellcheck)
{
    webkit_web_context_set_spell_checking_enabled (webkit_web_view_get_context (uzbl.gui.web_view), enable_spellcheck);
}
#else
GOBJECT_GETSET (int, enable_spellcheck,
                webkit_settings (), "enable-spell-checking")
#endif

#ifdef USE_WEBKIT2
IMPLEMENT_GETTER (gchar *, spellcheck_languages)
{
    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    const gchar * const * langs = webkit_web_context_get_spell_checking_languages (context);

    if (!langs) {
        return g_strdup ("");
    }

    return g_strjoinv (",", (gchar **)langs);
}

IMPLEMENT_SETTER (gchar *, spellcheck_languages)
{
    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    gchar **langs = g_strsplit (spellcheck_languages, ",", 0);

    webkit_web_context_set_spell_checking_languages (context, (const gchar * const *)langs);

    g_strfreev (langs);
}
#else
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
#endif

/* Form variables */
#ifdef USE_WEBKIT2
GOBJECT_GETSET (int, resizable_text_areas,
                webkit_settings (), "enable-resizable-text-areas")
#else
GOBJECT_GETSET (int, resizable_text_areas,
                webkit_settings (), "resizable-text-areas")
#endif

#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, enable_spatial_navigation,
                webkit_settings (), "enable-spatial-navigation")

#define editing_behavior_choices(call)                \
    call (WEBKIT_EDITING_BEHAVIOR_MAC, "mac")         \
    call (WEBKIT_EDITING_BEHAVIOR_WINDOWS, "windows") \
    call (WEBKIT_EDITING_BEHAVIOR_UNIX, "unix")

#define _get_webkit_settings_editing_behavior() \
    object_get (webkit_settings (), "editing-behavior")
#define _set_webkit_settings_editing_behavior(val) \
    g_object_set (webkit_settings (),              \
        "editing-behavior", val,                   \
        NULL);

CHOICE_GETSET (WebKitEditingBehavior, editing_behavior,
               _get_webkit_settings_editing_behavior, _set_webkit_settings_editing_behavior)

#undef _get_webkit_settings_editing_behavior
#undef _set_webkit_settings_editing_behavior

#undef editing_behavior_choices
#endif

#ifdef USE_WEBKIT2
GOBJECT_GETSET (int, enable_tab_cycle,
                webkit_settings (), "enable-tabs-to-links")
#else
GOBJECT_GETSET (int, enable_tab_cycle,
                webkit_settings (), "tab-key-cycles-through-elements")
#endif

/* Text variables */
#ifdef USE_WEBKIT2
GOBJECT_GETSET (gchar *, default_encoding,
                webkit_settings (), "default-charset")
#else
GOBJECT_GETSET (gchar *, default_encoding,
                webkit_settings (), "default-encoding")
#endif

IMPLEMENT_GETTER (gchar *, custom_encoding)
{
    const gchar *encoding =
#ifdef USE_WEBKIT2
        webkit_web_view_get_custom_charset (uzbl.gui.web_view);
#else
        webkit_web_view_get_custom_encoding (uzbl.gui.web_view);
#endif
    return g_strdup (encoding);
}

IMPLEMENT_SETTER (gchar *, custom_encoding)
{
    if (!*custom_encoding) {
        custom_encoding = NULL;
    }

#ifdef USE_WEBKIT2
    webkit_web_view_set_custom_charset (uzbl.gui.web_view, custom_encoding);
#else
    webkit_web_view_set_custom_encoding (uzbl.gui.web_view, custom_encoding);
#endif
}

#ifndef USE_WEBKIT2
IMPLEMENT_GETTER (gchar *, current_encoding)
{
    const gchar *encoding = webkit_web_view_get_encoding (uzbl.gui.web_view);
    return g_strdup (encoding);
}

GOBJECT_GETSET (int, enforce_96_dpi,
                webkit_settings (), "enforce-96-dpi")
#endif

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

#ifdef USE_WEBKIT2
GOBJECT_GETSET (gchar *, pictograph_font_family,
                webkit_settings (), "pictograph-font-family")
#endif

/* Font size variables */
#ifndef USE_WEBKIT2
GOBJECT_GETSET (int, minimum_font_size,
                webkit_settings (), "minimum-font-size")

GOBJECT_GETSET (int, minimum_logical_font_size,
                webkit_settings (), "minimum-logical-font-size")
#endif

GOBJECT_GETSET (int, font_size,
                webkit_settings (), "default-font-size")

GOBJECT_GETSET (int, monospace_size,
                webkit_settings (), "default-monospace-font-size")

/* Feature variables */
GOBJECT_GETSET (int, enable_plugins,
                webkit_settings (), "enable-plugins")

#ifdef USE_WEBKIT2
GOBJECT_GETSET (int, enable_java_applet,
                webkit_settings (), "enable-java")
#else
GOBJECT_GETSET (int, enable_java_applet,
                webkit_settings (), "enable-java-applet")
#endif

#ifdef HAVE_PLUGIN_API
#ifdef USE_WEBKIT2
typedef WebKitPlugin WebKitWebPlugin;
typedef WebKitMimeInfo WebKitWebPluginMIMEType;
#endif

static void
plugin_list_append (WebKitWebPlugin *plugin, gpointer data);

IMPLEMENT_GETTER (gchar *, plugin_list)
{
#ifdef USE_WEBKIT2
    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    GList *plugins = NULL;

    {
        GError *err = NULL;

        uzbl_sync_call (plugins, context, err,
                        webkit_web_context_get_plugins);

        if (err) {
            /* TODO: Output message. */
            g_error_free (err);
        }
    }

    if (!plugins) {
        /* TODO: Don't ignore the error. */
        return g_strdup ("[]");
    }

#define plugin_foreach g_list_foreach
#else
    WebKitWebPluginDatabase *db = webkit_get_web_plugin_database ();
    GSList *plugins = webkit_web_plugin_database_get_plugins (db);

#define plugin_foreach g_slist_foreach
#endif

    GString *list = g_string_new ("[");

    plugin_foreach (plugins, (GFunc)plugin_list_append, list);

    g_string_append_c (list, ']');

#ifdef USE_WEBKIT2
    g_list_free (plugins);
#else
    webkit_web_plugin_database_plugins_list_free (plugins);
#endif

#undef plugin_foreach

    return g_string_free (list, FALSE);
}

static void
mimetype_list_append (WebKitWebPluginMIMEType *mimetype, GString *list);

void
plugin_list_append (WebKitWebPlugin *plugin, gpointer data)
{
    GString *list = (GString *)data;

    if (list->str[list->len - 1] != '[') {
        g_string_append (list, ", ");
    }

#ifdef USE_WEBKIT2
    typedef GList MIMETypeList;

#define mimetype_foreach g_list_foreach
#else
    typedef GSList MIMETypeList;

#define mimetype_foreach g_slist_foreach
#endif

    const gchar *desc = NULL;
#ifndef USE_WEBKIT2
    gboolean enabled = FALSE;
#endif
    MIMETypeList *mimetypes = NULL;
    const gchar *name = NULL;
    const gchar *path = NULL;

#ifdef USE_WEBKIT2
    desc = webkit_plugin_get_description (plugin);
    mimetypes = webkit_plugin_get_mime_info_list (plugin);
    name = webkit_plugin_get_name (plugin);
    path = webkit_plugin_get_path (plugin);
#else
    desc = webkit_web_plugin_get_description (plugin);
    enabled = webkit_web_plugin_get_enabled (plugin);
    mimetypes = webkit_web_plugin_get_mimetypes (plugin);
    name = webkit_web_plugin_get_name (plugin);
    path = webkit_web_plugin_get_path (plugin);
#endif

    /* Write out a JSON representation of the information */
    g_string_append_printf (list,
            "{\"name\": \"%s\", "
            "\"description\": \"%s\", "
#ifndef USE_WEBKIT2
            "\"enabled\": %s, "
#endif
            "\"path\": \"%s\", "
            "\"mimetypes\": [", /* Open array for the mimetypes */
            name,
            desc,
#ifndef USE_WEBKIT2
            enabled ? "true" : "false",
#endif
            path);

    mimetype_foreach (mimetypes, (GFunc)mimetype_list_append, list);

#undef plugin_foreach

#ifdef USE_WEBKIT2
    g_object_unref (plugin);
#endif

    /* Close the array and the object */
    g_string_append (list, "]}");
}

void
mimetype_list_append (WebKitWebPluginMIMEType *mimetype, GString *list)
{
    if (list->str[list->len - 1] != '[') {
        g_string_append (list, ", ");
    }

    const gchar *name = NULL;
    const gchar *desc = NULL;
    const gchar * const *extensions = NULL;

#ifdef USE_WEBKIT2
    name = webkit_mime_info_get_mime_type (mimetype);
    desc = webkit_mime_info_get_description (mimetype);
    extensions = webkit_mime_info_get_extensions (mimetype);
#else
    name = mimetype->name;
    desc = mimetype->description;
    extensions = (const gchar * const*)mimetype->extensions;
#endif

    /* Write out a JSON representation of the information. */
    g_string_append_printf (list,
            "{\"name\": \"%s\", "
            "\"description\": \"%s\", "
            "\"extensions\": [", /* Open array for the extensions. */
            name,
            desc);

    gboolean first = TRUE;

    while (*extensions) {
        if (first) {
            first = FALSE;
        } else {
            g_string_append_c (list, ',');
        }
        g_string_append_c (list, '\"');
        g_string_append (list, *extensions);
        g_string_append_c (list, '\"');

        ++extensions;
    }

    g_string_append (list, "]}");
}
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 14)
GOBJECT_GETSET (int, enable_webgl,
                webkit_settings (), "enable-webgl")
#endif

#if WEBKIT_CHECK_VERSION (1, 7, 5)
GOBJECT_GETSET (int, enable_webaudio,
                webkit_settings (), "enable-webaudio")
#endif

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 90) /* Documentation says 1.7.5, but it's not there. */
GOBJECT_GETSET (int, enable_3d_acceleration,
                webkit_settings (), "enable-accelerated-compositing")
#endif
#endif

#if WEBKIT_CHECK_VERSION (1, 9, 3)
GOBJECT_GETSET (int, enable_inline_media,
                webkit_settings (), "media-playback-allows-inline")

GOBJECT_GETSET (int, require_click_to_play,
                webkit_settings (), "media-playback-requires-user-gesture")
#endif

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 1)
GOBJECT_GETSET (int, enable_css_shaders,
                webkit_settings (), "enable-css-shaders")

GOBJECT_GETSET (int, enable_media_stream,
                webkit_settings (), "enable-media-stream")
#endif
#endif

/* HTML5 Database variables */
GOBJECT_GETSET (int, enable_database,
                webkit_settings (), "enable-html5-database")

GOBJECT_GETSET (int, enable_local_storage,
                webkit_settings (), "enable-html5-local-storage")

GOBJECT_GETSET (int, enable_pagecache,
                webkit_settings (), "enable-page-cache")

GOBJECT_GETSET (int, enable_offline_app_cache,
                webkit_settings (), "enable-offline-web-application-cache")

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 3, 13)
IMPLEMENT_GETTER (unsigned long long, app_cache_size)
{
    return webkit_application_cache_get_maximum_size ();
}

IMPLEMENT_SETTER (unsigned long long, app_cache_size)
{
    webkit_application_cache_set_maximum_size (app_cache_size);
}

/* FIXME: Seems to give garbage data?
IMPLEMENT_GETTER (gchar *, app_cache_directory)
{
    return g_strdup (webkit_application_cache_get_database_directory_path ());
}
*/
#endif

IMPLEMENT_GETTER (gchar *, web_database_directory)
{
    return g_strdup (webkit_get_web_database_directory_path ());
}

IMPLEMENT_SETTER (gchar *, web_database_directory)
{
    webkit_set_web_database_directory_path (web_database_directory);
}

IMPLEMENT_GETTER (unsigned long long, web_database_quota)
{
    return webkit_get_default_web_database_quota ();
}

IMPLEMENT_SETTER (unsigned long long, web_database_quota)
{
    webkit_set_default_web_database_quota (web_database_quota);
}

#if WEBKIT_CHECK_VERSION (1, 5, 2)
GOBJECT_GETSET (gchar *, local_storage_path,
                webkit_settings (), "html5-local-storage-database-path")
#endif
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 92)
IMPLEMENT_SETTER (gchar *, disk_cache_directory)
{
    g_free (uzbl.state.disk_cache_directory);
    uzbl.state.disk_cache_directory = g_strdup (disk_cache_directory);

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);

    webkit_web_context_set_disk_cache_directory (context, uzbl.state.disk_cache_directory);
}
#endif

IMPLEMENT_SETTER (gchar *, web_extensions_directory)
{
    g_free (uzbl.state.web_extensions_directory);
    uzbl.state.web_extensions_directory = g_strdup (web_extensions_directory);

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);

    webkit_web_context_set_web_extensions_directory (context, uzbl.state.web_extensions_directory);
}
#endif

/* Hacks */
GOBJECT_GETSET (int, enable_site_workarounds,
                webkit_settings (), "enable-site-specific-quirks")

GObject *
webkit_settings ()
{
    return G_OBJECT (webkit_web_view_get_settings (uzbl.gui.web_view));
}

#ifndef USE_WEBKIT2
GObject *
soup_session ()
{
    return G_OBJECT (uzbl.net.soup_session);
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

int
object_get (GObject *obj, const gchar *prop)
{
    int val;

    g_object_get (obj,
        prop, &val,
        NULL);

    return val;
}
#endif

#ifdef USE_WEBKIT2
WebKitCookieAcceptPolicy
cookie_policy ()
{
    WebKitCookieAcceptPolicy policy = WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS;

#if 0 /* TODO: Seems to hang... */
    GError *err = NULL;

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    WebKitCookieManager *manager = webkit_web_context_get_cookie_manager (context);

    uzbl_sync_call (policy, manager, err,
                    webkit_cookie_manager_get_accept_policy);

    if (err) {
        /* TODO: Output message. */
        g_error_free (err);
    }
#endif

    return policy;
}
#endif

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
            result = g_strconcat ("http://", uri, NULL);
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
