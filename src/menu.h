#ifndef __MENU__
#define __MENU__

#include <webkit/webkit.h>

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
