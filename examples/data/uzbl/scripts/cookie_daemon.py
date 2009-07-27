#!/usr/bin/env python

# Uzbl tabbing wrapper using a fifo socket interface
# Copyright (c) 2009, Tom Adams <tom@holizz.com>
# Copyright (c) 2009, Dieter Plaetinck <dieter AT plaetinck.be>
# Copyright (c) 2009, Mason Larobina <mason.larobina@gmail.com>
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


# Todo list:
#  - Setup some option parsing so the daemon can take optional command line
#    arguments. 


import cookielib
import os
import sys
import urllib2
import select
import socket
import time
import atexit
from signal import signal, SIGTERM

try:
    import cStringIO as StringIO

except ImportError:
    import StringIO


# ============================================================================
# ::: Default configuration section ::::::::::::::::::::::::::::::::::::::::::
# ============================================================================

# Location of the uzbl cache directory.
if 'XDG_CACHE_HOME' in os.environ.keys() and os.environ['XDG_CACHE_HOME']:
    cache_dir = os.path.join(os.environ['XDG_CACHE_HOME'], 'uzbl/')

else:
    cache_dir = os.path.join(os.environ['HOME'], '.cache/uzbl/')

# Location of the uzbl data directory.
if 'XDG_DATA_HOME' in os.environ.keys() and os.environ['XDG_DATA_HOME']:
    data_dir = os.path.join(os.environ['XDG_DATA_HOME'], 'uzbl/')

else:
    data_dir = os.path.join(os.environ['HOME'], '.local/share/uzbl/')

# Create cache dir and data dir if they are missing.
for path in [data_dir, cache_dir]:
    if not os.path.exists(path):
        os.makedirs(path) 

# Default config
cookie_socket = os.path.join(cache_dir, 'cookie_daemon_socket')
cookie_jar = os.path.join(data_dir, 'cookies.txt')

# Time out after x seconds of inactivity (set to 0 for never time out).
# Set to 0 by default until talk_to_socket is doing the spawning.
daemon_timeout = 0

# Enable/disable daemonizing the process (useful when debugging). 
# Set to False by default until talk_to_socket is doing the spawning.
daemon_mode = False

# Set true to print helpful debugging messages to the terminal.
verbose = True

# ============================================================================
# ::: End of configuration section :::::::::::::::::::::::::::::::::::::::::::
# ============================================================================


_scriptname = os.path.basename(sys.argv[0])
def echo(msg):
    if verbose:
        print "%s: %s" % (_scriptname, msg)


