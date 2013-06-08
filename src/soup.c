#include "soup.h"

#ifndef USE_WEBKIT2

#include "cookie-jar.h"
#include "events.h"
#include "type.h"
#include "util.h"
#include "uzbl-core.h"

struct _UzblPendingAuth;
typedef struct _UzblPendingAuth UzblPendingAuth;

static void
pending_auth_free (UzblPendingAuth *auth);
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
    uzbl.net.pending_auths = g_hash_table_new_full (
        g_str_hash, g_str_equal,
        g_free, (GDestroyNotify)pending_auth_free);

    soup_session_add_feature (session,
        SOUP_SESSION_FEATURE (uzbl.net.soup_cookie_jar));

    g_object_connect (G_OBJECT (session),
        "signal::request-queued",  G_CALLBACK (request_queued_cb), NULL,
        "signal::request-started", G_CALLBACK (request_started_cb), NULL,
        "signal::authenticate",    G_CALLBACK (authenticate_cb), NULL,
        NULL);
}

struct _UzblPendingAuth
{
    SoupAuth *auth;
    GList    *messages;
};

void
pending_auth_free (UzblPendingAuth *auth)
{
    if (!auth) {
        return;
    }

    g_object_unref (auth->auth);
    g_list_free_full (auth->messages, g_object_unref);
    g_free (auth);
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

    uzbl_events_send (REQUEST_STARTING, NULL,
        TYPE_STR, soup_uri_to_string (soup_message_get_uri (msg), FALSE),
        NULL);

    g_object_connect (G_OBJECT (msg),
        "signal::finished", G_CALLBACK (request_finished_cb), NULL,
        NULL);
}

void
request_finished_cb (SoupMessage *msg, gpointer data)
{
    UZBL_UNUSED (data);

    uzbl_events_send (REQUEST_FINISHED, NULL,
        TYPE_STR, soup_uri_to_string (soup_message_get_uri (msg), FALSE),
        NULL);
}

static UzblPendingAuth *
pending_auth_new (SoupAuth *auth);
static void
pending_auth_add_message (UzblPendingAuth *auth,
                          SoupMessage *message);

void
authenticate_cb (SoupSession *session,
                 SoupMessage *msg,
                 SoupAuth    *auth,
                 gboolean     retrying,
                 gpointer     data)
{
    UZBL_UNUSED (data);

    UzblPendingAuth *pending;
    char *authinfo = soup_auth_get_info (auth);

    pending = g_hash_table_lookup (uzbl.net.pending_auths, authinfo);
    if (pending == NULL) {
        pending = pending_auth_new (auth);
        g_hash_table_insert (uzbl.net.pending_auths, authinfo, pending);
    }

    pending_auth_add_message (pending, msg);
    soup_session_pause_message (session, msg);

    uzbl_events_send (AUTHENTICATE, NULL,
        TYPE_STR, authinfo,
        TYPE_STR, soup_auth_get_host (auth),
        TYPE_STR, soup_auth_get_realm (auth),
        TYPE_STR, (retrying ? "retrying" : ""),
        NULL);
}

UzblPendingAuth *
pending_auth_new (SoupAuth *auth)
{
    UzblPendingAuth *self = g_new (UzblPendingAuth, 1);

    self->auth = auth;
    self->messages = NULL;

    g_object_ref (auth);

    return self;
}

void
pending_auth_add_message (UzblPendingAuth *auth, SoupMessage *message)
{
    auth->messages = g_list_append (auth->messages, g_object_ref (message));
}

void
uzbl_soup_authenticate (const char *authinfo,
                        const char *username,
                        const char *password)
{
    UzblPendingAuth *pending = g_hash_table_lookup (
        uzbl.net.pending_auths,
        authinfo);

    if (!pending) {
        return;
    }

    soup_auth_authenticate (pending->auth, username, password);
    for (GList *l = pending->messages; l; l = l->next) {
        soup_session_unpause_message (
            uzbl.net.soup_session,
            SOUP_MESSAGE (l->data));
    }

    g_hash_table_remove (uzbl.net.pending_auths, authinfo);
}

#endif
