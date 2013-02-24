#include "commands.h"
#include "uzbl-core.h"
#include "events.h"
#include "gui.h"
#include "util.h"
#include "menu.h"
#include "variables.h"
#include "type.h"
#include "soup.h"

typedef void (*UzblCommand)(WebKitWebView*, GArray *argv, GString *result);

struct _UzblCommandInfo {
    const gchar *name;
    UzblCommand  function;
    gboolean     split;
};

static void        view_reload(WebKitWebView *page, GArray *argv, GString *result);
static void        view_reload_bypass_cache(WebKitWebView *page, GArray *argv, GString *result);
static void        view_stop_loading(WebKitWebView *page, GArray *argv, GString *result);
static void        view_zoom_in(WebKitWebView *page, GArray *argv, GString *result);
static void        view_zoom_out(WebKitWebView *page, GArray *argv, GString *result);
static void        view_go_back(WebKitWebView *page, GArray *argv, GString *result);
static void        view_go_forward(WebKitWebView *page, GArray *argv, GString *result);
static void        toggle_zoom_type (WebKitWebView* page, GArray *argv, GString *result);
static void        scroll_cmd(WebKitWebView* page, GArray *argv, GString *result);
static void        print(WebKitWebView *page, GArray *argv, GString *result);
static void        event(WebKitWebView *page, GArray *argv, GString *result);
static void        load_uri(WebKitWebView * web_view, GArray *argv, GString *result);
static void        chain(WebKitWebView *page, GArray *argv, GString *result);
static void        close_uzbl(WebKitWebView *page, GArray *argv, GString *result);
static void        spawn_async(WebKitWebView *web_view, GArray *argv, GString *result);
static void        spawn_sh_async(WebKitWebView *web_view, GArray *argv, GString *result);
static void        spawn_sync(WebKitWebView *web_view, GArray *argv, GString *result);
static void        spawn_sh_sync(WebKitWebView *web_view, GArray *argv, GString *result);
static void        spawn_sync_exec(WebKitWebView *web_view, GArray *argv, GString *result);
static void        search_forward_text (WebKitWebView *page, GArray *argv, GString *result);
static void        search_reverse_text (WebKitWebView *page, GArray *argv, GString *result);
static void        search_clear(WebKitWebView *page, GArray *argv, GString *result);
static void        dehilight (WebKitWebView *page, GArray *argv, GString *result);
static void        hardcopy(WebKitWebView *page, GArray *argv, GString *result);
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 9, 6)
static void        snapshot(WebKitWebView *page, GArray *argv, GString *result);
#endif
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 9, 90)
static void        load(WebKitWebView *page, GArray *argv, GString *result);
static void        save(WebKitWebView *page, GArray *argv, GString *result);
#endif
#endif
static void        remove_all_db(WebKitWebView *page, GArray *argv, GString *result);
#if WEBKIT_CHECK_VERSION (1, 3, 8)
static void        plugin_refresh(WebKitWebView *page, GArray *argv, GString *result);
static void        plugin_toggle(WebKitWebView *page, GArray *argv, GString *result);
#endif
static void        include(WebKitWebView *page, GArray *argv, GString *result);
/* Deprecated (use inspector instead) */
static void        show_inspector(WebKitWebView *page, GArray *argv, GString *result);
static void        inspector(WebKitWebView *page, GArray *argv, GString *result);
#if WEBKIT_CHECK_VERSION (1, 5, 1)
static void        spell_checker(WebKitWebView *page, GArray *argv, GString *result);
#endif
static void        add_cookie(WebKitWebView *page, GArray *argv, GString *result);
static void        delete_cookie(WebKitWebView *page, GArray *argv, GString *result);
static void        clear_cookies(WebKitWebView *pag, GArray *argv, GString *result);
static void        download(WebKitWebView *pag, GArray *argv, GString *result);
static void        set_var(WebKitWebView *page, GArray *argv, GString *result);
static void        toggle_var(WebKitWebView *page, GArray *argv, GString *result);
static void        run_js (WebKitWebView * web_view, GArray *argv, GString *result);
static void        toggle_zoom_type (WebKitWebView* page, GArray *argv, GString *result);
static void        toggle_status (WebKitWebView* page, GArray *argv, GString *result);
static void        act_dump_config(WebKitWebView* page, GArray *argv, GString *result);
static void        act_dump_config_as_events(WebKitWebView* page, GArray *argv, GString *result);
static void        auth(WebKitWebView* page, GArray *argv, GString *result);

