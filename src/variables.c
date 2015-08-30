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
            unsigned long long ull = strtoull (val, NULL, 10);
            sendev = set_variable_ull (var, ull);
            break;
        }
        case TYPE_DOUBLE:
        {
            gdouble d = strtod (val, NULL);
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

        if (values && values->len) {
            gchar *current = get_variable_string (var);

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

            g_free (current);
        } else {
            next = "";

            if (!strcmp (*var->value.s, "0")) {
                next = "1";
            } else if (!strcmp (*var->value.s, "1")) {
                next = "0";
            }
        }

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
    if ((variable->type == TYPE_STR) && variable->value.s) {
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
                variable_expand (get_variable (ret), buf);

                p = vend;
                break;
            case EXPAND_SHELL:
            {
                if (stage == EXPAND_IGNORE_SHELL) {
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

                    g_string_append (buf, spawn_ret->str);
                }
                g_string_free (spawn_ret, TRUE);

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
                    g_string_append (buf, uzbl_ret->str);
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
                if (stage == ignore) {
                    break;
                }

                GString *js_ret = g_string_new ("");

                GArray *tmp = uzbl_commands_args_new ();
                uzbl_commands_args_append (tmp, g_strdup (js_ctx));
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

                uzbl_commands_args_append (tmp, g_strdup (source));

                gchar *exp_cmd = expand_impl (cmd, ignore);
                g_array_append_val (tmp, exp_cmd);

                uzbl_commands_run_argv ("js", tmp, js_ret);

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
                gchar *exp_cmd = expand_impl (ret, EXPAND_INITIAL);
                gchar *escaped = g_markup_escape_text (exp_cmd, strlen (exp_cmd));

                g_string_append (buf, escaped);

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
        g_string_append_printf (buf, "%g", get_variable_double (var));
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

#ifdef USE_WEBKIT2
#if !WEBKIT_CHECK_VERSION (2, 5, 1)
#define HAVE_PAGE_VIEW_MODE
#endif
#else
#define HAVE_PAGE_VIEW_MODE
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 3, 2)
#define HAVE_ENABLE_MEDIA_STREAM_API
#endif
#else
#if WEBKIT_CHECK_VERSION (1, 11, 1)
#define HAVE_ENABLE_MEDIA_STREAM_API
#endif
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 3, 3)
#define HAVE_SPATIAL_NAVIGATION
#endif
#else
#define HAVE_SPATIAL_NAVIGATION
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 7, 2)
#define HAVE_LOCAL_STORAGE_PATH
#endif
#else
#if WEBKIT_CHECK_VERSION (1, 5, 2)
#define HAVE_LOCAL_STORAGE_PATH
#endif
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 7, 4)
#define HAVE_EDITABLE
#endif
#else
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

/* Handler variables */
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, enable_builtin_auth);
#endif

/* Window variables */
DECLARE_SETTER (gchar *, icon);
DECLARE_SETTER (gchar *, icon_name);
DECLARE_GETSET (gchar *, window_role);
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, auto_resize_window);
#endif

/* UI variables */
DECLARE_SETTER (int, show_status);
DECLARE_SETTER (int, status_top);
DECLARE_SETTER (gchar *, status_background);
#ifdef USE_WEBKIT2
DECLARE_GETSET (int, enable_compositing_debugging);
#if WEBKIT_CHECK_VERSION (2, 7, 4)
DECLARE_GETSET (gchar *, background_color);
#endif
#endif

/* Customization */
#if !WEBKIT_CHECK_VERSION (1, 9, 0)
DECLARE_GETSET (int, default_context_menu);
#endif

/* Printing variables */
DECLARE_GETSET (int, print_backgrounds);

/* Network variables */
#ifndef USE_WEBKIT2
DECLARE_GETSET (gchar *, proxy_url);
DECLARE_GETSET (int, max_conns);
DECLARE_GETSET (int, max_conns_host);
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
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 9, 1)
DECLARE_GETSET (int, allow_file_to_file_access);
#endif
#endif

/* Inspector variables */
#ifndef USE_WEBKIT2
DECLARE_GETSET (int, profile_js);
DECLARE_GETSET (int, profile_timeline);
#endif

/* Page variables */
DECLARE_GETSET (gchar *, useragent);
#ifndef USE_WEBKIT2
DECLARE_GETTER (gchar *, accept_languages);
#endif
DECLARE_SETTER (gchar *, accept_languages);
DECLARE_GETSET (gdouble, zoom_level);
#ifndef USE_WEBKIT2
DECLARE_GETTER (gdouble, zoom_step);
#endif
DECLARE_SETTER (gdouble, zoom_step);
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
#ifdef HAVE_PAGE_VIEW_MODE
DECLARE_GETSET (gchar *, page_view_mode);
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
#ifdef HAVE_EDITABLE
DECLARE_GETSET (int, editable);
#endif

/* Javascript variables */
DECLARE_GETSET (int, enable_scripts);
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
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 1, 1)
DECLARE_GETSET (int, javascript_console_to_stdout);
#endif
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
#ifdef HAVE_SPATIAL_NAVIGATION
DECLARE_GETSET (int, enable_spatial_navigation);
#endif
#ifndef USE_WEBKIT2
DECLARE_GETSET (gchar *, editing_behavior);
#endif
DECLARE_GETSET (int, enable_tab_cycle);

