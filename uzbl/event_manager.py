#!/usr/bin/env python
from __future__ import print_function

# Event Manager for Uzbl
# Copyright (c) 2009-2010, Mason Larobina <mason.larobina@gmail.com>
# Copyright (c) 2009, Dieter Plaetinck <dieter@plaetinck.be>
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

E V E N T _ M A N A G E R . P Y
===============================

Event manager for uzbl written in python.

'''

import atexit
import imp
import logging
import os
import sys
import time
import weakref
import re
import errno
from collections import defaultdict
from functools import partial
from glob import glob
from itertools import count
from optparse import OptionParser
from select import select
from signal import signal, SIGTERM, SIGINT, SIGKILL
from socket import socket, AF_UNIX, SOCK_STREAM
from traceback import format_exc

from uzbl.core import Uzbl

def xdghome(key, default):
    '''Attempts to use the environ XDG_*_HOME paths if they exist otherwise
    use $HOME and the default path.'''

    xdgkey = "XDG_%s_HOME" % key
    if xdgkey in os.environ.keys() and os.environ[xdgkey]:
        return os.environ[xdgkey]

    return os.path.join(os.environ['HOME'], default)

# Setup xdg paths.
DATA_DIR = os.path.join(xdghome('DATA', '.local/share/'), 'uzbl/')
CACHE_DIR = os.path.join(xdghome('CACHE', '.cache/'), 'uzbl/')

# Define some globals.
SCRIPTNAME = os.path.basename(sys.argv[0])

logger = logging.getLogger(SCRIPTNAME)


def get_exc():
    '''Format `format_exc` for logging.'''
    return "\n%s" % format_exc().rstrip()


def expandpath(path):
    '''Expand and realpath paths.'''
    return os.path.realpath(os.path.expandvars(path))



def daemonize():
    '''Daemonize the process using the Stevens' double-fork magic.'''

    logger.info('entering daemon mode')

    try:
        if os.fork():
            os._exit(0)

    except OSError:
        logger.critical('failed to daemonize', exc_info=True)
        sys.exit(1)

    os.chdir('/')
    os.setsid()
    os.umask(0)

    try:
        if os.fork():
            os._exit(0)

    except OSError:
        logger.critical('failed to daemonize', exc_info=True)
        sys.exit(1)

    if sys.stdout.isatty():
        sys.stdout.flush()
        sys.stderr.flush()

    devnull = '/dev/null'
    stdin = file(devnull, 'r')
    stdout = file(devnull, 'a+')
    stderr = file(devnull, 'a+', 0)

    os.dup2(stdin.fileno(), sys.stdin.fileno())
    os.dup2(stdout.fileno(), sys.stdout.fileno())
    os.dup2(stderr.fileno(), sys.stderr.fileno())

    logger.info('entered daemon mode')


def make_dirs(path):
    '''Make all basedirs recursively as required.'''

    try:
        dirname = os.path.dirname(path)
        if not os.path.isdir(dirname):
            logger.debug('creating directories %r', dirname)
            os.makedirs(dirname)

    except OSError:
        logger.error('failed to create directories', exc_info=True)


class EventHandler(object):
    '''Event handler class. Used to store args and kwargs which are merged
    come time to call the callback with the event args and kwargs.'''

    nextid = count().next

    def __init__(self, plugin, event, callback, args, kwargs):
        self.id = self.nextid()
        self.plugin = plugin
        self.event = event
        self.callback = callback
        self.args = args
        self.kwargs = kwargs

    def __repr__(self):
        elems = ['id=%d' % self.id, 'event=%s' % self.event,
            'callback=%r' % self.callback]

        if self.args:
            elems.append('args=%s' % repr(self.args))

        if self.kwargs:
            elems.append('kwargs=%s' % repr(self.kwargs))

        elems.append('plugin=%s' % self.plugin.name)
        return u'<handler(%s)>' % ', '.join(elems)

    def call(self, uzbl, *args, **kwargs):
        '''Execute the handler function and merge argument lists.'''

        args = args + self.args
        kwargs = dict(self.kwargs.items() + kwargs.items())
        self.callback(uzbl, *args, **kwargs)


