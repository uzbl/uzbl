#include "variables.h"

#include "commands.h"
#include "events.h"
#include "gui.h"
#include "io.h"
#include "js.h"
#include "sync.h"
#include "type.h"
#include "util.h"
#include "comm.h"
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
 *   - Add CWORD variable (range of word under cursor).
 *   - Replace selection (frame).
 */

typedef void (*UzblFunction) ();

typedef union {
    int                *i;
    gdouble            *d;
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

struct _UzblVariablesPrivate;
typedef struct _UzblVariablesPrivate UzblVariablesPrivate;

struct _UzblVariables {
    /* Table of all variables commands. */
    GHashTable *table;

    /* All builtin variable storage is in here. */
    UzblVariablesPrivate *priv;
};

/* =========================== PUBLIC API =========================== */

static UzblVariablesPrivate *
uzbl_variables_private_new (GHashTable *table);
static void
uzbl_variables_private_free (UzblVariablesPrivate *priv);
static void
variable_free (UzblVariable *variable);
static void
init_js_variables_api ();

void
uzbl_variables_init ()
{
    uzbl.variables = g_malloc (sizeof (UzblVariables));

    uzbl.variables->table = g_hash_table_new_full (g_str_hash, g_str_equal,
        g_free, (GDestroyNotify)variable_free);

    uzbl.variables->priv = uzbl_variables_private_new (uzbl.variables->table);

    init_js_variables_api ();
}

void
uzbl_variables_free ()
{
    g_hash_table_destroy (uzbl.variables->table);

    uzbl_variables_private_free (uzbl.variables->priv);

    g_free (uzbl.variables);
    uzbl.variables = NULL;
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
static gboolean
set_variable_string (UzblVariable *var, const gchar *val);
static gboolean
set_variable_int (UzblVariable *var, int i);
static gboolean
set_variable_ull (UzblVariable *var, unsigned long long ull);
static gboolean
set_variable_double (UzblVariable *var, gdouble d);
static void
send_variable_event (const gchar *name, const UzblVariable *var);

gboolean
uzbl_variables_set (const gchar *name, gchar *val)
{
    if (!val) {
        return FALSE;
    }

    UzblVariable *var = get_variable (name);
    gboolean sendev = TRUE;

    if (var) {
        if (!var->writeable) {
            return FALSE;
        }

        switch (var->type) {
        case TYPE_STR:
            sendev = set_variable_string (var, val);
            break;
        case TYPE_INT:
        {
            int i = (int)strtol (val, NULL, 10);
            sendev = set_variable_int (var, i);
            break;
        }
        case TYPE_ULL:
        {
            unsigned long long ull = g_ascii_strtoull (val, NULL, 10);
            sendev = set_variable_ull (var, ull);
            break;
        }
        case TYPE_DOUBLE:
        {
            gdouble d = g_ascii_strtod (val, NULL);
            sendev = set_variable_double (var, d);
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

        g_hash_table_insert (uzbl.variables->table,
            g_strdup (name), (gpointer)var);

        /* Set the value. */
        *(var->value.s) = g_strdup (val);
    }

    if (sendev) {
        send_variable_event (name, var);
    }

    return TRUE;
}

static gchar *
get_variable_string (const UzblVariable *var);
static int
get_variable_int (const UzblVariable *var);
static unsigned long long
get_variable_ull (const UzblVariable *var);
static gdouble
get_variable_double (const UzblVariable *var);

gboolean
uzbl_variables_toggle (const gchar *name, GArray *values)
{
    UzblVariable *var = get_variable (name);

    if (!var) {
        if (values && values->len) {
            return uzbl_variables_set (name, argv_idx (values, 0));
        } else {
            return uzbl_variables_set (name, "1");
        }

        return FALSE;
    }

    gboolean sendev = TRUE;

    switch (var->type) {
    case TYPE_STR:
{
        const gchar *next;
        gchar *current = get_variable_string (var);

        if (values && values->len) {
            guint i = 0;
            const gchar *first   = argv_idx (values, 0);
            const gchar *this    = first;
                         next    = argv_idx (values, ++i);

            while (next && strcmp (current, this)) {
                this = next;
                next = argv_idx (values, ++i);
            }

            if (!next) {
                next = first;
            }

        } else {
            next = "";

            if (!strcmp (current, "0")) {
                next = "1";
            } else if (!strcmp (current, "1")) {
                next = "0";
            }
        }

        g_free (current);
        sendev = set_variable_string (var, next);
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

        sendev = set_variable_int (var, next);
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

        sendev = set_variable_ull (var, next);
        break;
    }
    case TYPE_DOUBLE:
    {
        gdouble current = get_variable_double (var);
        gdouble next;

        if (values && values->len) {
            guint i = 0;

            gdouble first = strtod (argv_idx (values, 0), NULL);
            gdouble  this = first;

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

        sendev = set_variable_double (var, next);
        break;
    }
    default:
        g_assert_not_reached ();
    }

    if (sendev) {
        send_variable_event (name, var);
    }

    return sendev;
}

typedef enum {
    EXPAND_INITIAL,
    EXPAND_IGNORE_SHELL,
    EXPAND_IGNORE_JS,
    EXPAND_IGNORE_CLEAN_JS,
    EXPAND_IGNORE_UZBL_JS,
    EXPAND_IGNORE_UZBL
} UzblExpandStage;

struct _ExpandContext {
    UzblExpandStage  stage;
    const gchar     *p;
    GString         *buf;
    GArray          *argv;
    GTask           *task;
};
typedef struct _ExpandContext ExpandContext;

static void
expand_context_free (ExpandContext *ctx)
{
    if (ctx->buf) {
        g_string_free (ctx->buf, TRUE);
    }
    g_free (ctx);
}

static gchar *
expand_impl (const gchar *str, UzblExpandStage stage);

static void
expand_process (ExpandContext *ctx);

gchar *
uzbl_variables_expand (const gchar *str)
{
    return expand_impl (str, EXPAND_INITIAL);
}

void
uzbl_variables_expand_async (const gchar         *str,
                             GAsyncReadyCallback  callback,
                             gpointer             data)
{
    GTask *task = g_task_new (NULL, NULL, callback, data);
    if (!str) {
        g_task_return_pointer (task, g_strdup (""), g_free);
        g_object_unref (task);
        return;
    }

    ExpandContext *ctx = g_new (ExpandContext, 1);
    ctx->stage = EXPAND_INITIAL;
    ctx->p = str;
    ctx->task = task;
    ctx->buf = g_string_new ("");
    g_task_set_task_data (task, ctx, (GDestroyNotify) expand_context_free);
    expand_process (ctx);
}

gchar*
uzbl_variables_expand_finish (GObject       *source,
                              GAsyncResult  *res,
                              GError       **error)
{
    UZBL_UNUSED (source);
    GTask *task = G_TASK (res);
    return g_task_propagate_pointer (task, error);
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
VAR_GETTER (gdouble, double)

static void
dump_variable (gpointer key, gpointer value, gpointer data);

void
uzbl_variables_dump ()
{
    g_hash_table_foreach (uzbl.variables->table, dump_variable, NULL);
}

static void
dump_variable_event (gpointer key, gpointer value, gpointer data);

void
uzbl_variables_dump_events ()
{
    g_hash_table_foreach (uzbl.variables->table, dump_variable_event, NULL);
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

void
variable_free (UzblVariable *variable)
{
    if ((variable->type == TYPE_STR) && variable->value.s && variable->writeable) {
        g_free (*variable->value.s);
        if (!variable->builtin) {
            g_free (variable->value.s);
        }
    }

    g_free (variable);
}

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

UzblVariable *
get_variable (const gchar *name)
{
    return (UzblVariable *)g_hash_table_lookup (uzbl.variables->table, name);
}

gboolean
set_variable_string (UzblVariable *var, const gchar *val)
{
    typedef gboolean (*setter_t) (const gchar *);

    if (var->set) {
        return ((setter_t)var->set) (val);
    } else {
        g_free (*(var->value.s));
        *(var->value.s) = g_strdup (val);
    }

    return TRUE;
}

#define TYPE_SETTER(type, name, member)               \
    gboolean                                          \
    set_variable_##name (UzblVariable *var, type val) \
    {                                                 \
        typedef gboolean (*setter_t) (type);          \
                                                      \
        if (var->set) {                               \
            return ((setter_t)var->set) (val);        \
        } else {                                      \
            *var->value.member = val;                 \
        }                                             \
                                                      \
        return TRUE;                                  \
    }

TYPE_SETTER (int, int, i)
TYPE_SETTER (unsigned long long, ull, ull)
TYPE_SETTER (gdouble, double, d)

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
    case TYPE_DOUBLE:
        type = "double";
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

    uzbl_gui_update_title ();
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
TYPE_GETTER (gdouble, double, d)

typedef enum {
    EXPAND_SHELL,
    EXPAND_JS,
    EXPAND_ESCAPE,
    EXPAND_UZBL,
    EXPAND_UZBL_JS,
    EXPAND_CLEAN_JS,
    EXPAND_VAR,
    EXPAND_VAR_BRACE
} UzblExpandType;

static UzblExpandType
expand_type (const gchar *str);

gchar *
expand_impl (const gchar *str, UzblExpandStage stage)
{
    if (!str) {
        return g_strdup ("");
    }

    ExpandContext *ctx = g_new (ExpandContext, 1);
    ctx->stage = stage;
    ctx->p = str;
    ctx->task = NULL;
    GString *buf = ctx->buf = g_string_new ("");

    expand_process (ctx);
    g_free (ctx);
    return g_string_free (buf, FALSE);
}

static void
expand_run_command_cb (GObject      *source,
                       GAsyncResult *res,
                       gpointer      data);

void expand_process (ExpandContext *ctx)
{
    const gchar *p = ctx->p;
    while (*p) {
        switch (*p) {
        case '@':
        {
            UzblExpandType etype = expand_type (p);
            char end_char = '\0';
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
                end_char = ')';
                goto expand_impl_find_end;
            case EXPAND_UZBL:
                end_char = '/';
                goto expand_impl_find_end;
            case EXPAND_UZBL_JS:
                end_char = '*';
                goto expand_impl_find_end;
            case EXPAND_CLEAN_JS:
                end_char = '-';
                goto expand_impl_find_end;
            case EXPAND_JS:
                end_char = '>';
                goto expand_impl_find_end;
            case EXPAND_ESCAPE:
                end_char = ']';
                goto expand_impl_find_end;
expand_impl_find_end:
            {
                ++p;
                char end[3] = { end_char, '@', '\0' };
                vend = strstr (p, end);
                if (!vend) {
                    vend = strchr (p, '\0');
                }
                break;
            }
            }
            assert (vend);

            ret = g_strndup (p, vend - p);

            UzblExpandStage ignore = EXPAND_INITIAL;
            const char *js_ctx = "";

            switch (etype) {
            case EXPAND_VAR_BRACE:
                /* Skip the end brace. */
                ++vend;
                /* FALLTHROUGH */
            case EXPAND_VAR:
                variable_expand (get_variable (ret), ctx->buf);

                p = vend;
                break;
            case EXPAND_SHELL:
            {
                if (ctx->stage == EXPAND_IGNORE_SHELL) {
                    break;
                }

                GString *spawn_ret = g_string_new ("");
                const gchar *runner = NULL;
                const gchar *cmd = ret;
                gboolean quote = FALSE;

                if (*ret == '+') {
                    /* Execute program directly. */
                    runner = "spawn_sync";
                    ++cmd;
                } else {
                    /* Execute program through shell, quote it first. */
                    runner = "spawn_sh_sync";
                    quote = TRUE;
                }

                gchar *exp_cmd = expand_impl (cmd, EXPAND_IGNORE_SHELL);

                if (quote) {
                    gchar *quoted = g_shell_quote (exp_cmd);
                    g_free (exp_cmd);
                    exp_cmd = quoted;
                }

                gchar *full_cmd = g_strdup_printf ("%s %s",
                    runner,
                    exp_cmd);

                uzbl_commands_run (full_cmd, spawn_ret);

                g_free (exp_cmd);
                g_free (full_cmd);

                if (spawn_ret->str) {
                    remove_trailing_newline (spawn_ret->str);

                    g_string_append (ctx->buf, spawn_ret->str);
                }
                g_string_free (spawn_ret, TRUE);

                p = vend + 2;

                break;
            }
            case EXPAND_UZBL:
            {
                if (ctx->stage == EXPAND_IGNORE_UZBL) {
                    break;
                }

                GString *uzbl_ret = g_string_new ("");

                GArray *tmp = uzbl_commands_args_new ();

                if (*ret == '+') {
                    /* Read commands from file. */
                    gchar *mycmd = expand_impl (ret + 1, EXPAND_IGNORE_UZBL);
                    g_array_append_val (tmp, mycmd);

                    uzbl_commands_run_argv ("include", tmp, uzbl_ret);
                } else {
                    /* Command string. */
                    gchar *mycmd = expand_impl (ret, EXPAND_IGNORE_UZBL);

                    uzbl_commands_run (mycmd, uzbl_ret);
                }

                uzbl_commands_args_free (tmp);

                if (uzbl_ret->str) {
                    g_string_append (ctx->buf, uzbl_ret->str);
                }
                g_string_free (uzbl_ret, TRUE);

                p = vend + 2;

                break;
            }
            case EXPAND_UZBL_JS:
                ignore = EXPAND_IGNORE_UZBL_JS;
                js_ctx = "uzbl";
                goto expand_impl_run_js;
            case EXPAND_CLEAN_JS:
                ignore = EXPAND_IGNORE_CLEAN_JS;
                js_ctx = "clean";
                goto expand_impl_run_js;
            case EXPAND_JS:
                ignore = EXPAND_IGNORE_JS;
                js_ctx = "page";
                goto expand_impl_run_js;
expand_impl_run_js:
            {
                if (ctx->stage == ignore) {
                    break;
                }

                if (!ctx->task) {
                   g_warning ("Trying to expand js in sync context");
                   break;
                }

                ctx->argv = uzbl_commands_args_new ();
                uzbl_commands_args_append (ctx->argv, g_strdup (js_ctx));
                const gchar *source = NULL;
                gchar *cmd = ret;

                if (*ret == '+') {
                    /* Read JS from file. */
                    source = "file";
                    ++cmd;
                } else {
                    /* JS from string. */
                    source = "string";
                }

                uzbl_commands_args_append (ctx->argv, g_strdup (source));

                gchar *exp_cmd = expand_impl (cmd, ignore);
                g_array_append_val (ctx->argv, exp_cmd);

                const UzblCommand *info = uzbl_commands_lookup ("js");
                uzbl_commands_run_async (info, ctx->argv, TRUE, expand_run_command_cb, ctx);
                ctx->p = vend + 2;
                return;
            }
            case EXPAND_ESCAPE:
            {
                gchar *exp_cmd = expand_impl (ret, EXPAND_INITIAL);
                gchar *escaped = g_markup_escape_text (exp_cmd, strlen (exp_cmd));

                g_string_append (ctx->buf, escaped);

                g_free (escaped);
                g_free (exp_cmd);
                p = vend + 2;
                break;
            }
            }

            g_free (ret);
            break;
        }
        case '\\':
            g_string_append_c (ctx->buf, *p);
            ++p;
            if (!*p) {
                break;
            }
            /* FALLTHROUGH */
        default:
            g_string_append_c (ctx->buf, *p);
            ++p;
            break;
        }
    }

    if (ctx->task) {
        char *r = g_string_free (ctx->buf, FALSE);
        ctx->buf = NULL;
        g_task_return_pointer (ctx->task, r, g_free);
        g_object_unref (ctx->task);
    }
}

static void
expand_run_command_cb (GObject      *source,
                       GAsyncResult *res,
                       gpointer      data)
{
    ExpandContext *ctx = (ExpandContext*) data;
    GError *err = NULL;
    GString *ret = uzbl_commands_run_finish (source, res, &err);
    uzbl_commands_args_free (ctx->argv);

    if (ret->str) {
        g_string_append (ctx->buf, ret->str);
        g_string_free (ret, TRUE);
    }

    expand_process (ctx);
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

    printf ("set %s %s\n", name, buf->str);

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

    gchar *var = uzbl_js_extract_string (propertyName);
    UzblVariable *uzbl_var = get_variable (var);

    g_free (var);

    return uzbl_var;
}

JSValueRef
js_get_variable (JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
    UZBL_UNUSED (object);
    UZBL_UNUSED (exception);

    gchar *var = uzbl_js_extract_string (propertyName);
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
    case TYPE_DOUBLE:
    {
        gdouble val = get_variable_double (uzbl_var);
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

    gchar *var = uzbl_js_extract_string (propertyName);
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
    case TYPE_DOUBLE:
        uzbl_comm_string_append_double (buf, get_variable_double (var));
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
    case '-':
        return EXPAND_CLEAN_JS;
    case '<':
        return EXPAND_JS;
    case '[':
        return EXPAND_ESCAPE;
    default:
        return EXPAND_VAR;
    }
}

/* ======================== VARIABLES  TABLE ======================== */

#if !WEBKIT_CHECK_VERSION (2, 5, 1)
#define HAVE_PAGE_VIEW_MODE
#endif

#if WEBKIT_CHECK_VERSION (2, 7, 2)
#define HAVE_LOCAL_STORAGE_PATH
#endif

#if WEBKIT_CHECK_VERSION (2, 7, 4)
#define HAVE_EDITABLE
#endif

/* Abbreviations to help keep the table's width humane. */
#define UZBL_SETTING(typ, val, w, getter, setter) \
    { .type = TYPE_##typ, .value = val, .writeable = w, .builtin = TRUE, .get = (UzblFunction)getter, .set = (UzblFunction)setter }

#define UZBL_VARIABLE(typ, val, getter, setter) \
    UZBL_SETTING (typ, val, TRUE, getter, setter)
#define UZBL_CONSTANT(typ, val, getter) \
    UZBL_SETTING (typ, val, FALSE, getter, NULL)

/* Variables */
#define UZBL_V_STRING(val, set) UZBL_VARIABLE (STR,    { .s = &(val) },         NULL,      set)
#define UZBL_V_INT(val, set)    UZBL_VARIABLE (INT,    { .i = (int*)&(val) },   NULL,      set)
#define UZBL_V_LONG(val, set)   UZBL_VARIABLE (ULL,    { .ull = &(val) },       NULL,      set)
#define UZBL_V_DOUBLE(val, set) UZBL_VARIABLE (DOUBLE, { .d = &(val) },         NULL,      set)
#define UZBL_V_FUNC(val, typ)   UZBL_VARIABLE (typ,    { .s = NULL },           get_##val, set_##val)

/* Constants */
#define UZBL_C_STRING(val)    UZBL_CONSTANT (STR,    { .s = &(val) },       NULL)
#define UZBL_C_INT(val)       UZBL_CONSTANT (INT,    { .i = (int*)&(val) }, NULL)
#define UZBL_C_LONG(val)      UZBL_CONSTANT (ULL,    { .ull = &(val) },     NULL)
#define UZBL_C_DOUBLE(val)    UZBL_CONSTANT (DOUBLE, { .d = &(val) },       NULL)
#define UZBL_C_FUNC(val, typ) UZBL_CONSTANT (typ,    { .s = NULL },         get_##val)

#define DECLARE_GETTER(type, name) \
    static type                    \
    get_##name ()
#define DECLARE_SETTER(type, name) \
    static gboolean                \
    set_##name (const type name)

#define DECLARE_GETSET(type, name) \
    DECLARE_GETTER (type, name);   \
    DECLARE_SETTER (type, name)

/* Communication variables */
DECLARE_SETTER (gchar *, fifo_dir);
DECLARE_SETTER (gchar *, socket_dir);

/* Window variables */
DECLARE_SETTER (gchar *, icon);
DECLARE_SETTER (gchar *, icon_name);
DECLARE_GETSET (gchar *, window_role);

/* UI variables */
DECLARE_SETTER (int, show_status);
DECLARE_SETTER (int, status_top);
DECLARE_SETTER (gchar *, status_background);
DECLARE_GETSET (int, enable_compositing_debugging);
#if WEBKIT_CHECK_VERSION (2, 7, 4)
DECLARE_GETSET (gchar *, background_color);
#endif

/* Printing variables */
DECLARE_GETSET (int, print_backgrounds);

/* Network variables */
DECLARE_GETSET (gchar *, ssl_policy);
DECLARE_GETSET (gchar *, cache_model);

/* Security variables */
DECLARE_GETSET (int, enable_private);
DECLARE_GETSET (int, enable_hyperlink_auditing);
DECLARE_GETSET (int, enable_xss_auditing);
/*DECLARE_GETSET (gchar *, cookie_location);*/
/*DECLARE_GETSET (gchar *, cookie_store);*/
DECLARE_GETSET (gchar *, cookie_policy);
DECLARE_GETSET (int, enable_dns_prefetch);
#if WEBKIT_CHECK_VERSION (2, 9, 1)
DECLARE_GETSET (int, allow_file_to_file_access);
#endif

/* Page variables */
DECLARE_GETSET (gchar *, useragent);
DECLARE_SETTER (gchar *, accept_languages);
DECLARE_GETSET (gdouble, zoom_level);
DECLARE_SETTER (gdouble, zoom_step);
DECLARE_GETSET (int, zoom_text_only);
DECLARE_GETSET (int, caret_browsing);
DECLARE_GETSET (int, enable_frame_flattening);
DECLARE_GETSET (int, enable_smooth_scrolling);
#ifdef HAVE_PAGE_VIEW_MODE
DECLARE_GETSET (gchar *, page_view_mode);
#endif
DECLARE_GETSET (int, enable_fullscreen);
#ifdef HAVE_EDITABLE
DECLARE_GETSET (int, editable);
#endif

/* Javascript variables */
DECLARE_GETSET (int, enable_scripts);
DECLARE_GETSET (int, javascript_windows);
DECLARE_GETSET (int, javascript_modal_dialogs);
DECLARE_GETSET (int, javascript_clipboard);
DECLARE_GETSET (int, javascript_console_to_stdout);

/* Image variables */
DECLARE_GETSET (int, autoload_images);
DECLARE_GETSET (int, always_load_icons);

/* Spell checking variables */
DECLARE_GETSET (int, enable_spellcheck);
DECLARE_GETSET (gchar *, spellcheck_languages);

/* Form variables */
DECLARE_GETSET (int, resizable_text_areas);
DECLARE_GETSET (int, enable_spatial_navigation);
DECLARE_GETSET (int, enable_tab_cycle);

/* Text variables */
DECLARE_GETSET (gchar *, default_encoding);
DECLARE_GETSET (gchar *, custom_encoding);

/* Font variables */
DECLARE_GETSET (gchar *, default_font_family);
DECLARE_GETSET (gchar *, monospace_font_family);
DECLARE_GETSET (gchar *, sans_serif_font_family);
DECLARE_GETSET (gchar *, serif_font_family);
DECLARE_GETSET (gchar *, cursive_font_family);
DECLARE_GETSET (gchar *, fantasy_font_family);
DECLARE_GETSET (gchar *, pictograph_font_family);

/* Font size variables */
DECLARE_GETSET (int, font_size);
DECLARE_GETSET (int, monospace_size);

/* Feature variables */
DECLARE_GETSET (int, enable_plugins);
DECLARE_GETSET (int, enable_java_applet);
DECLARE_GETSET (int, enable_webgl);
DECLARE_GETSET (int, enable_webaudio);
DECLARE_GETSET (int, enable_2d_acceleration);
DECLARE_GETSET (int, enable_inline_media);
DECLARE_GETSET (int, require_click_to_play);
DECLARE_GETSET (int, enable_media_stream);
DECLARE_GETSET (int, enable_media_source);

/* HTML5 Database variables */
DECLARE_GETSET (int, enable_database);
DECLARE_GETSET (int, enable_local_storage);
DECLARE_GETSET (int, enable_pagecache);
DECLARE_GETSET (int, enable_offline_app_cache);
#ifdef HAVE_LOCAL_STORAGE_PATH
DECLARE_GETTER (gchar *, local_storage_path);
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 4)
DECLARE_GETTER (gchar *, base_cache_directory);
DECLARE_GETTER (gchar *, base_data_directory);
DECLARE_GETTER (gchar *, disk_cache_directory);
DECLARE_GETTER (gchar *, indexed_db_directory);
DECLARE_GETTER (gchar *, offline_app_cache_directory);
DECLARE_GETTER (gchar *, websql_directory);
#elif WEBKIT_CHECK_VERSION (2, 9, 2)
DECLARE_GETSET (gchar *, indexed_db_directory);
DECLARE_SETTER (gchar *, disk_cache_directory);
#endif

/* Hacks */
DECLARE_GETSET (int, enable_site_workarounds);

/* Constants */
DECLARE_GETTER (gchar *, inspected_uri);
DECLARE_GETTER (gchar *, geometry);
DECLARE_GETTER (gchar *, plugin_list);
DECLARE_GETTER (int, is_online);
#if WEBKIT_CHECK_VERSION (2, 7, 4)
DECLARE_GETTER (int, is_playing_audio);
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 4)
DECLARE_GETTER (gchar *, editor_state);
#endif
DECLARE_GETTER (int, WEBKIT_MAJOR);
DECLARE_GETTER (int, WEBKIT_MINOR);
DECLARE_GETTER (int, WEBKIT_MICRO);
DECLARE_GETTER (int, WEBKIT_MAJOR_COMPILE);
DECLARE_GETTER (int, WEBKIT_MINOR_COMPILE);
DECLARE_GETTER (int, WEBKIT_MICRO_COMPILE);
DECLARE_GETTER (int, WEBKIT_UA_MAJOR);
DECLARE_GETTER (int, WEBKIT_UA_MINOR);
DECLARE_GETTER (int, HAS_WEBKIT2);
DECLARE_GETTER (gchar *, ARCH_UZBL);
DECLARE_GETTER (gchar *, COMMIT);
DECLARE_GETTER (int, PID);

struct _UzblVariablesPrivate {
    /* Uzbl variables */
    gboolean verbose;
    gboolean frozen;
    gboolean print_events;
    gboolean handle_multi_button;

    /* Communication variables */
    gchar *fifo_dir;
    gchar *socket_dir;

    /* Window variables */
    gchar *icon;
    gchar *icon_name;

    /* UI variables */
    gboolean show_status;
    gboolean status_top;
    gchar *status_background;
#if GTK_CHECK_VERSION (3, 15, 0)
    GtkCssProvider *status_background_provider;
#endif

    /* Security variables */
    gboolean permissive;

    /* Page variables */
    gboolean forward_keys;
    gchar *accept_languages;
    gdouble zoom_step;

    /* HTML5 Database variables */
#if !WEBKIT_CHECK_VERSION (2, 9, 4)
    gchar *disk_cache_directory;
#endif
};

typedef struct {
    const char *name;
    UzblVariable var;
} UzblVariableEntry;

UzblVariablesPrivate *
uzbl_variables_private_new (GHashTable *table)
{
    UzblVariablesPrivate *priv = g_malloc0 (sizeof (UzblVariablesPrivate));

    const UzblVariableEntry
    builtin_variable_table[] = {
        /* name                           entry                                                type/callback */
        /* Uzbl variables */
        { "verbose",                      UZBL_V_INT (priv->verbose,                           NULL)},
        { "frozen",                       UZBL_V_INT (priv->frozen,                            NULL)},
        { "print_events",                 UZBL_V_INT (priv->print_events,                      NULL)},
        { "handle_multi_button",          UZBL_V_INT (priv->handle_multi_button,               NULL)},

        /* Communication variables */
        { "fifo_dir",                     UZBL_V_STRING (priv->fifo_dir,                       set_fifo_dir)},
        { "socket_dir",                   UZBL_V_STRING (priv->socket_dir,                     set_socket_dir)},

        /* Window variables */
        { "icon",                         UZBL_V_STRING (priv->icon,                           set_icon)},
        { "icon_name",                    UZBL_V_STRING (priv->icon_name,                      set_icon_name)},
        { "window_role",                  UZBL_V_FUNC (window_role,                            STR)},

        /* UI variables */
        { "show_status",                  UZBL_V_INT (priv->show_status,                       set_show_status)},
        { "status_top",                   UZBL_V_INT (priv->status_top,                        set_status_top)},
        { "status_background",            UZBL_V_STRING (priv->status_background,              set_status_background)},
        { "enable_compositing_debugging", UZBL_V_FUNC (enable_compositing_debugging,           INT)},
#if WEBKIT_CHECK_VERSION (2, 7, 4)
        { "background_color",             UZBL_V_FUNC (background_color,                       STR)},
#endif

        /* Printing variables */
        { "print_backgrounds",            UZBL_V_FUNC (print_backgrounds,                      INT)},

        /* Network variables */
        { "ssl_policy",                   UZBL_V_FUNC (ssl_policy,                             STR)},
        { "cache_model",                  UZBL_V_FUNC (cache_model,                            STR)},

        /* Security variables */
        { "enable_private",               UZBL_V_FUNC (enable_private,                         INT)},
        { "permissive",                   UZBL_V_INT (priv->permissive,                        NULL)},
        { "enable_hyperlink_auditing",    UZBL_V_FUNC (enable_hyperlink_auditing,              INT)},
        { "enable_xss_auditing",          UZBL_V_FUNC (enable_xss_auditing,                    INT)},
        { "cookie_policy",                UZBL_V_FUNC (cookie_policy,                          STR)},
        { "enable_dns_prefetch",          UZBL_V_FUNC (enable_dns_prefetch,                    INT)},
#if WEBKIT_CHECK_VERSION (2, 9, 1)
        { "allow_file_to_file_access",    UZBL_V_FUNC (allow_file_to_file_access,              INT)},
#endif

        /* Page variables */
        { "forward_keys",                 UZBL_V_INT (priv->forward_keys,                      NULL)},
        { "useragent",                    UZBL_V_FUNC (useragent,                              STR)},
        { "accept_languages",             UZBL_V_STRING (priv->accept_languages,               set_accept_languages)},
        { "zoom_level",                   UZBL_V_FUNC (zoom_level,                             DOUBLE)},
        { "zoom_step",                    UZBL_V_DOUBLE (priv->zoom_step,                      set_zoom_step)},
        { "zoom_text_only",               UZBL_V_FUNC (zoom_text_only,                         INT)},
        { "caret_browsing",               UZBL_V_FUNC (caret_browsing,                         INT)},
        { "enable_frame_flattening",      UZBL_V_FUNC (enable_frame_flattening,                INT)},
        { "enable_smooth_scrolling",      UZBL_V_FUNC (enable_smooth_scrolling,                INT)},
#ifdef HAVE_PAGE_VIEW_MODE
        { "page_view_mode",               UZBL_V_FUNC (page_view_mode,                         STR)},
#endif
        { "enable_fullscreen",            UZBL_V_FUNC (enable_fullscreen,                      INT)},
#ifdef HAVE_EDITABLE
        { "editable",                     UZBL_V_FUNC (editable,                               INT)},
#endif

        /* Javascript variables */
        { "enable_scripts",               UZBL_V_FUNC (enable_scripts,                         INT)},
        { "javascript_windows",           UZBL_V_FUNC (javascript_windows,                     INT)},
        { "javascript_modal_dialogs",     UZBL_V_FUNC (javascript_modal_dialogs,               INT)},
        { "javascript_clipboard",         UZBL_V_FUNC (javascript_clipboard,                   INT)},
        { "javascript_console_to_stdout", UZBL_V_FUNC (javascript_console_to_stdout,           INT)},

        /* Image variables */
        { "autoload_images",              UZBL_V_FUNC (autoload_images,                        INT)},
        { "always_load_icons",            UZBL_V_FUNC (always_load_icons,                      INT)},

        /* Spell checking variables */
        { "enable_spellcheck",            UZBL_V_FUNC (enable_spellcheck,                      INT)},
        { "spellcheck_languages",         UZBL_V_FUNC (spellcheck_languages,                   STR)},

        /* Form variables */
        { "resizable_text_areas",         UZBL_V_FUNC (resizable_text_areas,                   INT)},
        { "enable_spatial_navigation",    UZBL_V_FUNC (enable_spatial_navigation,              INT)},
        { "enable_tab_cycle",             UZBL_V_FUNC (enable_tab_cycle,                       INT)},

        /* Text variables */
        { "default_encoding",             UZBL_V_FUNC (default_encoding,                       STR)},
        { "custom_encoding",              UZBL_V_FUNC (custom_encoding,                        STR)},

        /* Font variables */
        { "default_font_family",          UZBL_V_FUNC (default_font_family,                    STR)},
        { "monospace_font_family",        UZBL_V_FUNC (monospace_font_family,                  STR)},
        { "sans_serif_font_family",       UZBL_V_FUNC (sans_serif_font_family,                 STR)},
        { "serif_font_family",            UZBL_V_FUNC (serif_font_family,                      STR)},
        { "cursive_font_family",          UZBL_V_FUNC (cursive_font_family,                    STR)},
        { "fantasy_font_family",          UZBL_V_FUNC (fantasy_font_family,                    STR)},
        { "pictograph_font_family",       UZBL_V_FUNC (pictograph_font_family,                 STR)},

        /* Font size variables */
        { "font_size",                    UZBL_V_FUNC (font_size,                              INT)},
        { "monospace_size",               UZBL_V_FUNC (monospace_size,                         INT)},

        /* Feature variables */
        { "enable_plugins",               UZBL_V_FUNC (enable_plugins,                         INT)},
        { "enable_java_applet",           UZBL_V_FUNC (enable_java_applet,                     INT)},
        { "enable_webgl",                 UZBL_V_FUNC (enable_webgl,                           INT)},
        { "enable_webaudio",              UZBL_V_FUNC (enable_webaudio,                        INT)},
        { "enable_2d_acceleration",       UZBL_V_FUNC (enable_2d_acceleration,                 INT)},
        { "enable_inline_media",          UZBL_V_FUNC (enable_inline_media,                    INT)},
        { "require_click_to_play",        UZBL_V_FUNC (require_click_to_play,                  INT)},
        { "enable_media_stream",          UZBL_V_FUNC (enable_media_stream,                    INT)},
        { "enable_media_source",          UZBL_V_FUNC (enable_media_source,                    INT)},

        /* HTML5 Database variables */
        { "enable_database",              UZBL_V_FUNC (enable_database,                        INT)},
        { "enable_local_storage",         UZBL_V_FUNC (enable_local_storage,                   INT)},
        { "enable_pagecache",             UZBL_V_FUNC (enable_pagecache,                       INT)},
        { "enable_offline_app_cache",     UZBL_V_FUNC (enable_offline_app_cache,               INT)},
#ifdef HAVE_LOCAL_STORAGE_PATH
        { "local_storage_path",           UZBL_C_FUNC (local_storage_path,                     STR)},
#endif
        { "disk_cache_directory",
#if WEBKIT_CHECK_VERSION (2, 9, 4)
                                          UZBL_C_FUNC (disk_cache_directory,                   STR)
#else
                                          UZBL_V_STRING (priv->disk_cache_directory,           set_disk_cache_directory)
#endif
                                          },
        /* Hacks */
        { "enable_site_workarounds",      UZBL_V_FUNC (enable_site_workarounds,                INT)},

        /* Constants */
        { "inspected_uri",                UZBL_C_FUNC (inspected_uri,                          STR)},
        { "geometry",                     UZBL_C_FUNC (geometry,                               STR)},
        { "plugin_list",                  UZBL_C_FUNC (plugin_list,                            STR)},
        { "is_online",                    UZBL_C_FUNC (is_online,                              INT)},
        { "web_extensions_directory",     UZBL_C_STRING (uzbl.state.web_extensions_directory)},
#if WEBKIT_CHECK_VERSION (2, 7, 4)
        { "is_playing_audio",             UZBL_C_FUNC (is_playing_audio,                       INT)},
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 4)
        { "editor_state",                 UZBL_C_FUNC (editor_state,                           STR)},
        { "base_cache_directory",         UZBL_C_FUNC (base_cache_directory,                   STR)},
        { "base_data_directory",          UZBL_C_FUNC (base_data_directory,                    STR)},
        { "offline_app_cache_directory",  UZBL_C_FUNC (offline_app_cache_directory,            STR)},
        { "websql_directory",             UZBL_C_FUNC (websql_directory,                       STR)},
        { "indexed_db_directory",         UZBL_C_FUNC (indexed_db_directory,                   STR)},
#endif
        { "uri",                          UZBL_C_STRING (uzbl.state.uri)},
        { "embedded",                     UZBL_C_INT (uzbl.state.plug_mode)},
        { "WEBKIT_MAJOR",                 UZBL_C_FUNC (WEBKIT_MAJOR,                           INT)},
        { "WEBKIT_MINOR",                 UZBL_C_FUNC (WEBKIT_MINOR,                           INT)},
        { "WEBKIT_MICRO",                 UZBL_C_FUNC (WEBKIT_MICRO,                           INT)},
        { "WEBKIT_MAJOR_COMPILE",         UZBL_C_FUNC (WEBKIT_MAJOR_COMPILE,                   INT)},
        { "WEBKIT_MINOR_COMPILE",         UZBL_C_FUNC (WEBKIT_MINOR_COMPILE,                   INT)},
        { "WEBKIT_MICRO_COMPILE",         UZBL_C_FUNC (WEBKIT_MICRO_COMPILE,                   INT)},
        { "WEBKIT_UA_MAJOR",              UZBL_C_FUNC (WEBKIT_UA_MAJOR,                        INT)},
        { "WEBKIT_UA_MINOR",              UZBL_C_FUNC (WEBKIT_UA_MINOR,                        INT)},
        { "HAS_WEBKIT2",                  UZBL_C_FUNC (HAS_WEBKIT2,                            INT)},
        { "ARCH_UZBL",                    UZBL_C_FUNC (ARCH_UZBL,                              STR)},
        { "COMMIT",                       UZBL_C_FUNC (COMMIT,                                 STR)},
        { "TITLE",                        UZBL_C_STRING (uzbl.gui.main_title)},
        { "SELECTED_URI",                 UZBL_C_STRING (uzbl.state.selected_url)},
        { "NAME",                         UZBL_C_STRING (uzbl.state.instance_name)},
        { "PID",                          UZBL_C_FUNC (PID,                                    INT)},
        { "_",                            UZBL_C_STRING (uzbl.state.last_result)},

        /* Add a terminator entry. */
        { NULL,                           UZBL_SETTING (INT, { .i = NULL }, 0, NULL, NULL)}
    };

    const UzblVariableEntry *entry = builtin_variable_table;
    while (entry->name) {
        UzblVariable *value = g_malloc (sizeof (UzblVariable));
        memcpy (value, &entry->var, sizeof (UzblVariable));

        g_hash_table_insert (table,
            (gpointer)g_strdup (entry->name),
            (gpointer)value);

        ++entry;
    }

    return priv;
}

void
uzbl_variables_private_free (UzblVariablesPrivate *priv)
{
#if GTK_CHECK_VERSION (3, 15, 0)
    if (priv->status_background_provider) {
        g_object_unref (priv->status_background_provider);
    }
#endif

    /* All other members are deleted by the table's free function. */
    g_free (priv);
}

/* =================== VARIABLES IMPLEMENTATIONS ==================== */

#define IMPLEMENT_GETTER(type, name) \
    type                             \
    get_##name ()

#define IMPLEMENT_SETTER(type, name) \
    gboolean                         \
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

#define GOBJECT_GETTER2(type, name, rtype, obj, prop) \
    IMPLEMENT_GETTER (type, name)                     \
    {                                                 \
        type name;                                    \
        rtype rname;                                  \
                                                      \
        g_object_get (G_OBJECT (obj),                 \
            prop, &rname,                             \
            NULL);                                    \
        name = rname;                                 \
                                                      \
        return name;                                  \
    }

#define GOBJECT_SETTER(type, name, obj, prop) \
    IMPLEMENT_SETTER (type, name)             \
    {                                         \
        g_object_set (G_OBJECT (obj),         \
            prop, name,                       \
            NULL);                            \
                                              \
        return TRUE;                          \
    }

#define GOBJECT_SETTER2(type, name, rtype, obj, prop) \
    IMPLEMENT_SETTER (type, name)             \
    {                                         \
        rtype rname = name;                   \
                                              \
        g_object_set (G_OBJECT (obj),         \
            prop, rname,                      \
            NULL);                            \
                                              \
        return TRUE;                          \
    }

#define GOBJECT_GETSET(type, name, obj, prop) \
    GOBJECT_GETTER (type, name, obj, prop)    \
    GOBJECT_SETTER (type, name, obj, prop)

#define GOBJECT_GETSET2(type, name, rtype, obj, prop) \
    GOBJECT_GETTER2 (type, name, rtype, obj, prop)    \
    GOBJECT_SETTER2 (type, name, rtype, obj, prop)

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
        name##_choices (ENUM_TO_STRING)     \
        default:                            \
            break;                          \
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
            return FALSE;                   \
        }                                   \
                                            \
        set (out);                          \
                                            \
        return TRUE;                        \
    }

#if WEBKIT_CHECK_VERSION (2, 7, 2) && !WEBKIT_CHECK_VERSION (2, 9, 2)
static GObject *
webkit_context ();
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 4)
static GObject *
webkit_data_manager ();
#endif
static GObject *
webkit_settings ();
static GObject *
webkit_view ();

/* Communication variables */
IMPLEMENT_SETTER (gchar *, fifo_dir)
{
    if (uzbl_io_init_fifo (fifo_dir)) {
        g_free (uzbl.variables->priv->fifo_dir);
        uzbl.variables->priv->fifo_dir = g_strdup (fifo_dir);

        return TRUE;
    }

    return FALSE;
}

IMPLEMENT_SETTER (gchar *, socket_dir)
{
    if (uzbl_io_init_socket (socket_dir)) {
        g_free (uzbl.variables->priv->socket_dir);
        uzbl.variables->priv->socket_dir = g_strdup (socket_dir);

        return TRUE;
    }

    return FALSE;
}

/* Window variables */
IMPLEMENT_SETTER (gchar *, icon)
{
    if (!uzbl.gui.main_window) {
        return FALSE;
    }

    if (file_exists (icon)) {
        /* Clear icon_name. */
        g_free (uzbl.variables->priv->icon_name);
        uzbl.variables->priv->icon_name = NULL;

        g_free (uzbl.variables->priv->icon);
        uzbl.variables->priv->icon = g_strdup (icon);

        gtk_window_set_icon_from_file (GTK_WINDOW (uzbl.gui.main_window), uzbl.variables->priv->icon, NULL);

        return TRUE;
    }

    uzbl_debug ("Icon \"%s\" not found. ignoring.\n", icon);

    return FALSE;
}

IMPLEMENT_SETTER (gchar *, icon_name)
{
    if (!uzbl.gui.main_window) {
        return FALSE;
    }

    /* Clear icon path. */
    g_free (uzbl.variables->priv->icon);
    uzbl.variables->priv->icon = NULL;

    g_free (uzbl.variables->priv->icon_name);
    uzbl.variables->priv->icon_name = g_strdup (icon_name);

    gtk_window_set_icon_name (GTK_WINDOW (uzbl.gui.main_window), uzbl.variables->priv->icon_name);

    return TRUE;
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
        return FALSE;
    }

    gtk_window_set_role (GTK_WINDOW (uzbl.gui.main_window), window_role);

    return TRUE;
}

