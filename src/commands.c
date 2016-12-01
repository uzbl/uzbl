#include "commands.h"

#include "events.h"
#include "gui.h"
#include "io.h"
#include "js.h"
#include "menu.h"
#include "requests.h"
#include "scheme.h"
#include "setup.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"
#include "variables.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* TODO: (WebKit2)
 *
 *   - Add commands for registering custom schemes.
 *   - Add commands for handling the back/forward list.
 *   - Add "edit" command for cut/copy/paste/select/etc (also WK1).
 *   - Add dumping the source (see WebKitWebResource).
 *   - Add resource management commands (also WK1?).
 */

struct _UzblCommands {
    /* Table of all builtin commands. */
    GHashTable *table;

    /* Search variables */
    WebKitFindOptions  search_options;
    WebKitFindOptions  search_options_last;
    gboolean           search_forward;
    gchar             *search_text;

#if WEBKIT_CHECK_VERSION (2, 5, 1)
    /* Script variables */
    GHashTable *script_handler_data_table;
#endif
};

typedef void (*UzblCommandCallback) (GArray *argv, GString *result);
typedef void (*UzblCommandTask) (GArray *argv, GTask *task);

struct _UzblCommand {
    const gchar         *name;
    UzblCommandCallback  function;
    gboolean             split;
    gboolean             send_event;
    gboolean             task;
};

static const UzblCommand
builtin_command_table[];

typedef struct _UzblCommandRun UzblCommandRun;
struct _UzblCommandRun {
    const UzblCommand *info;
    const GArray      *argv;
    GString     *result;
};

#if WEBKIT_CHECK_VERSION (2, 5, 1)
typedef struct _UzblScriptHandlerData UzblScriptHandlerData;
static void
script_handler_data_free (gpointer data);
#endif

/* =========================== PUBLIC API =========================== */

static void
init_js_commands_api ();

void
uzbl_commands_init ()
{
    uzbl.commands = g_malloc (sizeof (UzblCommands));

    uzbl.commands->table = g_hash_table_new (g_str_hash, g_str_equal);

    uzbl.commands->search_options = 0;
    uzbl.commands->search_options_last = 0;
    uzbl.commands->search_forward = FALSE;
    uzbl.commands->search_text = NULL;

#if WEBKIT_CHECK_VERSION (2, 5, 1)
    uzbl.commands->script_handler_data_table = g_hash_table_new_full (
        g_str_hash, g_str_equal,
        g_free, script_handler_data_free);
#endif

    const UzblCommand *cmd = &builtin_command_table[0];
    while (cmd->name) {
        g_hash_table_insert (uzbl.commands->table,
            (gpointer)cmd->name,
            (gpointer)cmd);

        ++cmd;
    }

    init_js_commands_api ();
}

void
uzbl_commands_free ()
{
    g_hash_table_destroy (uzbl.commands->table);

    g_free (uzbl.commands->search_text);

#if WEBKIT_CHECK_VERSION (2, 5, 1)
    g_hash_table_destroy (uzbl.commands->script_handler_data_table);
#endif

    g_free (uzbl.commands);
    uzbl.commands = NULL;
}

void
uzbl_commands_send_builtin_event ()
{
    GString *command_list = g_string_new ("");

    g_string_append_c (command_list, '[');

    gboolean first = TRUE;

    const UzblCommand *cmd = builtin_command_table;
    while (cmd->name) {
        if (!first) {
            g_string_append_c (command_list, ',');
        }
        g_string_append_c (command_list, '\"');
        g_string_append (command_list, cmd->name);
        g_string_append_c (command_list, '\"');

        ++cmd;
        first = FALSE;
    }

    g_string_append_c (command_list, ']');

    uzbl_events_send (BUILTINS, NULL,
        TYPE_FORMATTEDSTR, command_list->str,
        NULL);

    g_string_free (command_list, TRUE);
}

GArray *
uzbl_commands_args_new ()
{
    return g_array_new (TRUE, TRUE, sizeof (gchar *));
}

void
uzbl_commands_args_append (GArray *argv, const gchar *arg)
{
    const gchar *safe_arg = (arg ? arg : g_strdup (""));
    g_array_append_val (argv, safe_arg);
}

void
uzbl_commands_args_free (GArray *argv)
{
    if (!argv) {
        return;
    }

    while (argv->len) {
        g_free (argv_idx (argv, argv->len - 1));
        g_array_remove_index (argv, argv->len - 1);
    }
    g_array_free (argv, TRUE);
}

static void
parse_command_arguments (const gchar *args, GArray *argv, gboolean split);

const UzblCommand *
uzbl_commands_parse (const gchar *cmd, GArray *argv)
{
    if (!cmd || cmd[0] == '#' || !*cmd) {
        return NULL;
    }

    gchar *exp_line = uzbl_variables_expand (cmd);
    if (!exp_line || !*exp_line) {
        g_free (exp_line);
        return NULL;
    }

    /* Separate the line into the command and its parameters. */
    gchar **tokens = g_strsplit (exp_line, " ", 2);

    const gchar *command = tokens[0];
    const gchar *arg_string = tokens[1];

    /* Look up the command. */
    const UzblCommand *info = g_hash_table_lookup (uzbl.commands->table, command);

    if (!info) {
        uzbl_events_send (COMMAND_ERROR, NULL,
            TYPE_STR, command,
            NULL);

        g_free (exp_line);
        g_strfreev (tokens);

        return NULL;
    }

    /* Parse the arguments. */
    if (argv && arg_string) {
        parse_command_arguments (arg_string, argv, info->split);
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

    if (info->task) {
        g_debug ("trying to run task in sync mode");
        return;
    }

    info->function (argv, result);

    if (result) {
        g_free (uzbl.state.last_result);
        uzbl.state.last_result = g_strdup (result->str);
    }

    if (info->send_event) {
        uzbl_events_send (COMMAND_EXECUTED, NULL,
            TYPE_NAME, info->name,
            TYPE_STR_ARRAY, argv,
            NULL);
    }
}

static void
command_done_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      data);

static GString*
command_finish (GObject       *source,
                GAsyncResult  *res,
                GError       **err);

void
uzbl_commands_run_async (const UzblCommand   *info,
                         GArray              *argv,
                         gboolean             capture,
                         GAsyncReadyCallback  callback,
                         gpointer             data)
{
    if (!info) {
        return;
    }

    GTask *task = g_task_new (NULL, NULL, callback, data);
    UzblCommandRun *run = g_new (UzblCommandRun, 1);
    run->info = info;
    run->argv = argv;
    if (capture) {
        run->result = g_string_new ("");
    }

    g_task_set_task_data (task, (gpointer) run, g_free);
    if (info->task) {
        GTask *subtask = g_task_new (NULL, NULL,
                                     command_done_cb, (gpointer) task);
        g_task_set_task_data (subtask, (gpointer) run->result, NULL);
        ((UzblCommandTask)info->function) (argv, subtask);
        return;
    }

    info->function (argv, run->result);
    g_task_return_pointer (task, run->result, free_gstring);
    g_object_unref (task);
}

GString*
uzbl_commands_run_finish (GObject       *source,
                          GAsyncResult  *res,
                          GError       **error)
{
    UZBL_UNUSED (source);
    GTask *task = G_TASK (res);
    UzblCommandRun *run = g_task_get_task_data (task);

    GString *result = (GString*) g_task_propagate_pointer (task, error);

    g_free (uzbl.state.last_result);
    uzbl.state.last_result = result ? g_strdup (result->str) : g_strdup ("");

    if (run->info->send_event) {
        uzbl_events_send (COMMAND_EXECUTED, NULL,
            TYPE_NAME, run->info->name,
            TYPE_STR_ARRAY, run->argv,
            NULL);
    }

    return result;
}

void
command_done_cb (GObject      *source,
                 GAsyncResult *res,
                 gpointer      data)
{
    GTask *task = G_TASK (data);
    GError *err = NULL;
    command_finish (source, res, &err);
    UzblCommandRun *run = g_task_get_task_data (task);
    if (err) {
        g_debug ("error running task: %s", err->message);
        g_clear_error (&err);
    }
    g_task_return_pointer (task, run->result, free_gstring);
    g_object_unref (task);
}

GString*
command_finish (GObject       *source,
                GAsyncResult  *res,
                GError       **err)
{
    UZBL_UNUSED (source);
    GTask *task = G_TASK (res);
    return (GString*) g_task_propagate_pointer (task, err);
}

