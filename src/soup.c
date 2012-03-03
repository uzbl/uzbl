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

struct _PendingAuth
{
    SoupAuth *auth;
    GList    *messages;
};
typedef struct _PendingAuth PendingAuth;

static PendingAuth *pending_auth_new         (SoupAuth *auth);
static void         pending_auth_free        (PendingAuth *self);
static void         pending_auth_add_message (PendingAuth *self,
                                              SoupMessage *message);

void
uzbl_soup_init (SoupSession *session)
{
    uzbl.net.soup_cookie_jar = uzbl_cookie_jar_new ();
    uzbl.net.pending_auths = g_hash_table_new_full (
        g_str_hash, g_str_equal,
        g_free, pending_auth_free
    );

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

void authenticate (const char *authinfo,
                   const char *username,
                   const char *password)
{
    PendingAuth *pending = g_hash_table_lookup (
        uzbl.net.pending_auths,
        authinfo
    );

    if (pending == NULL) {
        return;
    }

    soup_auth_authenticate (pending->auth, username, password);
    for(GList *l = pending->messages; l != NULL; l = l->next) {
        soup_session_unpause_message (
            uzbl.net.soup_session,
            SOUP_MESSAGE (l->data)
        );
    }

    g_hash_table_remove (uzbl.net.pending_auths, authinfo);
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
    PendingAuth *pending;
    char *authinfo = soup_auth_get_info (auth);

    pending = g_hash_table_lookup (uzbl.net.pending_auths, authinfo);
    if (pending == NULL) {
        pending = pending_auth_new (auth);
        g_hash_table_insert (uzbl.net.pending_auths, authinfo, pending);
    }

    pending_auth_add_message (pending, msg);
    soup_session_pause_message (session, msg);

    send_event (
        AUTHENTICATE, NULL,
        TYPE_STR, authinfo,
        TYPE_STR, soup_auth_get_host (auth),
        TYPE_STR, soup_auth_get_realm (auth),
        TYPE_STR, (retrying ? "retrying" : ""),
        NULL
    );
}

static PendingAuth *pending_auth_new (SoupAuth *auth)
{
    PendingAuth *self = g_new (PendingAuth, 1);
    self->auth = auth;
    self->messages = NULL;
    g_object_ref (auth);
    return self;
}

static void pending_auth_free (PendingAuth *self)
{
    g_object_unref (self->auth);
    g_list_free_full (self->messages, g_object_unref);
    g_free (self);
}

static void pending_auth_add_message (PendingAuth *self, SoupMessage *message)
{
    self->messages = g_list_append (self->messages, g_object_ref (message));
}
