#define _POSIX_SOURCE

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/unistd.h>

#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <stdlib.h>

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
  /* delete the cookie socket if it was left behind on a previous run */
  unlink(cookied_socket_path);

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

const char *whitelist_path      = NULL;
GPtrArray *whitelisted_hosts    = NULL;
time_t    whitelist_update_time = 0;

void whitelist_line_cb(const gchar* line, void *user_data) {
  (void) user_data;

  gchar *norm_host;

  const gchar *p = line;
  while(isspace(*p))
    p++;

  if(p[0] == '#' || !p[0]) /* ignore comments and blank lines */
    return;

  if(p[0] == '.')
    norm_host = g_strdup(p);
  else
    norm_host = g_strconcat(".", p, NULL);

  g_ptr_array_add(whitelisted_hosts, g_strchomp(norm_host));
}

gboolean load_whitelist(const char *whitelist_path) {
  if(!file_exists(whitelist_path))
    return FALSE;

  /* check if the whitelist file was updated */
  struct stat f;
  if(stat(whitelist_path, &f) < 0)
    return FALSE;

  if(whitelisted_hosts == NULL)
    whitelisted_hosts = g_ptr_array_new();

  if(f.st_mtime > whitelist_update_time) {
    /* the file was updated, reload the whitelist */
    if(verbose) puts("reloading whitelist");
    while(whitelisted_hosts->len > 0) {
      g_free(g_ptr_array_index(whitelisted_hosts, 0));
      g_ptr_array_remove_index_fast(whitelisted_hosts, 0);
    }
    for_each_line_in_file(whitelist_path, whitelist_line_cb, NULL);
    whitelist_update_time = f.st_mtime;
  }

  return TRUE;
}

gboolean should_save_cookie(const char *host) {
  if(!load_whitelist(whitelist_path))
    return TRUE; /* some error with the file, assume no whitelist */

  /* we normalize the hostname so it has a . in front like the whitelist entries */
  gchar *test_host = (host[0] == '.') ? g_strdup(host) : g_strconcat(".", host, NULL);
  int hl = strlen(test_host);

  /* test against each entry in the whitelist */
  gboolean result = FALSE;
  guint i;
  for(i = 0; i < whitelisted_hosts->len; i++) {
      /* a match means the host ends with (or is equal to) the whitelist entry */
      const gchar *entry = g_ptr_array_index(whitelisted_hosts, i);
      int el = strlen(entry);
      result = (el <= hl) && !strcmp(test_host + (hl - el), entry);

      if(result)
        break;
  }

  g_free(test_host);

  return result;
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

      if(should_save_cookie(host)) {
        char *eql = strchr(name_and_val, '=');
        eql[0] = 0;

        const char *name  = name_and_val;
        const char *value = eql + 1;

        SoupCookie *cookie = soup_cookie_new(name, value, host, path, SOUP_COOKIE_MAX_AGE_ONE_YEAR);

        soup_cookie_jar_add_cookie(j, cookie);
      } else if(verbose)
        puts("no, blacklisted.");

      if(write(fd, "", 1) < 0)
        fprintf(stderr, "write failed (%s)", strerror(errno));
    }

    soup_uri_free(uri);
}

void
wait_for_things_to_happen_and_then_do_things(SoupCookieJar* j, int cookie_socket) {
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
}

void usage(const char *progname) {
  printf("%s [-s socket-path] [-f cookies.txt] [-w whitelist-file] [-n] [-v]\n", progname);
  puts("\t-n\tdon't daemonise the process");
  puts("\t-v\tbe verbose");
}

void daemonise() {
  int r = fork();

  if(r < 0) {
    fprintf(stderr, "fork failed (%s)", strerror(errno));
    exit(1);
  } else if (r > 0) {
    /* this is the parent, which has done its job */
    exit(0);
  }

  if(setsid() < 0) {
    fprintf(stderr, "setsid failed (%s)", strerror(errno));
    exit(1);
  }
}

const char *pid_file_path       = NULL;
const char *cookied_socket_path = NULL;

void cleanup_after_signal(int signal) {
  (void) signal;
  unlink(pid_file_path);
  unlink(cookied_socket_path);
  exit(0);
}

int main(int argc, char *argv[]) {
  int i;

  const char *cookies_txt_path    = NULL;
  gboolean foreground = FALSE;

  for(i = 1; i < argc && argv[i][0] == '-'; i++) {
    switch(argv[i][1]) {
      case 's':
        cookied_socket_path = argv[++i];
        break;
      case 'f':
        cookies_txt_path    = argv[++i];
        break;
      case 'w':
        whitelist_path      = argv[++i];
        break;
      case 'n':
        foreground          = TRUE;
        break;
      case 'v':
        verbose             = 1;
        break;
      default:
        usage(argv[0]);
        return 1;
    }
  }

  if(verbose)
    foreground = TRUE;

  if(!foreground)
    daemonise();

  if(!cookies_txt_path)
    cookies_txt_path    = g_strconcat(get_xdg_var(XDG[1]), "/uzbl/cookies.txt", NULL);

  if(!cookied_socket_path)
    cookied_socket_path = g_strconcat(get_xdg_var(XDG[2]), "/uzbl/cookie_daemon_socket", NULL);

  if(!whitelist_path)
    whitelist_path      = g_strconcat(get_xdg_var(XDG[0]), "/uzbl/cookie_whitelist", NULL);

  /* write out and lock the pid file.
   * this ensures that only one uzbl-cookie-manager is running per-socket.
   * (we should probably also lock the cookies.txt to prevent accidents...) */
  pid_file_path = g_strconcat(cookied_socket_path, ".pid", NULL);
  int lockfd = open(pid_file_path, O_RDWR|O_CREAT, 0600);
  if(lockfd < 0) {
    fprintf(stderr, "couldn't open pid file %s (%s)\n", pid_file_path, strerror(errno));
    return 1;
  }

  if(flock(lockfd, LOCK_EX|LOCK_NB) < 0) {
    fprintf(stderr, "couldn't lock pid file %s (%s)\n", pid_file_path, strerror(errno));
    fprintf(stderr, "uzbl-cookie-manager is probably already running\n");
    return 1;
  }

  gchar* pids = g_strdup_printf("%d\n", getpid());
  write(lockfd, pids, strlen(pids));
  g_free(pids);

  struct sigaction sa;
  sa.sa_handler = cleanup_after_signal;
  if(sigaction(SIGINT,  &sa, NULL) || sigaction(SIGTERM, &sa, NULL)) {
    fprintf(stderr, "sigaction failed (%s)\n", strerror(errno));
    return 1;
  }

  if(!foreground) {
    /* close STDIO */
    close(0);
    close(1);
    close(2);
  }

  g_type_init();

  SoupCookieJar *j = soup_cookie_jar_text_new(cookies_txt_path, FALSE);

  int cookie_socket = setup_socket(cookied_socket_path);
  if(cookie_socket < 0)
    return 1;

  wait_for_things_to_happen_and_then_do_things(j, cookie_socket);

  return 0;
}