/* UI variables */
IMPLEMENT_SETTER (int, show_status)
{
    uzbl.variables->priv->show_status = show_status;

    if (uzbl.gui.status_bar) {
        gtk_widget_set_visible (uzbl.gui.status_bar, uzbl.variables->priv->show_status);
    }

    return TRUE;
}

IMPLEMENT_SETTER (int, status_top)
{
    if (!uzbl.gui.scrolled_win || !uzbl.gui.status_bar) {
        return FALSE;
    }

    uzbl.variables->priv->status_top = status_top;

    g_object_ref (uzbl.gui.scrolled_win);
    g_object_ref (uzbl.gui.status_bar);
    gtk_container_remove (GTK_CONTAINER (uzbl.gui.vbox), uzbl.gui.scrolled_win);
    gtk_container_remove (GTK_CONTAINER (uzbl.gui.vbox), uzbl.gui.status_bar);

    if (uzbl.variables->priv->status_top) {
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

    return TRUE;
}

IMPLEMENT_SETTER (char *, status_background)
{
    /* Labels and hboxes do not draw their own background. Applying this on the
     * vbox/main_window is ok as the statusbar is the only affected widget. If
     * not, we could also use GtkEventBox. */
    GtkWidget *widget = uzbl.gui.main_window ? uzbl.gui.main_window : GTK_WIDGET (uzbl.gui.plug);

    gboolean parsed = FALSE;

#if GTK_CHECK_VERSION (3, 15, 0)
    GtkStyleContext *ctx = gtk_widget_get_style_context (widget);
    GtkCssProvider *provider = gtk_css_provider_new ();
    GError *err = NULL;
    gchar *css_content = g_strdup_printf (
        "* {\n"
        "  background-color: %s;\n"
        "}\n", status_background);
    gtk_css_provider_load_from_data (provider,
        css_content, -1, &err);
    g_free (css_content);
    if (!err) {
        if (uzbl.variables->priv->status_background_provider) {
            gtk_style_context_remove_provider (ctx, GTK_STYLE_PROVIDER (uzbl.variables->priv->status_background_provider));
            g_object_unref (uzbl.variables->priv->status_background_provider);
        }
        /* Using a slightly higher priority here because the user set this
         * manually and user CSS has already happened. */
        gtk_style_context_add_provider (ctx, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER + 1);
        uzbl.variables->priv->status_background_provider = provider;
        parsed = TRUE;
    } else {
        uzbl_debug ("Failed to parse status_background color: '%s': %s\n", status_background, err->message);
        g_object_unref (provider);
        g_error_free (err);
    }
#else
    GdkRGBA color;
    parsed = gdk_rgba_parse (&color, status_background);
    if (parsed) {
        gtk_widget_override_background_color (widget, GTK_STATE_NORMAL, &color);
    }
#endif

    if (!parsed) {
        return FALSE;
    }

    g_free (uzbl.variables->priv->status_background);
    uzbl.variables->priv->status_background = g_strdup (status_background);

    return TRUE;
}

GOBJECT_GETSET2 (int, enable_compositing_debugging,
                 gboolean, webkit_settings (), "draw-compositing-indicators")

#if WEBKIT_CHECK_VERSION (2, 7, 4)
IMPLEMENT_GETTER (gchar *, background_color)
{
    GdkRGBA color;

    webkit_web_view_get_background_color (uzbl.gui.web_view, &color);

    return gdk_rgba_to_string (&color);
}

IMPLEMENT_SETTER (gchar *, background_color)
{
    GdkRGBA color;

    gboolean parsed = gdk_rgba_parse (&color, background_color);
    if (!parsed) {
        return FALSE;
    }
    webkit_web_view_set_background_color (uzbl.gui.web_view, &color);

    return TRUE;
}
#endif

/* Printing variables */
GOBJECT_GETSET2 (int, print_backgrounds,
                 gboolean, webkit_settings (), "print-backgrounds")

/* Network variables */
#define ssl_policy_choices(call)                     \
    call (WEBKIT_TLS_ERRORS_POLICY_IGNORE, "ignore") \
    call (WEBKIT_TLS_ERRORS_POLICY_FAIL, "fail")

#define _webkit_web_context_get_tls_errors_policy() \
    webkit_web_context_get_tls_errors_policy (webkit_web_view_get_context (uzbl.gui.web_view))
#define _webkit_web_context_set_tls_errors_policy(val) \
    webkit_web_context_set_tls_errors_policy (webkit_web_view_get_context (uzbl.gui.web_view), val)

CHOICE_GETSET (WebKitTLSErrorsPolicy, ssl_policy,
               _webkit_web_context_get_tls_errors_policy, _webkit_web_context_set_tls_errors_policy)

#undef _webkit_web_context_get_tls_errors_policy
#undef _webkit_web_context_set_tls_errors_policy

#undef ssl_policy_choices

#define cache_model_choices(call)                                 \
    call (WEBKIT_CACHE_MODEL_DOCUMENT_VIEWER , "document_viewer") \
    call (WEBKIT_CACHE_MODEL_WEB_BROWSER, "web_browser")          \
    call (WEBKIT_CACHE_MODEL_DOCUMENT_BROWSER, "document_browser")

#define _webkit_web_context_get_cache_model() \
    webkit_web_context_get_cache_model (webkit_web_view_get_context (uzbl.gui.web_view))
#define _webkit_web_context_set_cache_model(val) \
    webkit_web_context_set_cache_model (webkit_web_view_get_context (uzbl.gui.web_view), val)

CHOICE_GETSET (WebKitCacheModel, cache_model,
               _webkit_web_context_get_cache_model, _webkit_web_context_set_cache_model)

#undef _webkit_web_context_get_cache_model
#undef _webkit_web_context_set_cache_model

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
        g_setenv (priv_envvar, "true", 1);
    } else {
        g_unsetenv (priv_envvar);
    }

    if (get_enable_private_webkit () != enable_private) {
        WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
        WebKitCookieManager *manager = webkit_web_context_get_cookie_manager (context);
        webkit_cookie_manager_delete_all_cookies (manager);
    }

    set_enable_private_webkit (enable_private);

    return TRUE;
}

