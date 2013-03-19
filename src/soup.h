/**
 * Uzbl tweaks and extension for soup
 */

#ifndef UZBL_SOUP_H
#define UZBL_SOUP_H

#include <libsoup/soup.h>

void
uzbl_soup_init (SoupSession *session);

void
uzbl_soup_authenticate (const char  *authinfo,
                        const char  *username,
                        const char  *password);
#endif
