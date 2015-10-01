#include "scheme.h"

#include "scheme-request.h"
#include "uzbl-core.h"

#include <string.h>

/* =========================== PUBLIC API =========================== */

void
uzbl_scheme_init ()
{
    soup_session_add_feature_by_type (uzbl.net.soup_session, UZBL_TYPE_SCHEME_REQUEST);
}

void
uzbl_scheme_add_handler (const gchar *scheme, const gchar *command)
{
    uzbl_scheme_request_add_handler (scheme, command);
}