/* Text variables */
DECLARE_GETSET (gchar *, default_encoding);
DECLARE_GETSET (gchar *, custom_encoding);
#ifndef USE_WEBKIT2
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
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 1, 1)
DECLARE_GETSET (int, enable_2d_acceleration);
#endif
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 3)
DECLARE_GETSET (int, enable_inline_media);
DECLARE_GETSET (int, require_click_to_play);
#endif
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 1) && !WEBKIT_CHECK_VERSION (2, 3, 5)
DECLARE_GETSET (int, enable_css_shaders);
#endif
#endif
#ifdef HAVE_ENABLE_MEDIA_STREAM_API
DECLARE_GETSET (int, enable_media_stream);
#endif
#if WEBKIT_CHECK_VERSION (2, 3, 3)
DECLARE_GETSET (int, enable_media_source);
#endif

/* HTML5 Database variables */
DECLARE_GETSET (int, enable_database);
DECLARE_GETSET (int, enable_local_storage);
DECLARE_GETSET (int, enable_pagecache);
DECLARE_GETSET (int, enable_offline_app_cache);
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 3, 13)
DECLARE_GETSET (unsigned long long, app_cache_size);
#endif
DECLARE_GETSET (gchar *, web_database_directory);
DECLARE_GETSET (unsigned long long, web_database_quota);
#endif
#ifdef HAVE_LOCAL_STORAGE_PATH
DECLARE_GETSET (gchar *, local_storage_path);
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 92)
DECLARE_SETTER (gchar *, disk_cache_directory);
#endif
DECLARE_SETTER (gchar *, web_extensions_directory);
#if WEBKIT_CHECK_VERSION (2, 9, 2)
DECLARE_GETSET (gchar *, indexed_db_directory);
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 4)
DECLARE_GETTER (gchar *, disk_cache_directory);
DECLARE_GETSET (gchar *, base_cache_directory);
DECLARE_GETSET (gchar *, base_data_directory);
DECLARE_GETSET (gchar *, offline_app_cache_directory);
DECLARE_GETSET (gchar *, websql_directory);
#endif
#endif

/* Hacks */
DECLARE_GETSET (int, enable_site_workarounds);

/* Constants */
#if WEBKIT_CHECK_VERSION (1, 3, 17)
DECLARE_GETTER (gchar *, inspected_uri);
#endif
#ifndef USE_WEBKIT2
DECLARE_GETTER (gchar *, current_encoding);
#endif
DECLARE_GETTER (gchar *, geometry);
#ifdef HAVE_PLUGIN_API
DECLARE_GETTER (gchar *, plugin_list);
#endif
DECLARE_GETTER (int, is_online);
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 7, 4)
DECLARE_GETTER (int, is_playing_audio);
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 4)
DECLARE_GETTER (gchar *, editor_state);
#endif
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

    /* Customization */
#if WEBKIT_CHECK_VERSION (1, 9, 0)
    gboolean default_context_menu;
#endif

    /* Network variables */
#ifndef USE_WEBKIT2
    gchar *http_debug;
    SoupLogger *soup_logger;
#endif

    /* Security variables */
    gboolean permissive;
#ifndef USE_WEBKIT2
    gboolean maintain_history;
#endif

    /* Page variables */
    gboolean forward_keys;
#ifdef USE_WEBKIT2
    gchar *accept_languages;
    gdouble zoom_step;
#endif

    /* HTML5 Database variables */
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 9, 4)
    WebKitWebsiteDataManager *data_manager;
#elif WEBKIT_CHECK_VERSION (1, 11, 92)
    gchar *disk_cache_directory;
#endif
    gchar *web_extensions_directory;
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

        /* Handler variables */
#ifndef USE_WEBKIT2
        { "enable_builtin_auth",          UZBL_V_FUNC (enable_builtin_auth,                    INT)},
#endif

        /* Window variables */
        { "icon",                         UZBL_V_STRING (priv->icon,                           set_icon)},
        { "icon_name",                    UZBL_V_STRING (priv->icon_name,                      set_icon_name)},
        { "window_role",                  UZBL_V_FUNC (window_role,                            STR)},
#ifndef USE_WEBKIT2
        { "auto_resize_window",           UZBL_V_FUNC (auto_resize_window,                     INT)},
#endif

        /* UI variables */
        { "show_status",                  UZBL_V_INT (priv->show_status,                       set_show_status)},
        { "status_top",                   UZBL_V_INT (priv->status_top,                        set_status_top)},
        { "status_background",            UZBL_V_STRING (priv->status_background,              set_status_background)},
#ifdef USE_WEBKIT2
        { "enable_compositing_debugging", UZBL_V_FUNC (enable_compositing_debugging,           INT)},
#if WEBKIT_CHECK_VERSION (2, 7, 4)
        { "background_color",             UZBL_V_FUNC (background_color,                       STR)},
#endif
#endif

        /* Customization */
        { "default_context_menu",
#if WEBKIT_CHECK_VERSION (1, 9, 0)
                                          UZBL_V_INT (priv->default_context_menu,              NULL)
#else
                                          UZBL_V_FUNC (default_context_menu,                   INT)
#endif
                                          },

        /* Printing variables */
        { "print_backgrounds",            UZBL_V_FUNC (print_backgrounds,                      INT)},

        /* Network variables */
#ifndef USE_WEBKIT2
        { "proxy_url",                    UZBL_V_FUNC (proxy_url,                              STR)},
        { "max_conns",                    UZBL_V_FUNC (max_conns,                              INT)},
        { "max_conns_host",               UZBL_V_FUNC (max_conns_host,                         INT)},
        { "http_debug",                   UZBL_V_STRING (priv->http_debug,                     set_http_debug)},
        { "ssl_ca_file",                  UZBL_V_FUNC (ssl_ca_file,                            STR)},
