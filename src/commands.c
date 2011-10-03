#include "commands.h"
#include "uzbl-core.h"
#include "events.h"
#include "util.h"
#include "menu.h"
#include "callbacks.h"
#include "variables.h"
#include "type.h"

/* -- command to callback/function map for things we cannot attach to any signals */
CommandInfo cmdlist[] =
{   /* key                              function      no_split      */
    { "back",                           view_go_back, 0                },
    { "forward",                        view_go_forward, 0             },
    { "scroll",                         scroll_cmd, 0                  },
    { "reload",                         view_reload, 0                 },
    { "reload_ign_cache",               view_reload_bypass_cache, 0    },
    { "stop",                           view_stop_loading, 0           },
    { "zoom_in",                        view_zoom_in, 0                }, //Can crash (when max zoom reached?).
    { "zoom_out",                       view_zoom_out, 0               },
    { "toggle_zoom_type",               toggle_zoom_type, 0            },
    { "uri",                            load_uri, TRUE                 },
    { "js",                             run_js, TRUE                   },
    { "script",                         run_external_js, 0             },
    { "toggle_status",                  toggle_status, 0               },
    { "spawn",                          spawn_async, 0                 },
    { "sync_spawn",                     spawn_sync, 0                  },
    { "sync_spawn_exec",                spawn_sync_exec, 0             }, // needed for load_cookies.sh :(
    { "sh",                             spawn_sh_async, 0              },
    { "sync_sh",                        spawn_sh_sync, 0               },
    { "exit",                           close_uzbl, 0                  },
    { "search",                         search_forward_text, TRUE      },
    { "search_reverse",                 search_reverse_text, TRUE      },
    { "search_clear",                   search_clear, TRUE             },
    { "dehilight",                      dehilight, 0                   },
    { "set",                            set_var, TRUE                  },
    { "dump_config",                    act_dump_config, 0             },
    { "dump_config_as_events",          act_dump_config_as_events, 0   },
    { "chain",                          chain, 0                       },
    { "print",                          print, TRUE                    },
    { "event",                          event, TRUE                    },
    { "request",                        event, TRUE                    },
    { "menu_add",                       menu_add, TRUE                 },
    { "menu_link_add",                  menu_add_link, TRUE            },
    { "menu_image_add",                 menu_add_image, TRUE           },
    { "menu_editable_add",              menu_add_edit, TRUE            },
    { "menu_separator",                 menu_add_separator, TRUE       },
    { "menu_link_separator",            menu_add_separator_link, TRUE  },
    { "menu_image_separator",           menu_add_separator_image, TRUE },
    { "menu_editable_separator",        menu_add_separator_edit, TRUE  },
    { "menu_remove",                    menu_remove, TRUE              },
    { "menu_link_remove",               menu_remove_link, TRUE         },
    { "menu_image_remove",              menu_remove_image, TRUE        },
    { "menu_editable_remove",           menu_remove_edit, TRUE         },
    { "hardcopy",                       hardcopy, TRUE                 },
    { "include",                        include, TRUE                  },
    { "show_inspector",                 show_inspector, 0              },
    { "add_cookie",                     add_cookie, 0                  },
    { "delete_cookie",                  delete_cookie, 0               },
    { "clear_cookies",                  clear_cookies, 0               },
    { "download",                       download, 0                    }
};

void
commands_hash() {
    unsigned int i;
    uzbl.behave.commands = g_hash_table_new(g_str_hash, g_str_equal);

    for (i = 0; i < LENGTH(cmdlist); i++)
        g_hash_table_insert(uzbl.behave.commands, (gpointer) cmdlist[i].key, &cmdlist[i]);
}

void
builtins() {
    unsigned int i;
    unsigned int len = LENGTH(cmdlist);
    GString*     command_list = g_string_new("");

    for (i = 0; i < len; i++) {
        g_string_append(command_list, cmdlist[i].key);
        g_string_append_c(command_list, ' ');
    }

    send_event(BUILTINS, NULL, TYPE_STR, command_list->str, NULL);
    g_string_free(command_list, TRUE);
}

/* VIEW funcs (little webkit wrappers) */
#define VIEWFUNC(name) void view_##name(WebKitWebView *page, GArray *argv, GString *result){(void)argv; (void)result; webkit_web_view_##name(page);}
VIEWFUNC(reload)
VIEWFUNC(reload_bypass_cache)
VIEWFUNC(stop_loading)
VIEWFUNC(zoom_in)
VIEWFUNC(zoom_out)
VIEWFUNC(go_back)
VIEWFUNC(go_forward)
#undef VIEWFUNC

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
add_cookie(WebKitWebView *page, GArray *argv, GString *result) {
    (void) page; (void) result;
    gchar *host, *path, *name, *value;
    gboolean secure = 0;
    SoupDate *expires = NULL;

    if(argv->len != 6)
        return;

    // Parse with same syntax as ADD_COOKIE event
    host = argv_idx (argv, 0);
    path = argv_idx (argv, 1);
    name = argv_idx (argv, 2);
    value = argv_idx (argv, 3);
    secure = strcmp (argv_idx (argv, 4), "https") == 0;
    if (strlen (argv_idx (argv, 5)) != 0)
        expires = soup_date_new_from_time_t (
            strtoul (argv_idx (argv, 5), NULL, 10));

    // Create new cookie
    SoupCookie * cookie = soup_cookie_new (name, value, host, path, -1);
    soup_cookie_set_secure (cookie, secure);
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
        const CommandInfo *c = parse_command_parts(cmd, a);
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