GOBJECT_GETSET2 (int, enable_private_webkit,
                 gboolean, webkit_settings (), "enable-private-browsing")

GOBJECT_GETSET2 (int, enable_hyperlink_auditing,
                 gboolean, webkit_settings (), "enable-hyperlink-auditing")

GOBJECT_GETSET2 (int, enable_xss_auditing,
                 gboolean, webkit_settings (), "enable-xss-auditor")

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

#undef cookie_policy_choices

GOBJECT_GETSET2 (int, enable_dns_prefetch,
                 gboolean, webkit_settings (), "enable-dns-prefetching")

#if WEBKIT_CHECK_VERSION (2, 9, 1)
GOBJECT_GETSET2 (int, allow_file_to_file_access,
                 gboolean, webkit_settings (), "allow-file-access-from-file-urls");
#endif

/* Page variables */
IMPLEMENT_GETTER (gchar *, useragent)
{
    gchar *useragent;

    g_object_get (webkit_settings (),
        "user-agent", &useragent,
        NULL);

    return useragent;
}

IMPLEMENT_SETTER (gchar *, useragent)
{
    if (!useragent || !*useragent) {
        return FALSE;
    }

    g_object_set (webkit_settings (),
        "user-agent", useragent,
        NULL);

    return TRUE;
}