#endif
        { "ssl_policy",                   UZBL_V_FUNC (ssl_policy,                             STR)},
        { "cache_model",                  UZBL_V_FUNC (cache_model,                            STR)},

        /* Security variables */
        { "enable_private",               UZBL_V_FUNC (enable_private,                         INT)},
        { "permissive",                   UZBL_V_INT (priv->permissive,                        NULL)},
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
        { "maintain_history",             UZBL_V_INT (priv->maintain_history,                  set_maintain_history)},
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 9, 1)
        { "allow_file_to_file_access",    UZBL_V_FUNC (allow_file_to_file_access,              INT)},
#endif
#endif

        /* Inspector variables */
#ifndef USE_WEBKIT2
        { "profile_js",                   UZBL_V_FUNC (profile_js,                             INT)},
        { "profile_timeline",             UZBL_V_FUNC (profile_timeline,                       INT)},
#endif

        /* Page variables */
        { "forward_keys",                 UZBL_V_INT (priv->forward_keys,                      NULL)},
        { "useragent",                    UZBL_V_FUNC (useragent,                              STR)},
        { "accept_languages",
#ifdef USE_WEBKIT2
                                          UZBL_V_STRING (priv->accept_languages,               set_accept_languages)
#else
                                          UZBL_V_FUNC (accept_languages,                       STR)
#endif
                                          },
        { "zoom_level",                   UZBL_V_FUNC (zoom_level,                             DOUBLE)},
        { "zoom_step",
#ifdef USE_WEBKIT2
                                          UZBL_V_DOUBLE (priv->zoom_step,                      set_zoom_step)
#else
                                          UZBL_V_FUNC (zoom_step,                              DOUBLE)
#endif
                                          },
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
#ifdef HAVE_PAGE_VIEW_MODE
        { "page_view_mode",               UZBL_V_FUNC (page_view_mode,                         STR)},
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
#ifdef HAVE_EDITABLE
        { "editable",                     UZBL_V_FUNC (editable,                               INT)},
#endif

        /* Javascript variables */
        { "enable_scripts",               UZBL_V_FUNC (enable_scripts,                         INT)},
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
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 1, 1)
        { "javascript_console_to_stdout", UZBL_V_FUNC (javascript_console_to_stdout,           INT)},
#endif
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
#ifdef HAVE_SPATIAL_NAVIGATION
        { "enable_spatial_navigation",    UZBL_V_FUNC (enable_spatial_navigation,              INT)},
#endif
#ifndef USE_WEBKIT2
        { "editing_behavior",             UZBL_V_FUNC (editing_behavior,                       STR)},
#endif
        { "enable_tab_cycle",             UZBL_V_FUNC (enable_tab_cycle,                       INT)},

        /* Text variables */
        { "default_encoding",             UZBL_V_FUNC (default_encoding,                       STR)},
        { "custom_encoding",              UZBL_V_FUNC (custom_encoding,                        STR)},
#ifndef USE_WEBKIT2
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
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 1, 1)
        { "enable_2d_acceleration",       UZBL_V_FUNC (enable_2d_acceleration,                 INT)},
#endif
#endif
#if WEBKIT_CHECK_VERSION (1, 9, 3)
        { "enable_inline_media",          UZBL_V_FUNC (enable_inline_media,                    INT)},
        { "require_click_to_play",        UZBL_V_FUNC (require_click_to_play,                  INT)},
#endif
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 1) && !WEBKIT_CHECK_VERSION (2, 3, 5)
        { "enable_css_shaders",           UZBL_V_FUNC (enable_css_shaders,                     INT)},
#endif
#endif
#ifdef HAVE_ENABLE_MEDIA_STREAM_API
        { "enable_media_stream",          UZBL_V_FUNC (enable_media_stream,                    INT)},
#endif
#if WEBKIT_CHECK_VERSION (2, 3, 3)
        { "enable_media_source",          UZBL_V_FUNC (enable_media_source,                    INT)},
#endif

        /* HTML5 Database variables */
        { "enable_database",              UZBL_V_FUNC (enable_database,                        INT)},
        { "enable_local_storage",         UZBL_V_FUNC (enable_local_storage,                   INT)},
        { "enable_pagecache",             UZBL_V_FUNC (enable_pagecache,                       INT)},
        { "enable_offline_app_cache",     UZBL_V_FUNC (enable_offline_app_cache,               INT)},
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 3, 13)
        { "app_cache_size",               UZBL_V_FUNC (app_cache_size,                         ULL)},
#endif
        { "web_database_directory",       UZBL_V_FUNC (web_database_directory,                 STR)},
        { "web_database_quota",           UZBL_V_FUNC (web_database_quota,                     ULL)},
#endif
#ifdef HAVE_LOCAL_STORAGE_PATH
        { "local_storage_path",           UZBL_V_FUNC (local_storage_path,                     STR)},
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 92)
        { "disk_cache_directory",
#if WEBKIT_CHECK_VERSION (2, 9, 4)
                                          UZBL_V_FUNC (disk_cache_directory,                   STR)
#else
                                          UZBL_V_STRING (priv->disk_cache_directory,           set_disk_cache_directory)
#endif
                                          },
#endif
        { "web_extensions_directory",     UZBL_V_STRING (priv->web_extensions_directory,       set_web_extensions_directory)},
