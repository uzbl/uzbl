/* Socket code more or less completely copied from here: http://www.ecst.csuchico.edu/~beej/guide/ipc/usock.html */

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>
#include <webkit/webkit.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

static gchar* sockpath;
static gchar* command;

static GOptionEntry entries[] =
{
    { "socket",  's', 0, G_OPTION_ARG_STRING, &sockpath, "Socket path of the client uzbl", NULL },
    { "command", 'c', 0, G_OPTION_ARG_STRING, &command,  "The uzbl command to execute",    NULL },
    { NULL,       0,  0, 0,                    NULL,      NULL,                            NULL }
};

int
main(int argc, char* argv[]) {
    GError *error = NULL;
    GOptionContext* context = g_option_context_new ("- some stuff here maybe someday");
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group        (context, gtk_get_option_group (TRUE));
    g_option_context_parse            (context, &argc, &argv, &error);

    int s, len;
    struct sockaddr_un remote;

    if ((s = socket (AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror ("socket");
        exit (1);
    }

    remote.sun_family = AF_UNIX;
    strcpy (remote.sun_path, (char *) sockpath);
    len = strlen (remote.sun_path) + sizeof (remote.sun_family);

    if (connect (s, (struct sockaddr *) &remote, len) == -1) {
        perror ("connect");
        exit (1);
    }

    if (send (s, command, strlen (command), 0) == -1) {
        perror ("send");
        exit (1);
    }

    close(s);

    return 0;
}
/* vi: set et ts=4: */