IMPLEMENT_SETTER (gchar *, accept_languages)
{
    if (!*accept_languages || *accept_languages == ' ') {
        return FALSE;
    }

    uzbl.variables->priv->accept_languages = g_strdup (accept_languages);

    gchar **languages = g_strsplit (uzbl.variables->priv->accept_languages, ",", 0);

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    webkit_web_context_set_preferred_languages (context, (const gchar * const *)languages);

    g_strfreev (languages);

    return TRUE;
}

GOBJECT_GETSET2 (gdouble, zoom_level,
                 gfloat, webkit_view (), "zoom-level")

IMPLEMENT_SETTER (gdouble, zoom_step)
{
    if (zoom_step < 0) {
        return FALSE;
    }

    uzbl.variables->priv->zoom_step = zoom_step;

    return TRUE;
}

GOBJECT_GETSET2 (int, zoom_text_only,
                 gboolean, webkit_settings (), "zoom-text-only")

GOBJECT_GETSET2 (int, caret_browsing,
                 gboolean, webkit_settings (), "enable-caret-browsing")

GOBJECT_GETSET2 (int, enable_frame_flattening,
                 gboolean, webkit_settings (), "enable-frame-flattening")

GOBJECT_GETSET2 (int, enable_smooth_scrolling,
                 gboolean, webkit_settings (), "enable-smooth-scrolling")