class CookieMonster:
    '''The uzbl cookie daemon class.'''

    def __init__(self, cookie_socket, cookie_jar, daemon_timeout,\
      daemon_mode):

        self.cookie_socket = os.path.expandvars(cookie_socket)
        self.server_socket = None
        self.cookie_jar = os.path.expandvars(cookie_jar)
        self.daemon_mode = daemon_mode
        self.jar = None
        self.daemon_timeout = daemon_timeout
        self.last_request = time.time()

    
    def run(self):
        '''Start the daemon.'''
        
        # Check if another daemon is running. The reclaim_socket function will
        # exit if another daemon is detected listening on the cookie socket 
        # and remove the abandoned socket if there isnt. 
        if os.path.exists(self.cookie_socket):
            self.reclaim_socket()

        # Daemonize process.
        if self.daemon_mode:
            echo("entering daemon mode.")
            self.daemonize()
        
        # Register a function to cleanup on exit. 
        atexit.register(self.quit)

        # Make SIGTERM act orderly.
        signal(SIGTERM, lambda signum, stack_frame: sys.exit(1))
     
        # Create cookie daemon socket.
        self.create_socket()
        
        # Create cookie jar object from file.
        self.open_cookie_jar()
        
        try:
            # Listen for incoming cookie puts/gets.
            self.listen()
       
        except KeyboardInterrupt:
            print

        except:
            # Clean up
            self.del_socket()

            # Raise exception
            raise

    
    def reclaim_socket(self):
        '''Check if another process (hopefully a cookie_daemon.py) is listening
        on the cookie daemon socket. If another process is found to be 
        listening on the socket exit the daemon immediately and leave the 
        socket alone. If the connect fails assume the socket has been abandoned
        and delete it (to be re-created in the create socket function).'''
    
        try:
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
            sock.connect(self.cookie_socket)
            sock.close()

        except socket.error:
            # Failed to connect to cookie_socket so assume it has been
            # abandoned by another cookie daemon process.
            echo("reclaiming abandoned cookie_socket %r." % self.cookie_socket)
            if os.path.exists(self.cookie_socket):
                os.remove(self.cookie_socket)

            return 

        echo("detected another process listening on %r." % self.cookie_socket)
        echo("exiting.")
        # Use os._exit() to avoid tripping the atexit cleanup function.
        os._exit(1)


    def daemonize(function):
        '''Daemonize the process using the Stevens' double-fork magic.'''

        try:
            if os.fork(): os._exit(0)

        except OSError, e:
            sys.stderr.write("fork #1 failed: %s\n" % e)
            sys.exit(1)
        
        os.chdir('/')
        os.setsid()
        os.umask(0)
        
        try:
            if os.fork(): os._exit(0)

        except OSError, e:
            sys.stderr.write("fork #2 failed: %s\n" % e)
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
        

    def open_cookie_jar(self):
        '''Open the cookie jar.'''
        
        # Create cookie jar object from file.
        self.jar = cookielib.MozillaCookieJar(self.cookie_jar)

        try:
            # Attempt to load cookies from the cookie jar.
            self.jar.load(ignore_discard=True)

            # Ensure restrictive permissions are set on the cookie jar 
            # to prevent other users on the system from hi-jacking your 
            # authenticated sessions simply by copying your cookie jar.
            os.chmod(self.cookie_jar, 0600)

        except:
            pass


    def create_socket(self):
        '''Create AF_UNIX socket for interprocess uzbl instance <-> cookie
        daemon communication.'''
    
        self.server_socket = socket.socket(socket.AF_UNIX,\
          socket.SOCK_SEQPACKET)

        if os.path.exists(self.cookie_socket):
            # Accounting for super-rare super-fast racetrack condition.
            self.reclaim_socket()
            
        self.server_socket.bind(self.cookie_socket)
        
        # Set restrictive permissions on the cookie socket to prevent other
        # users on the system from data-mining your cookies. 
        os.chmod(self.cookie_socket, 0600)


    def listen(self):
        '''Listen for incoming cookie PUT and GET requests.'''

        while True:
            # This line tells the socket how many pending incoming connections 
            # to enqueue. I haven't had any broken pipe errors so far while 
            # using the non-obvious value of 1 under heavy load conditions.
            self.server_socket.listen(1)
           
            if bool(select.select([self.server_socket],[],[],1)[0]):
                client_socket, _ = self.server_socket.accept()
                self.handle_request(client_socket)
                self.last_request = time.time()
            
            if self.daemon_timeout:
                idle = time.time() - self.last_request
                if idle > self.daemon_timeout: break
        

    def handle_request(self, client_socket):
        '''Connection made, now to serve a cookie PUT or GET request.'''
         
        # Receive cookie request from client.
        data = client_socket.recv(8192)
        if not data: return

        # Cookie argument list in packet is null separated.
        argv = data.split("\0")

        # Determine whether or not to print cookie data to terminal.
        print_cookie = (verbose and not self.daemon_mode)
        if print_cookie: print ' '.join(argv[:4])
        
        action = argv[0]
        
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
                if print_cookie: print cookie

            else:
                client_socket.send("\0")

        elif action == "PUT":
            if len(argv) > 3:
                set_cookie = argv[4]
                if print_cookie: print set_cookie

            else:
                set_cookie = None

            hdr = urllib2.httplib.HTTPMessage(\
              StringIO.StringIO('Set-Cookie: %s' % set_cookie))
            res = urllib2.addinfourl(StringIO.StringIO(), hdr,\
              req.get_full_url())
            self.jar.extract_cookies(res,req)
            self.jar.save(ignore_discard=True) 

        if print_cookie: print
            
        client_socket.close()


    def quit(self, *args):
        '''Called on exit to make sure all loose ends are tied up.'''
        
        # Only one loose end so far.
        self.del_socket()

        os._exit(0)
    

    def del_socket(self):
        '''Remove the cookie_socket file on exit. In a way the cookie_socket 
        is the daemons pid file equivalent.'''
    
        if self.server_socket:
            self.server_socket.close()

        if os.path.exists(self.cookie_socket):
            os.remove(self.cookie_socket)


if __name__ == "__main__":
    
    CookieMonster(cookie_socket, cookie_jar, daemon_timeout,\
      daemon_mode).run()

