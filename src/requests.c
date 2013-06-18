#include "requests.h"

#include "comm.h"
#include "commands.h"
#include "events.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"

#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

/* =========================== PUBLIC API =========================== */

int
uzbl_requests_init ()
{
    uzbl.state.reply_buffer = g_ptr_array_new ();
    g_ptr_array_set_free_func (uzbl.state.reply_buffer, g_free);

    return 0;
}

static GString *
send_request_sockets (GPtrArray *sockets, GString *msg, const gchar *cookie);

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

#define COOKIE_PREFIX "/tmp/uzbl-REQUEST-"

    char *req_id = g_strdup (COOKIE_PREFIX "XXXXXX");
    int fd = mkstemp (req_id);
    char *cookie = g_strdup (req_id + strlen (COOKIE_PREFIX));

    GString *str = vuzbl_requests_send (request, cookie, vacopy);

    unlink (req_id);
    g_free (cookie);
    g_free (req_id);
    close (fd);

    va_end (vacopy);
    va_end (vargs);

    return str;
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

GString *
send_request_sockets (GPtrArray *sockets, GString *msg, const gchar *cookie)
{
    GString *reply_cookie = g_string_new ("");
    GString *req_result = g_string_new ("");
    GError *error = NULL;
    GIOStatus ret;
    gsize len;
    guint i = 0;

    g_string_printf (reply_cookie, "REPLY-%s ", cookie);

    while (!req_result->len && (i < sockets->len)) {
        GIOChannel *gio = g_ptr_array_index (sockets, i++);

        if (!gio) {
            continue;
        }

        if (gio->is_writeable && gio->is_readable) {
            ret = g_io_channel_write_chars (gio,
                msg->str, msg->len,
                &len, &error);

            if (ret == G_IO_STATUS_ERROR) {
                g_warning ("Error sending request to socket: %s", error->message);
                g_clear_error (&error);
            } else {
                if (g_io_channel_flush (gio, &error) == G_IO_STATUS_ERROR) {
                    g_warning ("Error flushing: %s", error->message);
                    g_clear_error (&error);
                }
            }

            /* TODO: Add a timeout here. */
            /* TODO: Clear the reply buffer occasionally. */
            g_mutex_lock (&uzbl.state.reply_buffer_lock);
            do {
                guint i = 0;
                gboolean found = FALSE;
                gchar *reply = NULL;

                while (i < uzbl.state.reply_buffer->len) {
                    reply = g_ptr_array_index (uzbl.state.reply_buffer, i);

                    if (!strprefix (reply, reply_cookie->str)) {
                        found = TRUE;
                        break;
                    }

                    i++;
                }

                if (found) {
                    g_string_assign (req_result, reply + reply_cookie->len);
                    g_ptr_array_remove_index (uzbl.state.reply_buffer, i);
                    break;
                }

                g_cond_wait (&uzbl.state.reply_buffer_cond, &uzbl.state.reply_buffer_lock);
            } while (true);
            g_mutex_unlock (&uzbl.state.reply_buffer_lock);
        }
    }

    g_string_free (reply_cookie, TRUE);
    return req_result;
}

static GString *
send_formatted_request (GString *request, const gchar *cookie);

GString *
vuzbl_requests_send (const gchar *request, const gchar *cookie, va_list vargs)
{
    GString *request_id = g_string_new ("");
    g_string_printf (request_id, "REQUEST-%s", cookie);

    GString *rq = uzbl_comm_vformat (request_id->str, request, vargs);
    GString *result = send_formatted_request (rq, cookie);

    g_string_free (request_id, TRUE);
    g_string_free (rq, TRUE);

    return result;
}

static GString *
send_request_socket (GString *msg, const gchar *cookie);

GString *
send_formatted_request (GString *request, const gchar *cookie)
{
    if (request && !strchr (request->str, '\n')) {
        /* An request string is not supposed to contain newlines as it will be
         * interpreted as two requests. */
        g_string_append_c (request, '\n');

        return send_request_socket (request, cookie);
    }

    return g_string_new ("");
}

GString *
send_request_socket (GString *msg, const gchar *cookie)
{
    if (uzbl.comm.connect_chan) {
        /* Write to all --connect-socket sockets. */
        return send_request_sockets (uzbl.comm.connect_chan, msg, cookie);
    }

    return g_string_new ("");
}