#if WEBKIT_CHECK_VERSION (2, 9, 2)
        { "indexed_db_directory",         UZBL_V_FUNC (indexed_db_directory,                   STR)},
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 4)
        { "base_cache_directory",         UZBL_V_FUNC (base_cache_directory,                   STR)},
        { "base_data_directory",          UZBL_V_FUNC (base_data_directory,                    STR)},
        { "offline_app_cache_directory",  UZBL_V_FUNC (offline_app_cache_directory,            STR)},
        { "websql_directory",             UZBL_V_FUNC (websql_directory,                       STR)},
#endif
#endif

        /* Hacks */
        { "enable_site_workarounds",      UZBL_V_FUNC (enable_site_workarounds,                INT)},

        /* Constants */
#if WEBKIT_CHECK_VERSION (1, 3, 17)
        { "inspected_uri",                UZBL_C_FUNC (inspected_uri,                          STR)},
#endif
#ifndef USE_WEBKIT2
        { "current_encoding",             UZBL_C_FUNC (current_encoding,                       STR)},
#endif
        { "geometry",                     UZBL_C_FUNC (geometry,                               STR)},
#ifdef HAVE_PLUGIN_API
        { "plugin_list",                  UZBL_C_FUNC (plugin_list,                            STR)},
#endif
        { "is_online",                    UZBL_C_FUNC (is_online,                              INT)},
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 7, 4)
        { "is_playing_audio",             UZBL_C_FUNC (is_playing_audio,                       INT)},
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 4)
        { "editor_state",                 UZBL_C_FUNC (editor_state,                           STR)},
#endif
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

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 9, 4)
    priv->data_manager = webkit_website_data_manager_new (NULL);
#endif
#endif

    return priv;
}

void
uzbl_variables_private_free (UzblVariablesPrivate *priv)
{
#if GTK_CHECK_VERSION (3, 15, 0)
    g_object_unref (priv->status_background_provider);
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 9, 4)
    g_object_unref (priv->data_manager);
#endif
#endif

    /* All other members are deleted by the table's free function. */
    g_free (priv);
}

void
uzbl_variables_setup_data_manager ()
{
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 9, 4)
    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    WebKitWebsiteDataManager *manager = uzbl.variables->priv->data_manager;

    g_object_set (G_OBJECT (context),
        "website-data-manager", manager,
        NULL);
#endif
#endif
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

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 7, 2) && !WEBKIT_CHECK_VERSION (2, 9, 2)
static GObject *
webkit_context ();
#endif
#if WEBKIT_CHECK_VERSION (2, 9, 4)
static GObject *
webkit_data_manager ();
#endif
#endif
static GObject *
webkit_settings ();
static GObject *
webkit_view ();
#ifndef USE_WEBKIT2
static GObject *
soup_session ();
static GObject *
inspector ();
static int
object_get (GObject *obj, const gchar *prop);
#endif

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

    if (enable_builtin_auth) {
        if (!auth) {
            soup_session_add_feature_by_type (
                uzbl.net.soup_session, (GType)WEBKIT_TYPE_SOUP_AUTH_DIALOG);
        }
    } else if (auth) {
        soup_session_remove_feature (uzbl.net.soup_session, auth);
    }

    return TRUE;
}
#endif

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

#ifndef USE_WEBKIT2
GOBJECT_GETSET2 (int, auto_resize_window,
                 gboolean, webkit_settings (), "auto-resize-window")
#endif

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
#elif GTK_CHECK_VERSION (2, 91, 0)
    GdkRGBA color;
    parsed = gdk_rgba_parse (&color, status_background);
    if (parsed) {
        gtk_widget_override_background_color (widget, GTK_STATE_NORMAL, &color);
    }
#else
    GdkColor color;
    parsed = gdk_color_parse (status_background, &color);
    if (parsed) {
        gtk_widget_modify_bg (widget, GTK_STATE_NORMAL, &color);
    }
#endif

    if (!parsed) {
        return FALSE;
    }

    g_free (uzbl.variables->priv->status_background);
    uzbl.variables->priv->status_background = g_strdup (status_background);

    return TRUE;
}

#ifdef USE_WEBKIT2
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
#endif

/* Customization */
#if !WEBKIT_CHECK_VERSION (1, 9, 0)
GOBJECT_GETSET2 (int, default_context_menu,
                 gboolean, webkit_settings (), "enable-default-context-menu")
#endif

/* Printing variables */
GOBJECT_GETSET2 (int, print_backgrounds,
                 gboolean, webkit_settings (), "print-backgrounds")

/* Network variables */
#ifndef USE_WEBKIT2
IMPLEMENT_GETTER (gchar *, proxy_url)
{
    SoupURI *soup_uri = NULL;

    g_object_get (soup_session (),
        SOUP_SESSION_PROXY_URI, &soup_uri,
        NULL);

    if (!soup_uri) {
        return g_strdup ("");
    }

    gchar *proxy_url = soup_uri_to_string (soup_uri, TRUE);

    soup_uri_free (soup_uri);

    return proxy_url;
}

IMPLEMENT_SETTER (gchar *, proxy_url)
{
    SoupURI *soup_uri = NULL;

    if (proxy_url && *proxy_url && *proxy_url != ' ') {
        soup_uri = soup_uri_new (proxy_url);
    }

    if (!soup_uri) {
        return FALSE;
    }

    g_object_set (soup_session (),
        SOUP_SESSION_PROXY_URI, soup_uri,
        NULL);

    soup_uri_free (soup_uri);

    return TRUE;
}

GOBJECT_GETSET (int, max_conns,
                soup_session (), SOUP_SESSION_MAX_CONNS)

GOBJECT_GETSET (int, max_conns_host,
                soup_session (), SOUP_SESSION_MAX_CONNS_PER_HOST)

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
        return FALSE;
    }

