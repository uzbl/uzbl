#include "cookie-jar.h"

#include "events.h"
#include "type.h"

#include <libsoup/soup.h>

/* =========================== PUBLIC API =========================== */

static void
soup_cookie_jar_socket_init (UzblCookieJar *jar);
static void
soup_cookie_jar_socket_class_init (UzblCookieJarClass *socket_class);

G_DEFINE_TYPE (UzblCookieJar, soup_cookie_jar_socket, SOUP_TYPE_COOKIE_JAR)

UzblCookieJar *
uzbl_cookie_jar_new ()
{
    return g_object_new (UZBL_TYPE_COOKIE_JAR, NULL);
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

void
soup_cookie_jar_socket_init (UzblCookieJar *jar)
{
    jar->in_manual_add = 0;
}

static void
changed (SoupCookieJar *jar, SoupCookie *old_cookie, SoupCookie *new_cookie);
static void
finalize (GObject *object);

void
soup_cookie_jar_socket_class_init (UzblCookieJarClass *socket_class)
{
    G_OBJECT_CLASS (socket_class)->finalize       = finalize;
    SOUP_COOKIE_JAR_CLASS (socket_class)->changed = changed;
}

void
changed (SoupCookieJar *jar, SoupCookie *old_cookie, SoupCookie *new_cookie)
{
    SoupCookie *cookie = new_cookie ? new_cookie : old_cookie;

    UzblCookieJar *uzbl_jar = UZBL_COOKIE_JAR (jar);

    /* Send a ADD_COOKIE or DELETE_COOKIE event depending on what has changed.
     * These events aren't sent when a cookie changes due to an add_cookie or
     * delete_cookie command because otherwise a loop would occur when a cookie
     * change is propagated to other uzbl instances using add_cookie or
     * delete_cookie. */
    if (!uzbl_jar->in_manual_add) {
        gchar *base_scheme = cookie->secure ? "https" : "http";
        gchar *scheme = g_strdup (base_scheme);

        if (cookie->http_only) {
            gchar *old_scheme = scheme;
            scheme = g_strconcat (scheme, "Only", NULL);
            g_free (old_scheme);
        }

        gchar *expires = NULL;
        if (cookie->expires) {
            expires = g_strdup_printf ("%ld", (long)soup_date_to_time_t (cookie->expires));
        }

        uzbl_events_send (new_cookie ? ADD_COOKIE : DELETE_COOKIE, NULL,
            TYPE_STR, cookie->domain,
            TYPE_STR, cookie->path,
            TYPE_STR, cookie->name,
            TYPE_STR, cookie->value,
            TYPE_STR, scheme,
            TYPE_STR, expires ? expires : "",
            NULL);

        g_free (expires);
        g_free (scheme);
    }
}

void
finalize (GObject *object)
{
    G_OBJECT_CLASS (soup_cookie_jar_socket_parent_class)->finalize (object);
}
