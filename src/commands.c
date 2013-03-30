#include "commands.h"

#include "events.h"
#include "gui.h"
#include "menu.h"
#include "soup.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"
#include "variables.h"

#include <JavaScriptCore/JavaScript.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* ========================= COMMAND TABLE ========================== */

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 4)
#define HAVE_PLUGIN_API
#endif
#else
#if WEBKIT_CHECK_VERSION (1, 3, 8)
#define HAVE_PLUGIN_API
#endif
#endif

typedef void (*UzblCommandCallback) (GArray *argv, GString *result);

struct _UzblCommand {
    const gchar         *name;
    UzblCommandCallback  function;
    gboolean             split;
    gboolean             send_event;
};

#define DECLARE_COMMAND(cmd) \
    static void              \
    cmd_##cmd (GArray *argv, GString *result)

/* Navigation commands */
DECLARE_COMMAND (back);
DECLARE_COMMAND (forward);
DECLARE_COMMAND (reload);
DECLARE_COMMAND (reload_force);
DECLARE_COMMAND (stop);
DECLARE_COMMAND (uri);
#ifndef USE_WEBKIT2
DECLARE_COMMAND (auth);
#endif
DECLARE_COMMAND (download);
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 9, 90)
DECLARE_COMMAND (load);
DECLARE_COMMAND (save);
#endif
#endif

/* Cookie commands */
DECLARE_COMMAND (cookie);

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 92)
#define HAVE_SNAPSHOT
#endif
#else
#if WEBKIT_CHECK_VERSION (1, 9, 6)
#define HAVE_SNAPSHOT
#endif
#endif

/* Display commands */
DECLARE_COMMAND (scroll);
DECLARE_COMMAND (zoom_in);
DECLARE_COMMAND (zoom_out);
DECLARE_COMMAND (hardcopy);
#ifdef HAVE_SNAPSHOT
DECLARE_COMMAND (snapshot);
#endif

/* Content commands */
#ifdef HAVE_PLUGIN_API
DECLARE_COMMAND (plugin);
#endif
#ifndef USE_WEBKIT2
DECLARE_COMMAND (remove_all_db);
#if WEBKIT_CHECK_VERSION (1, 5, 1)
DECLARE_COMMAND (spell_checker);
#endif
#endif

/* Search commands */
DECLARE_COMMAND (search_forward);
DECLARE_COMMAND (search_reverse);
DECLARE_COMMAND (search_clear);
DECLARE_COMMAND (search_reset);

/* Inspector commands */
DECLARE_COMMAND (inspector_show);
DECLARE_COMMAND (inspector);

/* Execution commands */
DECLARE_COMMAND (js);
DECLARE_COMMAND (js_file);
DECLARE_COMMAND (spawn);
DECLARE_COMMAND (spawn_sync);
DECLARE_COMMAND (spawn_sync_exec);
DECLARE_COMMAND (spawn_sh);
DECLARE_COMMAND (spawn_sh_sync);

/* Uzbl commands */
DECLARE_COMMAND (chain);
DECLARE_COMMAND (include);
DECLARE_COMMAND (exit);

/* Variable commands */
DECLARE_COMMAND (set);
DECLARE_COMMAND (toggle);
DECLARE_COMMAND (dump_config);
DECLARE_COMMAND (dump_config_as_events);
DECLARE_COMMAND (print);

/* Event commands */
DECLARE_COMMAND (event);

static UzblCommand
builtin_command_table[] =
{   /* name                             function                      split  send_event */
    /* Navigation commands */
    { "back",                           cmd_back,                     TRUE,  TRUE  },
    { "forward",                        cmd_forward,                  TRUE,  TRUE  },
    { "reload",                         cmd_reload,                   TRUE,  TRUE  },
    { "reload_ign_cache",               cmd_reload_force,             TRUE,  TRUE  }, /* TODO: Rename to "reload_force". */
    { "stop",                           cmd_stop,                     TRUE,  TRUE  },
    { "uri",                            cmd_uri,                      FALSE, TRUE  },
#ifndef USE_WEBKIT2
    { "auth",                           cmd_auth,                     TRUE,  TRUE  },
#endif
    { "download",                       cmd_download,                 TRUE,  TRUE  },
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 9, 90)
    { "load",                           cmd_load,                     FALSE, TRUE  },
    { "save",                           cmd_save,                     FALSE, TRUE  },
#endif
#endif

    /* Cookie commands */
    { "cookie",                         cmd_cookie,                   TRUE,  TRUE  },

    { "scroll",                         cmd_scroll,                   TRUE,  TRUE  },
    { "zoom_in",                        cmd_zoom_in,                  TRUE,  TRUE  }, /* TODO: Rework to be "zoom in". */
    { "zoom_out",                       cmd_zoom_out,                 TRUE,  TRUE  }, /* TODO: Rework to be "zoom out". */
    { "hardcopy",                       cmd_hardcopy,                 FALSE, TRUE  },
#ifdef HAVE_SNAPSHOT
    { "snapshot",                       cmd_snapshot,                 FALSE, TRUE  },
#endif

    /* Content commands */
#ifdef HAVE_PLUGIN_API
    { "plugin",                         cmd_plugin,                   TRUE,  TRUE  },
#endif
#ifndef USE_WEBKIT2
    { "remove_all_db",                  cmd_remove_all_db,            TRUE,  TRUE  },
#if WEBKIT_CHECK_VERSION (1, 5, 1)
    { "spell_checker",                  cmd_spell_checker,            TRUE,  TRUE  },