UzblCommandInfo cmdlist[] =
{   /* name                             function                   split */
    { "back",                           view_go_back,              TRUE  },
    { "forward",                        view_go_forward,           TRUE  },
    { "scroll",                         scroll_cmd,                TRUE  },
    { "reload",                         view_reload,               TRUE  },
    { "reload_ign_cache",               view_reload_bypass_cache,  TRUE  },
    { "stop",                           view_stop_loading,         TRUE  },
    { "zoom_in",                        view_zoom_in,              TRUE  }, //Can crash (when max zoom reached?).
    { "zoom_out",                       view_zoom_out,             TRUE  },
    { "toggle_zoom_type",               toggle_zoom_type,          TRUE  },
    { "uri",                            load_uri,                  FALSE },
    { "js",                             run_js,                    FALSE },
    { "script",                         run_external_js,           TRUE  },
    { "toggle_status",                  toggle_status,             TRUE  },
    { "spawn",                          spawn_async,               TRUE  },
    { "sync_spawn",                     spawn_sync,                TRUE  },
    { "sync_spawn_exec",                spawn_sync_exec,           TRUE  }, // needed for load_cookies.sh :(
    { "sh",                             spawn_sh_async,            TRUE  },
    { "sync_sh",                        spawn_sh_sync,             TRUE  },
    { "exit",                           close_uzbl,                TRUE  },
    { "search",                         search_forward_text,       FALSE },
    { "search_reverse",                 search_reverse_text,       FALSE },
    { "search_clear",                   search_clear,              FALSE },
    { "dehilight",                      dehilight,                 TRUE  },
    { "set",                            set_var,                   FALSE },
    { "toggle",                         toggle_var,                TRUE  },
    { "dump_config",                    act_dump_config,           TRUE  },
    { "dump_config_as_events",          act_dump_config_as_events, TRUE  },
    { "chain",                          chain,                     TRUE  },
    { "print",                          print,                     FALSE },
    { "event",                          event,                     FALSE },
    { "request",                        event,                     FALSE },
    { "menu_add",                       menu_add,                  FALSE },
    { "menu_link_add",                  menu_add_link,             FALSE },
    { "menu_image_add",                 menu_add_image,            FALSE },
    { "menu_editable_add",              menu_add_edit,             FALSE },
    { "menu_separator",                 menu_add_separator,        FALSE },
    { "menu_link_separator",            menu_add_separator_link,   FALSE },
    { "menu_image_separator",           menu_add_separator_image,  FALSE },
    { "menu_editable_separator",        menu_add_separator_edit,   FALSE },
    { "menu_remove",                    menu_remove,               FALSE },
    { "menu_link_remove",               menu_remove_link,          FALSE },
    { "menu_image_remove",              menu_remove_image,         FALSE },
    { "menu_editable_remove",           menu_remove_edit,          FALSE },
    { "hardcopy",                       hardcopy,                  FALSE },
#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 9, 6)
    { "snapshot",                       snapshot,                  FALSE },
#endif
#endif
#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 9, 90)
    { "load",                           load,                      FALSE },
    { "save",                           save,                      FALSE },
#endif
#endif
    { "remove_all_db",                  remove_all_db,             TRUE  },
#if WEBKIT_CHECK_VERSION (1, 3, 8)
    { "plugin_refresh",                 plugin_refresh,            FALSE },
    { "plugin_toggle",                  plugin_toggle,             FALSE },
#endif
    { "include",                        include,                   FALSE },
    /* Deprecated */
    { "show_inspector",                 show_inspector,            TRUE  },
    { "inspector",                      inspector,                 FALSE },
#if WEBKIT_CHECK_VERSION (1, 5, 1)
    { "spell_checker",                  spell_checker,             FALSE },
#endif
    { "add_cookie",                     add_cookie,                TRUE  },
    { "delete_cookie",                  delete_cookie,             TRUE  },
    { "clear_cookies",                  clear_cookies,             TRUE  },
    { "download",                       download,                  TRUE  },
    { "auth",                           auth,                      TRUE  }
};

