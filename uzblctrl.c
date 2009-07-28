/* -*- c-basic-offset: 4; -*- */
/* Socket code more or less completely copied from here: http://www.ecst.csuchico.edu/~beej/guide/ipc/usock.html */

#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

static gchar* sockpath;
static gchar* command;

static GOptionEntry entries[] =
{
    { "socket",  's', 0, G_OPTION_ARG_STRING, &sockpath, "Path to the uzbl socket",        NULL },
    { "command", 'c', 0, G_OPTION_ARG_STRING, &command,  "The uzbl command to execute",    NULL },
    { NULL,       0,  0, 0,                    NULL,      NULL,                            NULL }
};

int
main(int argc, char* argv[]) {
    GError *error = NULL;
    GOptionContext* context = g_option_context_new ("- utility for controlling and interacting with uzbl through its socket file"); /* TODO: get stuff back from uzbl */
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_add_group        (context, gtk_get_option_group (TRUE));
    g_option_context_parse            (context, &argc, &argv, &error);


    if (sockpath && command) {
        int s, len;
        struct sockaddr_un remote;
        char tmp;

        if ((s = socket (AF_UNIX, SOCK_STREAM, 0)) == -1) {
            perror ("socket");
            exit (EXIT_FAILURE);
        }

        remote.sun_family = AF_UNIX;
        strcpy (remote.sun_path, (char *) sockpath);
        len = strlen (remote.sun_path) + sizeof (remote.sun_family);

        if (connect (s, (struct sockaddr *) &remote, len) == -1) {
            perror ("connect");
            exit (EXIT_FAILURE);
        }

        if ((send (s, command, strlen (command), 0) == -1) ||
            (send (s, "\n", 1, 0) == -1)) {
            perror ("send");
            exit (EXIT_FAILURE);
        }

        while ((len = recv (s, &tmp, 1, 0))) {
            putchar(tmp);
            if (tmp == '\n')
                break;
        }

        close(s);

        return 0;
    } else {
        fprintf(stderr, "Usage: uzblctrl -s /path/to/socket -c \"command\"");
        return 1;
    }
}
/* vi: set et ts=4: */