#endif
#endif

    /* Menu commands */
    { "menu_add",                       cmd_menu_add,                 FALSE, TRUE  }, /* TODO: Rework to be "menu add". */
    { "menu_link_add",                  cmd_menu_add_link,            FALSE, TRUE  }, /* TODO: Rework to be "menu add link". */
    { "menu_image_add",                 cmd_menu_add_image,           FALSE, TRUE  }, /* TODO: Rework to be "menu add image". */
    { "menu_editable_add",              cmd_menu_add_edit,            FALSE, TRUE  }, /* TODO: Rework to be "menu add edit". */
    { "menu_separator",                 cmd_menu_add_separator,       FALSE, TRUE  }, /* TODO: Rework to be "menu add separator". */
    { "menu_link_separator",            cmd_menu_add_separator_link,  FALSE, TRUE  }, /* TODO: Rework to be "menu add separator link". */
    { "menu_image_separator",           cmd_menu_add_separator_image, FALSE, TRUE  }, /* TODO: Rework to be "menu add separator image". */
    { "menu_editable_separator",        cmd_menu_add_separator_edit,  FALSE, TRUE  }, /* TODO: Rework to be "menu add separator edit". */
    { "menu_remove",                    cmd_menu_remove,              FALSE, TRUE  }, /* TODO: Rework to be "menu remove". */
    { "menu_link_remove",               cmd_menu_remove_link,         FALSE, TRUE  }, /* TODO: Rework to be "menu remove link". */
    { "menu_image_remove",              cmd_menu_remove_image,        FALSE, TRUE  }, /* TODO: Rework to be "menu remove image". */
    { "menu_editable_remove",           cmd_menu_remove_edit,         FALSE, TRUE  }, /* TODO: Rework to be "menu remove edit". */

    /* Search commands */
    { "search",                         cmd_search_forward,           FALSE, TRUE  }, /* TODO: Rework to be "search forward". */
    { "search_reverse",                 cmd_search_reverse,           FALSE, TRUE  }, /* TODO: Rework to be "search reverse". */
    { "search_clear",                   cmd_search_clear,             FALSE, TRUE  }, /* TODO: Rework to be "search clear". */
    { "dehilight",                      cmd_search_reset,             TRUE,  TRUE  }, /* TODO: Rework to be "search reset". */

    /* Inspector commands */
    { "show_inspector",                 cmd_inspector_show,           TRUE,  TRUE  }, /* Deprecated. */
    { "inspector",                      cmd_inspector,                FALSE, TRUE  },

    /* Execution commands */
    { "js",                             cmd_js,                       FALSE, TRUE  },
    { "script",                         cmd_js_file,                  TRUE,  TRUE  }, /* TODO: Rename to "js_file". */
    { "spawn",                          cmd_spawn,                    TRUE,  TRUE  },
    { "sync_spawn",                     cmd_spawn_sync,               TRUE,  TRUE  }, /* TODO: Rename to "spawn_sync". */
    /* XXX: Should this command be removed? */
    { "sync_spawn_exec",                cmd_spawn_sync_exec,          TRUE,  TRUE  }, /* TODO: Rename to "spawn_sync_exec". */
    { "sh",                             cmd_spawn_sh,                 TRUE,  TRUE  }, /* TODO: Rename to "spawn_sh". */
    { "sync_sh",                        cmd_spawn_sh_sync,            TRUE,  TRUE  }, /* TODO: Rename to "spawn_sh_sync". */

    /* Uzbl commands */
    { "chain",                          cmd_chain,                    TRUE,  TRUE  },
    { "include",                        cmd_include,                  FALSE, TRUE  },
    { "exit",                           cmd_exit,                     TRUE,  TRUE  },

    /* Variable commands */
    { "set",                            cmd_set,                      FALSE, FALSE },
    { "toggle",                         cmd_toggle,                   TRUE,  TRUE  },
    { "dump_config",                    cmd_dump_config,              TRUE,  TRUE  },
    { "dump_config_as_events",          cmd_dump_config_as_events,    TRUE,  TRUE  },
    { "print",                          cmd_print,                    FALSE, TRUE  },

    /* Event commands */
    { "event",                          cmd_event,                    FALSE, FALSE },
    { "request",                        cmd_event,                    FALSE, FALSE }, /* XXX: Deprecated (event). */

    /* Terminator */
    { NULL,                             NULL,                         FALSE, FALSE }
};

/* =========================== PUBLIC API =========================== */

static void
parse_command_from_file (const char *cmd);

void
uzbl_commands_init ()
{
    UzblCommand *cmd = &builtin_command_table[0];

    uzbl.behave.commands = g_hash_table_new (g_str_hash, g_str_equal);

    while (cmd->name) {
        g_hash_table_insert (uzbl.behave.commands, (gpointer)cmd->name, cmd);

        ++cmd;
    }
}

void
uzbl_commands_send_builtin_event ()
{
    UzblCommand *cmd = &builtin_command_table[0];

    GString *command_list = g_string_new ("");

    while (cmd->name) {
        if (command_list->len) {
            g_string_append_c (command_list, ' ');
        }
        g_string_append (command_list, cmd->name);

        ++cmd;
    }

    uzbl_events_send (BUILTINS, NULL,
        TYPE_STR, command_list->str,
        NULL);

    g_string_free (command_list, TRUE);
}

static void
parse_command_arguments (const gchar *args, GArray *argv, gboolean split);

const UzblCommand *
uzbl_commands_parse (const gchar *cmd, GArray *argv)
{
    UzblCommand *info = NULL;

    gchar *exp_line = uzbl_variables_expand (cmd);
    if (!exp_line || !*exp_line) {
        g_free (exp_line);
        return NULL;
    }

    /* Separate the line into the command and its parameters. */
    gchar **tokens = g_strsplit (exp_line, " ", 2);

    /* Look up the command. */
    info = g_hash_table_lookup (uzbl.behave.commands, tokens[0]);

    if (!info) {
        uzbl_events_send (COMMAND_ERROR, NULL,
            TYPE_STR, exp_line,
            NULL);

        g_free (exp_line);
        g_strfreev (tokens);

        return NULL;
    }

    /* Parse the arguments. */
    if (argv) {
        parse_command_arguments (tokens[1], argv, info->split);
    }

    g_free (exp_line);
    g_strfreev (tokens);

    return info;
}

