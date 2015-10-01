#include "comm.h"

#include "type.h"
#include "util.h"
#include "uzbl-core.h"
#include "variables.h"

#include <string.h>

/* =========================== PUBLIC API =========================== */

static GString *
append_escaped (GString *dest, const gchar *src);

GString *
uzbl_comm_vformat (const gchar *directive, const gchar *function, va_list vargs)
{
    GString *message = g_string_sized_new (512);
    char *str;

    int next;
    g_string_printf (message, "%s [%s] %s", directive, uzbl.state.instance_name, function);

    while ((next = va_arg (vargs, int))) {
        g_string_append_c (message, ' ');
        switch (next) {
        case TYPE_INT:
            g_string_append_printf (message, "%d", va_arg (vargs, int));
            break;
        case TYPE_ULL:
            g_string_append_printf (message, "%llu", va_arg (vargs, unsigned long long));
            break;
        case TYPE_STR:
            /* A string that needs to be escaped. */
            g_string_append_c (message, '\'');
            append_escaped (message, va_arg (vargs, char *));
            g_string_append_c (message, '\'');
            break;
        case TYPE_FORMATTEDSTR:
            /* A string has already been escaped. */
            g_string_append (message, va_arg (vargs, char *));
            break;
        case TYPE_STR_ARRAY: {
            GArray *a = va_arg (vargs, GArray *);
            const char *p;
            int i = 0;

            while ((p = argv_idx (a, i))) {
                if (i) {
                    g_string_append_c (message, ' ');
                }
                g_string_append_c (message, '\'');
                append_escaped (message, p);
                g_string_append_c (message, '\'');

                ++i;
            }
            break;
        }
        case TYPE_NAME:
            str = va_arg (vargs, char *);
            g_assert (uzbl_variables_is_valid (str));
            g_string_append (message, str);
            break;
        case TYPE_DOUBLE:
        {
            /* Make sure the formatted double fits in the buffer. */
            if (message->allocated_len - message->len < G_ASCII_DTOSTR_BUF_SIZE) {
                g_string_set_size (message, message->len + G_ASCII_DTOSTR_BUF_SIZE);
            }

            /* Format in C locale. */
            char *tmp = g_ascii_formatd (
                message->str + message->len,
                message->allocated_len - message->len,
                "%.2g", va_arg (vargs, double));
            message->len += strlen (tmp);
            break;
        }
        }
    }

    g_string_append_c (message, '\n');

    return message;
}

/* ===================== HELPER IMPLEMENTATIONS ===================== */

GString *
append_escaped (GString *dest, const gchar *src)
{
    g_assert (dest);
    g_assert (src);

    /* Hint that we are going to append another string. */
    int oldlen = dest->len;
    g_string_set_size (dest, dest->len + strlen (src) * 2);
    g_string_truncate (dest, oldlen);

    /* Append src char by char with baddies escaped. */
    for (const gchar *p = src; *p; ++p) {
        switch (*p) {
        case '\\':
            g_string_append (dest, "\\\\");
            break;
        case '\'':
            g_string_append (dest, "\\\'");
            break;
        case '\n':
            g_string_append (dest, "\\n");
            break;
        default:
            g_string_append_c (dest, *p);
            break;
        }
    }

    return dest;
}
