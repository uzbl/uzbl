#!/usr/bin/env python

# The Python Cookie Daemon for Uzbl.
# Copyright (c) 2009, Tom Adams <tom@holizz.com>
# Copyright (c) 2009, Dieter Plaetinck <dieter@plaetinck.be>
# Copyright (c) 2009, Mason Larobina <mason.larobina@gmail.com>
# Copyright (c) 2009, Michael Fiano <axionix@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

'''
The Python Cookie Daemon
========================

This daemon is a re-write of the original cookies.py script found in uzbl's
master branch. This script provides more functionality than the original
cookies.py by adding numerous command line options to specify different cookie
jar locations, socket locations, verbose output, etc. This functionality is
very useful as it allows you to run multiple daemons at once serving cookies
to different groups of uzbl instances as required.

Keeping up to date
==================

Check the cookie daemon uzbl-wiki page for more information on where to
find the latest version of the cookie_daemon.py

    http://www.uzbl.org/wiki/cookie_daemon.py

Command line options
====================

Use the following command to get a full list of the cookie_daemon.py command
line options:

  ./cookie_daemon.py --help

Talking with uzbl
=================

In order to get uzbl to talk to a running cookie daemon you add the following
to your uzbl config:

  set cookie_handler = talk_to_socket $XDG_CACHE_HOME/uzbl/cookie_daemon_socket

Or if you prefer using the $HOME variable:

  set cookie_handler = talk_to_socket $HOME/.cache/uzbl/cookie_daemon_socket

Todo list
=========

 - Use a pid file to make force killing a running daemon possible.

Reporting bugs / getting help
=============================

The best way to report bugs and or get help with the cookie daemon is to
contact the maintainers it the #uzbl irc channel found on the Freenode IRC
network (irc.freenode.org).
'''

import cookielib
import os
import sys
import urllib2
import select
import socket
import time
import atexit
from traceback import print_exc
from signal import signal, SIGTERM
from optparse import OptionParser
from os.path import join

try:
    import cStringIO as StringIO

except ImportError:
    import StringIO


# ============================================================================
# ::: Default configuration section ::::::::::::::::::::::::::::::::::::::::::
# ============================================================================

def xdghome(key, default):
    '''Attempts to use the environ XDG_*_HOME paths if they exist otherwise
    use $HOME and the default path.'''

    xdgkey = "XDG_%s_HOME" % key
    if xdgkey in os.environ.keys() and os.environ[xdgkey]:
        return os.environ[xdgkey]

    return join(os.environ['HOME'], default)

# Setup xdg paths.
CACHE_DIR = join(xdghome('CACHE', '.cache/'), 'uzbl/')
DATA_DIR = join(xdghome('DATA', '.local/share/'), 'uzbl/')
CONFIG_DIR = join(xdghome('CONFIG', '.config/'), 'uzbl/')

# Ensure data paths exist.
for path in [CACHE_DIR, DATA_DIR, CONFIG_DIR]:
    if not os.path.exists(path):
        os.makedirs(path)

# Default config
config = {

  # Default cookie jar, whitelist, and daemon socket locations.
  'cookie_jar': join(DATA_DIR, 'cookies.txt'),
  'cookie_whitelist': join(CONFIG_DIR, 'cookie_whitelist'),
  'cookie_socket': join(CACHE_DIR, 'cookie_daemon_socket'),

  # Don't use a cookie whitelist policy by default.
  'use_whitelist': False,

  # Time out after x seconds of inactivity (set to 0 for never time out).
  # WARNING: Do not use this option if you are manually launching the daemon.
  'daemon_timeout': 0,

  # Daemonise by default.
  'daemon_mode': True,

  # Optionally print helpful debugging messages to the terminal.
  'verbose': False,

} # End of config dictionary.


# ============================================================================
# ::: End of configuration section :::::::::::::::::::::::::::::::::::::::::::
# ============================================================================


_SCRIPTNAME = os.path.basename(sys.argv[0])
def echo(msg):
    '''Prints only if the verbose flag has been set.'''

    if config['verbose']:
        sys.stderr.write("%s: %s\n" % (_SCRIPTNAME, msg))


def error(msg):
    '''Prints error message and exits.'''

    sys.stderr.write("%s: error: %s\n" % (_SCRIPTNAME, msg))
    sys.exit(1)


def mkbasedir(filepath):
    '''Create the base directories of the file in the file-path if the dirs
    don't exist.'''

    dirname = os.path.dirname(filepath)
    if not os.path.exists(dirname):
        echo("creating dirs: %r" % dirname)
        os.makedirs(dirname)


