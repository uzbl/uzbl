#include <libsoup/soup-cookie.h>

#include "cookie-jar.h"
#include "uzbl-core.h"
#include "events.h"

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
        gchar *scheme = cookie->secure ? "https" : "http";

        gchar *expires = NULL;
        if(cookie->expires)
            expires = g_strdup_printf ("%d", soup_date_to_time_t (cookie->expires));

        gchar * eventstr = g_strdup_printf ("'%s' '%s' '%s' '%s' '%s' '%s'",
            cookie->domain, cookie->path, cookie->name, cookie->value, scheme, expires?expires:"");
        if(new_cookie)
            send_event(ADD_COOKIE, eventstr, NULL);
        else
            send_event(DELETE_COOKIE, eventstr, NULL);
        g_free(eventstr);
        if(expires)
            g_free(expires);
    }
}
