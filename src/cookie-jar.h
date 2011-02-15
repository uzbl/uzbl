#ifndef UZBL_COOKIE_JAR_H
#define UZBL_COOKIE_JAR_H

#include <libsoup/soup-cookie-jar.h>

#define UZBL_TYPE_COOKIE_JAR            (soup_cookie_jar_socket_get_type ())
#define UZBL_COOKIE_JAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), UZBL_TYPE_COOKIE_JAR,        UzblCookieJar))
#define UZBL_COOKIE_JAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), UZBL_TYPE_COOKIE_JAR,         UzblCookieJarClass))

typedef struct {
    SoupCookieJar parent;

    gboolean in_manual_add;
} UzblCookieJar;

typedef struct {
    SoupCookieJarClass parent_class;
} UzblCookieJarClass;

UzblCookieJar *uzbl_cookie_jar_new();

#endif