void
uzbl_commands_run_argv (const gchar *cmd, GArray *argv, GString *result)
{
    /* Look up the command. */
    const UzblCommand *info = g_hash_table_lookup (uzbl.commands->table, cmd);

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
    GArray *argv = uzbl_commands_args_new ();
    const UzblCommand *info = uzbl_commands_parse (cmd, argv);

    uzbl_commands_run_parsed (info, argv, result);

    uzbl_commands_args_free (argv);
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

#if WEBKIT_CHECK_VERSION (2, 5, 1)
struct _UzblScriptHandlerData {
    gchar *name;
};

void
script_handler_data_free (gpointer data)
{
    UzblScriptHandlerData *handler_data = (UzblScriptHandlerData *)data;

    g_free (handler_data->name);

    g_free (handler_data);
}
#endif

static JSValueRef
call_command (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

void
init_js_commands_api ()
{
    JSObjectRef uzbl_obj = uzbl_js_object (uzbl.state.jscontext, "uzbl");
    JSObjectRef commands_obj = JSObjectMake (uzbl.state.jscontext, NULL, NULL);

    static JSClassDefinition
    command_class_def = {
        0,                     // version
        kJSClassAttributeNone, // attributes
        "UzblCommand",         // class name
        NULL,                  // parent class
        NULL,                  // static values
        NULL,                  // static functions
        NULL,                  // initialize
        NULL,                  // finalize
        NULL,                  // has property
        NULL,                  // get property
        NULL,                  // set property
        NULL,                  // delete property
        NULL,                  // get property names
        call_command,          // call as function
        NULL,                  // call as contructor
        NULL,                  // has instance
        NULL                   // convert to type
    };

    JSClassRef command_class = JSClassCreate (&command_class_def);

    const UzblCommand *cmd = builtin_command_table;
    while (cmd->name) {
        JSObjectRef command_obj = JSObjectMake (uzbl.state.jscontext, command_class, NULL);

        JSStringRef name = JSStringCreateWithUTF8CString (cmd->name);
        JSValueRef name_val = JSValueMakeString(uzbl.state.jscontext, name);

        uzbl_js_set (uzbl.state.jscontext,
            command_obj, "name", name_val,
            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete);
        uzbl_js_set (uzbl.state.jscontext,
            commands_obj, cmd->name, command_obj,
            kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete);

        ++cmd;
    }

    uzbl_js_set (uzbl.state.jscontext,
        uzbl_obj, "commands", commands_obj,
        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete);

    JSClassRelease (command_class);
}

static GArray *
split_quoted (const gchar *src);

static gchar *
unescape (gchar *src);

void
parse_command_arguments (const gchar *args, GArray *argv, gboolean split)
{
    if (!args) {
        return;
    }

    if (!split) {
        /* Pass the parameters through in one chunk. */
        uzbl_commands_args_append (argv, unescape (g_strdup (args)));
        return;
    }

    GArray *par = split_quoted (args);
    if (par) {
        guint i;
        for (i = 0; i < par->len; ++i) {
            const gchar *arg = argv_idx (par, i);
            uzbl_commands_args_append (argv, g_strdup (arg));
        }
    }

    uzbl_commands_args_free (par);
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

static void
parse_command_from_file (const char *cmd);

void
parse_command_from_file_cb (const gchar *line, gpointer data)
{
    UZBL_UNUSED (data);

    parse_command_from_file (line);
}

JSValueRef
call_command (JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    UZBL_UNUSED (thisObject);

    JSValueRef json_ret = NULL;
    JSValueRef command_val = uzbl_js_get (ctx, function, "name");
    gchar *command = uzbl_js_to_string (ctx, command_val);

    UzblCommand *info = g_hash_table_lookup (uzbl.commands->table, command);

    if (!info) {
        gchar *error_str = g_strdup_printf ("Unknown command: %s", command);
        JSStringRef error = JSStringCreateWithUTF8CString (error_str);

        *exception = JSValueMakeString (ctx, error);

        JSStringRelease (error);
        g_free (error_str);

        json_ret = JSValueMakeUndefined (ctx);

        g_free (command);
        return json_ret;
    }

    g_free (command);

    if (json_ret) {
        return json_ret;
    }

    GArray *argv = uzbl_commands_args_new ();
    GString *result = g_string_new ("");
    size_t i;

    for (i = 0; i < argumentCount; ++i) {
        gchar *arg = uzbl_js_to_string (ctx, arguments[i]);

        uzbl_commands_args_append (argv, arg);
    }

    uzbl_commands_run_parsed (info, argv, result);

    JSStringRef result_str = JSStringCreateWithUTF8CString (result->str);

    json_ret = JSValueMakeFromJSONString (ctx, result_str);

    if (!json_ret) {
        uzbl_debug ("Failed to parse result as JSON for command \'%s\': %s\n", info->name, result->str);

        json_ret = JSValueMakeString (ctx, result_str);
    }

    JSStringRelease (result_str);
    g_string_free (result, TRUE);
    uzbl_commands_args_free (argv);

    return json_ret;
}

GArray *
split_quoted (const gchar *src)
{
    /* Split on unquoted space or tab, return array of strings; remove a layer
     * of quotes and backslashes if unquote. */
    if (!src) {
        return NULL;
    }

    GArray *argv = uzbl_commands_args_new ();
    GString *str = g_string_new ("");
    const gchar *p = src;

    gboolean ctx_double_quote = FALSE;
    gboolean ctx_single_quote = FALSE;

    // Strip leading whitespace
    while (isspace(*p)) {
        ++p;
    }

    while (*p) {
        if ((*p == '\\') && p[1]) {
            /* Escaped character. */
            g_string_append_c (str, *++p);
            ++p;
        } else if ((*p == '"') && !ctx_single_quote) {
            /* Double quoted argument. */
            ctx_double_quote = !ctx_double_quote;
            ++p;
        } else if ((*p == '\'') && !ctx_double_quote) {
            /* Single quoted argument. */
            ctx_single_quote = !ctx_single_quote;
            ++p;
        } else if (isspace (*p) && !ctx_double_quote && !ctx_single_quote) {
            /* Argument separator. */
            while (isspace(*++p));

            uzbl_commands_args_append (argv, g_strdup (str->str));
            g_string_truncate (str, 0);
        } else {
            /* Regular character. */
            g_string_append_c (str, *p++);
        }
    }

    /* Append last argument. */
    uzbl_commands_args_append (argv, g_strdup (str->str));

    g_string_free (str, TRUE);

    return argv;
}

static gchar *
unescape (gchar *src)
{
    gchar *s = src;
    gchar *p = src;

    while(*s != '\0') {
        if (*s == '\\') {
            s++;
        }
        *(p++) = *(s++);
    }

    *p = '\0';

    return src;
}

void
parse_command_from_file (const char *cmd)
{
    if (!cmd || !*cmd) {
        return;
    }

    /* Strip trailing newline, and any other whitespace in front. */
    gchar *work_string = g_strdup (cmd);
    g_strstrip (work_string);

    uzbl_commands_run (work_string, NULL);
    g_free (work_string);
}

/* ========================= COMMAND TABLE ========================== */

#define DECLARE_COMMAND(cmd) \
    static void              \
    cmd_##cmd (GArray *argv, GString *result)

#define DECLARE_TASK(cmd)                                                 \
    static void                                                           \
    cmd_##cmd (GArray *argv, GTask *task)

/* Navigation commands */
DECLARE_COMMAND (back);
DECLARE_COMMAND (forward);
DECLARE_COMMAND (reload);
DECLARE_COMMAND (stop);
DECLARE_COMMAND (uri);
DECLARE_COMMAND (download);

/* Page commands */
DECLARE_COMMAND (load);
DECLARE_COMMAND (save);

/* Cookie commands */
DECLARE_COMMAND (cookie);

/* Display commands */
DECLARE_COMMAND (scroll);
DECLARE_COMMAND (zoom);
DECLARE_COMMAND (hardcopy);
DECLARE_COMMAND (geometry);
DECLARE_COMMAND (snapshot);

/* Content commands */
DECLARE_COMMAND (plugin);
DECLARE_COMMAND (cache);
DECLARE_COMMAND (favicon);
DECLARE_COMMAND (css);
#if WEBKIT_CHECK_VERSION (2, 7, 2)
DECLARE_COMMAND (script);
#endif
DECLARE_COMMAND (scheme);

/* Menu commands */
DECLARE_COMMAND (menu);

/* Search commands */
DECLARE_COMMAND (search);

/* Security commands */
DECLARE_COMMAND (security);
DECLARE_COMMAND (dns);

/* Inspector commands */
DECLARE_COMMAND (inspector);

/* Execution commands */
DECLARE_TASK (js);
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
DECLARE_COMMAND (choose);
DECLARE_COMMAND (request);

static const UzblCommand
builtin_command_table[] = {
    /* name                             function                      split  send_event  task*/
    /* Navigation commands */
    { "back",                           cmd_back,                     TRUE,  TRUE,  FALSE },
    { "forward",                        cmd_forward,                  TRUE,  TRUE,  FALSE },
    { "reload",                         cmd_reload,                   TRUE,  TRUE,  FALSE },
    { "stop",                           cmd_stop,                     TRUE,  TRUE,  FALSE },
    { "uri",                            cmd_uri,                      TRUE,  TRUE,  FALSE },
    { "download",                       cmd_download,                 TRUE,  TRUE,  FALSE },

    /* Page commands */
    { "load",                           cmd_load,                     TRUE,  TRUE,  FALSE },
    { "save",                           cmd_save,                     TRUE,  TRUE,  FALSE },

    /* Cookie commands */
    { "cookie",                         cmd_cookie,                   TRUE,  TRUE,  FALSE },

    /* Display commands */
    { "scroll",                         cmd_scroll,                   TRUE,  TRUE,  FALSE },
    { "zoom",                           cmd_zoom,                     TRUE,  TRUE,  FALSE },
    { "hardcopy",                       cmd_hardcopy,                 TRUE,  TRUE,  FALSE },
    { "geometry",                       cmd_geometry,                 TRUE,  TRUE,  FALSE },
    { "snapshot",                       cmd_snapshot,                 TRUE,  TRUE,  FALSE },

    /* Content commands */
    { "plugin",                         cmd_plugin,                   TRUE,  TRUE,  FALSE },
    { "cache",                          cmd_cache,                    TRUE,  TRUE,  FALSE },
    { "favicon",                        cmd_favicon,                  TRUE,  TRUE,  FALSE },
    { "css",                            cmd_css,                      TRUE,  TRUE,  FALSE },
#if WEBKIT_CHECK_VERSION (2, 7, 2)
    { "script",                         cmd_script,                   TRUE,  TRUE,  FALSE },
#endif
    { "scheme",                         cmd_scheme,                   FALSE, TRUE,  FALSE },

    /* Menu commands */
    { "menu",                           cmd_menu,                     TRUE,  TRUE,  FALSE },

    /* Search commands */
    { "search",                         cmd_search,                   FALSE, TRUE,  FALSE },

    /* Security commands */
    { "security",                       cmd_security,                 TRUE,  TRUE,  FALSE },
    { "dns",                            cmd_dns,                      TRUE,  TRUE,  FALSE },

    /* Inspector commands */
    { "inspector",                      cmd_inspector,                TRUE,  TRUE,  FALSE },

    /* Execution commands */
    { "js",                             (UzblCommandCallback) cmd_js, TRUE,  TRUE,  TRUE  },
    /* TODO: Consolidate into one command. */
    { "spawn",                          cmd_spawn,                    TRUE,  TRUE,  FALSE },
    { "spawn_sync",                     cmd_spawn_sync,               TRUE,  TRUE,  FALSE },
    { "spawn_sync_exec",                cmd_spawn_sync_exec,          TRUE,  TRUE,  FALSE },
    { "spawn_sh",                       cmd_spawn_sh,                 TRUE,  TRUE,  FALSE },
    { "spawn_sh_sync",                  cmd_spawn_sh_sync,            TRUE,  TRUE,  FALSE },

    /* Uzbl commands */
    { "chain",                          cmd_chain,                    TRUE,  TRUE,  FALSE },
    { "include",                        cmd_include,                  FALSE, TRUE,  FALSE },
    { "exit",                           cmd_exit,                     TRUE,  TRUE,  FALSE },

    /* Variable commands */
    { "set",                            cmd_set,                      FALSE, FALSE, FALSE},
    { "toggle",                         cmd_toggle,                   TRUE,  TRUE,  FALSE },
    /* TODO: Add more dump commands (e.g., current frame/page source) */
    { "dump_config",                    cmd_dump_config,              TRUE,  TRUE,  FALSE },
    { "dump_config_as_events",          cmd_dump_config_as_events,    TRUE,  TRUE,  FALSE },
    { "print",                          cmd_print,                    FALSE, TRUE,  FALSE },

    /* Event commands */
    { "event",                          cmd_event,                    FALSE, FALSE, FALSE },
    { "choose",                         cmd_choose,                   TRUE,  TRUE,  FALSE },
    { "request",                        cmd_request,                  TRUE,  TRUE,  FALSE },

    /* Terminator */
    { NULL,                             NULL,                         FALSE, FALSE, FALSE }
};

/* ==================== COMMAND  IMPLEMENTATIONS ==================== */

#define IMPLEMENT_COMMAND(cmd) \
    void                       \
    cmd_##cmd (GArray *argv, GString *result)
#define IMPLEMENT_TASK(cmd)                                               \
    void                                                                  \
    cmd_##cmd (GArray *argv, GTask *task)

/* Navigation commands */

IMPLEMENT_COMMAND (back)
{
    UZBL_UNUSED (result);

    const gchar *count = argv_idx (argv, 0);

    int n = count ? atoi (count) : 1;

    /* TODO: Iterate the back/forward list and use webkit_web_view_go_to_back_forward_list_item. */
    int i;
    for (i = 0; (i < n) && webkit_web_view_can_go_back (uzbl.gui.web_view); ++i) {
        webkit_web_view_go_back (uzbl.gui.web_view);
    }
}

IMPLEMENT_COMMAND (forward)
{
    UZBL_UNUSED (result);

    const gchar *count = argv_idx (argv, 0);

    int n = count ? atoi (count) : 1;

    /* TODO: Iterate the back/forward list and use webkit_web_view_go_to_back_forward_list_item. */
    int i;
    for (i = 0; (i < n) && webkit_web_view_can_go_forward (uzbl.gui.web_view); ++i) {
        webkit_web_view_go_forward (uzbl.gui.web_view);
    }
}

IMPLEMENT_COMMAND (reload)
{
    UZBL_UNUSED (result);

    const gchar *type = argv_idx (argv, 0);

    if (!type) {
        type = "cached";
    }

    if (!g_strcmp0 (type, "cached")) {
        webkit_web_view_reload (uzbl.gui.web_view);
    } else if (!g_strcmp0 (type, "full")) {
        webkit_web_view_reload_bypass_cache (uzbl.gui.web_view);
    } else {
        uzbl_debug ("Unrecognized reload type: %s\n", type);
    }
}

IMPLEMENT_COMMAND (stop)
{
    UZBL_UNUSED (argv);
    UZBL_UNUSED (result);

    webkit_web_view_stop_loading (uzbl.gui.web_view);
}

static gchar *
make_uri_from_user_input (const gchar *uri);

IMPLEMENT_COMMAND (uri)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    if (uzbl_variables_get_int ("frozen")) {
        return;
    }

    gchar *uri = argv_idx (argv, 0);

    g_strstrip (uri);

    /* Don't do anything when given a blank URL. */
    if (!*uri) {
        return;
    }

    g_free (uzbl.state.uri);
    uzbl.state.uri = g_strdup (uri);

    /* Evaluate javascript: URIs. */
    static const char *js_protocol = "javascript:";
    if (g_str_has_prefix (uri, js_protocol)) {
        const gchar *js_code = uri + strlen (js_protocol);
        uzbl_js_run_string_async (JSCTX_PAGE, js_code, NULL, NULL);
        return;
    }

    /* Attempt to parse the URI. */
    gchar *newuri = make_uri_from_user_input (uri);

    webkit_web_view_load_uri (uzbl.gui.web_view, newuri);

    g_free (newuri);
}

IMPLEMENT_COMMAND (download)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *uri = argv_idx (argv, 0);

    WebKitDownload *download = webkit_web_view_download_uri (uzbl.gui.web_view, uri);

    g_object_unref (download);
}

/* Page commands */

IMPLEMENT_COMMAND (load)
{
    UZBL_UNUSED (result);

    if (uzbl_variables_get_int ("frozen")) {
        return;
    }

    ARG_CHECK (argv, 1);

    const gchar *format = argv_idx (argv, 0);

    if (!g_strcmp0 (format, "html")) {
        ARG_CHECK (argv, 3);

        const gchar *content = argv_idx (argv, 1);
        const gchar *baseuri = argv_idx (argv, 2);

        webkit_web_view_load_html (uzbl.gui.web_view, content, baseuri);
    } else if (!g_strcmp0 (format, "text")) {
        ARG_CHECK (argv, 2);

        const gchar *content = argv_idx (argv, 1);

        webkit_web_view_load_plain_text (uzbl.gui.web_view, content);
    } else if (!g_strcmp0 (format, "error_html")) {
        ARG_CHECK (argv, 3);

        const gchar *content = argv_idx (argv, 1);
        const gchar *uri = argv_idx (argv, 2);
        const gchar *baseuri = argv_idx (argv, 3);

        webkit_web_view_load_alternate_html (uzbl.gui.web_view, content, uri, baseuri);
    } else {
        uzbl_debug ("Unrecognized load command: %s\n", format);
    }
}

static void
save_to_file_async_cb (GObject *object, GAsyncResult *res, gpointer data);
static void
save_async_cb (GObject *object, GAsyncResult *res, gpointer data);

IMPLEMENT_COMMAND (save)
{
    UZBL_UNUSED (result);

    const gchar *mode_str = argv_idx (argv, 0);

    if (!mode_str) {
        mode_str = "mhtml";
    }

    WebKitSaveMode mode = WEBKIT_SAVE_MODE_MHTML;

    if (!g_strcmp0 ("mhtml", mode_str)) {
        mode = WEBKIT_SAVE_MODE_MHTML;
    } else {
        uzbl_debug ("Unrecognized save format: %s\n", mode_str);
        return;
    }

    if (1 < argv->len) {
        const gchar *path = argv_idx (argv, 1);
        GFile *gfile = g_file_new_for_path (path);

        webkit_web_view_save_to_file (uzbl.gui.web_view, gfile, mode,
                                      NULL, save_to_file_async_cb, NULL);
    } else {
        webkit_web_view_save (uzbl.gui.web_view, mode,
                              NULL, save_async_cb, NULL);
    }
}

/* Cookie commands */

IMPLEMENT_COMMAND (cookie)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    WebKitCookieManager *manager = webkit_web_context_get_cookie_manager (context);

    if (!g_strcmp0 (command, "add")) {
        uzbl_debug ("Manual cookie additions are unsupported in WebKit2.\n");
    } else if (!g_strcmp0 (command, "delete")) {
        uzbl_debug ("Manual cookie deletions are unsupported in WebKit2.\n");
    } else if (!g_strcmp0 (command, "clear")) {
        ARG_CHECK (argv, 2);

        const gchar *type = argv_idx (argv, 1);

        if (!g_strcmp0 (type, "all")) {
            webkit_cookie_manager_delete_all_cookies (manager);
        } else if (!g_strcmp0 (type, "domain")) {
            guint i;
            for (i = 2; i < argv->len; ++i) {
                const gchar *domain = argv_idx (argv, i);

                webkit_cookie_manager_delete_cookies_for_domain (manager, domain);
            }
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

        if (*end && (*end == '%') && (*(end + 1) == '!')) {
            value = (max_value - lower) * amount * 0.01 + lower;
        } else if (*end == '%') {
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

IMPLEMENT_COMMAND (zoom)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

    gdouble new_zoom = webkit_web_view_get_zoom_level (uzbl.gui.web_view);

    if (!g_strcmp0 (command, "in")) {
        gdouble step;

        if (argv->len < 2) {
            step = uzbl_variables_get_double ("zoom_step");
        } else {
            const gchar *value_str = argv_idx (argv, 1);

            step = strtod (value_str, NULL);
        }

        new_zoom += step;
    } else if (!g_strcmp0 (command, "out")) {
        gdouble step;

        if (argv->len < 2) {
            step = uzbl_variables_get_double ("zoom_step");
        } else {
            const gchar *value_str = argv_idx (argv, 1);

            step = strtod (value_str, NULL);
        }

        new_zoom -= step;
    } else if (!g_strcmp0 (command, "set")) {
        ARG_CHECK (argv, 2);

        const gchar *value_str = argv_idx (argv, 1);

        new_zoom = strtod (value_str, NULL);
    }

    if (new_zoom) {
        webkit_web_view_set_zoom_level (uzbl.gui.web_view, new_zoom);
    }
}

IMPLEMENT_COMMAND (hardcopy)
{
    UZBL_UNUSED (result);

    const gchar *region = argv_idx (argv, 0);

    if (!region) {
        region = "page";
    }

    if (!g_strcmp0 (region, "page")) {
        WebKitPrintOperation *print_op = webkit_print_operation_new (uzbl.gui.web_view);

        /* TODO: Allow control of print operations here? See GtkPageSetup and
         * GtkPrintSettings. */

        WebKitPrintOperationResponse response = webkit_print_operation_run_dialog (print_op, GTK_WINDOW (uzbl.gui.main_window));

        switch (response) {
        case WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL:
            break;
        case WEBKIT_PRINT_OPERATION_RESPONSE_PRINT:
            webkit_print_operation_print (print_op);
            break;
        default:
            uzbl_debug ("Unknown response for a print action; assuming cancel\n");
            break;
        }

        g_object_unref (print_op);
    } else {
        uzbl_debug ("Unrecognized hardcopy region: %s\n", region);
    }
}

IMPLEMENT_COMMAND (geometry)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    gchar *geometry = argv_idx (argv, 0);

    if (geometry[0] == 'm') { /* m/maximize/maximized */
        gtk_window_maximize (GTK_WINDOW (uzbl.gui.main_window));
    } else {
        int x = 0;
        int y = 0;
        unsigned w = 0;
        unsigned h=0;

        /* We used to use gtk_window_parse_geometry () but that didn't work how
         * it was supposed to. */
        int ret = XParseGeometry (geometry, &x, &y, &w, &h);

        if (ret & XValue) {
            gtk_window_move (GTK_WINDOW (uzbl.gui.main_window), x, y);
        }

        if (ret & WidthValue) {
            gtk_window_resize (GTK_WINDOW (uzbl.gui.main_window), w, h);
        }
    }
}

IMPLEMENT_COMMAND (snapshot)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 3);

    cairo_surface_t *surface = NULL;

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
        return;
    }

    WebKitSnapshotOptions options = WEBKIT_SNAPSHOT_OPTIONS_NONE;

    guint sz = argv->len;
    guint i;

    for (i = 3; i < sz; ++i) {
        const gchar *option = argv_idx (argv, i);

        if (!g_strcmp0 (option, "selection")) {
            options |= WEBKIT_SNAPSHOT_OPTIONS_INCLUDE_SELECTION_HIGHLIGHTING;
#if WEBKIT_CHECK_VERSION (2, 7, 4)
        } else if (!g_strcmp0 (option, "transparent")) {
            options |= WEBKIT_SNAPSHOT_OPTIONS_TRANSPARENT_BACKGROUND;
#endif
        } else {
            uzbl_debug ("Unrecognized snapshot option: %s\n", option);
        }
    }

    GError *err = NULL;

