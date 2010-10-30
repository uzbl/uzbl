#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/unistd.h>

#include <libsoup/soup-cookie.h>
#include <libsoup/soup-cookie-jar-text.h>
#include <libsoup/soup-uri.h>

#include "../src/util.h"

extern const XDG_Var XDG[];

int verbose = 0;

#define SOCK_BACKLOG 10
#define MAX_COOKIE_LENGTH 4096

char cookie_buffer[MAX_COOKIE_LENGTH];

int setup_socket(const char *cookied_socket_path) {
  int socket_fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);

  if(socket_fd < 0) {
    fprintf(stderr, "socket failed (%s)\n", strerror(errno));
    return -1;
  }

  struct sockaddr_un sa;
  sa.sun_family = AF_UNIX;
  strcpy(sa.sun_path, cookied_socket_path);

  if(bind(socket_fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
    fprintf(stderr, "bind failed (%s)\n", strerror(errno));
    return -1;
  }

  if(listen(socket_fd, SOCK_BACKLOG) < 0) {
    fprintf(stderr, "listen failed (%s)\n", strerror(errno));
    return -1;
  }

  return socket_fd;
}

void handle_request(SoupCookieJar *j, const char *buff, int len, int fd) {
    const char *command = buff;

    const char *scheme = command + strlen(command) + 1;
    if((scheme - buff) > len) {
      fprintf(stderr, "got malformed or partial request\n");
      return;
    }

    const char *host = scheme + strlen(scheme) + 1;
    if((host - buff) > len) {
      fprintf(stderr, "got malformed or partial request\n");
      return;
    }

    const char *path = host + strlen(host) + 1;
    if((path - buff) > len) {
      fprintf(stderr, "got malformed or partial request\n");
      return;
    }

    /* glue the parts back together into a SoupURI */
    char *u  = g_strconcat(scheme, "://", host, path, NULL);
    if(verbose) printf("%s %s\n", command, u);
    SoupURI *uri = soup_uri_new(u);
    g_free(u);

    if(!strcmp(command, "GET")) {
      char *result = soup_cookie_jar_get_cookies(j, uri, TRUE);
      if(result) {
        if(verbose) puts(result);
        if(write(fd, result, strlen(result)+1) < 0)
          fprintf(stderr, "write failed (%s)", strerror(errno));

        g_free(result);
      } else {
        if(verbose) puts("-");
        if(write(fd, "", 1) < 0)
          fprintf(stderr, "write failed (%s)", strerror(errno));
      }
    } else if(!strcmp(command, "PUT")) {
      const char *name_and_val = path + strlen(path) + 1;
      if((name_and_val - buff) > len) {
        fprintf(stderr, "got malformed or partial request\n");
        return;
      }

      if(verbose) puts(name_and_val);

      char *eql = strchr(name_and_val, '=');
      eql[0] = 0;

      const char *name  = name_and_val;
      const char *value = eql + 1;

      SoupCookie *cookie = soup_cookie_new(name, value, host, path, SOUP_COOKIE_MAX_AGE_ONE_YEAR);

      soup_cookie_jar_add_cookie(j, cookie);

      if(write(fd, "", 1) < 0)
        fprintf(stderr, "write failed (%s)", strerror(errno));
    }

    soup_uri_free(uri);
}

void usage(const char *progname) {
  printf("%s [-s socket-path] [-f cookies.txt] [-w whitelist-file] [-v]\n", progname);
}

int main(int argc, char *argv[]) {
  int i;

  const char *cookies_txt_path    = NULL;
  const char *cookied_socket_path = NULL;

  for(i = 1; i < argc && argv[i][0] == '-'; i++) {
    switch(argv[i][1]) {
      case 's':
        cookied_socket_path = argv[++i];
        break;
      case 'f':
        cookies_txt_path    = argv[++i];
        break;
      case 'v':
        verbose             = 1;
        break;
      default:
        usage(argv[0]);
        return 1;
    }
  }

  if(!cookies_txt_path)
    cookies_txt_path    = find_xdg_file(1, "/uzbl/cookies.txt");

  if(!cookied_socket_path)
    cookied_socket_path = g_strconcat(get_xdg_var(XDG[2]), "/uzbl/cookie_daemon_socket", NULL);

  g_type_init();

  SoupCookieJar *j = soup_cookie_jar_text_new(cookies_txt_path, FALSE);

  int cookie_socket = setup_socket(cookied_socket_path);
  if(cookie_socket < 0) {
    return 1;
  }

  GArray *connections = g_array_new (FALSE, FALSE, sizeof (int));

  while(1) {
    unsigned int i;
    int r;
    fd_set fs;

    int maxfd = cookie_socket;
    FD_ZERO(&fs);
    FD_SET(maxfd, &fs);

    for(i = 0; i < connections->len; i++) {
      int fd = g_array_index(connections, int, i);
      if(fd > maxfd) maxfd = fd;
      FD_SET(fd, &fs);
    }

    r = select(maxfd+1, &fs, NULL, NULL, NULL);
    if(r < 0) {
      fprintf(stderr, "select failed (%s)\n", strerror(errno));
      continue;
    }

    if(FD_ISSET(cookie_socket, &fs)) {
      /* handle new connection */
      int fd = accept(cookie_socket, NULL, NULL);
      g_array_append_val(connections, fd);
      if(verbose) puts("got connection.");
    }

    for(i = 0; i < connections->len; i++) {
      /* handle activity on a connection */
      int fd = g_array_index(connections, int, i);
      if(FD_ISSET(fd, &fs)) {
        r = read(fd, cookie_buffer, MAX_COOKIE_LENGTH);
        if(r < 0) {
          fprintf(stderr, "read failed (%s)\n", strerror(errno));
          continue;
        } else if(r == 0) {
          if(verbose) puts("client hung up.");
          g_array_remove_index(connections, i);
          i--; /* other elements in the array are moved down to fill the gap */
          continue;
        }
        cookie_buffer[r] = 0;

        handle_request(j, cookie_buffer, r, fd);
      }
    }
  }

  return 0;
}