void
uzbl_commands_run_parsed (const UzblCommand *info, GArray *argv, GString *result)
{
    if (!info) {
        return;
    }

    GArray *argv_copy = NULL;

    if (info->send_event) {
        argv_copy = g_array_new (TRUE, FALSE, sizeof (gchar *));

        /* Store the arguments for the event. */
        g_array_append_vals (argv_copy, argv->data, argv->len);
    }

    info->function (argv, result);

    if (result) {
        g_free (uzbl.state.last_result);
        uzbl.state.last_result = g_strdup (result->str);
    }

    if (info->send_event) {
        uzbl_events_send (COMMAND_EXECUTED, NULL,
            TYPE_NAME, info->name,
            TYPE_STR_ARRAY, argv_copy,
            NULL);

        g_array_free (argv_copy, TRUE);
    }
}

void
uzbl_commands_run_argv (const gchar *cmd, GArray *argv, GString *result)
{
    /* Look up the command. */
    const UzblCommand *info = g_hash_table_lookup (uzbl.behave.commands, cmd);

    if (!info) {
        uzbl_events_send (COMMAND_ERROR, NULL,
            TYPE_STR, cmd,
            NULL);

        return;
    }

    uzbl_commands_run_parsed (info, argv, result);
}

void
uzbl_commands_run (const gchar *cmd, GString *result)
{
    GArray *argv = g_array_new (TRUE, TRUE, sizeof (gchar *));
    const UzblCommand *info = uzbl_commands_parse (cmd, argv);

    uzbl_commands_run_parsed (info, argv, result);

    while (argv->len) {
        g_free (argv_idx (argv, argv->len - 1));
        g_array_remove_index (argv, argv->len - 1);
    }
    g_array_free (argv, TRUE);
}

typedef void (*UzblLineCallback) (const gchar *line, gpointer data);

static gboolean
for_each_line_in_file (const gchar *path, UzblLineCallback callback, gpointer data);
static void
parse_command_from_file_cb (const gchar *line, gpointer data);

void
uzbl_commands_load_file (const gchar *path)
{
    if (!for_each_line_in_file (path, parse_command_from_file_cb, NULL)) {
        gchar *tmp = g_strdup_printf ("File %s can not be read.", path);
        uzbl_events_send (COMMAND_ERROR, NULL,
            TYPE_STR, tmp,
            NULL);

        g_free (tmp);
    }
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

static void
sharg_append (GArray *argv, const gchar *str);
static gchar **
split_quoted (const gchar *src, const gboolean unquote);

void
parse_command_arguments (const gchar *args, GArray *argv, gboolean split)
{
    if (!args) {
        return;
    }

    if (!split) {
        /* Pass the parameters through in one chunk. */
        /* FIXME: Valgrind says there's a memory leak here... */
        sharg_append (argv, g_strdup (args));
        return;
    }

    gchar **par = split_quoted (args, TRUE);
    if (par) {
        guint i;
        for (i = 0; i < g_strv_length (par); ++i) {
            sharg_append (argv, g_strdup (par[i]));
        }
        g_strfreev (par);
    }
}

gboolean
for_each_line_in_file (const gchar *path, UzblLineCallback callback, gpointer data)
{
    gchar *line = NULL;
    gsize len;

    GIOChannel *chan = g_io_channel_new_file (path, "r", NULL);

    if (!chan) {
        return FALSE;
    }

    while (g_io_channel_read_line (chan, &line, &len, NULL, NULL) == G_IO_STATUS_NORMAL) {
        callback (line, data);
        g_free (line);
    }

    g_io_channel_unref (chan);

    return TRUE;
}

void
parse_command_from_file_cb (const gchar *line, gpointer data)
{
    UZBL_UNUSED (data);

    parse_command_from_file (line);
}

void
parse_command_from_file (const char *cmd)
{
    if (!cmd || !*cmd) {
        return;
    }

    gchar *work_string = g_strdup (cmd);

    /* Strip trailing newline, and any other whitespace in front. */
    g_strstrip (work_string);

    /* Skip comments. */
    if (*work_string != '#') {
        uzbl_commands_run (work_string, NULL);
    }

    g_free (work_string);
}

void
sharg_append (GArray *argv, const gchar *str)
{
    const gchar *s = (str ? str : g_strdup (""));
    g_array_append_val (argv, s);
}

gchar **
split_quoted (const gchar *src, const gboolean unquote)
{
    /* Split on unquoted space or tab, return array of strings; remove a layer
     * of quotes and backslashes if unquote. */
    if (!src) {
        return NULL;
    }

    GArray *argv = g_array_new (TRUE, FALSE, sizeof (gchar *));
    GString *str = g_string_new ("");
    const gchar *p;

    gboolean ctx_double_quote = FALSE;
    gboolean ctx_single_quote = FALSE;

    for (p = src; *p; ++p) {
        if ((*p == '\\') && p[1]) {
            /* Escaped character. */
            if (unquote) {
                g_string_append_c (str, *++p);
            } else {
                g_string_append_c (str, *p++);
                g_string_append_c (str, *p);
            }
        } else if ((*p == '"') && !ctx_single_quote) {
            /* Double quoted argument. */
            if (unquote) {
                ctx_double_quote = !ctx_double_quote;
            } else {
                g_string_append_c (str, *p);
                ctx_double_quote = !ctx_double_quote;
            }
        } else if ((*p == '\'') && !ctx_double_quote) {
            /* Single quoted argument. */
            if (unquote) {
                ctx_single_quote = !ctx_single_quote;
            } else {
                g_string_append_c (str, *p);
                ctx_single_quote = ! ctx_single_quote;
            }
        } else if (isspace (*p) && !ctx_double_quote && !ctx_single_quote) {
            /* Argument separator. */
            /* FIXME: Is "a  b" three arguments? */
            gchar *dup = g_strdup (str->str);
            g_array_append_val (argv, dup);
            g_string_truncate (str, 0);
        } else {
            /* Regular character. */
            g_string_append_c (str, *p);
        }
    }

    /* Append last argument. */
    gchar *dup = g_strdup (str->str);
    g_array_append_val (argv, dup);

    gchar **ret = (gchar **)argv->data;

    g_array_free (argv, FALSE);
    g_string_free (str, TRUE);

    return ret;
}

/* ==================== COMMAND  IMPLEMENTATIONS ==================== */

#define IMPLEMENT_COMMAND(cmd) \
    void                       \
    cmd_##cmd (GArray *argv, GString *result)

/* Navigation commands */

IMPLEMENT_COMMAND (back)
{
    UZBL_UNUSED (result);

    const gchar *count = argv_idx (argv, 0);

    int n = count ? atoi (count) : 1;

    webkit_web_view_go_back_or_forward (uzbl.gui.web_view, -n);
}

IMPLEMENT_COMMAND (forward)
{
    UZBL_UNUSED (result);

    const gchar *count = argv_idx (argv, 0);

    int n = count ? atoi (count) : 1;

    webkit_web_view_go_back_or_forward (uzbl.gui.web_view, n);
}

IMPLEMENT_COMMAND (reload)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_view_reload (uzbl.gui.web_view);
}