#if 0 /* Broken since the call is meant to be async. */
    webkit_web_view_get_snapshot (uzbl.gui.web_view, region, options,
                                  NULL, NULL, NULL);
    surface = webkit_web_view_get_snapshot_finish (uzbl.gui.web_view, NULL, &err);
#else
    (void)region;
#endif

    if (!surface && err) {
        uzbl_debug ("Failed to save snapshot: %s\n", err->message);
        /* TODO: Don't ignore the error. */
        g_error_free (err);
    }

    if (!surface) {
        uzbl_debug ("Failed to create a valid snapshot\n");
        return;
    }

    if (!g_strcmp0 (format, "png")) {
        cairo_surface_write_to_png (surface, path);
    } else {
        uzbl_debug ("Unrecognized snapshot format: %s\n", format);
    }

    cairo_surface_destroy (surface);
}

/* Content commands */

IMPLEMENT_COMMAND (plugin)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

    if (FALSE) {
    } else if (!g_strcmp0 (command, "search")) {
        ARG_CHECK (argv, 2);

        const gchar *directory = argv_idx (argv, 1);
        WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);

        webkit_web_context_set_additional_plugins_directory (context, directory);
    } else {
        uzbl_debug ("Unrecognized plugin command: %s\n", command);
    }
}

IMPLEMENT_COMMAND (cache)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

    if (!g_strcmp0 (command, "clear")) {
        WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);

        webkit_web_context_clear_cache (context);
    } else {
        uzbl_debug ("Unrecognized cache command: %s\n", command);
    }
}