void
commands_hash() {
    unsigned int i;
    uzbl.behave.commands = g_hash_table_new(g_str_hash, g_str_equal);

    for (i = 0; i < LENGTH(cmdlist); i++)
        g_hash_table_insert(uzbl.behave.commands, (gpointer) cmdlist[i].name, &cmdlist[i]);
}

void
builtins() {
    unsigned int i;
    unsigned int len = LENGTH(cmdlist);
    GString*     command_list = g_string_new("");

    for (i = 0; i < len; i++) {
        g_string_append(command_list, cmdlist[i].name);
        g_string_append_c(command_list, ' ');
    }

    send_event(BUILTINS, NULL, TYPE_STR, command_list->str, NULL);
    g_string_free(command_list, TRUE);
}

gchar**
split_quoted(const gchar* src, const gboolean unquote) {
    /* split on unquoted space or tab, return array of strings;
       remove a layer of quotes and backslashes if unquote */
    if (!src) return NULL;

    gboolean dq = FALSE;
    gboolean sq = FALSE;
    GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
    GString *s = g_string_new ("");
    const gchar *p;
    gchar **ret;
    gchar *dup;
    for (p = src; *p != '\0'; p++) {
        if ((*p == '\\') && unquote && p[1]) g_string_append_c(s, *++p);
        else if (*p == '\\' && p[1]) { g_string_append_c(s, *p++);
                               g_string_append_c(s, *p); }
        else if ((*p == '"') && unquote && !sq) dq = !dq;
        else if (*p == '"' && !sq) { g_string_append_c(s, *p);
                                     dq = !dq; }
        else if ((*p == '\'') && unquote && !dq) sq = !sq;
        else if (*p == '\'' && !dq) { g_string_append_c(s, *p);
                                      sq = ! sq; }
        else if ((*p == ' ' || *p == '\t') && !dq && !sq) {
            dup = g_strdup(s->str);
            g_array_append_val(a, dup);
            g_string_truncate(s, 0);
        } else g_string_append_c(s, *p);
    }
    dup = g_strdup(s->str);
    g_array_append_val(a, dup);
    ret = (gchar**)a->data;
    g_array_free (a, FALSE);
    g_string_free (s, TRUE);
    return ret;
}

void
parse_command_arguments(const gchar *p, GArray *a, gboolean split) {
    if (!split && p) { /* pass the parameters through in one chunk */
        sharg_append(a, g_strdup(p));
        return;
    }

    gchar **par = split_quoted(p, TRUE);
    if (par) {
        guint i;
        for (i = 0; i < g_strv_length(par); i++)
            sharg_append(a, g_strdup(par[i]));
        g_strfreev (par);
    }
}

const UzblCommandInfo *
parse_command_parts(const gchar *line, GArray *a) {
    UzblCommandInfo *c = NULL;

    gchar *exp_line = expand(line, 0);
    if(exp_line[0] == '\0') {
        g_free(exp_line);
        return NULL;
    }

    /* separate the line into the command and its parameters */
    gchar **tokens = g_strsplit(exp_line, " ", 2);

    /* look up the command */
    c = g_hash_table_lookup(uzbl.behave.commands, tokens[0]);

    if(!c) {
        send_event(COMMAND_ERROR, NULL,
            TYPE_STR, exp_line,
            NULL);
        g_free(exp_line);
        g_strfreev(tokens);
        return NULL;
    }

    gchar *p = g_strdup(tokens[1]);
    g_free(exp_line);
    g_strfreev(tokens);

    /* parse the arguments */
    parse_command_arguments(p, a, c->split);
    g_free(p);

    return c;
}