IMPLEMENT_COMMAND (reload_force)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_view_reload_bypass_cache (uzbl.gui.web_view);
}

IMPLEMENT_COMMAND (stop)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_view_stop_loading (uzbl.gui.web_view);
}

IMPLEMENT_COMMAND (uri)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    gchar *uri = argv_idx (argv, 0);

    uzbl_variables_set ("uri", uri);
}

#ifndef USE_WEBKIT2 /* FIXME: Implement. */
IMPLEMENT_COMMAND (auth)
{
    UZBL_UNUSED (result);

    gchar *info;
    gchar *username;
    gchar *password;

    ARG_CHECK (argv, 3);

    info = argv_idx (argv, 0);
    username = argv_idx (argv, 1);
    password = argv_idx (argv, 2);

    uzbl_soup_authenticate (info, username, password);
}
#endif

IMPLEMENT_COMMAND (download)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *uri         = argv_idx (argv, 0);
    const gchar *destination = NULL;

    if (argv->len > 1) {
        destination = argv_idx (argv, 1);
    }

    WebKitNetworkRequest *req = webkit_network_request_new (uri);
    WebKitDownload *download = webkit_download_new (req);

    /* TODO: Make a download function. */
    download_cb (uzbl.gui.web_view, download, (gpointer)destination);

    if (webkit_download_get_destination_uri (download)) {
        webkit_download_start (download);
    } else {
        g_object_unref (download);
    }

    g_object_unref (req);
}

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 9, 90)
IMPLEMENT_COMMAND (load)
{
    UZBL_UNUSED (result);

    guint sz = argv->len;

    const gchar *content = (sz > 0) ? argv_idx (argv, 0) : NULL;
    const gchar *content_uri = (sz > 1) ? argv_idx (argv, 1) : NULL;
    const gchar *base_uri = (sz > 2) ? argv_idx (argv, 2) : NULL;

    webkit_web_view_load_alternate_html (uzbl.gui.web_view, content, content_uri, base_uri);
}

IMPLEMENT_COMMAND (save)
{
    UZBL_UNUSED (result);

    guint sz = argv->len;

    const gchar *mode_str = (sz > 0) ? argv_idx (argv, 0) : NULL;

    WebKitSaveMode mode = WEBKIT_SAVE_MODE_MHTML;

    if (!mode) {
        mode = WEBKIT_SAVE_MODE_MHTML;
    } else if (!strcmp ("mhtml", mode_str)) {
        mode = WEBKIT_SAVE_MODE_MHTML;
    }

    if (sz > 1) {
        const gchar *path = argv_idx (argv, 1);
        GFile *gfile = g_file_new_for_path (path);

        webkit_web_view_save_to_file (page, gfile, mode, NULL, NULL, NULL);
        /* TODO: Don't ignore the error. */
        webkit_web_view_save_to_file_finish (page, NULL, NULL);
    } else {
        webkit_web_view_save (page, mode, NULL, NULL, NULL);
        /* TODO: Don't ignore the error. */
        webkit_web_view_save_finish (page, NULL, NULL);
    }
}
#endif
#endif

/* Cookie commands */

IMPLEMENT_COMMAND (cookie)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

#ifdef USE_WEBKIT2
    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    WebKitCookieManager *manager = webkit_web_context_get_cookie_manager (context);