#undef http_debug_choices

    g_free (uzbl.variables->priv->http_debug);
    uzbl.variables->priv->http_debug = g_strdup (http_debug);

    if (uzbl.variables->priv->soup_logger) {
        soup_session_remove_feature (
            uzbl.net.soup_session, SOUP_SESSION_FEATURE (uzbl.variables->priv->soup_logger));
        g_object_unref (uzbl.variables->priv->soup_logger);
    }

    uzbl.variables->priv->soup_logger = soup_logger_new (out, -1);
    soup_session_add_feature (
        uzbl.net.soup_session, SOUP_SESSION_FEATURE (uzbl.variables->priv->soup_logger));

    return TRUE;
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
        g_setenv (priv_envvar, "true", 1);
    } else {
        g_unsetenv (priv_envvar);
    }

    set_enable_private_webkit (enable_private);

    return TRUE;
}

GOBJECT_GETSET2 (int, enable_private_webkit,
                 gboolean, webkit_settings (), "enable-private-browsing")

#ifndef USE_WEBKIT2
GOBJECT_GETSET2 (int, enable_universal_file_access,
                 gboolean, webkit_settings (), "enable-universal-access-from-file-uris")

GOBJECT_GETSET2 (int, enable_cross_file_access,
                 gboolean, webkit_settings (), "enable-file-access-from-file-uris")
#endif

GOBJECT_GETSET2 (int, enable_hyperlink_auditing,
                 gboolean, webkit_settings (), "enable-hyperlink-auditing")

#ifdef USE_WEBKIT2
GOBJECT_GETSET2 (int, enable_xss_auditing,
                 gboolean, webkit_settings (), "enable-xss-auditor")
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
GOBJECT_GETSET2 (int, enable_dns_prefetch,
                 gboolean, webkit_settings (), "enable-dns-prefetching")
#endif

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 2)
GOBJECT_GETSET2 (int, display_insecure_content,
                 gboolean, webkit_settings (), "enable-display-of-insecure-content")

GOBJECT_GETSET2 (int, run_insecure_content,
                 gboolean, webkit_settings (), "enable-running-of-insecure-content")
#endif

IMPLEMENT_SETTER (int, maintain_history)
{
    uzbl.variables->priv->maintain_history = maintain_history;

    webkit_web_view_set_maintains_back_forward_list (uzbl.gui.web_view, maintain_history);

    return TRUE;
}
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 9, 1)
GOBJECT_GETSET2 (int, allow_file_to_file_access,
                 gboolean, webkit_settings (), "allow-file-access-from-file-urls");
#endif
#endif

/* Inspector variables */
#ifndef USE_WEBKIT2
GOBJECT_GETSET2 (int, profile_js,
                 gboolean, inspector (), "javascript-profiling-enabled")

GOBJECT_GETSET2 (int, profile_timeline,
                 gboolean, inspector (), "timeline-profiling-enabled")
#endif

/* Page variables */
IMPLEMENT_GETTER (gchar *, useragent)
{
    gchar *useragent;

#ifdef USE_WEBKIT2
    g_object_get (webkit_settings (),
        "user-agent", &useragent,
        NULL);
#else
    g_object_get (soup_session (),
        SOUP_SESSION_USER_AGENT, &useragent,
        NULL);
#endif

    return useragent;
}

IMPLEMENT_SETTER (gchar *, useragent)
{
    if (!useragent || !*useragent) {
        return FALSE;
    }

#ifndef USE_WEBKIT2
    g_object_set (soup_session (),
        SOUP_SESSION_USER_AGENT, useragent,
        NULL);
#endif
    g_object_set (webkit_settings (),
        "user-agent", useragent,
        NULL);

    return TRUE;
}

#ifdef USE_WEBKIT2
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
#else
IMPLEMENT_GETTER (gchar *, accept_languages)
{
    gboolean is_auto;

    g_object_get (soup_session (),
        SOUP_SESSION_ACCEPT_LANGUAGE_AUTO, &is_auto,
        NULL);

    if (is_auto) {
        return g_strdup ("auto");
    }

    gchar *accept_languages;

    g_object_get (soup_session (),
        SOUP_SESSION_ACCEPT_LANGUAGE, &accept_languages,
        NULL);

    return accept_languages;
}

IMPLEMENT_SETTER (gchar *, accept_languages)
{
    if (!g_strcmp0 (accept_languages, "auto")) {
        g_object_set (soup_session (),
            SOUP_SESSION_ACCEPT_LANGUAGE_AUTO, TRUE,
            NULL);

        return TRUE;
    }

    g_object_set (soup_session (),
        SOUP_SESSION_ACCEPT_LANGUAGE, accept_languages,
        NULL);

    return TRUE;
}
#endif

GOBJECT_GETSET2 (gdouble, zoom_level,
                 gfloat, webkit_view (), "zoom-level")

#ifndef USE_WEBKIT2
GOBJECT_GETTER2 (gdouble, zoom_step,
                 gfloat, webkit_settings (), "zoom-step")
#endif

IMPLEMENT_SETTER (gdouble, zoom_step)
{
    if (zoom_step < 0) {
        return FALSE;
    }

#ifdef USE_WEBKIT2
    uzbl.variables->priv->zoom_step = zoom_step;
#else
    gfloat zoom_stepf = zoom_step;
    g_object_set (webkit_settings (),
        "zoom-step", zoom_stepf,
        NULL);
#endif

    return TRUE;
}