IMPLEMENT_COMMAND (favicon)
{
    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

    WebKitFaviconDatabase *database = NULL;

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    database = webkit_web_context_get_favicon_database (context);

    if (!g_strcmp0 (command, "clear")) {
        webkit_favicon_database_clear (database);
    } else if (!g_strcmp0 (command, "uri")) {
        ARG_CHECK (argv, 2);

        const gchar *uri = argv_idx (argv, 1);

        gchar *favicon_uri = webkit_favicon_database_get_favicon_uri (database, uri);

        g_string_append (result, favicon_uri);

        g_free (favicon_uri);
    } else if (!g_strcmp0 (command, "save")) {
        /* TODO: Implement. */
    } else {
        uzbl_debug ("Unrecognized favicon command: %s\n", command);
    }
}

IMPLEMENT_COMMAND (css)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

#if WEBKIT_CHECK_VERSION (2, 5, 1)
#define HAS_USER_CONTENT_MANAGER
    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager (uzbl.gui.web_view);
#else
    WebKitWebViewGroup *group = webkit_web_view_get_group (uzbl.gui.web_view);
#endif

    if (!g_strcmp0 (command, "add")) {
        ARG_CHECK (argv, 2);

        const gchar *uri = argv_idx (argv, 1);

        const gchar *where = argv->len >= 3 ? argv_idx (argv, 2) : "";
#ifdef HAS_USER_CONTENT_MANAGER
        const gchar *level = argv->len >= 4 ? argv_idx (argv, 3) : "";
#endif

#ifndef HAS_USER_CONTENT_MANAGER
        typedef WebKitInjectedContentFrames WebKitUserContentInjectedFrames;
#define WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES WEBKIT_INJECTED_CONTENT_FRAMES_ALL
#define WEBKIT_USER_CONTENT_INJECT_TOP_FRAME WEBKIT_INJECTED_CONTENT_FRAMES_TOP_ONLY
#endif

        WebKitUserContentInjectedFrames frames = WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES;
#ifdef HAS_USER_CONTENT_MANAGER
        WebKitUserStyleLevel style_level = WEBKIT_USER_STYLE_LEVEL_USER;
#endif

        if (!g_strcmp0 (where, "all")) {
            frames = WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES;
        } else if (!g_strcmp0 (where, "top_only")) {
            frames = WEBKIT_USER_CONTENT_INJECT_TOP_FRAME;
        } else if (*where) {
            uzbl_debug ("Unrecognized frame target: %s\n", where);
        }

#ifdef HAS_USER_CONTENT_MANAGER
        if (!g_strcmp0 (level, "user")) {
            style_level = WEBKIT_USER_STYLE_LEVEL_USER;
        } else if (!g_strcmp0 (level, "author")) {
            style_level = WEBKIT_USER_STYLE_LEVEL_AUTHOR;
        } else if (*level) {
            uzbl_debug ("Unrecognized style sheet level: %s\n", level);
        }
#else
        const gchar *baseuri = argv->len >= 4 ? argv_idx (argv, 3) : NULL;
#endif

        const gchar *whitelist = argv->len >= 5 ? argv_idx (argv, 4) : NULL;
        const gchar *blacklist = argv->len >= 6 ? argv_idx (argv, 5) : NULL;

#ifndef HAS_USER_CONTENT_MANAGER
        if (baseuri && !*baseuri) {
            baseuri = NULL;
        }
#endif
        if (whitelist && !*whitelist) {
            whitelist = NULL;
        }
        if (blacklist && !*blacklist) {
            blacklist = NULL;
        }

        gchar **whitelist_list = NULL;
        gchar **blacklist_list = NULL;

        if (whitelist) {
            whitelist_list = g_strsplit (whitelist, ",", 0);
        }

        if (blacklist) {
            blacklist_list = g_strsplit (blacklist, ",", 0);
        }

#ifdef HAS_USER_CONTENT_MANAGER
        WebKitUserStyleSheet *sheet = webkit_user_style_sheet_new (
            uri,
            frames,
            style_level,
            (const gchar * const *)whitelist_list,
            (const gchar * const *)blacklist_list);
        webkit_user_content_manager_add_style_sheet (manager, sheet);
        webkit_user_style_sheet_unref (sheet);
#else
        webkit_web_view_group_add_user_style_sheet (group,
            uri,
            baseuri,
            (const gchar * const *)whitelist_list,
            (const gchar * const *)blacklist_list,
            frames);
#endif

        if (whitelist_list) {
            g_strfreev (whitelist_list);
        }
        if (blacklist_list) {
            g_strfreev (blacklist_list);
        }
    } else if (!g_strcmp0 (command, "clear")) {
#ifdef HAS_USER_CONTENT_MANAGER
        webkit_user_content_manager_remove_all_style_sheets (manager);
#else
        webkit_web_view_group_remove_all_user_style_sheets (group);
#endif
    } else {
        uzbl_debug ("Unrecognized css command: %s\n", command);
    }
}

