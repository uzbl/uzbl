/*
 * Uzbl Commands
 */
#ifndef __COMMANDS__
#define __COMMANDS__

#include <webkit/webkit.h>

typedef void (*Command)(WebKitWebView*, GArray *argv, GString *result);

typedef struct {
    const gchar *key;
    Command      function;
    gboolean     no_split;
} CommandInfo;

/**
 * Initialises the hash table uzbl.behave.commands with the available commands.
 */
void
commands_hash();

/**
 * Sends the BUILTINS events with the available commands.
 */
void
builtins();


void        view_reload(WebKitWebView *page, GArray *argv, GString *result);
void        view_reload_bypass_cache(WebKitWebView *page, GArray *argv, GString *result);
void        view_stop_loading(WebKitWebView *page, GArray *argv, GString *result);
void        view_zoom_in(WebKitWebView *page, GArray *argv, GString *result);
void        view_zoom_out(WebKitWebView *page, GArray *argv, GString *result);
void        view_go_back(WebKitWebView *page, GArray *argv, GString *result);
void        view_go_forward(WebKitWebView *page, GArray *argv, GString *result);
void        toggle_zoom_type (WebKitWebView* page, GArray *argv, GString *result);
void        scroll_cmd(WebKitWebView* page, GArray *argv, GString *result);
void        print(WebKitWebView *page, GArray *argv, GString *result);
void        event(WebKitWebView *page, GArray *argv, GString *result);
void        load_uri(WebKitWebView * web_view, GArray *argv, GString *result);
void        chain(WebKitWebView *page, GArray *argv, GString *result);
void        close_uzbl(WebKitWebView *page, GArray *argv, GString *result);
void        spawn_async(WebKitWebView *web_view, GArray *argv, GString *result);
void        spawn_sh_async(WebKitWebView *web_view, GArray *argv, GString *result);
void        spawn_sync(WebKitWebView *web_view, GArray *argv, GString *result);
void        spawn_sh_sync(WebKitWebView *web_view, GArray *argv, GString *result);
void        spawn_sync_exec(WebKitWebView *web_view, GArray *argv, GString *result);
void        search_forward_text (WebKitWebView *page, GArray *argv, GString *result);
void        search_reverse_text (WebKitWebView *page, GArray *argv, GString *result);
void        search_clear(WebKitWebView *page, GArray *argv, GString *result);
void        dehilight (WebKitWebView *page, GArray *argv, GString *result);
void        hardcopy(WebKitWebView *page, GArray *argv, GString *result);
void        include(WebKitWebView *page, GArray *argv, GString *result);
void        show_inspector(WebKitWebView *page, GArray *argv, GString *result);
void        add_cookie(WebKitWebView *page, GArray *argv, GString *result);
void        delete_cookie(WebKitWebView *page, GArray *argv, GString *result);
void        clear_cookies(WebKitWebView *pag, GArray *argv, GString *result);
void        download(WebKitWebView *pag, GArray *argv, GString *result);
void        set_var(WebKitWebView *page, GArray *argv, GString *result);
void        toggle_var(WebKitWebView *page, GArray *argv, GString *result);
void        run_js (WebKitWebView * web_view, GArray *argv, GString *result);
void        run_external_js (WebKitWebView * web_view, GArray *argv, GString *result);
void        toggle_zoom_type (WebKitWebView* page, GArray *argv, GString *result);
void        toggle_status (WebKitWebView* page, GArray *argv, GString *result);
void        act_dump_config(WebKitWebView* page, GArray *argv, GString *result);
void        act_dump_config_as_events(WebKitWebView* page, GArray *argv, GString *result);
void        auth(WebKitWebView* page, GArray *argv, GString *result);

#endif