#ifdef HAVE_PAGE_VIEW_MODE
#define page_view_mode_choices(call)   \
    call (WEBKIT_VIEW_MODE_WEB, "web") \
    call (WEBKIT_VIEW_MODE_SOURCE, "source")

#define _webkit_web_view_get_page_view_mode() \
    webkit_web_view_get_view_mode (uzbl.gui.web_view)
#define _webkit_web_view_set_page_view_mode(val) \
    webkit_web_view_set_view_mode (uzbl.gui.web_view, val)

typedef WebKitViewMode page_view_mode_t;

CHOICE_GETSET (page_view_mode_t, page_view_mode,
               _webkit_web_view_get_page_view_mode, _webkit_web_view_set_page_view_mode)

#undef _webkit_web_view_get_page_view_mode
#undef _webkit_web_view_set_page_view_mode

#undef page_view_mode_choices
#endif

GOBJECT_GETSET2 (int, enable_fullscreen,
                 gboolean, webkit_settings (), "enable-fullscreen")

#ifdef HAVE_EDITABLE
GOBJECT_GETSET2 (int, editable,
                 gboolean, webkit_view (), "editable")
#endif

/* Javascript variables */
GOBJECT_GETSET2 (int, enable_scripts,
                 gboolean, webkit_settings (), "enable-javascript")