void
run_parsed_command(const UzblCommandInfo *c, GArray *a, GString *result) {
    /* send the COMMAND_EXECUTED event, except for set and event/request commands */
    if(strcmp("set", c->name)   &&
       strcmp("event", c->name) &&
       strcmp("request", c->name)) {
        Event *event = format_event (COMMAND_EXECUTED, NULL,
            TYPE_NAME, c->name,
            TYPE_STR_ARRAY, a,
            NULL);

        /* might be destructive on array a */
        c->function(uzbl.gui.web_view, a, result);

        send_formatted_event (event);
        event_free (event);
    }
    else
        c->function(uzbl.gui.web_view, a, result);

    if(result) {
        g_free(uzbl.state.last_result);
        uzbl.state.last_result = g_strdup(result->str);
    }
}

void
parse_cmd_line(const char *ctl_line, GString *result) {
    gchar *work_string = g_strdup(ctl_line);

    /* strip trailing newline, and any other whitespace in front */
    g_strstrip(work_string);

    if( strcmp(work_string, "") ) {
        if((work_string[0] != '#')) { /* ignore comments */
            GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
            const UzblCommandInfo *c = parse_command_parts(work_string, a);
            if(c)
                run_parsed_command(c, a, result);
            g_array_free (a, TRUE);
        }
    }

    g_free(work_string);
}

/* VIEW funcs (little webkit wrappers) */
#define VIEWFUNC(name) void view_##name(WebKitWebView *page, GArray *argv, GString *result){(void)argv; (void)result; webkit_web_view_##name(page);}
VIEWFUNC(reload)
VIEWFUNC(reload_bypass_cache)
VIEWFUNC(stop_loading)
VIEWFUNC(zoom_in)
VIEWFUNC(zoom_out)
#undef VIEWFUNC

void
view_go_back(WebKitWebView *page, GArray *argv, GString *result) {
    (void)result;
    int n = argv_idx(argv, 0) ? atoi(argv_idx(argv, 0)) : 1;
    webkit_web_view_go_back_or_forward(page, -n);
}

void
view_go_forward(WebKitWebView *page, GArray *argv, GString *result) {
    (void)result;
    int n = argv_idx(argv, 0) ? atoi(argv_idx(argv, 0)) : 1;
    webkit_web_view_go_back_or_forward(page, n);
}

void
toggle_zoom_type (WebKitWebView* page, GArray *argv, GString *result) {
    (void)page; (void)argv; (void)result;

    int current_type = get_zoom_type();
    set_zoom_type(!current_type);
}

void
toggle_status (WebKitWebView* page, GArray *argv, GString *result) {
    (void)page; (void)argv; (void)result;

    int current_status = get_show_status();
    set_show_status(!current_status);
}

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
void
scroll_cmd(WebKitWebView* page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar *direction = g_array_index(argv, gchar*, 0);
    gchar *argv1     = g_array_index(argv, gchar*, 1);
    GtkAdjustment *bar = NULL;

    if (g_strcmp0(direction, "horizontal") == 0)
        bar = uzbl.gui.bar_h;
    else if (g_strcmp0(direction, "vertical") == 0)
        bar = uzbl.gui.bar_v;
    else {
        if(uzbl.state.verbose)
            puts("Unrecognized scroll format");
        return;
    }

    if (g_strcmp0(argv1, "begin") == 0)
        gtk_adjustment_set_value(bar, gtk_adjustment_get_lower(bar));
    else if (g_strcmp0(argv1, "end") == 0)
        gtk_adjustment_set_value (bar, gtk_adjustment_get_upper(bar) -
                                gtk_adjustment_get_page_size(bar));
    else
        scroll(bar, argv1);
}

void
set_var(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;

    if(!argv_idx(argv, 0))
        return;

    gchar **split = g_strsplit(argv_idx(argv, 0), "=", 2);
    if (split[0] != NULL) {
        gchar *value = split[1] ? g_strchug(split[1]) : " ";
        set_var_value(g_strstrip(split[0]), value);
    }
    g_strfreev(split);
}

