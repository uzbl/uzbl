/*
 ** Uzbl event routines
 ** (c) 2009 by Robert Manea
*/

#include <glib.h>

#include "uzbl-core.h"
#include "events.h"
#include "util.h"
#include "type.h"

#include <signal.h>

const char *event_table[] = {
/* TODO: Add UZBL_ prefix to built-in events? */
#define event_string(evt) #evt

    UZBL_EVENTS (event_string)

#undef event_string
};

/* for now this is just a alias for GString */
struct _UzblEvent
{
    GString message;
};

typedef void UzblSignalFunc(int);

static UzblSignalFunc *
setup_signal (int sig, UzblSignalFunc *shandler);
static void
clear_event_buffer (int sig);
static int
set_event_timeout (guint sec);

int
uzbl_events_init ()
{
    if (setup_signal (SIGALRM, clear_event_buffer) == SIG_ERR) {
        fprintf (stderr, "uzbl: error hooking %d: %s\n", SIGALRM, strerror (errno));
        return -1;
    }
    set_event_timeout (10);

    return 0;
}

UzblSignalFunc *
setup_signal (int sig, UzblSignalFunc *shandler)
{
    struct sigaction new_handler;

    new_handler.sa_handler = shandler;
    sigemptyset (&new_handler.sa_mask);
    new_handler.sa_flags = 0;

    /* TODO: Save the old handler? */
    if (sigaction (sig, &new_handler, NULL)) {
        return SIG_ERR;
    }

    return NULL;
}

void
clear_event_buffer (int sig)
{
    UZBL_UNUSED (sig);

    if (uzbl.state.event_buffer) {
        g_ptr_array_free (uzbl.state.event_buffer, TRUE);
        uzbl.state.event_buffer = NULL;
    }
}

int
set_event_timeout (guint sec)
{
    struct itimerval t;
    memset (&t, 0, sizeof (t));

    t.it_value.tv_sec = sec;
    t.it_value.tv_usec = 0;

    return setitimer (ITIMER_REAL, &t, NULL);
}

void
uzbl_events_free (UzblEvent *event)
{
    g_string_free ((GString *)event, TRUE);
}

static void
send_event_sockets (GPtrArray *sockets, GString *msg);

void
uzbl_events_replay_buffer ()
{
    guint i = 0;

    set_event_timeout (0);

    /* replay buffered events */
    while (i < uzbl.state.event_buffer->len) {
        GString *tmp = g_ptr_array_index (uzbl.state.event_buffer, i++);
        send_event_sockets (uzbl.comm.connect_chan, tmp);
        g_string_free (tmp, TRUE);
    }

    g_ptr_array_free (uzbl.state.event_buffer, TRUE);
    uzbl.state.event_buffer = NULL;
}

void
send_event_sockets (GPtrArray *sockets, GString *msg)
{
    GError *error = NULL;
    GIOStatus ret;
    gsize len;
    guint i = 0;

    while (i < sockets->len) {
        GIOChannel *gio = g_ptr_array_index (sockets, i++);

        if(gio && gio->is_writeable && msg) {
            ret = g_io_channel_write_chars (gio,
                    msg->str, msg->len,
                    &len, &error);

            if (ret == G_IO_STATUS_ERROR) {
                g_warning ("Error sending event to socket: %s", error->message);
                g_clear_error (&error);
            } else {
                if (g_io_channel_flush (gio, &error) == G_IO_STATUS_ERROR) {
                    g_warning ("Error flushing: %s", error->message);
                    g_clear_error (&error);
                }
            }
        }
    }
}

static UzblEvent *
vformat_event (UzblEventType type, const gchar *custom_event, va_list vargs);

UzblEvent *
uzbl_events_format (UzblEventType type, const gchar *custom_event, ...)
{
    va_list vargs;
    va_list vacopy;

    va_start (vargs, custom_event);
    va_copy (vacopy, vargs);

    UzblEvent *event = vformat_event (type, custom_event, vacopy);

    va_end (vacopy);
    va_end (vargs);

    return event;
}