GOBJECT_GETSET2 (int, javascript_windows,
                 gboolean, webkit_settings (), "javascript-can-open-windows-automatically")

GOBJECT_GETSET2 (int, javascript_modal_dialogs,
                 gboolean, webkit_settings (), "allow-modal-dialogs")

GOBJECT_GETSET2 (int, javascript_clipboard,
                 gboolean, webkit_settings (), "javascript-can-access-clipboard")

GOBJECT_GETSET2 (int, javascript_console_to_stdout,
                 gboolean, webkit_settings (), "enable-write-console-messages-to-stdout")

/* Image variables */
GOBJECT_GETSET2 (int, autoload_images,
                 gboolean, webkit_settings (), "auto-load-images")

GOBJECT_GETSET2 (int, always_load_icons,
                 gboolean, webkit_settings (), "load-icons-ignoring-image-load-setting")

/* Spell checking variables */
IMPLEMENT_GETTER (int, enable_spellcheck)
{
    return webkit_web_context_get_spell_checking_enabled (webkit_web_view_get_context (uzbl.gui.web_view));
}

IMPLEMENT_SETTER (int, enable_spellcheck)
{
    webkit_web_context_set_spell_checking_enabled (webkit_web_view_get_context (uzbl.gui.web_view), enable_spellcheck);

    return TRUE;
}

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

    return TRUE;
}