def daemon_running(cookie_socket):
    '''Check if another process (hopefully a cookie_daemon.py) is listening
    on the cookie daemon socket. If another process is found to be
    listening on the socket exit the daemon immediately and leave the
    socket alone. If the connect fails assume the socket has been abandoned
    and delete it (to be re-created in the create socket function).'''

    if not os.path.exists(cookie_socket):
        return False

    if os.path.isfile(cookie_socket):
        raise Exception("regular file at %r is not a socket" % cookie_socket)


    if os.path.isdir(cookie_socket):
        raise Exception("directory at %r is not a socket" % cookie_socket)

    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        sock.connect(cookie_socket)
        sock.close()
        echo("detected daemon listening on %r" % cookie_socket)
        return True

    except socket.error:
        # Failed to connect to cookie_socket so assume it has been
        # abandoned by another cookie daemon process.
        if os.path.exists(cookie_socket):
            echo("deleting abandoned socket at %r" % cookie_socket)
            os.remove(cookie_socket)

    return False


def send_command(cookie_socket, cmd):
    '''Send a command to a running cookie daemon.'''

    if not daemon_running(cookie_socket):
        return False

    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        sock.connect(cookie_socket)
        sock.send(cmd)
        sock.close()
        echo("sent command %r to %r" % (cmd, cookie_socket))
        return True

    except socket.error:
        print_exc()
        error("failed to send message %r to %r" % (cmd, cookie_socket))
        return False


def kill_daemon(cookie_socket):
    '''Send the "EXIT" command to running cookie_daemon.'''

    if send_command(cookie_socket, "EXIT"):
        # Now ensure the cookie_socket is cleaned up.
        start = time.time()
        while os.path.exists(cookie_socket):
            time.sleep(0.1)
            if (time.time() - start) > 5:
                error("force deleting socket %r" % cookie_socket)
                os.remove(cookie_socket)
                return

        echo("stopped daemon listening on %r"% cookie_socket)

    else:
        if os.path.exists(cookie_socket):
            os.remove(cookie_socket)
            echo("removed abandoned/broken socket %r" % cookie_socket)


def daemonize():
    '''Daemonize the process using the Stevens' double-fork magic.'''

    try:
        if os.fork():
            os._exit(0)

    except OSError:
        print_exc()
        sys.stderr.write("fork #1 failed")
        sys.exit(1)

    os.chdir('/')
    os.setsid()
    os.umask(0)

    try:
        if os.fork():
            os._exit(0)

    except OSError:
        print_exc()
        sys.stderr.write("fork #2 failed")
        sys.exit(1)

    sys.stdout.flush()
    sys.stderr.flush()

    devnull = '/dev/null'
    stdin = file(devnull, 'r')
    stdout = file(devnull, 'a+')
    stderr = file(devnull, 'a+', 0)

    os.dup2(stdin.fileno(), sys.stdin.fileno())
    os.dup2(stdout.fileno(), sys.stdout.fileno())
    os.dup2(stderr.fileno(), sys.stderr.fileno())