#if WEBKIT_CHECK_VERSION (2, 7, 2)
static void
script_message_callback (WebKitUserContentManager *manager, WebKitJavascriptResult *res, gpointer data);

IMPLEMENT_COMMAND (script)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

    WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager (uzbl.gui.web_view);

    if (!g_strcmp0 (command, "add")) {
        ARG_CHECK (argv, 2);

        const gchar *uri = argv_idx (argv, 1);

        const gchar *where = argv->len >= 3 ? argv_idx (argv, 2) : "";
        const gchar *location = argv->len >= 4 ? argv_idx (argv, 3) : "";

        WebKitUserContentInjectedFrames frames = WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES;
        WebKitUserScriptInjectionTime when = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START;

        if (!g_strcmp0 (where, "all")) {
            frames = WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES;
        } else if (!g_strcmp0 (where, "top_only")) {
            frames = WEBKIT_USER_CONTENT_INJECT_TOP_FRAME;
        } else if (*where) {
            uzbl_debug ("Unrecognized frame target: %s\n", where);
        }

        if (!g_strcmp0 (location, "start")) {
            when = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START;
        } else if (!g_strcmp0 (location, "end")) {
            when = WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END;
        } else if (*location) {
            uzbl_debug ("Unrecognized script injection location: %s\n", location);
        }

        const gchar *whitelist = argv->len >= 5 ? argv_idx (argv, 4) : NULL;
        const gchar *blacklist = argv->len >= 6 ? argv_idx (argv, 5) : NULL;

        if (whitelist && !*whitelist) {
            whitelist = NULL;
        }
        if (blacklist && !*blacklist) {
            blacklist = NULL;
        }

        gchar **whitelist_list = NULL;
        gchar **blacklist_list = NULL;

        if (whitelist) {
            whitelist_list = g_strsplit (whitelist, ",", 0);
        }

        if (blacklist) {
            blacklist_list = g_strsplit (blacklist, ",", 0);
        }

        WebKitUserScript *script = webkit_user_script_new (
            uri,
            frames,
            when,
            (const gchar * const *)whitelist_list,
            (const gchar * const *)blacklist_list);
        webkit_user_content_manager_add_script (manager, script);
        webkit_user_script_unref (script);

        if (whitelist_list) {
            g_strfreev (whitelist_list);
        }
        if (blacklist_list) {
            g_strfreev (blacklist_list);
        }
    } else if (!g_strcmp0 (command, "clear")) {
        webkit_user_content_manager_remove_all_scripts (manager);
#if WEBKIT_CHECK_VERSION (2, 7, 2)
    } else if (!g_strcmp0 (command, "listen")) {
        ARG_CHECK (argv, 2);

        const gchar *name = argv_idx (argv, 1);

        UzblScriptHandlerData *handler_data = g_hash_table_lookup (uzbl.commands->script_handler_data_table, name);
        if (handler_data) {
            uzbl_debug ("Removing old script message handler for %s\n", name);
            g_hash_table_remove (uzbl.commands->script_handler_data_table, name);
        }

        handler_data = g_malloc0 (sizeof (UzblScriptHandlerData));
        handler_data->name = g_strdup (name);

        gchar *signal_name = g_strdup_printf ("signal::script-message-handler::%s",
            name);
        g_object_connect (G_OBJECT (manager),
            signal_name, G_CALLBACK (script_message_callback), handler_data,
            NULL);
        g_free (signal_name);

        webkit_user_content_manager_register_script_message_handler (manager, name);

        g_hash_table_insert (uzbl.commands->script_handler_data_table, g_strdup (name), handler_data);
    } else if (!g_strcmp0 (command, "ignore")) {
        ARG_CHECK (argv, 2);

        const gchar *name = argv_idx (argv, 1);

        UzblScriptHandlerData *handler_data = g_hash_table_lookup (uzbl.commands->script_handler_data_table, name);
        if (!handler_data) {
            uzbl_debug ("No script message handler '%s' to ignore\n", name);
            return;
        }

        webkit_user_content_manager_unregister_script_message_handler (manager, name);

        gchar *signal_name = g_strdup_printf ("signal::script-message-handler::%s",
            name);
        g_object_disconnect (G_OBJECT (manager),
            signal_name, G_CALLBACK (script_message_callback), handler_data,
            NULL);
        g_free (signal_name);

        g_hash_table_remove (uzbl.commands->script_handler_data_table, name);
#endif
    } else {
        uzbl_debug ("Unrecognized script command: %s\n", command);
    }
}
#endif

