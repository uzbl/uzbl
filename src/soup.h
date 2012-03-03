/**
 * Uzbl tweaks and extension for soup
 */

#ifndef __UZBL_SOUP__
#define __UZBL_SOUP__

#include <libsoup/soup.h>

/**
 * Attach uzbl specific behaviour to the given SoupSession
 */
void uzbl_soup_init (SoupSession *session);
#endif
