#ifndef __MENU__
#define __MENU__

#ifdef USE_WEBKIT2
#include <webkit2/webkit2.h>
#else
#include <webkit/webkit.h>
#endif

typedef struct {
    gchar*   name;
    gchar*   cmd;
    gboolean issep;
    guint    context;
    WebKitHitTestResult* hittest;
} MenuItem;

void    menu_add(WebKitWebView *page, GArray *argv, GString *result);
void    menu_add_link(WebKitWebView *page, GArray *argv, GString *result);
void    menu_add_image(WebKitWebView *page, GArray *argv, GString *result);
void    menu_add_edit(WebKitWebView *page, GArray *argv, GString *result);
void    menu_add_separator(WebKitWebView *page, GArray *argv, GString *result);
void    menu_add_separator_link(WebKitWebView *page, GArray *argv, GString *result);
void    menu_add_separator_image(WebKitWebView *page, GArray *argv, GString *result);
void    menu_add_separator_edit(WebKitWebView *page, GArray *argv, GString *result);
void    menu_remove(WebKitWebView *page, GArray *argv, GString *result);
void    menu_remove_link(WebKitWebView *page, GArray *argv, GString *result);
void    menu_remove_image(WebKitWebView *page, GArray *argv, GString *result);
void    menu_remove_edit(WebKitWebView *page, GArray *argv, GString *result);
#endif