class Plugin(object):
    '''Plugin module wrapper object.'''

    # Special functions exported from the Plugin instance to the
    # plugin namespace.
    special_functions = ['export', 'export_dict', 'connect',
            'connect_dict', 'logger']

    def __init__(self, parent, name, path, plugin):
        self.parent = parent
        self.name = name
        self.path = path
        self.plugin = plugin
        self.logger = logging.getLogger('plugin.%s' % name)

        # Weakrefs to all handlers created by this plugin
        self.handlers = set([])

        # Plugins init hook
        init = getattr(plugin, 'init', None)
        self.init = init if callable(init) else None

        # Plugins optional after hook
        after = getattr(plugin, 'after', None)
        self.after = after if callable(after) else None

        # Plugins optional cleanup hook
        cleanup = getattr(plugin, 'cleanup', None)
        self.cleanup = cleanup if callable(cleanup) else None

        # temporary allow plugins without hooks
        # assert init or after or cleanup, "missing hooks in plugin"

        # Export plugin's instance methods to plugin namespace
        for attr in self.special_functions:
            plugin.__dict__[attr] = getattr(self, attr)

    def __repr__(self):
        return u'<plugin(%r)>' % self.plugin

    def export(self, uzbl, attr, obj, prepend=True):
        '''Attach `obj` to `uzbl` instance. This is the preferred method
        of sharing functionality, functions, data and objects between
        plugins.

        If the object is callable you may wish to turn the callable object
        in to a meta-instance-method by prepending `uzbl` to the call stack.
        You can change this behaviour with the `prepend` argument.
        '''

        assert attr not in uzbl.exports, "attr %r already exported by %r" %\
            (attr, uzbl.exports[attr][0])

        prepend = True if prepend and callable(obj) else False
        uzbl.__dict__[attr] = partial(obj, uzbl) if prepend else obj
        uzbl.exports[attr] = (self, obj, prepend)
        uzbl.logger.info('exported %r to %r by plugin %r, prepended %r',
            obj, 'uzbl.%s' % attr, self.name, prepend)

    def export_dict(self, uzbl, exports):
        for (attr, object) in exports.items():
            self.export(uzbl, attr, object)

    def find_handler(self, event, callback, args, kwargs):
        '''Check if a handler with the identical callback and arguments
        exists and return it.'''

        # Remove dead refs
        self.handlers -= set(filter(lambda ref: not ref(), self.handlers))

        # Find existing identical handler
        for handler in [ref() for ref in self.handlers]:
            if handler.event == event and handler.callback == callback \
              and handler.args == args and handler.kwargs == kwargs:
                return handler

    def connect(self, uzbl, event, callback, *args, **kwargs):
        '''Create an event handler object which handles `event` events.

        Arguments passed to the connect function (`args` and `kwargs`) are
        stored in the handler object and merged with the event arguments
        come handler execution.

        All handler functions must behave like a `uzbl` instance-method (that
        means `uzbl` is prepended to the callback call arguments).'''

        # Sanitise and check event name
        event = event.upper().strip()
        assert event and ' ' not in event

        assert callable(callback), 'callback must be callable'

        # Check if an identical handler already exists
        handler = self.find_handler(event, callback, args, kwargs)
        if not handler:
            # Create a new handler
            handler = EventHandler(self, event, callback, args, kwargs)
            self.handlers.add(weakref.ref(handler))
            self.logger.info('new %r', handler)

        uzbl.handlers[event].append(handler)
        uzbl.logger.info('connected %r', handler)
        return handler

    def connect_dict(self, uzbl, connects):
        for (event, callback) in connects.items():
            self.connect(uzbl, event, callback)



