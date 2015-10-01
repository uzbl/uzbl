#include "soup.h"

#include "commands.h"
#include "cookie-jar.h"
#include "events.h"
#include "io.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"
#include "variables.h"

static void
request_queued_cb (SoupSession *session,
                   SoupMessage *msg,
                   gpointer data);
static void
request_started_cb (SoupSession *session,
                    SoupMessage *msg,
                    gpointer data);
static void
authenticate_cb (SoupSession *session,
                 SoupMessage *msg,
                 SoupAuth    *auth,
                 gboolean     retrying,
                 gpointer     data);

void
uzbl_soup_init (SoupSession *session)
{
    uzbl.net.soup_cookie_jar = uzbl_cookie_jar_new ();

    soup_session_add_feature (session,
        SOUP_SESSION_FEATURE (uzbl.net.soup_cookie_jar));

    g_object_connect (G_OBJECT (session),
        "signal::request-queued",  G_CALLBACK (request_queued_cb), NULL,
        "signal::request-started", G_CALLBACK (request_started_cb), NULL,
        "signal::authenticate",    G_CALLBACK (authenticate_cb), NULL,
        NULL);
}

void
request_queued_cb (SoupSession *session,
                   SoupMessage *msg,
                   gpointer     data)
{
    UZBL_UNUSED (session);
    UZBL_UNUSED (data);

    gchar *str = soup_uri_to_string (soup_message_get_uri (msg), FALSE);

    uzbl_events_send (REQUEST_QUEUED, NULL,
        TYPE_STR, str,
        NULL);

    g_free (str);
}

static void
request_finished_cb (SoupMessage *msg, gpointer data);

void
request_started_cb (SoupSession *session,
                    SoupMessage *msg,
                    gpointer     data)
{
    UZBL_UNUSED (session);
    UZBL_UNUSED (data);

    gchar *str = soup_uri_to_string (soup_message_get_uri (msg), FALSE);

    uzbl_events_send (REQUEST_STARTING, NULL,
        TYPE_STR, str,
        NULL);

    g_free (str);

    g_object_connect (G_OBJECT (msg),
        "signal::finished", G_CALLBACK (request_finished_cb), NULL,
        NULL);
}

void
request_finished_cb (SoupMessage *msg, gpointer data)
{
    UZBL_UNUSED (data);

    gchar *str = soup_uri_to_string (soup_message_get_uri (msg), FALSE);

    uzbl_events_send (REQUEST_FINISHED, NULL,
        TYPE_STR, str,
        NULL);

    g_free (str);
}

typedef struct {
    SoupSession *session;
    SoupMessage *message;
    SoupAuth *auth;
} UzblAuthenticateData;

static void
authenticate (GString *result, gpointer data);

void
authenticate_cb (SoupSession *session,
                 SoupMessage *msg,
                 SoupAuth    *auth,
                 gboolean     retrying,
                 gpointer     data)
{
    UZBL_UNUSED (data);

    if (uzbl_variables_get_int ("enable_builtin_auth")) {
        return;
    }

    gchar *handler = uzbl_variables_get_string ("authentication_handler");

    GArray *args = uzbl_commands_args_new ();
    const UzblCommand *authentication_command = uzbl_commands_parse (handler, args);
    g_free (handler);

    if (!authentication_command) {
        uzbl_commands_args_free (args);
        return;
    }

    const gchar *host = soup_auth_get_host (auth);
    const gchar *realm = soup_auth_get_host (auth);
    const gchar *retry = retrying ? "retrying" : "initial";
    const gchar *soup_scheme = soup_auth_get_scheme_name (auth);
    gboolean is_proxy = soup_auth_is_for_proxy (auth);
    const gchar *proxy = is_proxy ? "proxy" : "origin";
    SoupURI *uri = soup_message_get_uri (msg);
    guint port = soup_uri_get_port (uri);
    const gchar *save_str = "cant_save";

    const gchar *scheme = "unknown";
    if (!g_strcmp0 (soup_scheme, "Basic")) {
        scheme = "http_basic";
    } else if (!g_strcmp0 (soup_scheme, "Digest")) {
        scheme = "http_digest";
    } else if (!g_strcmp0 (soup_scheme, "NTLM")) {
        scheme = "ntlm";
    }

    gchar *port_str = g_strdup_printf ("%u", port);

    uzbl_commands_args_append (args, g_strdup (host));
    uzbl_commands_args_append (args, g_strdup (realm));
    uzbl_commands_args_append (args, g_strdup (retry));
    uzbl_commands_args_append (args, g_strdup (scheme));
    uzbl_commands_args_append (args, g_strdup (proxy));
    uzbl_commands_args_append (args, g_strdup (port_str));
    uzbl_commands_args_append (args, g_strdup (save_str));

    /* TODO: Append protection space paths. */

    g_free (port_str);

    UzblAuthenticateData *auth_data = g_malloc (sizeof (UzblAuthenticateData));
    auth_data->session = session;
    auth_data->message = msg;
    auth_data->auth = auth;

    g_object_ref (session);
    g_object_ref (msg);
    g_object_ref (auth);

    soup_session_pause_message (session, msg);

    uzbl_io_schedule_command (authentication_command, args, authenticate, auth_data);
}

void
authenticate (GString *result, gpointer data)
{
    UzblAuthenticateData *auth = (UzblAuthenticateData *)data;

    gchar **tokens = g_strsplit (result->str, "\n", 0);

    const gchar *action = tokens[0];
    const gchar *username = action ? tokens[1] : NULL;
    const gchar *password = username ? tokens[2] : NULL;

    if (!action) {
        /* No default credentials. */
    } else if (!g_strcmp0 (action, "IGNORE")) {
        /* Don't authenticate. */
    } else if (!g_strcmp0 (action, "AUTH") && username && password) {
        soup_auth_authenticate (auth->auth, username, password);
    }

    soup_session_unpause_message (auth->session, auth->message);

    g_strfreev (tokens);

    g_object_unref (auth->auth);
    g_object_unref (auth->message);
    g_object_unref (auth->session);

    g_free (auth);
}