IMPLEMENT_COMMAND (scheme)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    gchar **split = g_strsplit (argv_idx (argv, 0), " ", 2);

    gchar *scheme = split[0];
    gchar *command = split[1];

    if (scheme && command) {
        uzbl_scheme_add_handler (g_strstrip (scheme), g_strchug (command));
    }

    g_strfreev (split);
}

static void
free_menu_item (gpointer item);

/* Menu commands */

IMPLEMENT_COMMAND (menu)
{
    if (!uzbl.gui.menu_items) {
        uzbl.gui.menu_items = g_ptr_array_new_with_free_func (free_menu_item);
    }

    ARG_CHECK (argv, 1);

    const gchar *action_str = argv_idx (argv, 0);

    typedef enum {
        ACTION_ADD,
        ACTION_REMOVE,
        ACTION_QUERY,

        ACTION_NONE
    } MenuAction;

    MenuAction action = ACTION_NONE;
    gboolean is_sep = FALSE;

    if (!g_strcmp0 (action_str, "add")) {
        action = ACTION_ADD;
    } else if (!g_strcmp0 (action_str, "add_separator")) {
        action = ACTION_ADD;
        is_sep = TRUE;
    } else if (!g_strcmp0 (action_str, "remove")) {
        action = ACTION_REMOVE;
    } else if (!g_strcmp0 (action_str, "query")) {
        action = ACTION_QUERY;
    } else if (!g_strcmp0 (action_str, "list")) {
        if (!result) {
            return;
        }

        guint i = 0;
        gboolean need_comma = FALSE;
        UzblMenuItem *mi = NULL;

        g_string_append_c (result, '[');

        for (i = 0; i < uzbl.gui.menu_items->len; ++i) {
            mi = g_ptr_array_index (uzbl.gui.menu_items, i);

            if (need_comma) {
                g_string_append_c (result, ',');
            }

            g_string_append_c (result, '\"');
            g_string_append (result, mi->name);
            g_string_append_c (result, '\"');
            need_comma = TRUE;
        }

        g_string_append_c (result, ']');
    } else {
        uzbl_debug ("Unrecognized menu action: %s\n", action_str);;
        return;
    }

    ARG_CHECK (argv, 2);

    gboolean is_remove = (action == ACTION_REMOVE);
    gboolean is_query = (action == ACTION_QUERY);

    if (action == ACTION_ADD) {
        ARG_CHECK (argv, 3);

        const gchar *object = argv_idx (argv, 1);
        const gchar *name = argv_idx (argv, 2);
        WebKitHitTestResultContext context;

        if (!g_strcmp0 (object, "document")) {
            context = WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT;
        } else if (!g_strcmp0 (object, "link")) {
            context = WEBKIT_HIT_TEST_RESULT_CONTEXT_LINK;
        } else if (!g_strcmp0 (object, "image")) {
            context = WEBKIT_HIT_TEST_RESULT_CONTEXT_IMAGE;
        } else if (!g_strcmp0 (object, "media")) {
            context = WEBKIT_HIT_TEST_RESULT_CONTEXT_MEDIA;
        } else if (!g_strcmp0 (object, "editable")) {
            context = WEBKIT_HIT_TEST_RESULT_CONTEXT_EDITABLE;
        } else if (!g_strcmp0 (object, "scrollbar")) {
            context = WEBKIT_HIT_TEST_RESULT_CONTEXT_SCROLLBAR;
#if WEBKIT_CHECK_VERSION (2, 7, 1)
        } else if (!g_strcmp0 (object, "selection")) {
            context = WEBKIT_HIT_TEST_RESULT_CONTEXT_SELECTION;
#endif
        } else {
            uzbl_debug ("Unrecognized menu object: %s\n", object);;
            return;
        }

        const gchar *command = NULL;

        if (!is_sep) {
            command = argv_idx (argv, 3);
        }

        UzblMenuItem *mi = (UzblMenuItem *)malloc (sizeof (UzblMenuItem));
        mi->name = g_strdup (name);
        mi->cmd = g_strdup (command ? command : "");
        mi->context = context;
        mi->issep = is_sep;

        g_ptr_array_add (uzbl.gui.menu_items, mi);
    } else if (is_remove || (is_query && result)) {
        ARG_CHECK (argv, 2);

        const gchar *name = argv_idx (argv, 1);

        guint i = 0;
        UzblMenuItem *mi = NULL;

        for (i = uzbl.gui.menu_items->len; i; --i) {
            mi = g_ptr_array_index (uzbl.gui.menu_items, i - 1);

            if (!g_strcmp0 (name, mi->name)) {
                if (is_query) {
                    if (mi->cmd) {
                        g_string_append (result, mi->cmd);
                    }

                    return;
                } else if (is_remove) {
                    g_ptr_array_remove_index (uzbl.gui.menu_items, i - 1);
                }
            }
        }
    }
}

/* Search commands */

typedef enum {
    OPTION_NONE,
    OPTION_SET,
    OPTION_UNSET,
    OPTION_TOGGLE,
    OPTION_DEFAULT
} UzblFlagOperation;

IMPLEMENT_COMMAND (search)
{
    ARG_CHECK (argv, 1);

    const gchar *full_command = argv_idx (argv, 0);

    gchar **tokens = g_strsplit (full_command, " ", 2);

    const gchar *command = tokens[0];
    const gchar *arg_string = tokens[1];

    static const WebKitFindOptions default_options = WEBKIT_FIND_OPTIONS_WRAP_AROUND;

#define webkit2_search_options(call)                        \
    call ("word_start", WEBKIT_FIND_OPTIONS_AT_WORD_STARTS) \
    call ("camel_case", WEBKIT_FIND_OPTIONS_TREAT_MEDIAL_CAPITAL_AS_WORD_START)

#define search_options(call)                       \
    webkit2_search_options(call)                   \
    call ("wrap", WEBKIT_FIND_OPTIONS_WRAP_AROUND) \
    call ("case_insensitive", WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE)

    WebKitFindController *finder = webkit_web_view_get_find_controller (uzbl.gui.web_view);
    gboolean reset = FALSE;

    if (!g_strcmp0 (command, "option")) {
        if (!arg_string) {
            goto search_exit;
        }

        gchar **options = g_strsplit (arg_string, " ", 0);
        gchar **option_iter = options;

        while (*option_iter) {
            const gchar *option_str = *option_iter++;
            UzblFlagOperation mode = OPTION_NONE;
            WebKitFindOptions option = WEBKIT_FIND_OPTIONS_NONE;

            switch (*option_str) {
            case '+':
                mode = OPTION_SET;
                break;
            case '-':
                mode = OPTION_UNSET;
                break;
            case '!':
                mode = OPTION_TOGGLE;
                break;
            case '~':
                mode = OPTION_DEFAULT;
                break;
            case '\0':
                continue;
                default:
                    break;
            }

            if (mode == OPTION_NONE) {
                mode = OPTION_SET;
            } else {
                ++option_str;
            }

#define check_string(name, flag)           \
    } if (!g_strcmp0 (option_str, name)) { \
        option = flag;

            /* TODO: Implement limit= option? */
            if (false) {
            search_options(check_string)
            }

#undef check_string

            if (mode == OPTION_DEFAULT) {
                if (option & default_options) {
                    mode = OPTION_SET;
                } else {
                    mode = OPTION_UNSET;
                }
            }

            switch (mode) {
            case OPTION_SET:
                uzbl.commands->search_options |= option;
                break;
            case OPTION_UNSET:
                uzbl.commands->search_options &= ~option;
                break;
            case OPTION_TOGGLE:
                uzbl.commands->search_options ^= option;
                break;
            case OPTION_NONE:
            case OPTION_DEFAULT:
            default:
                break;
            }
        }

        g_strfreev (options);

        if (uzbl.commands->search_text && (uzbl.commands->search_options != uzbl.commands->search_options_last)) {
            uzbl.commands->search_options_last = uzbl.commands->search_options;

            webkit_find_controller_search (finder, uzbl.commands->search_text, uzbl.commands->search_options, G_MAXUINT);
        }
    } else if (!g_strcmp0 (command, "options")) {
        if (!result) {
            return;
        }

        gboolean need_comma = FALSE;

        g_string_append_c(result, '[');

#define check_flag(name, flag)                  \
    if (uzbl.commands->search_options & flag) { \
        if (need_comma) {                       \
            g_string_append_c (result, ',');    \
        }                                       \
        g_string_append_c (result, '\"');       \
        g_string_append (result, name);         \
        g_string_append_c (result, '\"');       \
        need_comma = TRUE;                      \
    }

        search_options(check_flag)

#undef check_flag

        g_string_append_c(result, ']');
    } else if (!g_strcmp0 (command, "clear")) {
        reset = TRUE;
    } else if (!g_strcmp0 (command, "reset")) {
        reset = TRUE;

        g_free (uzbl.commands->search_text);
        uzbl.commands->search_text = NULL;

        uzbl.commands->search_options = default_options;
    } else if (!g_strcmp0 (command, "find") || !g_strcmp0 (command, "rfind")) {
        const gchar *key = arg_string;

        if (!key) {
            /* Stop if there is no search string. */
            goto search_exit;
        }

        if (!uzbl.commands->search_text) {
            uzbl.commands->search_text = g_strdup ("");
        }

        if (*key) {
            if (g_strcmp0 (key, uzbl.commands->search_text)) {
                g_free (uzbl.commands->search_text);
                uzbl.commands->search_text = g_strdup (key);
            }
        } else {
            /* On an empty search, use the previous search. */
            key = uzbl.commands->search_text;
        }

        if (uzbl.commands->search_options != uzbl.commands->search_options_last) {
            uzbl.commands->search_options_last = uzbl.commands->search_options;
        }

        gboolean forward = TRUE;

        if (*command == 'r') {
            forward = FALSE;
        }

        uzbl.commands->search_forward = forward;

        WebKitFindOptions options = uzbl.commands->search_options;

        if (!forward) {
            options |= WEBKIT_FIND_OPTIONS_BACKWARDS;
        }

        webkit_find_controller_search (finder, key, options, G_MAXUINT);
    } else if (!g_strcmp0 (command, "next")) {
        if (!uzbl.commands->search_text) {
            goto search_exit;
        }

        if (uzbl.commands->search_options != uzbl.commands->search_options_last) {
            uzbl.commands->search_options_last = uzbl.commands->search_options;
        }

        if (uzbl.commands->search_forward) {
            webkit_find_controller_search_next (finder);
        } else {
            webkit_find_controller_search_previous (finder);
        }
    } else if (!g_strcmp0 (command, "prev")) {
        if (!uzbl.commands->search_text) {
            goto search_exit;
        }

        if (uzbl.commands->search_options != uzbl.commands->search_options_last) {
            uzbl.commands->search_options_last = uzbl.commands->search_options;
        }

        if (uzbl.commands->search_forward) {
            webkit_find_controller_search_previous (finder);
        } else {
            webkit_find_controller_search_next (finder);
        }
    } else {
        uzbl_debug ("Unrecognized search command: %s\n", command);
    }

search_exit:
    g_strfreev (tokens);

    if (reset) {
        webkit_find_controller_search_finish (finder);
        uzbl.commands->search_options = WEBKIT_FIND_OPTIONS_WRAP_AROUND;
    }

#undef webkit2_search_options
#undef search_options
}