#ifdef HAVE_ZOOM_TEXT_API
#ifdef USE_WEBKIT2
GOBJECT_GETSET2 (int, zoom_text_only,
                 gboolean, webkit_settings (), "zoom-text-only")

#else
IMPLEMENT_GETTER (int, zoom_text_only)
{
    return !webkit_web_view_get_full_content_zoom (uzbl.gui.web_view);
}

IMPLEMENT_SETTER (int, zoom_text_only)
{
    webkit_web_view_set_full_content_zoom (uzbl.gui.web_view, !zoom_text_only);

    return TRUE;
}
#endif
#endif

GOBJECT_GETSET2 (int, caret_browsing,
                 gboolean, webkit_settings (), "enable-caret-browsing")

#if WEBKIT_CHECK_VERSION (1, 3, 5)
GOBJECT_GETSET2 (int, enable_frame_flattening,
                 gboolean, webkit_settings (), "enable-frame-flattening")
#endif

#if WEBKIT_CHECK_VERSION (1, 9, 0)
GOBJECT_GETSET2 (int, enable_smooth_scrolling,
                 gboolean, webkit_settings (), "enable-smooth-scrolling")
#endif

#ifdef HAVE_PAGE_VIEW_MODE
#ifdef USE_WEBKIT2
#define page_view_mode_choices(call)   \
    call (WEBKIT_VIEW_MODE_WEB, "web") \
    call (WEBKIT_VIEW_MODE_SOURCE, "source")

#define _webkit_web_view_get_page_view_mode() \
    webkit_web_view_get_view_mode (uzbl.gui.web_view)
#define _webkit_web_view_set_page_view_mode(val) \
    webkit_web_view_set_view_mode (uzbl.gui.web_view, val)

typedef WebKitViewMode page_view_mode_t;
#else
#define page_view_mode_choices(call) \
    call (TRUE, "source")            \
    call (FALSE, "web")

#define _webkit_web_view_get_page_view_mode() \
    webkit_web_view_get_view_source_mode (uzbl.gui.web_view)
#define _webkit_web_view_set_page_view_mode(val) \
    webkit_web_view_set_view_source_mode (uzbl.gui.web_view, val)

typedef gboolean page_view_mode_t;
#endif

CHOICE_GETSET (page_view_mode_t, page_view_mode,
               _webkit_web_view_get_page_view_mode, _webkit_web_view_set_page_view_mode)

#undef _webkit_web_view_get_page_view_mode
#undef _webkit_web_view_set_page_view_mode

#undef page_view_mode_choices
#endif

#ifndef USE_WEBKIT2
GOBJECT_GETSET2 (int, transparent,
                 gboolean, webkit_view (), "transparent")

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
GOBJECT_GETSET2 (int, enable_fullscreen,
                 gboolean, webkit_settings (), "enable-fullscreen")
#endif

#ifdef HAVE_EDITABLE
GOBJECT_GETSET2 (int, editable,
                 gboolean, webkit_view (), "editable")
#endif

/* Javascript variables */
GOBJECT_GETSET2 (int, enable_scripts,
                 gboolean, webkit_settings (),
#ifdef USE_WEBKIT2
                                    "enable-javascript"
#else
                                    "enable-scripts"
#endif
                )

GOBJECT_GETSET2 (int, javascript_windows,
                 gboolean, webkit_settings (), "javascript-can-open-windows-automatically")

#ifdef USE_WEBKIT2
GOBJECT_GETSET2 (int, javascript_modal_dialogs,
                 gboolean, webkit_settings (), "allow-modal-dialogs")
#endif

#ifndef USE_WEBKIT2
GOBJECT_GETSET2 (int, javascript_dom_paste,
                 gboolean, webkit_settings (), "enable-dom-paste")
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 0)
GOBJECT_GETSET2 (int, javascript_clipboard,
                 gboolean, webkit_settings (), "javascript-can-access-clipboard")
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 1, 1)
GOBJECT_GETSET2 (int, javascript_console_to_stdout,
                 gboolean, webkit_settings (), "enable-write-console-messages-to-stdout")
#endif
#endif

/* Image variables */
GOBJECT_GETSET2 (int, autoload_images,
                 gboolean, webkit_settings (), "auto-load-images")

#ifdef USE_WEBKIT2
GOBJECT_GETSET2 (int, always_load_icons,
                 gboolean, webkit_settings (), "load-icons-ignoring-image-load-setting")
#endif

#ifndef USE_WEBKIT2
GOBJECT_GETSET2 (int, autoshrink_images,
                 gboolean, webkit_settings (), "auto-shrink-images")

GOBJECT_GETSET2 (int, use_image_orientation,
                 gboolean, webkit_settings (), "respect-image-orientation")
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

    return TRUE;
}
#else
GOBJECT_GETSET2 (int, enable_spellcheck,
                 gboolean, webkit_settings (), "enable-spell-checking")
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

    return TRUE;
}
#else
GOBJECT_GETTER (gchar *, spellcheck_languages,
                webkit_settings (), "spell-checking-languages")

IMPLEMENT_SETTER (gchar *, spellcheck_languages)
{
    GObject *obj = webkit_get_text_checker ();

    if (!obj) {
        return FALSE;
    }
    if (!WEBKIT_IS_SPELL_CHECKER (obj)) {
        return FALSE;
    }

    WebKitSpellChecker *checker = WEBKIT_SPELL_CHECKER (obj);

    webkit_spell_checker_update_spell_checking_languages (checker, spellcheck_languages);
    g_object_set (webkit_settings (),
        "spell-checking-languages", spellcheck_languages,
        NULL);

    return TRUE;
}
#endif

