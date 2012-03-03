#include "uzbl-core.h"
#include "util.h"
#include "events.h"
#include "type.h"

static void handle_authentication    (SoupSession *session,
                                      SoupMessage *msg,
                                      SoupAuth    *auth,
                                      gboolean     retrying,
                                      gpointer     user_data);

static void handle_request_queued    (SoupSession *session,
                                      SoupMessage *msg,
                                      gpointer user_data);

static void handle_request_started   (SoupSession *session,
                                      SoupMessage *msg,
                                      gpointer user_data);

static void handle_request_finished (SoupMessage *msg,
                                     gpointer user_data);

void
uzbl_soup_init (SoupSession *session)
{
    uzbl.net.soup_cookie_jar = uzbl_cookie_jar_new ();

    soup_session_add_feature (
        session,
        SOUP_SESSION_FEATURE (uzbl.net.soup_cookie_jar)
    );

    g_signal_connect (
        session, "request-queued",
        G_CALLBACK (handle_request_queued), NULL
    );

    g_signal_connect (
        session, "request-started",
        G_CALLBACK (handle_request_started), NULL
    );

    g_signal_connect (
       session, "authenticate",
        G_CALLBACK (handle_authentication), NULL
    );
}

static void
handle_request_queued (SoupSession *session,
                       SoupMessage *msg,
                       gpointer     user_data)
{
    (void) session; (void) user_data;

    send_event (
        REQUEST_QUEUED, NULL,
        TYPE_STR, soup_uri_to_string (soup_message_get_uri (msg), FALSE),
        NULL
    );
}

static void
handle_request_started (SoupSession *session,
                        SoupMessage *msg,
                        gpointer     user_data)
{
    (void) session; (void) user_data;

    send_event (
        REQUEST_STARTING, NULL,
        TYPE_STR, soup_uri_to_string (soup_message_get_uri (msg), FALSE),
        NULL
    );

    g_signal_connect (
        G_OBJECT (msg), "finished",
        G_CALLBACK (handle_request_finished), NULL
    );
}

static void
handle_request_finished (SoupMessage *msg, gpointer user_data)
{
    (void) user_data;

    send_event (
        REQUEST_FINISHED, NULL,
        TYPE_STR, soup_uri_to_string (soup_message_get_uri (msg), FALSE),
        NULL
    );
}


static void
handle_authentication (SoupSession *session,
                       SoupMessage *msg,
                       SoupAuth    *auth,
                       gboolean     retrying,
                       gpointer     user_data)
{
    (void) user_data;

    if (uzbl.behave.authentication_handler && *uzbl.behave.authentication_handler != 0) {
        soup_session_pause_message(session, msg);

        GString *result = g_string_new ("");

        gchar *info  = g_strdup(soup_auth_get_info(auth));
        gchar *host  = g_strdup(soup_auth_get_host(auth));
        gchar *realm = g_strdup(soup_auth_get_realm(auth));

        GArray *a = g_array_new (TRUE, FALSE, sizeof(gchar*));
        const CommandInfo *c = parse_command_parts(uzbl.behave.authentication_handler, a);
        if(c) {
            sharg_append(a, info);
            sharg_append(a, host);
            sharg_append(a, realm);
            sharg_append(a, retrying ? "TRUE" : "FALSE");

            run_parsed_command(c, a, result);
        }
        g_array_free(a, TRUE);

        if (result->len > 0) {
            char  *username, *password;
            int    number_of_endls=0;

            username = result->str;

            gchar *p;
            for (p = result->str; *p; p++) {
                if (*p == '\n') {
                    *p = '\0';
                    if (++number_of_endls == 1)
                        password = p + 1;
                }
            }

            /* If stdout was correct (contains exactly two lines of text) do
             * authenticate. */
            if (number_of_endls == 2)
                soup_auth_authenticate(auth, username, password);
        }

        soup_session_unpause_message(session, msg);

        g_string_free(result, TRUE);
        g_free(info);
        g_free(host);
        g_free(realm);
    }
}