#endif

    if (!g_strcmp0 (command, "add")) {
#ifdef USE_WEBKIT2
        uzbl_debug ("Manual cookie additions are unsupported in WebKit2.\n");
#else
        gchar *host;
        gchar *path;
        gchar *name;
        gchar *value;
        gchar *scheme;
        gboolean secure = 0;
        gboolean httponly = 0;
        gchar *expires_arg;
        SoupDate *expires = NULL;

        ARG_CHECK (argv, 7);

        /* Parse with same syntax as ADD_COOKIE event. */
        host = argv_idx (argv, 1);
        path = argv_idx (argv, 2);
        name = argv_idx (argv, 3);
        value = argv_idx (argv, 4);
        scheme = argv_idx (argv, 5);
        if (!strprefix (scheme, "http")) {
            secure = (scheme[4] == 's');
            httponly = !strprefix (scheme + 4 + secure, "Only");
        }
        expires_arg = argv_idx (argv, 6);
        if (*expires_arg) {
            expires = soup_date_new_from_time_t (strtoul (expires_arg, NULL, 10));
        }

        /* Create new cookie. */
        /* TODO: Add support for adding non-session cookies. */
        static const int session_cookie = -1;
        SoupCookie *cookie = soup_cookie_new (name, value, host, path, session_cookie);
        soup_cookie_set_secure (cookie, secure);
        soup_cookie_set_http_only (cookie, httponly);
        if (expires) {
            soup_cookie_set_expires (cookie, expires);
        }

        /* Add cookie to jar. */
        uzbl.net.soup_cookie_jar->in_manual_add = 1;
        soup_cookie_jar_add_cookie (SOUP_COOKIE_JAR (uzbl.net.soup_cookie_jar), cookie);
        uzbl.net.soup_cookie_jar->in_manual_add = 0;
#endif
    } else if (!g_strcmp0 (command, "delete")) {
#ifdef USE_WEBKIT2
        uzbl_debug ("Manual cookie deletions are unsupported in WebKit2.\n");
#else
        ARG_CHECK (argv, 5);

        static const int expired_cookie = 0;
        SoupCookie *cookie = soup_cookie_new (
            argv_idx (argv, 3),
            argv_idx (argv, 4),
            argv_idx (argv, 1),
            argv_idx (argv, 2),
            expired_cookie);

        uzbl.net.soup_cookie_jar->in_manual_add = 1;
        soup_cookie_jar_delete_cookie (SOUP_COOKIE_JAR (uzbl.net.soup_cookie_jar), cookie);
        uzbl.net.soup_cookie_jar->in_manual_add = 0;
#endif
    } else if (!g_strcmp0 (command, "clear")) {
        ARG_CHECK (argv, 2);

        const gchar *type = argv_idx (argv, 1);

        if (!g_strcmp0 (type, "all")) {
#ifdef USE_WEBKIT2
            webkit_cookie_manager_delete_all_cookies (manager);
#else
            /* Replace the current cookie jar with a new empty jar. */
            soup_session_remove_feature (uzbl.net.soup_session,
                SOUP_SESSION_FEATURE (uzbl.net.soup_cookie_jar));
            g_object_unref (G_OBJECT (uzbl.net.soup_cookie_jar));
            uzbl.net.soup_cookie_jar = uzbl_cookie_jar_new ();
            soup_session_add_feature (uzbl.net.soup_session,
                SOUP_SESSION_FEATURE (uzbl.net.soup_cookie_jar));
#endif
#ifdef USE_WEBKIT2
        } else if (!g_strcmp0 (type, "domain")) {
            guint i;
            for (i = 2; i < argv->len; ++i) {
                const gchar *domain = argv_idx (argv, i);

                webkit_cookie_manager_delete_cookies_for_domain (manager, domain);
            }
#endif
        } else {
            uzbl_debug ("Unrecognized cookie clear type: %s\n", type);
        }
    } else {
        uzbl_debug ("Unrecognized cookie command: %s\n", command);
    }
}

/* Display commands */

/*
 * scroll vertical 20
 * scroll vertical 20%
 * scroll vertical -40
 * scroll vertical 20!
 * scroll vertical begin
 * scroll vertical end
 * scroll horizontal 10
 * scroll horizontal -500
 * scroll horizontal begin
 * scroll horizontal end
 */
IMPLEMENT_COMMAND (scroll)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 2);

    gchar *direction = argv_idx (argv, 0);
    gchar *amount_str = argv_idx (argv, 1);
    GtkAdjustment *bar = NULL;

    if (!g_strcmp0 (direction, "horizontal")) {
        bar = uzbl.gui.bar_h;
    } else if (!g_strcmp0 (direction, "vertical")) {
        bar = uzbl.gui.bar_v;
    } else {
        uzbl_debug ("Unrecognized scroll direction: %s\n", direction);
        return;
    }

    gdouble lower = gtk_adjustment_get_lower (bar);
    gdouble upper = gtk_adjustment_get_upper (bar);
    gdouble page = gtk_adjustment_get_page_size (bar);

    if (!g_strcmp0 (amount_str, "begin")) {
        gtk_adjustment_set_value (bar, lower);
    } else if (!g_strcmp0 (amount_str, "end")) {
        gtk_adjustment_set_value (bar, upper - page);
    } else {
        gchar *end;

        gdouble value = gtk_adjustment_get_value (bar);
        gdouble amount = g_ascii_strtod (amount_str, &end);
        gdouble max_value = upper - page;

        if (*end == '%') {
            value += page * amount * 0.01;
        } else if (*end == '!') {
            value = amount;
        } else {
            value += amount;
        }

        if (value < 0) {
            value = 0; /* Don't scroll past the beginning of the page. */
        }
        if (value > max_value) {
            value = max_value; /* Don't scroll past the end of the page. */
        }

        gtk_adjustment_set_value (bar, value);
    }
}

IMPLEMENT_COMMAND (zoom_in)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_view_zoom_in (uzbl.gui.web_view);
}

IMPLEMENT_COMMAND (zoom_out)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_view_zoom_out (uzbl.gui.web_view);
}

IMPLEMENT_COMMAND (hardcopy)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_frame_print (webkit_web_view_get_main_frame (uzbl.gui.web_view));
}

#ifdef HAVE_SNAPSHOT
IMPLEMENT_COMMAND (snapshot)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    cairo_surface_t *surface;

#ifdef USE_WEBKIT2
    ARG_CHECK (argv, 3);

    guint sz = argv->len;
    guint i;

    const gchar *path = argv_idx (argv, 0);
    const gchar *format = argv_idx (argv, 1);
    const gchar *region_str = argv_idx (argv, 2);

    WebKitSnapshotRegion region = WEBKIT_SNAPSHOT_REGION_VISIBLE;

    if (!g_strcmp0 (region_str, "visible")) {
        region = WEBKIT_SNAPSHOT_REGION_VISIBLE;
    } else if (!g_strcmp0 (region_str, "document")) {
        region = WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT;
    } else {
        uzbl_debug ("Unrecognized snapshot region: %s\n", region_str);
    }

    WebKitSnapshotOptions options = WEBKIT_SNAPSHOT_OPTIONS_NONE;

    for (i = 3; i < sz; ++i) {
        const gchar *option = argv_idx (argv, i);

        if (!g_strcmp0 (option, "selection")) {
            options |= WEBKIT_SNAPSHOT_OPTIONS_INCLUDE_SELECTION_HIGHLIGHTING;
        } else {
            uzbl_debug ("Unrecognized snapshot option: %s\n", option);
        }
    }

    webkit_web_view_get_snapshot (uzbl.gui.web_view, region, options,
                                  NULL, NULL, NULL);
    surface = webkit_web_view_get_snapshot_finish (uzbl.gui.web_view, NULL, NULL);

    if (!g_strcmp0 (format, "png")) {
        cairo_surface_write_to_png (surface, argv_idx (argv, 0));
    } else {
        uzbl_debug ("Unrecognized snapshot format: %s\n", format);
    }