/* Security commands */

IMPLEMENT_COMMAND (security)
{
    ARG_CHECK (argv, 3);

    const gchar *scheme = argv_idx (argv, 0);
    const gchar *command = argv_idx (argv, 1);
    const gchar *option = argv_idx (argv, 2);

    typedef gboolean (*UzblGetSecurityOption) (WebKitSecurityManager *manager, const gchar *scheme);
    typedef void     (*UzblSetSecurityOption) (WebKitSecurityManager *manager, const gchar *scheme);

    typedef struct {
        const gchar           *name;
        UzblGetSecurityOption  get;
        UzblSetSecurityOption  set;
    } UzblSecurityField;

#define SECURITY_FIELD(name, val) \
    { #name, webkit_security_manager_uri_scheme_is_##name, webkit_security_manager_register_uri_scheme_as_##name }

    static const UzblSecurityField
    fields[] = {
        SECURITY_FIELD (local,            LOCAL),
        SECURITY_FIELD (no_access,        NO_ACCESS_TO_OTHER_SCHEME),
        SECURITY_FIELD (display_isolated, DISPLAY_ISOLATED),
        SECURITY_FIELD (secure,           SECURE),
        SECURITY_FIELD (cors_enabled,     CORS_ENABLED),
        SECURITY_FIELD (empty_document,   EMPTY_DOCUMENT),
        { NULL, NULL, NULL }
    };

#undef SECURITY_FIELD

    const UzblSecurityField *field = &fields[0];

    while (field->name) {
        if (!g_strcmp0 (field->name, option)) {
            break;
        }

        ++field;
    }

    if (!field->name) {
        uzbl_debug ("Unrecognized option: %s\n", option);
    }

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    WebKitSecurityManager *manager = webkit_web_context_get_security_manager (context);

    if (!g_strcmp0 (command, "get")) {
        if (!result) {
            return;
        }

        gboolean set;
        set = field->get (manager, scheme);

        g_string_append (result, set ? "true" : "false");
    } else if (!g_strcmp0 (command, "set")) {
        field->set (manager, scheme);
    }
}

IMPLEMENT_COMMAND (dns)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

    if (!g_strcmp0 (command, "fetch")) {
        ARG_CHECK (argv, 2);

        const gchar *hostname = argv_idx (argv, 1);

        WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);

        webkit_web_context_prefetch_dns (context, hostname);
    } else {
        uzbl_debug ("Unrecognized dns command: %s\n", command);
    }
}

/* Inspector commands */

IMPLEMENT_COMMAND (inspector)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *command = argv_idx (argv, 0);

    if (!g_strcmp0 (command, "show")) {
        webkit_web_inspector_show (uzbl.gui.inspector);
    } else if (!g_strcmp0 (command, "close")) {
        webkit_web_inspector_close (uzbl.gui.inspector);
    } else if (!g_strcmp0 (command, "attach")) {
#if WEBKIT_CHECK_VERSION (2, 7, 1)
        if (webkit_web_inspector_get_can_attach (uzbl.gui.inspector)) {
            webkit_web_inspector_attach (uzbl.gui.inspector);
        } else {
            uzbl_debug ("Not enough space to attach the inspector to the window\n");
        }
#else
        webkit_web_inspector_attach (uzbl.gui.inspector);
#endif
    } else if (!g_strcmp0 (command, "detach")) {
        webkit_web_inspector_detach (uzbl.gui.inspector);
    } else {
        uzbl_debug ("Unrecognized inspector command: %s\n", command);
    }
}

/* Execution commands */

static void
run_js_cb (GObject      *source,
           GAsyncResult *result,
           gpointer      data);

IMPLEMENT_TASK (js)
{
    TASK_ARG_CHECK (task, argv, 3);

    const gchar *context = argv_idx (argv, 0);
    const gchar *where = argv_idx (argv, 1);
    const gchar *value = argv_idx (argv, 2);

    UzblJSContext jsctx;

    if (!g_strcmp0 (context, "uzbl")) {
        jsctx = JSCTX_UZBL;
    } else if (!g_strcmp0 (context, "clean")) {
        jsctx = JSCTX_CLEAN;
    } else if (!g_strcmp0 (context, "page")) {
        jsctx = JSCTX_PAGE;
    } else {
        uzbl_debug ("Unrecognized js context: %s", context);
        g_task_return_pointer (task, NULL, NULL);  // TODO: Error
        g_object_unref (task);
        return;
    }

    if (!g_strcmp0 (where, "string")) {
        uzbl_js_run_string_async (jsctx, value, run_js_cb, task);
    } else if (!g_strcmp0 (where, "file")) {
        GArray jsargs = {
            .data = (gchar*)&g_array_index (argv, gchar*, 3),
            .len = argv->len - 3
        };
        const gchar *req_path = value;
        gchar *path;
        if ((path = find_existing_file (req_path))) {
            uzbl_js_run_file_async (jsctx, path, &jsargs, run_js_cb, task);
        } else {
            g_task_return_pointer (task, NULL, NULL);
            g_object_unref (task);
        }
    } else {
        uzbl_debug ("Unrecognized code source: %s", where);
        g_task_return_pointer (task, NULL, NULL);  // TODO: Error
        g_object_unref (task);
        return;
    }

}