/* Form variables */
GOBJECT_GETSET2 (int, resizable_text_areas,
                 gboolean, webkit_settings (),
#ifdef USE_WEBKIT2
                                    "enable-resizable-text-areas"
#else
                                    "resizable-text-areas"
#endif
                )

#ifdef HAVE_SPATIAL_NAVIGATION
GOBJECT_GETSET2 (int, enable_spatial_navigation,
                 gboolean, webkit_settings (), "enable-spatial-navigation")
#endif

#ifndef USE_WEBKIT2
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
GOBJECT_GETSET2 (int, enable_tab_cycle,
                 gboolean, webkit_settings (), "enable-tabs-to-links")
#else
GOBJECT_GETSET2 (int, enable_tab_cycle,
                 gboolean, webkit_settings (), "tab-key-cycles-through-elements")
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

#ifdef USE_WEBKIT2
    webkit_web_view_set_custom_charset (uzbl.gui.web_view, custom_encoding);
#else
    webkit_web_view_set_custom_encoding (uzbl.gui.web_view, custom_encoding);
#endif

    return TRUE;
}

#ifndef USE_WEBKIT2
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
GOBJECT_GETSET2 (int, enable_plugins,
                 gboolean, webkit_settings (), "enable-plugins")

#ifdef USE_WEBKIT2
GOBJECT_GETSET2 (int, enable_java_applet,
                 gboolean, webkit_settings (), "enable-java")
#else
GOBJECT_GETSET2 (int, enable_java_applet,
                 gboolean, webkit_settings (), "enable-java-applet")
#endif

#if WEBKIT_CHECK_VERSION (1, 3, 14)
GOBJECT_GETSET2 (int, enable_webgl,
                 gboolean, webkit_settings (), "enable-webgl")
#endif

#if WEBKIT_CHECK_VERSION (1, 7, 5)
GOBJECT_GETSET2 (int, enable_webaudio,
                 gboolean, webkit_settings (), "enable-webaudio")
#endif

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 7, 90) /* Documentation says 1.7.5, but it's not there. */
GOBJECT_GETSET2 (int, enable_3d_acceleration,
                 gboolean, webkit_settings (), "enable-accelerated-compositing")
#endif
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 1, 1)
GOBJECT_GETSET2 (int, enable_2d_acceleration,
                 gboolean, webkit_settings (), "enable-accelerated-2d-canvas")
#endif
#endif

#if WEBKIT_CHECK_VERSION (1, 9, 3)
GOBJECT_GETSET2 (int, enable_inline_media,
                 gboolean, webkit_settings (), "media-playback-allows-inline")

GOBJECT_GETSET2 (int, require_click_to_play,
                 gboolean, webkit_settings (), "media-playback-requires-user-gesture")
#endif

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 11, 1) && !WEBKIT_CHECK_VERSION (2, 3, 5)
GOBJECT_GETSET2 (int, enable_css_shaders,
                 gboolean, webkit_settings (), "enable-css-shaders")
#endif
#endif

#ifdef HAVE_ENABLE_MEDIA_STREAM_API
GOBJECT_GETSET2 (int, enable_media_stream,
                 gboolean, webkit_settings (), "enable-media-stream")
#endif

#if WEBKIT_CHECK_VERSION (2, 3, 3)
GOBJECT_GETSET2 (int, enable_media_source,
                 gboolean, webkit_settings (), "enable-mediasource")
#endif

/* HTML5 Database variables */
GOBJECT_GETSET2 (int, enable_database,
                 gboolean, webkit_settings (), "enable-html5-database")

GOBJECT_GETSET2 (int, enable_local_storage,
                 gboolean, webkit_settings (), "enable-html5-local-storage")

GOBJECT_GETSET2 (int, enable_pagecache,
                 gboolean, webkit_settings (), "enable-page-cache")

GOBJECT_GETSET2 (int, enable_offline_app_cache,
                 gboolean, webkit_settings (), "enable-offline-web-application-cache")

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 3, 13)
IMPLEMENT_GETTER (unsigned long long, app_cache_size)
{
    return webkit_application_cache_get_maximum_size ();
}

IMPLEMENT_SETTER (unsigned long long, app_cache_size)
{
    webkit_application_cache_set_maximum_size (app_cache_size);

    return TRUE;
}
#endif

IMPLEMENT_GETTER (gchar *, web_database_directory)
{
    return g_strdup (webkit_get_web_database_directory_path ());
}

IMPLEMENT_SETTER (gchar *, web_database_directory)
{
    webkit_set_web_database_directory_path (web_database_directory);

    return TRUE;
}

IMPLEMENT_GETTER (unsigned long long, web_database_quota)
{
    return webkit_get_default_web_database_quota ();
}

IMPLEMENT_SETTER (unsigned long long, web_database_quota)
{
    webkit_set_default_web_database_quota (web_database_quota);

    return TRUE;
}
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 9, 4)
GOBJECT_GETSET (gchar *, local_storage_path,
                webkit_data_manager (), "local-storage-directory")
#elif WEBKIT_CHECK_VERSION (2, 7, 2)
GOBJECT_GETSET (gchar *, local_storage_path,
                webkit_context (), "local-storage-directory")
#endif
#else
#if WEBKIT_CHECK_VERSION (1, 5, 2)
GOBJECT_GETSET (gchar *, local_storage_path,
                webkit_settings (), "html5-local-storage-database-path")
#endif
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 9, 4)
GOBJECT_GETSET (gchar *, disk_cache_directory,
                webkit_data_manager (), "disk-cache-directory")