class CookieMonster:
    '''The uzbl cookie daemon class.'''

    def __init__(self):
        '''Initialise class variables.'''

        self.server_socket = None
        self.jar = None
        self.last_request = time.time()
        self._running = False


    def run(self):
        '''Start the daemon.'''

        # The check healthy function will exit if another daemon is detected
        # listening on the cookie socket and remove the abandoned socket if
        # there isnt.
        if os.path.exists(config['cookie_socket']):
            if daemon_running(config['cookie_socket']):
                sys.exit(1)

        # Daemonize process.
        if config['daemon_mode']:
            echo("entering daemon mode")
            daemonize()

        # Register a function to cleanup on exit.
        atexit.register(self.quit)

        # Make SIGTERM act orderly.
        signal(SIGTERM, lambda signum, stack_frame: sys.exit(1))

        # Create cookie jar object from file.
        self.open_cookie_jar()

        # Create a way to exit nested loops by setting a running flag.
        self._running = True

        while self._running:
            # Create cookie daemon socket.
            self.create_socket()

            try:
                # Enter main listen loop.
                self.listen()

            except KeyboardInterrupt:
                self._running = False
                print

            except socket.error:
                print_exc()

            except:
                # Clean up
                self.del_socket()

                # Raise exception
                raise

            # Always delete the socket before calling create again.
            self.del_socket()


    def load_whitelist(self):
        '''Load the cookie jar whitelist policy.'''

        cookie_whitelist = config['cookie_whitelist']

        if cookie_whitelist:
            mkbasedir(cookie_whitelist)

        # Create cookie whitelist file if it does not exist.
        if not os.path.exists(cookie_whitelist):
            open(cookie_whitelist, 'w').close()

        # Read cookie whitelist file into list.
        file = open(cookie_whitelist,'r')
        domain_list = [line.rstrip('\n') for line in file]
        file.close()

        # Define policy of allowed domains
        policy = cookielib.DefaultCookiePolicy(allowed_domains=domain_list)
        self.jar.set_policy(policy)

        # Save the last modified time of the whitelist.
        self._whitelistmtime = os.stat(cookie_whitelist).st_mtime


    def open_cookie_jar(self):
        '''Open the cookie jar.'''

        cookie_jar = config['cookie_jar']
        cookie_whitelist = config['cookie_whitelist']

        if cookie_jar:
            mkbasedir(cookie_jar)

        # Create cookie jar object from file.
        self.jar = cookielib.MozillaCookieJar(cookie_jar)

        # Load cookie whitelist policy.
        if config['use_whitelist']:
            self.load_whitelist()

        if cookie_jar:
            try:
                # Attempt to load cookies from the cookie jar.
                self.jar.load(ignore_discard=True)

                # Ensure restrictive permissions are set on the cookie jar
                # to prevent other users on the system from hi-jacking your
                # authenticated sessions simply by copying your cookie jar.
                os.chmod(cookie_jar, 0600)

            except:
                pass


    def reload_whitelist(self):
        '''Reload the cookie whitelist.'''

        cookie_whitelist = config['cookie_whitelist']
        if os.path.exists(cookie_whitelist):
            echo("reloading whitelist %r" % cookie_whitelist)
            self.open_cookie_jar()


    def create_socket(self):
        '''Create AF_UNIX socket for communication with uzbl instances.'''

        cookie_socket = config['cookie_socket']
        mkbasedir(cookie_socket)

        self.server_socket = socket.socket(socket.AF_UNIX,
          socket.SOCK_SEQPACKET)

        self.server_socket.bind(cookie_socket)

        # Set restrictive permissions on the cookie socket to prevent other
        # users on the system from data-mining your cookies.
        os.chmod(cookie_socket, 0600)


    def listen(self):
        '''Listen for incoming cookie PUT and GET requests.'''

        daemon_timeout = config['daemon_timeout']
        echo("listening on %r" % config['cookie_socket'])

        while self._running:
            # This line tells the socket how many pending incoming connections
            # to enqueue at once. Raising this number may or may not increase
            # performance.
            self.server_socket.listen(1)

            if bool(select.select([self.server_socket], [], [], 1)[0]):
                client_socket, _ = self.server_socket.accept()
                self.handle_request(client_socket)
                self.last_request = time.time()
                client_socket.close()
                continue

            if daemon_timeout:
                # Checks if the daemon has been idling for too long.
                idle = time.time() - self.last_request
                if idle > daemon_timeout:
                    self._running = False


    def handle_request(self, client_socket):
        '''Connection made, now to serve a cookie PUT or GET request.'''

        # Receive cookie request from client.
        data = client_socket.recv(8192)
        if not data:
            return

        # Cookie argument list in packet is null separated.
        argv = data.split("\0")
        action = argv[0].upper().strip()

        # Catch the EXIT command sent to kill running daemons.
        if action == "EXIT":
            self._running = False
            return

        # Catch whitelist RELOAD command.
        elif action == "RELOAD":
            self.reload_whitelist()
            return

        # Return if command unknown.
        elif action not in ['GET', 'PUT']:
            error("unknown command %r." % argv)
            return

        # Determine whether or not to print cookie data to terminal.
        print_cookie = (config['verbose'] and not config['daemon_mode'])
        if print_cookie:
            print ' '.join(argv[:4])

        uri = urllib2.urlparse.ParseResult(
          scheme=argv[1],
          netloc=argv[2],
          path=argv[3],
          params='',
          query='',
          fragment='').geturl()

        req = urllib2.Request(uri)

        if action == "GET":
            self.jar.add_cookie_header(req)
            if req.has_header('Cookie'):
                cookie = req.get_header('Cookie')
                client_socket.send(cookie)
                if print_cookie:
                    print cookie

            else:
                client_socket.send("\0")

        elif action == "PUT":
            cookie = argv[4] if len(argv) > 3 else None
            if print_cookie:
                print cookie

            self.put_cookie(req, cookie)

        if print_cookie:
            print


    def put_cookie(self, req, cookie=None):
        '''Put a cookie in the cookie jar.'''

        hdr = urllib2.httplib.HTTPMessage(\
          StringIO.StringIO('Set-Cookie: %s' % cookie))
        res = urllib2.addinfourl(StringIO.StringIO(), hdr,
          req.get_full_url())
        self.jar.extract_cookies(res, req)
        if config['cookie_jar']:
            self.jar.save(ignore_discard=True)


    def del_socket(self):
        '''Remove the cookie_socket file on exit. In a way the cookie_socket
        is the daemons pid file equivalent.'''

        if self.server_socket:
            try:
                self.server_socket.close()

            except:
                pass

        self.server_socket = None

        cookie_socket = config['cookie_socket']
        if os.path.exists(cookie_socket):
            echo("deleting socket %r" % cookie_socket)
            os.remove(cookie_socket)


    def quit(self):
        '''Called on exit to make sure all loose ends are tied up.'''

        self.del_socket()
        sys.exit(0)


