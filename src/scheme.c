#include "scheme.h"

#ifndef USE_WEBKIT2
#include "scheme-request.h"
#endif
#include "uzbl-core.h"

/* =========================== PUBLIC API =========================== */

void
uzbl_scheme_init ()
{
#ifndef USE_WEBKIT2
    soup_session_add_feature_by_type (uzbl.net.soup_session, UZBL_TYPE_SCHEME_REQUEST);
#endif
}

void
uzbl_scheme_add_handler (const gchar *scheme, const gchar *command)
{
#ifdef USE_WEBKIT2
    /* TODO: Implement. */
#else
    uzbl_scheme_request_add_handler (scheme, command);
#endif
}