void
toggle_var(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;

    if(!argv_idx(argv, 0))
        return;

    const gchar *var_name = argv_idx(argv, 0);

    uzbl_cmdprop *c = get_var_c(var_name);

    if(!c) {
        if (argv->len > 1) {
            set_var_value(var_name, argv_idx(argv, 1));
        } else {
            set_var_value(var_name, "1");
        }
        return;
    }

    switch(c->type) {
    case TYPE_STR:
    {
        const gchar *next;

        if(argv->len >= 3) {
            gchar *current = get_var_value_string_c(c);

            guint i = 2;
            const gchar *first   = argv_idx(argv, 1);
            const gchar *this    = first;
                         next    = argv_idx(argv, 2);

            while(next && strcmp(current, this)) {
                this = next;
                next = argv_idx(argv, ++i);
            }

            if(!next)
                next = first;

            g_free(current);
        } else
            next = "";

        set_var_value_string_c(c, next);
        break;
    }
    case TYPE_INT:
    {
        int current = get_var_value_int_c(c);
        int next;

        if(argv->len >= 3) {
            guint i = 2;

            int first = strtol(argv_idx(argv, 1), NULL, 10);
            int  this = first;

            const gchar *next_s = argv_idx(argv, 2);

            while(next_s && this != current) {
                this   = strtol(next_s, NULL, 10);
                next_s = argv_idx(argv, ++i);
            }

            if(next_s)
                next = strtol(next_s, NULL, 10);
            else
                next = first;
        } else
            next = !current;

        set_var_value_int_c(c, next);
        break;
    }
    case TYPE_ULL:
    {
        unsigned long long current = get_var_value_int_c(c);
        unsigned long long next;

        if(argv->len >= 3) {
            guint i = 2;

            unsigned long long first = strtoull(argv_idx(argv, 1), NULL, 10);
            unsigned long long  this = first;

            const gchar *next_s = argv_idx(argv, 2);

            while(next_s && this != current) {
                this   = strtoull(next_s, NULL, 10);
                next_s = argv_idx(argv, ++i);
            }

            if(next_s)
                next = strtoull(next_s, NULL, 10);
            else
                next = first;
        } else
            next = !current;

        set_var_value_ull_c(c, next);
        break;
    }
    case TYPE_FLOAT:
    {
        float current = get_var_value_float_c(c);
        float next;

        if(argv->len >= 3) {
            guint i = 2;

            float first = strtod(argv_idx(argv, 1), NULL);
            float  this = first;

            const gchar *next_s = argv_idx(argv, 2);

            while(next_s && this != current) {
                this   = strtod(next_s, NULL);
                next_s = argv_idx(argv, ++i);
            }

            if(next_s)
                next = strtod(next_s, NULL);
            else
                next = first;
        } else
            next = !current;

        set_var_value_float_c(c, next);
        break;
    }
    default:
        g_assert_not_reached();
    }

    send_set_var_event(var_name, c);
}

void
event(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    GString *event_name;
    gchar **split = NULL;

    if(!argv_idx(argv, 0))
       return;

    split = g_strsplit(argv_idx(argv, 0), " ", 2);
    if(split[0])
        event_name = g_string_ascii_up(g_string_new(split[0]));
    else
        return;

    send_event(0, event_name->str, TYPE_FORMATTEDSTR, split[1] ? split[1] : "", NULL);

    g_string_free(event_name, TRUE);
    g_strfreev(split);
}

void
print(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar* buf;

    if(!result)
        return;

    buf = expand(argv_idx(argv, 0), 0);
    g_string_assign(result, buf);
    g_free(buf);
}

void
hardcopy(WebKitWebView *page, GArray *argv, GString *result) {
    (void) argv; (void) result;
    webkit_web_frame_print(webkit_web_view_get_main_frame(page));
}

#ifndef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 9, 6)
void
snapshot(WebKitWebView *page, GArray *argv, GString *result) {
    (void) result;
    cairo_surface_t* surface;

    surface = webkit_web_view_get_snapshot(page);

    cairo_surface_write_to_png(surface, argv_idx(argv, 0));

    cairo_surface_destroy(surface);
}
#endif
#endif