UzblEvent *
vformat_event(UzblEventType type, const gchar *custom_event, va_list vargs)
{
    GString *event_message = g_string_sized_new (512);
    const gchar *event = custom_event ? custom_event : event_table[type];
    char *str;

    int next;
    g_string_printf (event_message, "EVENT [%s] %s", uzbl.state.instance_name, event);

    while ((next = va_arg (vargs, int)) != 0) {
        g_string_append_c (event_message, ' ');
        switch (next) {
            case TYPE_INT:
                g_string_append_printf (event_message, "%d", va_arg (vargs, int));
                break;
            case TYPE_ULL:
                g_string_append_printf (event_message, "%llu", va_arg (vargs, unsigned long long));
                break;
            case TYPE_STR:
                /* a string that needs to be escaped */
                g_string_append_c (event_message, '\'');
                append_escaped (event_message, va_arg (vargs, char*));
                g_string_append_c (event_message, '\'');
                break;
            case TYPE_FORMATTEDSTR:
                /* a string has already been escaped */
                g_string_append (event_message, va_arg (vargs, char*));
                break;
            case TYPE_STR_ARRAY:
            {
                GArray *a = va_arg (vargs, GArray *);
                const char *p;
                int i = 0;

                while ((p = argv_idx (a, i++))) {
                    if (i) {
                        g_string_append_c (event_message, ' ');
                    }
                    g_string_append_c (event_message, '\'');
                    append_escaped (event_message, p);
                    g_string_append_c (event_message, '\'');
                }
            }
            break;
            case TYPE_NAME:
                str = va_arg (vargs, char*);
                g_assert (valid_name (str));
                g_string_append (event_message, str);
                break;
            case TYPE_FLOAT:
            {
                /* ‘float’ is promoted to ‘double’ when passed through ‘...’ */

                /* Make sure the formatted double fits in the buffer. */
                if (event_message->allocated_len - event_message->len < G_ASCII_DTOSTR_BUF_SIZE) {
                    g_string_set_size (event_message, event_message->len + G_ASCII_DTOSTR_BUF_SIZE);
                }

                // format in C locale
                char *tmp = g_ascii_formatd (
                    event_message->str + event_message->len,
                    event_message->allocated_len - event_message->len,
                    "%.2f", va_arg (vargs, double));
                event_message->len += strlen (tmp);
                break;
            }
        }
    }

    return (UzblEvent *)event_message;
}

static void
vuzbl_events_send (UzblEventType type, const gchar *custom_event, va_list vargs);

void
uzbl_events_send (UzblEventType type, const gchar *custom_event, ...)
{
    va_list vargs;
    va_list vacopy;

    va_start (vargs, custom_event);
    va_copy (vacopy, vargs);

    vuzbl_events_send (type, custom_event, vacopy);

    va_end (vacopy);
    va_end (vargs);
}

void
vuzbl_events_send (UzblEventType type, const gchar *custom_event, va_list vargs)
{
    UzblEvent *event = vformat_event (type, custom_event, vargs);

    uzbl_events_send_formatted (event);

    uzbl_events_free (event);
}

static void
send_event_stdout (GString *msg);
static void
send_event_socket (GString *msg);

void
uzbl_events_send_formatted (const UzblEvent *event)
{
    /* A event string is not supposed to contain newlines as it will be
     * interpreted as two events */
    GString *event_message = (GString *)event;

    if (!strchr (event_message->str, '\n')) {
        g_string_append_c (event_message, '\n');

        if (uzbl.state.events_stdout) {
            send_event_stdout (event_message);
        }
        send_event_socket (event_message);
    }
}

void
send_event_stdout (GString *msg)
{
    fprintf (stdout, "%s", msg->str);
    fflush (stdout);
}

void
send_event_socket (GString *msg)
{
    if (uzbl.comm.connect_chan) {
        /* Write to all --connect-socket sockets. */
        send_event_sockets (uzbl.comm.connect_chan, msg);
        if (uzbl.state.event_buffer) {
            uzbl_events_replay_buffer ();
        }
    } else {
        /* Buffer events until a socket is set and connected or a timeout is
         * encountered. */
        if (!uzbl.state.event_buffer) {
            uzbl.state.event_buffer = g_ptr_array_new ();
        }
        g_ptr_array_add (uzbl.state.event_buffer, (gpointer)g_string_new (msg->str));
    }

    /* Write to all client sockets. */
    if (msg && uzbl.comm.client_chan) {
        send_event_sockets (uzbl.comm.client_chan, msg);
    }
}

static guint
key_to_modifier (guint keyval);
static gchar *
get_modifier_mask (guint state);

/* Transform gdk key events to our own events. */
void
uzbl_events_keypress (guint keyval, guint state, guint is_modifier, gint mode)
{
    gchar ucs[7];
    gint ulen;
    gchar *keyname;
    guint32 ukval = gdk_keyval_to_unicode (keyval);
    gchar *modifiers = NULL;
    guint mod = key_to_modifier (keyval);

    /* Get modifier state including this key press/release. */
    modifiers = get_modifier_mask ((mode == GDK_KEY_PRESS) ? (state | mod) : (state & ~mod));

    if (is_modifier && mod) {
        uzbl_events_send ((mode == GDK_KEY_PRESS) ? MOD_PRESS : MOD_RELEASE, NULL,
            TYPE_STR, modifiers,
            TYPE_NAME, get_modifier_mask (mod),
            NULL);
    } else if (g_unichar_isgraph (ukval)) {
        /* Check for printable unicode char. */
        /* TODO: Pass the keyvals through a GtkIMContext so that we also get
         * combining chars right. */
        ulen = g_unichar_to_utf8 (ukval, ucs);
        ucs[ulen] = 0;

        uzbl_events_send ((mode == GDK_KEY_PRESS) ? KEY_PRESS : KEY_RELEASE, NULL,
            TYPE_STR, modifiers,
            TYPE_STR, ucs,
            NULL);
    } else if ((keyname = gdk_keyval_name (keyval))) {
        /* Send keysym for non-printable chars. */
        uzbl_events_send ((mode == GDK_KEY_PRESS) ? KEY_PRESS : KEY_RELEASE, NULL,
            TYPE_STR, modifiers,
            TYPE_NAME, keyname,
            NULL);
    }

    g_free (modifiers);
}

