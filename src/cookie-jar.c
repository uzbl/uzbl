#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

#include <libsoup/soup-uri.h>
#include <libsoup/soup-cookie.h>

#include "cookie-jar.h"
#include "uzbl-core.h"
#include "events.h"

static void
uzbl_cookie_jar_session_feature_init(SoupSessionFeatureInterface *iface, gpointer user_data);

G_DEFINE_TYPE_WITH_CODE (UzblCookieJar, soup_cookie_jar_socket, SOUP_TYPE_COOKIE_JAR,
    G_IMPLEMENT_INTERFACE (SOUP_TYPE_SESSION_FEATURE, uzbl_cookie_jar_session_feature_init))

static void request_started (SoupSessionFeature *feature, SoupSession *session, SoupMessage *msg, SoupSocket *socket);
static void changed(SoupCookieJar *jar, SoupCookie *old_cookie, SoupCookie *new_cookie);

static void setup_handler(UzblCookieJar *jar);

static void connect_cookie_socket(UzblCookieJar *jar);
static void disconnect_cookie_socket(UzblCookieJar *jar);

static gchar *do_socket_request(UzblCookieJar *jar, gchar *request, int request_len);

static bool has_socket_handler(UzblCookieJar *jar) {
    return jar->socket_path != NULL;
}

static void
soup_cookie_jar_socket_init(UzblCookieJar *jar) {
    jar->handler     = NULL;
    jar->socket_path = NULL;
    jar->connection_fd = -1;
    jar->in_get_callback = 0;
    jar->in_manual_add = 0;
}

static void
finalize(GObject *object) {
    disconnect_cookie_socket(UZBL_COOKIE_JAR(object));
    G_OBJECT_CLASS(soup_cookie_jar_socket_parent_class)->finalize(object);
}

static void
soup_cookie_jar_socket_class_init(UzblCookieJarClass *socket_class) {
    G_OBJECT_CLASS(socket_class)->finalize              = finalize;
    SOUP_COOKIE_JAR_CLASS(socket_class)->changed        = changed;
}

/* override SoupCookieJar's request_started handler */
static void
uzbl_cookie_jar_session_feature_init(SoupSessionFeatureInterface *iface, gpointer user_data) {
    (void) user_data;
    iface->request_started = request_started;
}

UzblCookieJar *uzbl_cookie_jar_new() {
    return g_object_new(UZBL_TYPE_COOKIE_JAR, NULL);
}

void
uzbl_cookie_jar_set_handler(UzblCookieJar *jar, const gchar* handler) {
    jar->handler = handler;
    setup_handler(jar);
}

char *get_cookies(UzblCookieJar *jar, SoupURI *uri) {
    gchar *result, *path;
    GString *s = g_string_new ("GET");

    path = uri->path[0] ? uri->path : "/";

    if(has_socket_handler(jar)) {
        g_string_append_c(s, 0); /* null-terminate the GET */
        g_string_append_len(s, uri->scheme, strlen(uri->scheme)+1);
        g_string_append_len(s, uri->host,   strlen(uri->host)+1  );
        g_string_append_len(s, path,        strlen(path)+1       );

        result = do_socket_request(jar, s->str, s->len);
        /* try it again; older cookie daemons closed the connection after each request */
        if(result == NULL)
            result = do_socket_request(jar, s->str, s->len);
    } else {
        g_string_append_printf(s, " '%s' '%s' '%s'", uri->scheme, uri->host, uri->path);

        run_handler(jar->handler, s->str);
        result = g_strdup(uzbl.comm.sync_stdout);
    }
    g_string_free(s, TRUE);
    return result;
}

/* this is a duplicate of SoupCookieJar's request_started that uses our get_cookies instead */
static void
request_started(SoupSessionFeature *feature, SoupSession *session,
                SoupMessage *msg, SoupSocket *socket) {
    (void) session; (void) socket;
    gchar *cookies;

    UzblCookieJar *jar = UZBL_COOKIE_JAR (feature);
    SoupURI *uri = soup_message_get_uri(msg);
    gboolean add_to_internal_jar = false;

    if(jar->handler) {
        cookies = get_cookies(jar, uri);
    } else {
        /* no handler is set, fall back to the internal soup cookie jar */
        cookies = soup_cookie_jar_get_cookies(SOUP_COOKIE_JAR(jar), soup_message_get_uri (msg), TRUE);
    }

    if (cookies && cookies[0] != 0) {
        const gchar *next_cookie_start = cookies;

        if (add_to_internal_jar) {
          /* add the cookie data that we just obtained from the cookie handler
             to the cookie jar so that javascript has access to them.
             we set this flag so that we don't trigger the PUT handler. */
          jar->in_get_callback = true;
          do {
              SoupCookie *soup_cookie = soup_cookie_parse(next_cookie_start, uri);
              if(soup_cookie)
                  soup_cookie_jar_add_cookie(SOUP_COOKIE_JAR(uzbl.net.soup_cookie_jar), soup_cookie);
              next_cookie_start = strchr(next_cookie_start, ';');
          } while(next_cookie_start++ != NULL);
          jar->in_get_callback = false;
        }

        soup_message_headers_replace (msg->request_headers, "Cookie", cookies);
    } else {
        soup_message_headers_remove (msg->request_headers, "Cookie");
    }

    if(cookies)
        g_free (cookies);
}