#ifdef USE_WEBKIT2
#if WEBKIT_CHECK_VERSION (1, 9, 90)
void
load(WebKitWebView *page, GArray *argv, GString *result) {
    (void) result;

    guint sz = argv->len;

    const gchar *content = sz > 0 ? argv_idx(argv, 0) : NULL;
    const gchar *content_uri = sz > 2 ? argv_idx(argv, 1) : NULL;
    const gchar *base_uri = sz > 2 ? argv_idx(argv, 2) : NULL;

    webkit_web_view_load_alternate_html(page, content, content_uri, base_uri);
}

void
save(WebKitWebView *page, GArray *argv, GString *result) {
    (void) result;
    guint sz = argv->len;

    const gchar *mode_str = sz > 0 ? argv_idx(argv, 0) : NULL;

    WebKitSaveMode mode = WEBKIT_SAVE_MODE_MHTML;

    if (!mode) {
        mode = WEBKIT_SAVE_MODE_MHTML;
    } else if (!strcmp("mhtml", mode_str)) {
        mode = WEBKIT_SAVE_MODE_MHTML;
    }

    if (sz > 1) {
        const gchar *path = argv_idx(argv, 1);
        GFile *gfile = g_file_new_for_path(path);

        webkit_web_view_save_to_file(page, gfile, mode, NULL, NULL, NULL);
        /* TODO: Don't ignore the error */
        webkit_web_view_save_to_file_finish(page, NULL, NULL);
    } else {
        webkit_web_view_save(page, mode, NULL, NULL, NULL);
        /* TODO: Don't ignore the error */
        webkit_web_view_save_finish(page, NULL, NULL);
    }
}
#endif
#endif

void
remove_all_db(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) argv; (void) result;

    webkit_remove_all_web_databases ();
}

#if WEBKIT_CHECK_VERSION (1, 3, 8)
void
plugin_refresh(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) argv; (void) result;

    WebKitWebPluginDatabase *db = webkit_get_web_plugin_database ();
    webkit_web_plugin_database_refresh (db);
}

static void
plugin_toggle_one(WebKitWebPlugin *plugin, const gchar *name) {
    const gchar *plugin_name = webkit_web_plugin_get_name (plugin);

    if (!name || !g_strcmp0 (name, plugin_name)) {
        gboolean enabled = webkit_web_plugin_get_enabled (plugin);

        webkit_web_plugin_set_enabled (plugin, !enabled);
    }
}

void
plugin_toggle(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;

    WebKitWebPluginDatabase *db = webkit_get_web_plugin_database ();
    GSList *plugins = webkit_web_plugin_database_get_plugins (db);

    if (argv->len == 0) {
        g_slist_foreach (plugins, (GFunc)plugin_toggle_one, NULL);
    } else {
        guint i;
        for (i = 0; i < argv->len; ++i) {
            const gchar *plugin_name = argv_idx (argv, i);

            g_slist_foreach (plugins, (GFunc)plugin_toggle_one, &plugin_name);
        }
    }

    webkit_web_plugin_database_plugins_list_free (plugins);
}
#endif

void
include(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar *path = argv_idx(argv, 0);

    if(!path)
        return;

    if((path = find_existing_file(path))) {
		run_command_file(path);
        send_event(FILE_INCLUDED, NULL, TYPE_STR, path, NULL);
        g_free(path);
    }
}

void
show_inspector(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) argv; (void) result;

    webkit_web_inspector_show(uzbl.gui.inspector);
}

void
inspector(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;

    if (argv->len < 1) {
        return;
    }

    const gchar* command = argv_idx (argv, 0);

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
        /* TODO: Implement */
#endif
    }
}