def main():
    '''Main function.'''

    # Define command line parameters.
    usage = "usage: %prog [options] {start|stop|restart|reload}"
    parser = OptionParser(usage=usage)
    parser.add_option('-n', '--no-daemon', dest='no_daemon',
      action='store_true', help="don't daemonise the process.")

    parser.add_option('-v', '--verbose', dest="verbose",
      action='store_true', help="print verbose output.")

    parser.add_option('-t', '--daemon-timeout', dest='daemon_timeout',
      action="store", metavar="SECONDS", help="shutdown the daemon after x "\
      "seconds inactivity. WARNING: Do not use this when launching the "\
      "cookie daemon manually.")

    parser.add_option('-s', '--cookie-socket', dest="cookie_socket",
      metavar="SOCKET", help="manually specify the socket location.")

    parser.add_option('-j', '--cookie-jar', dest='cookie_jar',
      metavar="FILE", help="manually specify the cookie jar location.")

    parser.add_option('-m', '--memory', dest='memory', action='store_true',
      help="store cookies in memory only - do not write to disk")

    parser.add_option('-u', '--use-whitelist', dest='usewhitelist',
      action='store_true', help="use cookie whitelist policy")

    parser.add_option('-w', '--cookie-whitelist', dest='whitelist',
      action='store', help="manually specify whitelist location",
      metavar='FILE')

    # Parse the command line arguments.
    (options, args) = parser.parse_args()

    expand = lambda p: os.path.realpath(os.path.expandvars(p))

    initcommands = ['start', 'stop', 'restart', 'reload']
    for arg in args:
        if arg not in initcommands:
            error("unknown argument %r" % args[0])
            sys.exit(1)

    if len(args) > 1:
        error("the daemon only accepts one {%s} action at a time."
          % '|'.join(initcommands))
        sys.exit(1)

    if len(args):
        action = args[0]

    else:
        action = "start"

    if options.no_daemon:
        config['daemon_mode'] = False

    if options.cookie_socket:
        config['cookie_socket'] = expand(options.cookie_socket)

    if options.cookie_jar:
        config['cookie_jar'] = expand(options.cookie_jar)

    if options.memory:
        config['cookie_jar'] = None

    if options.whitelist:
        config['cookie_whitelist'] = expand(options.whitelist)

    if options.whitelist or options.usewhitelist:
        config['use_whitelist'] = True

    if options.daemon_timeout:
        try:
            config['daemon_timeout'] = int(options.daemon_timeout)

        except ValueError:
            error("expected int argument for -t, --daemon-timeout")

    # Expand $VAR's in config keys that relate to paths.
    for key in ['cookie_socket', 'cookie_jar', 'cookie_whitelist']:
        if config[key]:
            config[key] = os.path.expandvars(config[key])

    if options.verbose:
        config['verbose'] = True
        import pprint
        sys.stderr.write("%s\n" % pprint.pformat(config))

    # It would be better if we didn't need to start this python process just
    # to send a command to the socket, but unfortunately socat doesn't seem
    # to support SEQPACKET.
    if action == "reload":
        send_command(config['cookie_socket'], "RELOAD")

    if action in ['stop', 'restart']:
        kill_daemon(config['cookie_socket'])

    if action in ['start', 'restart']:
        CookieMonster().run()


if __name__ == "__main__":
    main()