#else
    surface = webkit_web_view_get_snapshot (uzbl.gui.web_view);

    cairo_surface_write_to_png (surface, argv_idx (argv, 0));
#endif

    cairo_surface_destroy (surface);
}
#endif

/* Content commands */

#ifdef HAVE_PLUGIN_API
#ifndef USE_WEBKIT2
static void
plugin_toggle_one (WebKitWebPlugin *plugin, gpointer data);
#endif

IMPLEMENT_COMMAND (plugin)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

#ifdef USE_WEBKIT2
    if (!g_strcmp0 (command, "search")) {
        ARG_CHECK (argv, 2);

        const gchar *directory = argv_idx (argv, 1);
        WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);

        webkit_web_context_set_additional_plugins_directory (context, directory);
#else
    if (!g_strcmp0 (command, "refresh")) {
        WebKitWebPluginDatabase *db = webkit_get_web_plugin_database ();
        webkit_web_plugin_database_refresh (db);
    } else if (!g_strcmp0 (command, "toggle")) {
        WebKitWebPluginDatabase *db = webkit_get_web_plugin_database ();
        GSList *plugins = webkit_web_plugin_database_get_plugins (db);

        if (argv->len == 0) {
            g_slist_foreach (plugins, (GFunc)plugin_toggle_one, NULL);
        } else {
            guint i;
            for (i = 1; i < argv->len; ++i) {
                const gchar *plugin_name = argv_idx (argv, i);

                g_slist_foreach (plugins, (GFunc)plugin_toggle_one, (gpointer)plugin_name);
            }
        }

        webkit_web_plugin_database_plugins_list_free (plugins);
#endif
    } else {
        uzbl_debug ("Unrecognized plugin command: %s\n", command);
    }
}
#endif

#ifndef USE_WEBKIT2
IMPLEMENT_COMMAND (remove_all_db)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_remove_all_web_databases ();
}

#if WEBKIT_CHECK_VERSION (1, 5, 1)
IMPLEMENT_COMMAND (spell_checker)
{
    ARG_CHECK (argv, 1);

    GObject *obj = webkit_get_text_checker ();

    if (!obj) {
        return;
    }
    if (!WEBKIT_IS_SPELL_CHECKER (obj)) {
        return;
    }

    WebKitSpellChecker *checker = WEBKIT_SPELL_CHECKER (obj);

    const gchar *command = argv_idx (argv, 0);

    if (!g_strcmp0 (command, "ignore")) {
        guint i;
        for (i = 1; i < argv->len; ++i) {
            const gchar *word = argv_idx (argv, i);

            webkit_spell_checker_ignore_word (checker, word);
        }
    } else if (!g_strcmp0 (command, "learn")) {
        guint i;
        for (i = 1; i < argv->len; ++i) {
            const gchar *word = argv_idx (argv, i);

            webkit_spell_checker_learn_word (checker, word);
        }
    } else if (!g_strcmp0 (command, "autocorrect")) {
        ARG_CHECK (argv, 2);

        gchar *word = argv_idx (argv, 1);

        char *new_word = webkit_spell_checker_get_autocorrect_suggestions_for_misspelled_word (checker, word);

        g_string_assign (result, new_word);

        free (new_word);
    } else if (!g_strcmp0 (command, "guesses")) {
        /* TODO Implement. */
    } else {
        uzbl_debug ("Unrecognized spell_checker command: %s\n", command);
    }
}
#endif
#endif

/* Search commands */

static void
search_text (WebKitWebView *view, const gchar *key, const gboolean forward);

IMPLEMENT_COMMAND (search_forward)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    search_text (uzbl.gui.web_view, argv_idx (argv, 0), TRUE);
}

IMPLEMENT_COMMAND (search_reverse)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    search_text (uzbl.gui.web_view, argv_idx (argv, 0), FALSE);
}

IMPLEMENT_COMMAND (search_clear)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_view_unmark_text_matches (uzbl.gui.web_view);

    g_free (uzbl.state.searchtx);
    uzbl.state.searchtx = NULL;
}

IMPLEMENT_COMMAND (search_reset)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_view_set_highlight_text_matches (uzbl.gui.web_view, FALSE);
}

/* Inspector commands */

IMPLEMENT_COMMAND (inspector_show)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_inspector_show (uzbl.gui.inspector);
}

IMPLEMENT_COMMAND (inspector)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

    if (!g_strcmp0 (command, "show")) {
        webkit_web_inspector_show (uzbl.gui.inspector);
    } else if (!g_strcmp0 (command, "close")) {
        webkit_web_inspector_close (uzbl.gui.inspector);
    } else if (!g_strcmp0 (command, "coord")) {
        if (argv->len < 3) {
            return;
        }

        gdouble x = strtod (argv_idx (argv, 1), NULL);
        gdouble y = strtod (argv_idx (argv, 2), NULL);

        /* Let's not tempt the dragons. */
        if (errno == ERANGE) {
            return;
        }

        webkit_web_inspector_inspect_coordinates (uzbl.gui.inspector, x, y);
#if WEBKIT_CHECK_VERSION (1, 3, 17)
    } else if (!g_strcmp0 (command, "node")) {
        /* TODO: Implement. */
#endif
    }
}

/* Execution commands */

static void
eval_js (WebKitWebView *view, const gchar *script, GString *result, const gchar *path);

IMPLEMENT_COMMAND (js)
{
    ARG_CHECK (argv, 1);

    eval_js (uzbl.gui.web_view, argv_idx (argv, 0), result, "(uzbl command)");
}