#if WEBKIT_CHECK_VERSION (1, 5, 1)
void
spell_checker(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;

    if (argv->len < 1) {
        return;
    }

    GObject *obj = webkit_get_text_checker ();

    if (!obj) {
        return;
    }
    if (!WEBKIT_IS_SPELL_CHECKER (obj)) {
        return;
    }

    WebKitSpellChecker *checker = WEBKIT_SPELL_CHECKER (obj);

    const gchar* command = argv_idx (argv, 0);

    if (!g_strcmp0 (command, "ignore")) {
        if (argv->len < 2) {
            return;
        }

        guint i;
        for (i = 1; i < argv->len; ++i) {
            const gchar *word = argv_idx (argv, i);

            webkit_spell_checker_ignore_word (checker, word);
        }
    } else if (!g_strcmp0 (command, "learn")) {
        if (argv->len < 2) {
            return;
        }

        guint i;
        for (i = 1; i < argv->len; ++i) {
            const gchar *word = argv_idx (argv, i);

            webkit_spell_checker_learn_word (checker, word);
        }
    } else if (!g_strcmp0 (command, "autocorrect")) {
        if (argv->len != 2) {
            return;
        }

        gchar *word = argv_idx (argv, 1);

        char *new_word = webkit_spell_checker_get_autocorrect_suggestions_for_misspelled_word (checker, word);

        g_string_assign (result, new_word);

        free (new_word);
    } else if (!g_strcmp0 (command, "guesses")) {
        /* TODO Implement */
    }
}
#endif

void
add_cookie(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar *host, *path, *name, *value, *scheme;
    gboolean secure = 0, httponly = 0;
    SoupDate *expires = NULL;

    if(argv->len != 6)
        return;

    // Parse with same syntax as ADD_COOKIE event
    host = argv_idx (argv, 0);
    path = argv_idx (argv, 1);
    name = argv_idx (argv, 2);
    value = argv_idx (argv, 3);
    scheme = argv_idx (argv, 4);
    if (strncmp (scheme, "http", 4) == 0) {
        secure = scheme[4] == 's';
        httponly = strncmp (&scheme[4+secure], "Only", 4) == 0;
    }
    if (argv->len >= 6 && *argv_idx (argv, 5))
        expires = soup_date_new_from_time_t (
            strtoul (argv_idx (argv, 5), NULL, 10));

    // Create new cookie
    SoupCookie * cookie = soup_cookie_new (name, value, host, path, -1);
    soup_cookie_set_secure (cookie, secure);
    soup_cookie_set_http_only (cookie, httponly);
    if (expires)
        soup_cookie_set_expires (cookie, expires);

    // Add cookie to jar
    uzbl.net.soup_cookie_jar->in_manual_add = 1;
    soup_cookie_jar_add_cookie (SOUP_COOKIE_JAR (uzbl.net.soup_cookie_jar), cookie);
    uzbl.net.soup_cookie_jar->in_manual_add = 0;
}

void
delete_cookie(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;

    if(argv->len < 4)
        return;

    SoupCookie * cookie = soup_cookie_new (
        argv_idx (argv, 2),
        argv_idx (argv, 3),
        argv_idx (argv, 0),
        argv_idx (argv, 1),
        0);

    uzbl.net.soup_cookie_jar->in_manual_add = 1;
    soup_cookie_jar_delete_cookie (SOUP_COOKIE_JAR (uzbl.net.soup_cookie_jar), cookie);
    uzbl.net.soup_cookie_jar->in_manual_add = 0;
}

void
clear_cookies(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) argv; (void) result;

    // Replace the current cookie jar with a new empty jar
    soup_session_remove_feature (uzbl.net.soup_session,
        SOUP_SESSION_FEATURE (uzbl.net.soup_cookie_jar));
    g_object_unref (G_OBJECT (uzbl.net.soup_cookie_jar));
    uzbl.net.soup_cookie_jar = uzbl_cookie_jar_new ();
    soup_session_add_feature(uzbl.net.soup_session,
        SOUP_SESSION_FEATURE (uzbl.net.soup_cookie_jar));
}

void
download(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void) result;

    const gchar *uri         = argv_idx(argv, 0);
    const gchar *destination = NULL;
    if(argv->len > 1)
        destination = argv_idx(argv, 1);

    WebKitNetworkRequest *req = webkit_network_request_new(uri);
    WebKitDownload *download = webkit_download_new(req);

    download_cb(web_view, download, (gpointer)destination);

    if(webkit_download_get_destination_uri(download))
        webkit_download_start(download);
    else
        g_object_unref(download);

    g_object_unref(req);
}

