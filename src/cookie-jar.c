#include <libsoup/soup-cookie.h>

#include "cookie-jar.h"
#include "uzbl-core.h"
#include "events.h"
#include "type.h"
#include "variables.h"

G_DEFINE_TYPE (UzblCookieJar, soup_cookie_jar_socket, SOUP_TYPE_COOKIE_JAR)

static void changed(SoupCookieJar *jar, SoupCookie *old_cookie, SoupCookie *new_cookie);

static void
soup_cookie_jar_socket_init(UzblCookieJar *jar) {
    jar->in_manual_add = 0;
}

static void
finalize(GObject *object) {
    G_OBJECT_CLASS(soup_cookie_jar_socket_parent_class)->finalize(object);
}

static void
soup_cookie_jar_socket_class_init(UzblCookieJarClass *socket_class) {
    G_OBJECT_CLASS(socket_class)->finalize              = finalize;
    SOUP_COOKIE_JAR_CLASS(socket_class)->changed        = changed;
}

UzblCookieJar *uzbl_cookie_jar_new() {
    return g_object_new(UZBL_TYPE_COOKIE_JAR, NULL);
}

static void
changed(SoupCookieJar *jar, SoupCookie *old_cookie, SoupCookie *new_cookie) {
    SoupCookie *cookie = new_cookie ? new_cookie : old_cookie;

    UzblCookieJar *uzbl_jar = UZBL_COOKIE_JAR(jar);

    /* send a ADD or DELETE -_COOKIE event depending on what has changed. these
     * events aren't sent when a cookie changes due to an add/delete_cookie
     * command because otherwise a loop would occur when a cookie change is
     * propagated to other uzbl instances using add/delete_cookie. */
    if(!uzbl_jar->in_manual_add) {
        gchar *scheme = cookie->secure
                ? cookie->http_only ? "httpsOnly" : "https"
                : cookie->http_only ? "httpOnly"  : "http";

        gchar *expires = NULL;
        if(cookie->expires)
            expires = g_strdup_printf ("%ld", (long)soup_date_to_time_t (cookie->expires));

            send_event (new_cookie ? ADD_COOKIE : DELETE_COOKIE, NULL,
                    TYPE_STR, cookie->domain,
                    TYPE_STR, cookie->path,
                    TYPE_STR, cookie->name,
                    TYPE_STR, cookie->value,
                    TYPE_STR, scheme,
                    TYPE_STR, expires ? expires : "",
                    NULL);

        if(expires)
            g_free(expires);
    }
}