class UzblEventDaemon(object):
    def __init__(self):
        self.opts = opts
        self.server_socket = None
        self._quit = False

        # Hold uzbl instances
        # {child socket: Uzbl instance, ..}
        self.uzbls = {}

        # Hold plugins
        # {plugin name: Plugin instance, ..}
        self.plugins = {}

        # Register that the event daemon server has started by creating the
        # pid file.
        make_pid_file(opts.pid_file)

        # Register a function to clean up the socket and pid file on exit.
        atexit.register(self.quit)

        # Add signal handlers.
        for sigint in [SIGTERM, SIGINT]:
            signal(sigint, self.quit)

        # Load plugins into self.plugins
        self.load_plugins()

    def load_plugins(self):
        '''Load event manager plugins.'''
        import uzbl.plugins
        import pkgutil

        path = uzbl.plugins.__path__
        for impr, name, ispkg in pkgutil.iter_modules(path, 'uzbl.plugins.'):
            module = __import__(name, globals(), locals(), ['*'])

            # Check if the plugin has a callable hook.
            hooks = filter(callable, [getattr(module, attr, None) \
                for attr in ['init', 'after', 'cleanup']])
            # temporarily allow plugin without hooks
            # assert hooks, "no hooks in plugin %r" % module

            logger.debug('creating plugin instance for %r plugin', name)
            plugin = Plugin(self, name, path, module)
            self.plugins[name] = plugin
            logger.info('new %r', plugin)

    def create_server_socket(self):
        '''Create the event manager daemon socket for uzbl instance duplex
        communication.'''

        # Close old socket.
        self.close_server_socket()

        sock = socket(AF_UNIX, SOCK_STREAM)
        sock.bind(opts.server_socket)
        sock.listen(5)

        self.server_socket = sock
        logger.debug('bound server socket to %r', opts.server_socket)

    def run(self):
        '''Main event daemon loop.'''

        logger.debug('entering main loop')

        # Create and listen on the server socket
        self.create_server_socket()

        if opts.daemon_mode:
            # Daemonize the process
            daemonize()

            # Update the pid file
            make_pid_file(opts.pid_file)

        try:
            # Accept incoming connections and listen for incoming data
            self.listen()

        except:
            if not self._quit:
                logger.critical('failed to listen', exc_info=True)

        # Clean up and exit
        self.quit()

        logger.debug('exiting main loop')

    def listen(self):
        '''Accept incoming connections and constantly poll instance sockets
        for incoming data.'''

        logger.info('listening on %r', opts.server_socket)

        # Count accepted connections
        connections = 0

        while (self.uzbls or not connections) or (not opts.auto_close):
            socks = [self.server_socket] + self.uzbls.keys()
            wsocks = [k for k, v in self.uzbls.items() if v.child_buffer]
            reads, writes, errors = select(socks, wsocks, socks, 1)

            if self.server_socket in reads:
                reads.remove(self.server_socket)

                # Accept connection and create uzbl instance.
                child_socket = self.server_socket.accept()[0]
                child_socket.setblocking(False)
                self.uzbls[child_socket] = Uzbl(self, child_socket, opts)
                connections += 1

            for uzbl in [self.uzbls[s] for s in writes]:
                uzbl.do_send()

            for uzbl in [self.uzbls[s] for s in reads]:
                uzbl.read()

            for uzbl in [self.uzbls[s] for s in errors]:
                uzbl.logger.error('socket read error')
                uzbl.close()

        logger.info('auto closing')

    def close_server_socket(self):
        '''Close and delete the server socket.'''

        try:
            if self.server_socket:
                logger.debug('closing server socket')
                self.server_socket.close()
                self.server_socket = None

            if os.path.exists(opts.server_socket):
                logger.info('unlinking %r', opts.server_socket)
                os.unlink(opts.server_socket)

        except:
            logger.error('failed to close server socket', exc_info=True)

    def quit(self, sigint=None, *args):
        '''Close all instance socket objects, server socket and delete the
        pid file.'''

        if sigint == SIGTERM:
            logger.critical('caught SIGTERM, exiting')

        elif sigint == SIGINT:
            logger.critical('caught SIGINT, exiting')

        elif not self._quit:
            logger.debug('shutting down event manager')

        self.close_server_socket()

        for uzbl in self.uzbls.values():
            uzbl.close()

        del_pid_file(opts.pid_file)

        if not self._quit:
            logger.info('event manager shut down')
            self._quit = True


def make_pid_file(pid_file):
    '''Creates a pid file at `pid_file`, fails silently.'''

    try:
        logger.debug('creating pid file %r', pid_file)
        make_dirs(pid_file)
        pid = os.getpid()
        fileobj = open(pid_file, 'w')
        fileobj.write('%d' % pid)
        fileobj.close()
        logger.info('created pid file %r with pid %d', pid_file, pid)

    except:
        logger.error('failed to create pid file', exc_info=True)


def del_pid_file(pid_file):
    '''Deletes a pid file at `pid_file`, fails silently.'''

    if os.path.isfile(pid_file):
        try:
            logger.debug('deleting pid file %r', pid_file)
            os.remove(pid_file)
            logger.info('deleted pid file %r', pid_file)

        except:
            logger.error('failed to delete pid file', exc_info=True)


def get_pid(pid_file):
    '''Reads a pid from pid file `pid_file`, fails None.'''

    try:
        logger.debug('reading pid file %r', pid_file)
        fileobj = open(pid_file, 'r')
        pid = int(fileobj.read())
        fileobj.close()
        logger.info('read pid %d from pid file %r', pid, pid_file)
        return pid

    except (IOError, ValueError):
        logger.error('failed to read pid', exc_info=True)
        return None


def pid_running(pid):
    '''Checks if a process with a pid `pid` is running.'''

    try:
        os.kill(pid, 0)
    except OSError:
        return False
    else:
        return True


def term_process(pid):
    '''Asks nicely then forces process with pid `pid` to exit.'''

    try:
        logger.info('sending SIGTERM to process with pid %r', pid)
        os.kill(pid, SIGTERM)

    except OSError:
        logger.error(get_exc())

    logger.debug('waiting for process with pid %r to exit', pid)
    start = time.time()
    while True:
        if not pid_running(pid):
            logger.debug('process with pid %d exit', pid)
            return True

        if (time.time() - start) > 5:
            logger.warning('process with pid %d failed to exit', pid)
            logger.info('sending SIGKILL to process with pid %d', pid)
            try:
                os.kill(pid, SIGKILL)
            except:
                logger.critical('failed to kill %d', pid, exc_info=True)
                raise

        if (time.time() - start) > 10:
            logger.critical('unable to kill process with pid %d', pid)
            raise OSError

        time.sleep(0.25)