static void
changed(SoupCookieJar *jar, SoupCookie *old_cookie, SoupCookie *new_cookie) {
    SoupCookie * cookie = new_cookie ? new_cookie : old_cookie;

    UzblCookieJar *uzbl_jar = UZBL_COOKIE_JAR(jar);

    /* when Uzbl begins an HTTP request, it GETs cookies from the handler
       and then adds them to the cookie jar so that javascript can access
       these cookies. this causes a 'changed' callback, which we don't want
       to do anything, so we just return.

       (if SoupCookieJar let us override soup_cookie_jar_get_cookies we
       wouldn't have to do this.) */
    if(uzbl_jar->in_get_callback)
        return;

    gchar *scheme = cookie->secure ? "https" : "http";

    /* send a ADD or DELETE -_COOKIE event depending on what have changed */
    if(!uzbl_jar->in_manual_add) {
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

    /* the cookie daemon is only interested in new cookies and changed
       ones, it can take care of deleting expired cookies on its own. */
    if(!new_cookie)
        return;

    GString *s = g_string_new ("PUT");

    if(has_socket_handler(uzbl_jar)) {
        g_string_append_c(s, 0); /* null-terminate the PUT */
        g_string_append_len(s,    scheme,                     strlen(scheme)+1);
        g_string_append_len(s,    new_cookie->domain,         strlen(new_cookie->domain)+1  );
        g_string_append_len(s,    new_cookie->path,           strlen(new_cookie->path)+1  );
        g_string_append_printf(s, "%s=%s", new_cookie->name,  new_cookie->value);

        gchar *result = do_socket_request(uzbl_jar, s->str, s->len+1);
        /* try it again; older cookie daemons closed the connection after each request */
        if(!result)
            result = do_socket_request(uzbl_jar, s->str, s->len+1);

        g_free(result);
    } else {
        g_string_append_printf(s, " '%s' '%s' '%s' '%s=%s'", scheme, new_cookie->domain, new_cookie->path, new_cookie->name, new_cookie->value);

        run_handler(uzbl_jar->handler, s->str);
    }

    g_string_free(s, TRUE);
}

static void
setup_handler(UzblCookieJar *jar) {
    if(jar->handler && strncmp(jar->handler, "talk_to_socket", strlen("talk_to_socket")) == 0) {
        /* extract the socket path from the handler. */
        jar->socket_path = jar->handler + strlen("talk_to_socket");
        while(isspace(*jar->socket_path))
            jar->socket_path++;
        if(*jar->socket_path == 0)
            return; /* there was no path specified. */
        disconnect_cookie_socket(jar);
        connect_cookie_socket(jar);
    } else {
        jar->socket_path = NULL;
    }
}

static void
connect_cookie_socket(UzblCookieJar *jar) {
    struct sockaddr_un sa;
    int fd;

    g_strlcpy(sa.sun_path, jar->socket_path, sizeof(sa.sun_path));
    sa.sun_family = AF_UNIX;

    /* create socket file descriptor and connect it to path */
    fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if(fd == -1) {
        g_printerr("connect_cookie_socket: creating socket failed (%s)\n", strerror(errno));
        return;
    }

    if(connect(fd, (struct sockaddr*)&sa, sizeof(sa))) {
        g_printerr("connect_cookie_socket: connect failed (%s)\n", strerror(errno));
        close(fd);
        return;
    }

    /* successful connection! */
    jar->connection_fd = fd;
}

static void
disconnect_cookie_socket(UzblCookieJar *jar) {
    if(jar->connection_fd > 0) {
        close(jar->connection_fd);
        jar->connection_fd = -1;
    }
}

static gchar *do_socket_request(UzblCookieJar *jar, gchar *request, int request_length) {
    int len;
    ssize_t ret;
    struct pollfd pfd;
    gchar *result = NULL;

    if(jar->connection_fd < 0)
        connect_cookie_socket(jar); /* connection was lost, reconnect */

    /* write request */
    ret = write(jar->connection_fd, request, request_length);
    if(ret == -1) {
        g_printerr("talk_to_socket: write failed (%s)\n", strerror(errno));
        disconnect_cookie_socket(jar);
        return NULL;
    }

    /* wait for a response, with a 500ms timeout */
    pfd.fd = jar->connection_fd;
    pfd.events = POLLIN;
    while(1) {
        ret = poll(&pfd, 1, 500);
        if(ret == 1) break;
        if(ret == 0) errno = ETIMEDOUT;
        if(errno == EINTR) continue;
        g_printerr("talk_to_socket: poll failed while waiting for input (%s)\n",
            strerror(errno));
        if(errno != ETIMEDOUT)
            disconnect_cookie_socket(jar);
        return NULL;
    }

    /* get length of response */
    if(ioctl(jar->connection_fd, FIONREAD, &len) == -1) {
        g_printerr("talk_to_socket: cannot find daemon response length, "
            "ioctl failed (%s)\n", strerror(errno));
        disconnect_cookie_socket(jar);
        return NULL;
    }

    /* there was an empty response. */
    if(len == 0)
      return g_strdup("");

    /* there is a response, read it */
    result = g_malloc(len + 1);
    if(!result) {
        g_printerr("talk_to_socket: failed to allocate %d bytes\n", len);
        return NULL;
    }
    result[len] = 0; /* ensure result is null terminated */

    gchar *p = result;
    while(len > 0) {
        ret = read(jar->connection_fd, p, len);
        if(ret == -1) {
            g_printerr("talk_to_socket: failed to read from socket (%s)\n",
                strerror(errno));
            disconnect_cookie_socket(jar);
            g_free(result);
            return NULL;
        } else {
            len -= ret;
            p += ret;
        }
    }

    return result;
}