guint
key_to_modifier (guint keyval)
{
/* backwards compatibility. */
#if !GTK_CHECK_VERSION (2, 22, 0)
#define GDK_KEY_Shift_L GDK_Shift_L
#define GDK_KEY_Shift_R GDK_Shift_R
#define GDK_KEY_Control_L GDK_Control_L
#define GDK_KEY_Control_R GDK_Control_R
#define GDK_KEY_Alt_L GDK_Alt_L
#define GDK_KEY_Alt_R GDK_Alt_R
#define GDK_KEY_Super_L GDK_Super_L
#define GDK_KEY_Super_R GDK_Super_R
#define GDK_KEY_ISO_Level3_Shift GDK_ISO_Level3_Shift
#endif

    /* FIXME: Should really use XGetModifierMapping and/or Xkb to get actual
     * modifier keys. */
    switch(keyval) {
        case GDK_KEY_Shift_L:
        case GDK_KEY_Shift_R:
            return GDK_SHIFT_MASK;
        case GDK_KEY_Control_L:
        case GDK_KEY_Control_R:
            return GDK_CONTROL_MASK;
        case GDK_KEY_Alt_L:
        case GDK_KEY_Alt_R:
            return GDK_MOD1_MASK;
        case GDK_KEY_Super_L:
        case GDK_KEY_Super_R:
            return GDK_MOD4_MASK;
        case GDK_KEY_ISO_Level3_Shift:
            return GDK_MOD5_MASK;
        default:
            return 0;
    }
}

gchar *
get_modifier_mask (guint state) {
    GString *modifiers = g_string_new ("");

    if (state & GDK_MODIFIER_MASK) {
#define CHECK_MODIFIER(mask, modifier)                 \
    do {                                               \
        if (state & GDK_##mask##_MASK) {               \
            g_string_append (modifiers, modifier "|"); \
        }                                              \
    } while (0)

        CHECK_MODIFIER (SHIFT,   "Shift");
        CHECK_MODIFIER (LOCK,    "ScrollLock");
        CHECK_MODIFIER (CONTROL, "Ctrl");
        CHECK_MODIFIER (MOD1,    "Mod1");
        /* Mod2 is usually NumLock. Ignore it since NumLock shouldn't be used
         * in bindings.
        CHECK_MODIFIER (MOD2,    "Mod2");
         */
        CHECK_MODIFIER (MOD3,    "Mod3");
        CHECK_MODIFIER (MOD4,    "Mod4");
        CHECK_MODIFIER (MOD5,    "Mod5");
        CHECK_MODIFIER (BUTTON1, "Button1");
        CHECK_MODIFIER (BUTTON2, "Button2");
        CHECK_MODIFIER (BUTTON3, "Button3");
        CHECK_MODIFIER (BUTTON4, "Button4");
        CHECK_MODIFIER (BUTTON5, "Button5");

#undef CHECK_MODIFIER

        if (modifiers->len) {
            gsize end = modifiers->len - 1;

            if (modifiers->str[end] == '|') {
                g_string_truncate (modifiers, end);
            }
        }
    }

    return g_string_free (modifiers, FALSE);
}

static guint
button_to_modifier (guint buttonval);

/* Transform gdk button events to our own events. */
void
uzbl_events_button (guint buttonval, guint state, gint mode)
{
    gchar *details;
    const char *reps;
    gchar *modifiers = NULL;
    guint mod = button_to_modifier (buttonval);

    /* Get modifier state including this button press/release. */
    modifiers = get_modifier_mask((mode != GDK_BUTTON_RELEASE) ? (state | mod) : (state & ~mod));

    switch (mode) {
        case GDK_2BUTTON_PRESS:
            reps = "2";
            break;
        case GDK_3BUTTON_PRESS:
            reps = "3";
            break;
        default:
            reps = "";
            break;
    }

    details = g_strdup_printf ("%sButton%d", reps, buttonval);

    uzbl_events_send ((mode == GDK_BUTTON_PRESS) ? KEY_PRESS : KEY_RELEASE, NULL,
        TYPE_STR, modifiers,
        TYPE_FORMATTEDSTR, details,
        NULL);

    g_free (details);
    g_free (modifiers);
}

guint
button_to_modifier (guint buttonval)
{
    if (buttonval <= 5) {
        /* TODO: Where does this come from? */
        return (1 << (7 + buttonval));
    }
    return 0;
}