#elif WEBKIT_CHECK_VERSION (1, 11, 92)
IMPLEMENT_SETTER (gchar *, disk_cache_directory)
{
    g_free (uzbl.variables->priv->disk_cache_directory);
    uzbl.variables->priv->disk_cache_directory = g_strdup (disk_cache_directory);

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    webkit_web_context_set_disk_cache_directory (context, uzbl.variables->priv->disk_cache_directory);

    return TRUE;
}
#endif

IMPLEMENT_SETTER (gchar *, web_extensions_directory)
{
    g_free (uzbl.variables->priv->web_extensions_directory);
    uzbl.variables->priv->web_extensions_directory = g_strdup (web_extensions_directory);

    WebKitWebContext *context = webkit_web_view_get_context (uzbl.gui.web_view);
    webkit_web_context_set_web_extensions_directory (context, uzbl.variables->priv->web_extensions_directory);

    return TRUE;
}

#if WEBKIT_CHECK_VERSION (2, 9, 4)
GOBJECT_GETSET (gchar *, indexed_db_directory,
                webkit_data_manager (), "indexeddb-directory")
#elif WEBKIT_CHECK_VERSION (2, 9, 2)
GOBJECT_GETSET (gchar *, indexed_db_directory,
                webkit_context (), "indexed-db-directory")
#endif

#if WEBKIT_CHECK_VERSION (2, 9, 4)
GOBJECT_GETSET (gchar *, base_cache_directory,
                webkit_data_manager (), "base-cache-directory");

GOBJECT_GETSET (gchar *, base_data_directory,
                webkit_data_manager (), "base-data-directory");

GOBJECT_GETSET (gchar *, offline_app_cache_directory,
                webkit_data_manager (), "offline-application-cache-directory");

GOBJECT_GETSET (gchar *, websql_directory,
                webkit_data_manager (), "websql-directory");
#endif
#endif

/* Hacks */
GOBJECT_GETSET2 (int, enable_site_workarounds,
                 gboolean, webkit_settings (), "enable-site-specific-quirks")

/* Constants */
#if WEBKIT_CHECK_VERSION (1, 3, 17)
IMPLEMENT_GETTER (gchar *, inspected_uri)
{
    return g_strdup (webkit_web_inspector_get_inspected_uri (uzbl.gui.inspector));
}
#endif

#ifndef USE_WEBKIT2
IMPLEMENT_GETTER (gchar *, current_encoding)
{
    const gchar *encoding = webkit_web_view_get_encoding (uzbl.gui.web_view);
    return g_strdup (encoding);
}
#endif

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

IMPLEMENT_GETTER (int, is_online)
{
    GNetworkMonitor *monitor = g_network_monitor_get_default ();
    return g_network_monitor_get_network_available (monitor);
}

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (2, 7, 4)
GOBJECT_GETTER2 (int, is_playing_audio,
                 gboolean, webkit_view (), "is-playing-audio");
#endif

#if WEBKIT_CHECK_VERSION (2, 9, 4)
IMPLEMENT_GETTER (gchar *, editor_state)
{
    WebKitEditorState *editor_state;
    guint state;
    gchar *state_str = NULL;

    g_object_get (webkit_view (),
        "editor-state", &editor_state,
        NULL);

    state = webkit_editor_state_get_typing_attributes (editor_state);

    if (!state) {
        state_str = g_strdup ("none");
    } else {
#define webkit_editor_state_attributes(call)                        \
    call(WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD,          "bold")      \
    call(WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC,        "italic")    \
    call(WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE,     "underline") \
    call(WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH, "strikethrough")

#define append_flag(flag, str)                                  \
    if (state & flag) {                                         \
        gchar *old_state_str = state_str;                       \
        if (state_str) {                                        \
            state_str = g_strdup_printf ("%s," str, state_str); \
        } else {                                                \
            state_str = g_strdup (str);                         \
        }                                                       \
        g_free (old_state_str);                                 \
    }
        webkit_editor_state_attributes(append_flag)
#undef append_flag

#undef webkit_editor_state_attributes
    }

    return state_str;
}
#endif
#endif

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
#endif

IMPLEMENT_GETTER (int, WEBKIT_MAJOR)
{
#ifdef USE_WEBKIT2
    return webkit_get_major_version ();
#else
    return webkit_major_version ();
#endif
}

IMPLEMENT_GETTER (int, WEBKIT_MINOR)
{
#ifdef USE_WEBKIT2
    return webkit_get_minor_version ();
#else
    return webkit_minor_version ();
#endif
}

IMPLEMENT_GETTER (int, WEBKIT_MICRO)
{
#ifdef USE_WEBKIT2
    return webkit_get_micro_version ();
#else
    return webkit_micro_version ();
#endif
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
#ifdef USE_WEBKIT2
    return 0; /* TODO: What is this in WebKit2? */
#else
    return WEBKIT_USER_AGENT_MAJOR_VERSION;
#endif
}

IMPLEMENT_GETTER (int, WEBKIT_UA_MINOR)
{
#ifdef USE_WEBKIT2
    return 0; /* TODO: What is this in WebKit2? */
#else
    return WEBKIT_USER_AGENT_MINOR_VERSION;
#endif
}

IMPLEMENT_GETTER (int, HAS_WEBKIT2)
{
#ifdef USE_WEBKIT2
    return TRUE;
#else
    return FALSE;
#endif
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

#ifdef USE_WEBKIT2
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
    return G_OBJECT (uzbl.variables->priv->data_manager);
}
#endif
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