IMPLEMENT_COMMAND (js_file)
{
    ARG_CHECK (argv, 1);

    gchar *req_path = argv_idx (argv, 0);
    gchar *path = NULL;

    if ((path = find_existing_file (req_path))) {
        gchar *file_contents = NULL;

        GIOChannel *chan = g_io_channel_new_file (path, "r", NULL);
        if (chan) {
            gsize len;
            g_io_channel_read_to_end (chan, &file_contents, &len, NULL);
            g_io_channel_unref (chan);
        }

        uzbl_debug ("External JavaScript file loaded: %s\n", req_path);

        gchar *arg = argv_idx (argv, 1);

        /* TODO: Make more generic? */
        gchar *js = str_replace ("%s", arg ? arg : "", file_contents);
        g_free (file_contents);

        eval_js (uzbl.gui.web_view, js, result, path);

        g_free (js);
        g_free (path);
    }
}

static void
spawn (GArray *argv, GString *result, gboolean exec);
static void
spawn_sh (GArray *argv, GString *result);

IMPLEMENT_COMMAND (spawn)
{
    UZBL_UNUSED (result);

    spawn (argv, NULL, FALSE);
}

IMPLEMENT_COMMAND (spawn_sync)
{
    spawn (argv, result, FALSE);
}

IMPLEMENT_COMMAND (spawn_sync_exec)
{
    if (!result) {
        GString *force_result = g_string_new ("");
        spawn (argv, force_result, TRUE);
        g_string_free (force_result, TRUE);
    } else {
        spawn (argv, result, TRUE);
    }
}

IMPLEMENT_COMMAND (spawn_sh)
{
    UZBL_UNUSED (result);

    spawn_sh (argv, NULL);
}

IMPLEMENT_COMMAND (spawn_sh_sync)
{
    spawn_sh (argv, result);
}

/* Uzbl commands */

IMPLEMENT_COMMAND (chain)
{
    ARG_CHECK (argv, 1);

    guint i = 0;
    const gchar *cmd;
    GString *res = g_string_new ("");

    while ((cmd = argv_idx (argv, i++))) {
        uzbl_commands_run (cmd, res);
    }

    if (result) {
        g_string_assign (result, res->str);
    }

    g_string_free (res, TRUE);
}

IMPLEMENT_COMMAND (include)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    gchar *req_path = argv_idx (argv, 0);
    gchar *path = NULL;

    if ((path = find_existing_file (req_path))) {
        uzbl_commands_load_file (path);
        uzbl_events_send (FILE_INCLUDED, NULL,
            TYPE_STR, path,
            NULL);
        g_free (path);
    }
}

IMPLEMENT_COMMAND (exit)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    /* Hide window a soon as possible to avoid getting stuck with a
     * non-response window in the cleanup steps. */
    if (uzbl.gui.main_window) {
        gtk_widget_destroy (uzbl.gui.main_window);
    } else if (uzbl.gui.plug) {
        gtk_widget_destroy (GTK_WIDGET (uzbl.gui.plug));
    }

    gtk_main_quit ();
}

/* Variable commands */

IMPLEMENT_COMMAND (set)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    gchar **split = g_strsplit (argv_idx (argv, 0), "=", 2);
    if (split[0] != NULL) {
        gchar *value = split[1] ? g_strchug (split[1]) : " ";
        uzbl_variables_set (g_strstrip (split[0]), value);
    }
    g_strfreev (split);
}

IMPLEMENT_COMMAND (toggle)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    gchar *var_name = argv_idx (argv, 0);

    g_array_remove_range (argv, 0, 1);

    uzbl_variables_toggle (var_name, argv);

    g_free (var_name);
}

IMPLEMENT_COMMAND (print)
{
    ARG_CHECK (argv, 1);

    gchar *buf;

    if (!result) {
        return;
    }

    buf = uzbl_variables_expand (argv_idx (argv, 0));
    g_string_assign (result, buf);
    g_free (buf);
}

IMPLEMENT_COMMAND (dump_config)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    uzbl_variables_dump ();
}

IMPLEMENT_COMMAND (dump_config_as_events)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    uzbl_variables_dump_events ();
}

IMPLEMENT_COMMAND (event)
{
    UZBL_UNUSED (result);

    GString *event_name;
    gchar **split = NULL;

    ARG_CHECK (argv, 1);

    split = g_strsplit (argv_idx (argv, 0), " ", 2);
    if (!split[0]) {
        return;
    }

    event_name = g_string_ascii_up (g_string_new (split[0]));

    uzbl_events_send (USER_EVENT, event_name->str,
        TYPE_FORMATTEDSTR, split[1] ? split[1] : "",
        NULL);

    g_string_free (event_name, TRUE);
    g_strfreev (split);
}

#ifndef USE_WEBKIT2
#ifdef HAVE_PLUGIN_API
void
plugin_toggle_one (WebKitWebPlugin *plugin, gpointer data)
{
    const gchar *name = (const gchar *)data;

    const gchar *plugin_name = webkit_web_plugin_get_name (plugin);

    if (!name || !g_strcmp0 (name, plugin_name)) {
        gboolean enabled = webkit_web_plugin_get_enabled (plugin);

        webkit_web_plugin_set_enabled (plugin, !enabled);
    }
}
#endif
#endif

void
search_text (WebKitWebView *view, const gchar *key, const gboolean forward)
{
    if (*key) {
        if (g_strcmp0 (uzbl.state.searchtx, key)) {
            webkit_web_view_unmark_text_matches (view);
            webkit_web_view_mark_text_matches (view, key, FALSE, 0);

            g_free (uzbl.state.searchtx);
            uzbl.state.searchtx = g_strdup (key);
        }
    }

    if (uzbl.state.searchtx) {
        uzbl_debug ("Searching: %s\n", uzbl.state.searchtx);

        webkit_web_view_set_highlight_text_matches (view, TRUE);
        webkit_web_view_search_text (view, uzbl.state.searchtx, FALSE, forward, TRUE);
    }
}

static char *
extract_js_prop (JSGlobalContextRef ctx, JSObjectRef obj, const gchar *prop);
static char *
js_value_to_string (JSGlobalContextRef ctx, JSValueRef obj);