void
load_uri(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void) web_view; (void) result;
    gchar * uri = argv_idx(argv, 0);
    set_var_value("uri", uri ? uri : "");
}

void
run_js (WebKitWebView * web_view, GArray *argv, GString *result) {
    if (argv_idx(argv, 0))
        eval_js(web_view, argv_idx(argv, 0), result, "(command)");
}

void
run_external_js (WebKitWebView * web_view, GArray *argv, GString *result) {
    (void) result;
    gchar *path = NULL;

    if (argv_idx(argv, 0) &&
        ((path = find_existing_file(argv_idx(argv, 0)))) ) {
        gchar *file_contents = NULL;

        GIOChannel *chan = g_io_channel_new_file(path, "r", NULL);
        if (chan) {
            gsize len;
            g_io_channel_read_to_end(chan, &file_contents, &len, NULL);
            g_io_channel_unref (chan);
        }

        if (uzbl.state.verbose)
            printf ("External JavaScript file %s loaded\n", argv_idx(argv, 0));

        gchar *js = str_replace("%s", argv_idx (argv, 1) ? argv_idx (argv, 1) : "", file_contents);
        g_free (file_contents);

        eval_js (web_view, js, result, path);
        g_free (js);
        g_free(path);
    }
}

void
search_clear(WebKitWebView *page, GArray *argv, GString *result) {
    (void) argv; (void) result;
    webkit_web_view_unmark_text_matches (page);
    g_free(uzbl.state.searchtx);
    uzbl.state.searchtx = NULL;
}

void
search_forward_text (WebKitWebView *page, GArray *argv, GString *result) {
    (void) result;
    search_text(page, argv_idx(argv, 0), TRUE);
}

void
search_reverse_text(WebKitWebView *page, GArray *argv, GString *result) {
    (void) result;
    search_text(page, argv_idx(argv, 0), FALSE);
}

void
dehilight(WebKitWebView *page, GArray *argv, GString *result) {
    (void) argv; (void) result;
    webkit_web_view_set_highlight_text_matches (page, FALSE);
}

void
chain(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page;
    guint i = 0;
    const gchar *cmd;
    GString *r = g_string_new ("");
    while ((cmd = argv_idx(argv, i++))) {
        GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
        const UzblCommandInfo *c = parse_command_parts(cmd, a);
        if (c)
            run_parsed_command(c, a, r);
        g_array_free (a, TRUE);
    }
    if(result)
        g_string_assign (result, r->str);

    g_string_free(r, TRUE);
}

void
close_uzbl (WebKitWebView *page, GArray *argv, GString *result) {
    (void)page; (void)argv; (void)result;
    // hide window a soon as possible to avoid getting stuck with a
    // non-response window in the cleanup steps
    if (uzbl.gui.main_window)
        gtk_widget_destroy(uzbl.gui.main_window);
    else if (uzbl.gui.plug)
        gtk_widget_destroy(GTK_WIDGET(uzbl.gui.plug));

    gtk_main_quit ();
}

void
spawn_async(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn(argv, NULL, FALSE);
}

void
spawn_sync(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view;
    spawn(argv, result, FALSE);
}

void
spawn_sync_exec(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view;
    if(!result) {
        GString *force_result = g_string_new("");
        spawn(argv, force_result, TRUE);
        g_string_free (force_result, TRUE);
    } else
        spawn(argv, result, TRUE);
}

void
spawn_sh_async(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn_sh(argv, NULL);
}

void
spawn_sh_sync(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void)result;
    spawn_sh(argv, result);
}

void
act_dump_config(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void) argv; (void)result;
    dump_config();
}

void
act_dump_config_as_events(WebKitWebView *web_view, GArray *argv, GString *result) {
    (void)web_view; (void) argv; (void)result;
    dump_config_as_events();
}

void
auth(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar *info, *username, *password;

    if(argv->len != 3)
        return;

    info = argv_idx (argv, 0);
    username = argv_idx (argv, 1);
    password = argv_idx (argv, 2);

    authenticate (info, username, password);
}