/* Form variables */
GOBJECT_GETSET2 (int, resizable_text_areas,
                 gboolean, webkit_settings (), "enable-resizable-text-areas")

GOBJECT_GETSET2 (int, enable_spatial_navigation,
                 gboolean, webkit_settings (), "enable-spatial-navigation")

GOBJECT_GETSET2 (int, enable_tab_cycle,
                 gboolean, webkit_settings (), "enable-tabs-to-links")

/* Text variables */
GOBJECT_GETSET (gchar *, default_encoding,
                webkit_settings (), "default-charset")

IMPLEMENT_GETTER (gchar *, custom_encoding)
{
    const gchar *encoding = webkit_web_view_get_custom_charset (uzbl.gui.web_view);

    if (!encoding) {
        return g_strdup ("");
    }

    return g_strdup (encoding);
}

IMPLEMENT_SETTER (gchar *, custom_encoding)
{
    if (!*custom_encoding) {
        custom_encoding = NULL;
    }

    webkit_web_view_set_custom_charset (uzbl.gui.web_view, custom_encoding);

    return TRUE;
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

GOBJECT_GETSET (gchar *, pictograph_font_family,
                webkit_settings (), "pictograph-font-family")

/* Font size variables */

GOBJECT_GETSET (int, font_size,
                webkit_settings (), "default-font-size")

GOBJECT_GETSET (int, monospace_size,
                webkit_settings (), "default-monospace-font-size")

/* Feature variables */
GOBJECT_GETSET2 (int, enable_plugins,
                 gboolean, webkit_settings (), "enable-plugins")

GOBJECT_GETSET2 (int, enable_java_applet,
                 gboolean, webkit_settings (), "enable-java")

GOBJECT_GETSET2 (int, enable_webgl,
                 gboolean, webkit_settings (), "enable-webgl")

GOBJECT_GETSET2 (int, enable_webaudio,
                 gboolean, webkit_settings (), "enable-webaudio")

GOBJECT_GETSET2 (int, enable_2d_acceleration,
                 gboolean, webkit_settings (), "enable-accelerated-2d-canvas")

GOBJECT_GETSET2 (int, enable_inline_media,
                 gboolean, webkit_settings (), "media-playback-allows-inline")

GOBJECT_GETSET2 (int, require_click_to_play,
                 gboolean, webkit_settings (), "media-playback-requires-user-gesture")

GOBJECT_GETSET2 (int, enable_media_stream,
                 gboolean, webkit_settings (), "enable-media-stream")

GOBJECT_GETSET2 (int, enable_media_source,
                 gboolean, webkit_settings (), "enable-mediasource")

/* HTML5 Database variables */
GOBJECT_GETSET2 (int, enable_database,
                 gboolean, webkit_settings (), "enable-html5-database")

GOBJECT_GETSET2 (int, enable_local_storage,
                 gboolean, webkit_settings (), "enable-html5-local-storage")

GOBJECT_GETSET2 (int, enable_pagecache,
                 gboolean, webkit_settings (), "enable-page-cache")

GOBJECT_GETSET2 (int, enable_offline_app_cache,
                 gboolean, webkit_settings (), "enable-offline-web-application-cache")

#if WEBKIT_CHECK_VERSION (2, 9, 4)
GOBJECT_GETTER (gchar *, local_storage_path,
                webkit_data_manager (), "local-storage-directory")
#elif WEBKIT_CHECK_VERSION (2, 7, 2)
GOBJECT_GETTER (gchar *, local_storage_path,
                webkit_context (), "local-storage-directory")
#endif

#if WEBKIT_CHECK_VERSION (2, 9, 4)
GOBJECT_GETTER (gchar *, disk_cache_directory,
                webkit_data_manager (), "disk-cache-directory")
#else
IMPLEMENT_SETTER (gchar *, disk_cache_directory)
{
    g_free (uzbl.variables->priv->disk_cache_directory);
    uzbl.variables->priv->disk_cache_directory = g_strdup (disk_cache_directory);

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    webkit_web_context_set_disk_cache_directory (context, uzbl.variables->priv->disk_cache_directory);

    return TRUE;
}
#endif

#if WEBKIT_CHECK_VERSION (2, 9, 4)
GOBJECT_GETTER (gchar *, indexed_db_directory,
                webkit_data_manager (), "indexeddb-directory")
#elif WEBKIT_CHECK_VERSION (2, 9, 2)
GOBJECT_GETSET (gchar *, indexed_db_directory,
                webkit_context (), "indexed-db-directory")
#endif

#if WEBKIT_CHECK_VERSION (2, 9, 4)
GOBJECT_GETTER (gchar *, base_cache_directory,
                webkit_data_manager (), "base-cache-directory");

GOBJECT_GETTER (gchar *, base_data_directory,
                webkit_data_manager (), "base-data-directory");

GOBJECT_GETTER (gchar *, offline_app_cache_directory,
                webkit_data_manager (), "offline-application-cache-directory");

GOBJECT_GETTER (gchar *, websql_directory,
                webkit_data_manager (), "websql-directory");
#endif

/* Hacks */
GOBJECT_GETSET2 (int, enable_site_workarounds,
                 gboolean, webkit_settings (), "enable-site-specific-quirks")

/* Constants */
IMPLEMENT_GETTER (gchar *, inspected_uri)
{
    return g_strdup (webkit_web_inspector_get_inspected_uri (uzbl.gui.inspector));
}

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

static void
plugin_list_append (WebKitPlugin *plugin, gpointer data);

IMPLEMENT_GETTER (gchar *, plugin_list)
{
    GList *plugins = NULL;

#if 0 /* TODO: Seems to hang... */
    {
        WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
        GError *err = NULL;

        uzbl_sync_call (plugins, context, err,
                        webkit_web_context_get_plugins);

        if (err) {
            /* TODO: Output message. */
            g_error_free (err);
        }
    }
#endif

    if (!plugins) {
        /* TODO: Don't ignore the error. */
        return g_strdup ("[]");
    }

    GString *list = g_string_new ("[");

    g_list_foreach (plugins, (GFunc)plugin_list_append, list);

    g_string_append_c (list, ']');

    g_list_free (plugins);

    return g_string_free (list, FALSE);
}

IMPLEMENT_GETTER (int, is_online)
{
    GNetworkMonitor *monitor = g_network_monitor_get_default ();
    return g_network_monitor_get_network_available (monitor);
}

#if WEBKIT_CHECK_VERSION (2, 7, 4)
GOBJECT_GETTER2 (int, is_playing_audio,
                 gboolean, webkit_view (), "is-playing-audio");
#endif

#if WEBKIT_CHECK_VERSION (2, 9, 4)
IMPLEMENT_GETTER (gchar *, editor_state)
{
    WebKitEditorState *editor_state;
    guint state;
    GString *state_str = g_string_new ("");

    editor_state = webkit_web_view_get_editor_state (uzbl.gui.web_view);
    state = webkit_editor_state_get_typing_attributes (editor_state);

    if (!state) {
        g_string_append (state_str, "none");
    } else {
        guint state_mask = 0;

#define webkit_editor_state_attributes(call)                        \
    call(WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD,          "bold")      \
    call(WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC,        "italic")    \
    call(WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE,     "underline") \
    call(WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH, "strikethrough")

#define append_flag(flag, str)                  \
    if (state & flag) {                         \
        state_mask |= flag;                     \
        if (state_str->len) {                   \
            g_string_append_c (state_str, ','); \
        }                                       \
        g_string_append (state_str, str);       \
    }
        webkit_editor_state_attributes(append_flag)

        /* Find any unknown flags. */
        state ^= state_mask;
        append_flag(G_MAXUINT, "unknown")
#undef append_flag

#undef webkit_editor_state_attributes
    }

    return g_string_free (state_str, FALSE);
}
#endif

static void
mimetype_list_append (WebKitMimeInfo *mimetype, GString *list);

void
plugin_list_append (WebKitPlugin *plugin, gpointer data)
{
    GString *list = (GString *)data;

    if (list->str[list->len - 1] != '[') {
        g_string_append (list, ", ");
    }

    typedef GList MIMETypeList;

    const gchar *desc = NULL;
    MIMETypeList *mimetypes = NULL;
    const gchar *name = NULL;
    const gchar *path = NULL;

    desc = webkit_plugin_get_description (plugin);
    mimetypes = webkit_plugin_get_mime_info_list (plugin);
    name = webkit_plugin_get_name (plugin);
    path = webkit_plugin_get_path (plugin);

    /* Write out a JSON representation of the information */
    g_string_append_printf (list,
            "{\"name\": \"%s\", "
            "\"description\": \"%s\", "
            "\"path\": \"%s\", "
            "\"mimetypes\": [", /* Open array for the mimetypes */
            name,
            desc,
            path);

    g_list_foreach (mimetypes, (GFunc)mimetype_list_append, list);

    g_object_unref (plugin);

    /* Close the array and the object */
    g_string_append (list, "]}");
}

void
mimetype_list_append (WebKitMimeInfo *mimetype, GString *list)
{
    if (list->str[list->len - 1] != '[') {
        g_string_append (list, ", ");
    }

    const gchar *name = NULL;
    const gchar *desc = NULL;
    const gchar * const *extensions = NULL;

    name = webkit_mime_info_get_mime_type (mimetype);
    desc = webkit_mime_info_get_description (mimetype);
    extensions = webkit_mime_info_get_extensions (mimetype);

    /* Write out a JSON representation of the information. */
    g_string_append_printf (list,
            "{\"name\": \"%s\", "
            "\"description\": \"%s\", "
            "\"extensions\": [", /* Open array for the extensions. */
            name,
            desc);

    gboolean first = TRUE;

    while (extensions && *extensions) {
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

IMPLEMENT_GETTER (int, WEBKIT_MAJOR)
{
    return webkit_get_major_version ();
}

IMPLEMENT_GETTER (int, WEBKIT_MINOR)
{
    return webkit_get_minor_version ();
}

IMPLEMENT_GETTER (int, WEBKIT_MICRO)
{
    return webkit_get_micro_version ();
}

IMPLEMENT_GETTER (int, WEBKIT_MAJOR_COMPILE)
{
    return WEBKIT_MAJOR_VERSION;
}

IMPLEMENT_GETTER (int, WEBKIT_MINOR_COMPILE)
{
    return WEBKIT_MINOR_VERSION;
}

IMPLEMENT_GETTER (int, WEBKIT_MICRO_COMPILE)
{
    return WEBKIT_MICRO_VERSION;
}

IMPLEMENT_GETTER (int, WEBKIT_UA_MAJOR)
{
    return 0; /* TODO: What is this in WebKit2? */
}

IMPLEMENT_GETTER (int, WEBKIT_UA_MINOR)
{
    return 0; /* TODO: What is this in WebKit2? */
}

IMPLEMENT_GETTER (int, HAS_WEBKIT2)
{
    return TRUE;
}

IMPLEMENT_GETTER (gchar *, ARCH_UZBL)
{
    return g_strdup (ARCH);
}

IMPLEMENT_GETTER (gchar *, COMMIT)
{
    return g_strdup (COMMIT);
}

IMPLEMENT_GETTER (int, PID)
{
    return (int)getpid ();
}

#if WEBKIT_CHECK_VERSION (2, 7, 2) && !WEBKIT_CHECK_VERSION (2, 9, 2)
GObject *
webkit_context ()
{
    return G_OBJECT (webkit_web_view_get_context (uzbl.gui.web_view));
}
#endif

#if WEBKIT_CHECK_VERSION (2, 9, 4)
GObject *
webkit_data_manager ()
{
    return G_OBJECT (webkit_web_context_get_website_data_manager (
        webkit_web_view_get_context (uzbl.gui.web_view)));
}
#endif

GObject *
webkit_settings ()
{
    return G_OBJECT (webkit_web_view_get_settings (uzbl.gui.web_view));
}

GObject *
webkit_view ()
{
    return G_OBJECT (uzbl.gui.web_view);
}


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