void
run_js_cb (GObject      *source,
           GAsyncResult *result,
           gpointer      data)
{
    GTask *task = G_TASK (data);
    GError *err = NULL;
    gchar *result_utf8 = uzbl_js_run_finish (source, result, &err);

    if (err) {
        g_task_return_error (task, err);
        g_object_unref (task);
        return;
    }

    GString *str = (GString*) g_task_get_task_data (task);
    if (result_utf8) {
        if (str && g_strcmp0 (result_utf8, "[object Object]")) {
            g_string_append (str, result_utf8);
        }

        g_free (result_utf8);
    }
    g_task_return_pointer (task, NULL, NULL);
    g_object_unref (task);
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
    if (!result) {
        GString *force_result = g_string_new ("");
        spawn (argv, force_result, FALSE);
        g_string_free (force_result, TRUE);
    } else {
        spawn (argv, result, FALSE);
    }
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
    while ((cmd = argv_idx (argv, i++))) {
        GString *res = NULL;

        if (result) {
            res = g_string_new ("");
        }

        uzbl_commands_run (cmd, res);

        if (result) {
            g_string_append (result, res->str);
            g_string_free (res, TRUE);
        }
    }
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

    uzbl.state.exit = TRUE;

    if (!uzbl.state.started) {
        uzbl_debug ("Exit called before uzbl is initialized?");
        return;
    }

    /* Hide window a soon as possible to avoid getting stuck with a
     * non-response window in the cleanup steps. */
    if (uzbl.gui.main_window) {
        gtk_widget_destroy (uzbl.gui.main_window);
    } else if (uzbl.gui.plug) {
        gtk_widget_destroy (GTK_WIDGET (uzbl.gui.plug));
    }

    if (uzbl.state.gtk_started) {
        gtk_main_quit ();
    }

    /* Stop the I/O thread. */
    uzbl_io_quit ();
}

/* Variable commands */

IMPLEMENT_COMMAND (set)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    gchar **split = g_strsplit (argv_idx (argv, 0), " ", 2);

    gchar *var = split[0];
    gchar *val = split[1];

    if (var) {
        gchar *value = val ? g_strchug (val) : "";
        uzbl_variables_set (g_strstrip (var), value);
    }
    g_strfreev (split);
}

IMPLEMENT_COMMAND (toggle)
{
    UZBL_UNUSED (result);

    ARG_CHECK (argv, 1);

    const gchar *var_name = argv_idx (argv, 0);

    GArray *toggle_args = uzbl_commands_args_new ();

    guint i;
    for (i = 1; i < argv->len; ++i) {
        const gchar *option = argv_idx (argv, i);
        uzbl_commands_args_append (toggle_args, g_strdup (option));
    }

    uzbl_variables_toggle (var_name, toggle_args);

    uzbl_commands_args_free (toggle_args);
}

IMPLEMENT_COMMAND (print)
{
    ARG_CHECK (argv, 1);

    gchar *buf;

    if (!result) {
        return;
    }

    buf = uzbl_variables_expand (argv_idx (argv, 0));
    g_string_append (result, buf);
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

    ARG_CHECK (argv, 1);

    gchar **split = g_strsplit (argv_idx (argv, 0), " ", 2);

    const gchar *event = split[0];
    const gchar *arg_string = split[1];

    if (event) {
        GString *event_name = g_string_ascii_up (g_string_new (event));

        uzbl_events_send (USER_EVENT, event_name->str,
            TYPE_FORMATTEDSTR, arg_string ? arg_string : "",
            NULL);

        g_string_free (event_name, TRUE);
    }

    g_strfreev (split);
}

static void
make_request (gint64 timeout, GArray *argv, GString *result);

IMPLEMENT_COMMAND (choose)
{
    make_request (-1, argv, result);
}

IMPLEMENT_COMMAND (request)
{
    make_request (1, argv, result);
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

void
save_to_file_async_cb (GObject *object, GAsyncResult *res, gpointer data)
{
    UZBL_UNUSED (data);

    WebKitWebView *view = WEBKIT_WEB_VIEW (object);
    GError *err = NULL;

    webkit_web_view_save_to_file_finish (view, res, &err);

    if (err) {
        uzbl_debug ("Failed to save page to file: %s\n", err->message);
        g_error_free (err);
    }
}

void
save_async_cb (GObject *object, GAsyncResult *res, gpointer data)
{
    UZBL_UNUSED (data);

    WebKitWebView *view = WEBKIT_WEB_VIEW (object);
    GError *err = NULL;

    GInputStream *stream = webkit_web_view_save_finish (view, res, &err);

    if (!stream) {
        uzbl_debug ("Failed to save page: %s\n", err->message);
        g_error_free (err);
        return;
    }

    /* TODO: What to do here? */

    g_object_unref (stream);
}

#if WEBKIT_CHECK_VERSION (2, 5, 1)
void
script_message_callback (WebKitUserContentManager *manager, WebKitJavascriptResult *res, gpointer data)
{
    UZBL_UNUSED (manager);

    UzblScriptHandlerData *handler_data = (UzblScriptHandlerData *)data;

    JSGlobalContextRef ctx = webkit_javascript_result_get_global_context (res);
    JSValueRef res_val = webkit_javascript_result_get_value (res);
    gchar *res_str = uzbl_js_to_string (ctx, res_val);

    uzbl_events_send (SCRIPT_MESSAGE, NULL,
        TYPE_STR, handler_data->name,
        TYPE_STR, res_str,
        NULL);

    g_free (res_str);
}
#endif

void
free_menu_item (gpointer data)
{
    UzblMenuItem *item = (UzblMenuItem *)data;

    g_free (item->name);
    g_free (item->cmd);

    g_free (item);
}

/* Make sure that the args string you pass can properly be interpreted (e.g.,
 * properly escaped against whitespace, quotes etc.). */
static gboolean
run_system_command (GArray *args, char **output_stdout);

void
spawn (GArray *argv, GString *result, gboolean exec)
{
    ARG_CHECK (argv, 1);

    const gchar *req_path = argv_idx (argv, 0);

    gchar *path = find_existing_file (req_path);

    if (!path) {
        /* Assume it's a valid command. */
        path = g_strdup (req_path);
    }

    GArray *args = uzbl_commands_args_new ();

    uzbl_commands_args_append (args, path);

    guint i;
    for (i = 1; i < argv->len; ++i) {
        const gchar *arg = argv_idx (argv, i);
        uzbl_commands_args_append (args, g_strdup (arg));
    }

    gchar *r = NULL;
    run_system_command (args, result ? &r : NULL);
    if (result && r) {
        g_string_append (result, r);
        if (exec) {
            /* Run each line of output from the program as a command. */
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
    uzbl_commands_args_free (args);
}

void
spawn_sh (GArray *argv, GString *result)
{
    gchar *shell = uzbl_variables_get_string ("shell_cmd");

    if (!*shell) {
        uzbl_debug ("spawn_sh: shell_cmd is not set!\n");
        g_free (shell);
        return;
    }
    guint i;

    GArray *sh_cmd = split_quoted (shell);
    g_free (shell);
    if (!sh_cmd) {
        return;
    }

    for (i = 0; i < argv->len; ++i) {
        const gchar *arg = argv_idx (argv, i);
        uzbl_commands_args_append (sh_cmd, g_strdup (arg));
    }

    gchar *r = NULL;
    run_system_command (sh_cmd, result ? &r : NULL);
    if (result && r) {
        remove_trailing_newline (r);
        g_string_append (result, r);
    }

    g_free (r);
    uzbl_commands_args_free (sh_cmd);
}

void
make_request (gint64 timeout, GArray *argv, GString *result)
{
    GString *request_name;
    GString *request_result;
    gchar **split = NULL;

    ARG_CHECK (argv, 1);

    const gchar *request = argv_idx (argv, 0);

    request_name = g_string_ascii_up (g_string_new (request));

    GArray *req_args = uzbl_commands_args_new ();

    guint i;
    for (i = 1; i < argv->len; ++i) {
        uzbl_commands_args_append (req_args, g_strdup (argv_idx (argv, i)));
    }

    request_result = uzbl_requests_send (timeout, request_name->str,
        TYPE_STR_ARRAY, req_args,
        NULL);

    uzbl_commands_args_free (req_args);

    g_string_free (request_name, TRUE);
    g_strfreev (split);

    g_string_append (result, request_result->str);
    g_string_free (request_result, TRUE);
}

gboolean
string_is_integer (const char *s)
{
    /* Is the given string made up entirely of decimal digits? */
    return (strspn (s, "0123456789") == strlen (s));
}

gboolean
run_system_command (GArray *args, char **output_stdout)
{
    GError *err = NULL;

    gboolean result;
    if (output_stdout) {
        result = g_spawn_sync (NULL, (gchar **)args->data, NULL, G_SPAWN_SEARCH_PATH,
                               NULL, NULL, output_stdout, NULL, NULL, &err);
        if (!result) {
            *output_stdout = g_strdup ("");
        }
    } else {
        result = g_spawn_async (NULL, (gchar **)args->data, NULL, G_SPAWN_SEARCH_PATH,
                                NULL, NULL, NULL, &err);
    }

    if (uzbl_variables_get_int ("verbose")) {
        GString *s = g_string_new ("spawned:");
        guint i;
        for (i = 0; i < args->len; ++i) {
            gchar *qarg = g_shell_quote (argv_idx (args, i));
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

    return result;
}
