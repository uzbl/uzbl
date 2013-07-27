#include "requests.h"

#include "comm.h"
#include "io.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"

#include <errno.h>
#include <string.h>

/* =========================== PUBLIC API =========================== */

typedef struct {
    GString *request;
    gchar *cookie;
} UzblBufferedRequest;

static GString *
vuzbl_requests_send (const gchar *request, const gchar *cookie, va_list vargs);

GString *
uzbl_requests_send (const gchar *request, ...)
{
    va_list vargs;
    va_list vacopy;

    va_start (vargs, request);
    va_copy (vacopy, vargs);

    GString *cookie = g_string_new ("");
    g_string_printf (cookie, "%u", g_random_int ());

    GString *str = vuzbl_requests_send (request, cookie->str, vacopy);

    g_string_free (cookie, TRUE);

    va_end (vacopy);
    va_end (vargs);

    return str;
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

static GString *
send_request_sockets (GString *request, const gchar *cookie);

GString *
vuzbl_requests_send (const gchar *request, const gchar *cookie, va_list vargs)
{
    GString *request_id = g_string_new ("");
    g_string_printf (request_id, "REQUEST-%s", cookie);

    GString *rq = uzbl_comm_vformat (request_id->str, request, vargs);
    GString *result = send_request_sockets (rq, cookie);

    g_string_free (request_id, TRUE);
    g_string_free (rq, TRUE);

    return result;
}

GString *
send_request_sockets (GString *msg, const gchar *cookie)
{
    uzbl_io_send (msg->str, TRUE);

    /* Require replies within 1 second. */
    gint64 deadline = g_get_monotonic_time () + 1 * G_TIME_SPAN_SECOND;

    GString *reply_cookie = g_string_new ("");
    g_string_printf (reply_cookie, "REPLY-%s ", cookie);

    GString *req_result = g_string_new ("");

    gboolean done = FALSE;
    do {
        gboolean timeout = FALSE;

        g_mutex_lock (&uzbl.state.reply_lock);
        while (!uzbl.state.reply) {
            if (!g_cond_wait_until (&uzbl.state.reply_cond, &uzbl.state.reply_lock, deadline)) {
                timeout = TRUE;
                break;
            }
        }

        if (timeout) {
            done = TRUE;
        } else if (!strprefix (uzbl.state.reply, reply_cookie->str)) {
            g_string_assign (req_result, uzbl.state.reply + reply_cookie->len);
            done = TRUE;
        }

        g_free (uzbl.state.reply);
        uzbl.state.reply = NULL;

        g_mutex_unlock (&uzbl.state.reply_lock);
    } while (!done);

    g_string_free (reply_cookie, TRUE);
    return req_result;
}
