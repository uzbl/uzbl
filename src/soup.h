#ifndef UZBL_SOUP_H
#define UZBL_SOUP_H

#include <libsoup/soup.h>

void
uzbl_soup_init (SoupSession *session);

void
uzbl_soup_disable_builtin_auth (SoupSession *session);

void
uzbl_soup_enable_builtin_auth (SoupSession *session);

#endif