def stop_action():
    '''Stop the event manager daemon.'''

    pid_file = opts.pid_file
    if not os.path.isfile(pid_file):
        logger.error('could not find running event manager with pid file %r',
            pid_file)
        return

    pid = get_pid(pid_file)
    if not pid_running(pid):
        logger.debug('no process with pid %r', pid)
        del_pid_file(pid_file)
        return

    logger.debug('terminating process with pid %r', pid)
    term_process(pid)
    del_pid_file(pid_file)
    logger.info('stopped event manager process with pid %d', pid)


def start_action():
    '''Start the event manager daemon.'''

    pid_file = opts.pid_file
    if os.path.isfile(pid_file):
        pid = get_pid(pid_file)
        if pid_running(pid):
            logger.error('event manager already started with pid %d', pid)
            return

        logger.info('no process with pid %d', pid)
        del_pid_file(pid_file)

    UzblEventDaemon().run()


def restart_action():
    '''Restart the event manager daemon.'''

    stop_action()
    start_action()


def list_action():
    '''List all the plugins that would be loaded in the current search
    dirs.'''

    from types import ModuleType
    import uzbl.plugins
    import pkgutil
    for line in pkgutil.iter_modules(uzbl.plugins.__path__, 'uzbl.plugins.'):
        imp, name, ispkg = line
        print(name)


def make_parser():
    parser = OptionParser('usage: %prog [options] {start|stop|restart|list}')
    add = parser.add_option

    add('-v', '--verbose',
        dest='verbose', default=2, action='count',
        help='increase verbosity')

    socket_location = os.path.join(CACHE_DIR, 'event_daemon')

    add('-s', '--server-socket',
        dest='server_socket', metavar="SOCKET", default=socket_location,
        help='server AF_UNIX socket location')

    add('-p', '--pid-file',
        metavar="FILE", dest='pid_file',
        help='pid file location, defaults to server socket + .pid')

    add('-n', '--no-daemon',
        dest='daemon_mode', action='store_false', default=True,
        help='do not daemonize the process')

    add('-a', '--auto-close',
        dest='auto_close', action='store_true', default=False,
        help='auto close after all instances disconnect')

    add('-o', '--log-file',
        dest='log_file', metavar='FILE',
        help='write logging output to a file, defaults to server socket +'
        ' .log')

    add('-q', '--quiet-events',
        dest='print_events', action="store_false", default=True,
        help="silence the printing of events to stdout")

    return parser


def init_logger():
    log_level = logging.CRITICAL - opts.verbose * 10
    logger = logging.getLogger()
    logger.setLevel(max(log_level, 10))

    # Console
    handler = logging.StreamHandler()
    handler.setLevel(max(log_level + 10, 10))
    handler.setFormatter(logging.Formatter(
        '%(name)s: %(levelname)s: %(message)s'))
    logger.addHandler(handler)

    # Logfile
    handler = logging.FileHandler(opts.log_file, 'w', 'utf-8', 1)
    handler.setLevel(max(log_level, 10))
    handler.setFormatter(logging.Formatter(
        '[%(created)f] %(name)s: %(levelname)s: %(message)s'))
    logger.addHandler(handler)


def main():
    global opts

    parser = make_parser()

    (opts, args) = parser.parse_args()

    opts.server_socket = expandpath(opts.server_socket)

    # Set default pid file location
    if not opts.pid_file:
        opts.pid_file = "%s.pid" % opts.server_socket

    else:
        opts.pid_file = expandpath(opts.pid_file)

    # Set default log file location
    if not opts.log_file:
        opts.log_file = "%s.log" % opts.server_socket

    else:
        opts.log_file = expandpath(opts.log_file)

    # Logging setup
    init_logger()
    logger.info('logging to %r', opts.log_file)

    if opts.auto_close:
        logger.debug('will auto close')
    else:
        logger.debug('will not auto close')

    if opts.daemon_mode:
        logger.debug('will daemonize')
    else:
        logger.debug('will not daemonize')

    # init like {start|stop|..} daemon actions
    daemon_actions = {'start': start_action, 'stop': stop_action,
        'restart': restart_action, 'list': list_action}

    if len(args) == 1:
        action = args[0]
        if action not in daemon_actions:
            parser.error('invalid action: %r' % action)

    elif not args:
        action = 'start'
        logger.warning('no daemon action given, assuming %r', action)

    else:
        parser.error('invalid action argument: %r' % args)

    logger.info('daemon action %r', action)
    # Do action
    daemon_actions[action]()

    logger.debug('process CPU time: %f', time.clock())


if __name__ == "__main__":
    main()


# vi: set et ts=4: