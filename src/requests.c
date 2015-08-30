#include "requests.h"

#include "comm.h"
#include "io.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"

#include <errno.h>
#include <string.h>

struct _UzblRequests {
    /* Reply buffer */
    GCond   reply_cond;
    GMutex  reply_lock;
    gchar  *reply;
};

/* =========================== PUBLIC API =========================== */

void
uzbl_requests_init ()
{
    uzbl.requests = g_malloc (sizeof (UzblRequests));

    /* Initialize variables */
    g_mutex_init (&uzbl.requests->reply_lock);
    g_cond_init (&uzbl.requests->reply_cond);
    uzbl.requests->reply = NULL;
}

void
uzbl_requests_free ()
{
    g_mutex_clear (&uzbl.requests->reply_lock);
    g_cond_clear (&uzbl.requests->reply_cond);
    g_free (uzbl.requests->reply);

    g_free (uzbl.requests);
    uzbl.requests = NULL;
}

void
uzbl_requests_set_reply (const gchar *reply)
{
    g_mutex_lock (&uzbl.requests->reply_lock);
    if (uzbl.requests->reply) {
        /* Stale reply? It's likely to be old, so let's nuke it. */
        g_free (uzbl.requests->reply);
    }
    uzbl.requests->reply = g_strdup (reply);
    g_cond_broadcast (&uzbl.requests->reply_cond);
    g_mutex_unlock (&uzbl.requests->reply_lock);
}

typedef struct {
    GString *request;
    gchar *cookie;
} UzblBufferedRequest;

static GString *
vuzbl_requests_send (gint64 timeout, const gchar *request, const gchar *cookie, va_list vargs);

GString *
uzbl_requests_send (gint64 timeout, const gchar *request, ...)
{
    va_list vargs;
    va_list vacopy;

    va_start (vargs, request);
    va_copy (vacopy, vargs);

    GString *cookie = g_string_new ("");
    g_string_printf (cookie, "%u", g_random_int ());

    GString *str = vuzbl_requests_send (timeout, request, cookie->str, vacopy);

    g_string_free (cookie, TRUE);

    va_end (vacopy);
    va_end (vargs);

    return str;
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

static GString *
send_request_sockets (gint64 timeout, GString *request, const gchar *cookie);

GString *
vuzbl_requests_send (gint64 timeout, const gchar *request, const gchar *cookie, va_list vargs)
{
    GString *request_id = g_string_new ("");
    g_string_printf (request_id, "REQUEST-%s", cookie);

    GString *rq = uzbl_comm_vformat (request_id->str, request, vargs);
    GString *result = send_request_sockets (timeout, rq, cookie);

    g_string_free (request_id, TRUE);
    g_string_free (rq, TRUE);

    return result;
}

GString *
send_request_sockets (gint64 timeout, GString *msg, const gchar *cookie)
{
    uzbl_io_send (msg->str, TRUE);

    /* Require replies within 1 second. */
    gint64 deadline = g_get_monotonic_time () + timeout * G_TIME_SPAN_SECOND;

    GString *reply_cookie = g_string_new ("");
    g_string_printf (reply_cookie, "REPLY-%s ", cookie);

    GString *req_result = g_string_new ("");

    gboolean done = FALSE;
    do {
        gboolean timeout = FALSE;

        g_mutex_lock (&uzbl.requests->reply_lock);
        while (!uzbl.requests->reply) {
            if (timeout > 0) {
                if (!g_cond_wait_until (&uzbl.requests->reply_cond, &uzbl.requests->reply_lock, deadline)) {
                    timeout = TRUE;
                    break;
                }
            } else {
                g_cond_wait (&uzbl.requests->reply_cond, &uzbl.requests->reply_lock);
            }
        }

        if (timeout) {
            done = TRUE;
        } else if (!strprefix (uzbl.requests->reply, reply_cookie->str)) {
            g_string_assign (req_result, uzbl.requests->reply + reply_cookie->len);
            done = TRUE;
        }

        g_free (uzbl.requests->reply);
        uzbl.requests->reply = NULL;

        g_mutex_unlock (&uzbl.requests->reply_lock);
    } while (!done);

    g_string_free (reply_cookie, TRUE);
    return req_result;
}