void
eval_js (WebKitWebView *view, const gchar *script, GString *result, const gchar *path)
{
    WebKitWebFrame *frame;
    JSGlobalContextRef context;
    JSObjectRef globalobject;
    JSStringRef js_file;
    JSStringRef js_script;
    JSValueRef js_result;
    JSValueRef js_exc = NULL;

    frame = webkit_web_view_get_main_frame (view);
    context = webkit_web_frame_get_global_context (frame);
    globalobject = JSContextGetGlobalObject (context);

    /* Evaluate the script and get return value. */
    js_script = JSStringCreateWithUTF8CString (script);
    js_file = JSStringCreateWithUTF8CString (path);
    js_result = JSEvaluateScript (context, js_script, globalobject, js_file, 0, &js_exc);
    if (result && js_result && !JSValueIsUndefined (context, js_result)) {
        char *result_utf8;

        result_utf8 = js_value_to_string (context, js_result);

        g_string_assign (result, result_utf8);

        free (result_utf8);
    } else if (js_exc) {
        JSObjectRef exc = JSValueToObject (context, js_exc, NULL);

        char *file;
        char *line;
        char *msg;

        file = extract_js_prop (context, exc, "sourceURL");
        line = extract_js_prop (context, exc, "line");
        msg = js_value_to_string (context, exc);

        uzbl_debug ("Exception occured while executing script:\n %s:%s: %s\n", file, line, msg);

        free (file);
        free (line);
        free (msg);
    }

    JSStringRelease (js_script);
    JSStringRelease (js_file);
}

/* Make sure that the args string you pass can properly be interpreted (e.g.,
 * properly escaped against whitespace, quotes etc). */
static gboolean
run_system_command (const gchar *command, const gchar **args, const gboolean sync,
                    char **output_stdout);

void
spawn (GArray *argv, GString *result, gboolean exec)
{
    gchar *path = NULL;

    ARG_CHECK (argv, 1);

    const gchar **args = &g_array_index (argv, const gchar *, 1);
    const gchar *req_path = argv_idx (argv, 0);

    path = find_existing_file (req_path);

    if (!path) {
        uzbl_debug ("Failed to spawn child process: %s not found\n", req_path);
        return;
    }

    gchar *r = NULL;
    run_system_command (path, args, result != NULL, result ? &r : NULL);
    if (result) {
        g_string_assign (result, r);
        /* Run each line of output from the program as a command. */
        if (exec && r) {
            gchar *head = r;
            gchar *tail;
            while ((tail = strchr (head, '\n'))) {
                *tail = '\0';
                parse_command_from_file (head);
                head = tail + 1;
            }
        }
    }

    g_free (r);
    g_free (path);
}

void
spawn_sh (GArray *argv, GString *result)
{
    if (!uzbl.behave.shell_cmd) {
        uzbl_debug ("spawn_sh: shell_cmd is not set!\n");
        return;
    }
    guint i;

    gchar **cmd = split_quoted (uzbl.behave.shell_cmd, TRUE);
    if (!cmd) {
        return;
    }

    g_array_insert_val (argv, 1, cmd[0]);

    for (i = g_strv_length (cmd) - 1; i; --i) {
        g_array_prepend_val (argv, cmd[i]);
    }

    if (result) {
        gchar *r = NULL;
        run_system_command (cmd[0], (const gchar **)argv->data, TRUE, &r);
        g_string_assign (result, r);
        g_free (r);
    } else {
        run_system_command (cmd[0], (const gchar **)argv->data, FALSE, NULL);
    }
}

char *
extract_js_prop (JSGlobalContextRef ctx, JSObjectRef obj, const gchar *prop)
{
    JSStringRef js_prop;
    JSValueRef js_prop_val;

    js_prop = JSStringCreateWithUTF8CString (prop);
    js_prop_val = JSObjectGetProperty (ctx, obj, js_prop, NULL);

    JSStringRelease (js_prop);

    return js_value_to_string (ctx, js_prop_val);
}

char *
js_value_to_string (JSGlobalContextRef ctx, JSValueRef val)
{
    JSStringRef str = JSValueToStringCopy (ctx, val, NULL);
    size_t size = JSStringGetMaximumUTF8CStringSize (str);

    char *result = NULL;

    if (size) {
        result = (gchar *)malloc (size * sizeof (char));
        JSStringGetUTF8CString (str, result, size);
    }

    JSStringRelease (str);

    return result;
}

gboolean
run_system_command (const gchar *command, const gchar **args, const gboolean sync,
             char **output_stdout)
{
    GError *err = NULL;

    GArray *cmd_args = g_array_new (TRUE, FALSE, sizeof (gchar *));
    guint i;
    guint len = g_strv_length ((gchar **)args);

    sharg_append (cmd_args, g_strdup (command));

    for (i = 0; i < len; ++i) {
        sharg_append (cmd_args, g_strdup (args[i]));
    }

    gboolean result;
    if (sync) {
        if (*output_stdout) {
            g_free (*output_stdout);
        }

        result = g_spawn_sync (NULL, (gchar **)cmd_args->data, NULL, G_SPAWN_SEARCH_PATH,
                               NULL, NULL, output_stdout, NULL, NULL, &err);
    } else {
        result = g_spawn_async (NULL, (gchar **)cmd_args->data, NULL, G_SPAWN_SEARCH_PATH,
                                NULL, NULL, NULL, &err);
    }

    if (uzbl.state.verbose) {
        GString *s = g_string_new ("spawned:");
        for (i = 0; i < cmd_args->len; ++i) {
            gchar *qarg = g_shell_quote (g_array_index (cmd_args, gchar*, i));
            g_string_append_printf (s, " %s", qarg);
            g_free (qarg);
        }
        g_string_append_printf (s, " -- result: %s", (result ? "true" : "false"));
        printf ("%s\n", s->str);
        g_string_free (s, TRUE);
        if (output_stdout) {
            printf ("Stdout: %s\n", *output_stdout);
        }
    }
    if (err) {
        g_printerr ("error on run_system_command: %s\n", err->message);
        g_error_free (err);
    }
    while (cmd_args->len) {
        g_free (argv_idx (cmd_args, cmd_args->len - 1));
        g_array_remove_index (cmd_args, cmd_args->len - 1);
    }
    g_array_free (cmd_args, TRUE);
    return result;
}
