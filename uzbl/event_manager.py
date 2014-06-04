#!/usr/bin/env python3


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
import asyncore
from collections import defaultdict
from functools import partial
from glob import glob
from itertools import count
from optparse import OptionParser
from select import select
from signal import signal, SIGTERM, SIGINT, SIGKILL
from traceback import format_exc

from uzbl.net import Listener, Protocol
from uzbl.core import Uzbl

def xdghome(key, default):
    '''Attempts to use the environ XDG_*_HOME paths if they exist otherwise
    use $HOME and the default path.'''

    xdgkey = "XDG_%s_HOME" % key
    if xdgkey in list(os.environ.keys()) and os.environ[xdgkey]:
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
    stdin = open(devnull, 'r')
    stdout = open(devnull, 'a+')
    stderr = open(devnull, 'a+')

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


class PluginDirectory(object):
    def __init__(self):
        self.global_plugins = []
        self.per_instance_plugins = []

    def load(self):
        ''' Import plugin files '''

        import uzbl.plugins
        import pkgutil

        path = uzbl.plugins.__path__
        for impr, name, ispkg in pkgutil.iter_modules(path, 'uzbl.plugins.'):
            __import__(name, globals(), locals())

        from uzbl.ext import global_registry, per_instance_registry
        self.global_plugins.extend(global_registry)
        self.per_instance_plugins.extend(per_instance_registry)


class UzblEventDaemon(object):
    def __init__(self, listener, plugind):
        listener.target = self
        self.opts = opts
        self.listener = listener
        self.plugind = plugind
        self._plugin_instances = []
        self._quit = False

        # Hold uzbl instances
        # {child socket: Uzbl instance, ..}
        self.uzbls = {}

        self.plugins = {}

        # Register that the event daemon server has started by creating the
        # pid file.
        make_pid_file(opts.pid_file)

        # Register a function to clean up the socket and pid file on exit.
        atexit.register(self.quit)

        # Add signal handlers.
        for sigint in [SIGTERM, SIGINT]:
            signal(sigint, self.quit)

        # Scan plugin directory for plugins
        self.plugind.load()

        # Initialise global plugins with instances in self.plugins
        self.init_plugins()

    def init_plugins(self):
        '''Initialise event manager plugins.'''
        self.plugins = {}

        for plugin in self.plugind.global_plugins:
            pinst = plugin(self)
            self._plugin_instances.append(pinst)
            self.plugins[plugin] = pinst

    def run(self):
        '''Main event daemon loop.'''

        logger.debug('entering main loop')

        if opts.daemon_mode:
            # Daemonize the process
            daemonize()

            # Update the pid file
            make_pid_file(opts.pid_file)

        asyncore.loop()

        # Clean up and exit
        self.quit()

        logger.debug('exiting main loop')

    def add_instance(self, sock):
        proto = Protocol(sock)
        uzbl = Uzbl(self, proto, opts)
        self.uzbls[sock] = uzbl
        for plugin in self.plugins.values():
            plugin.new_uzbl(uzbl)

    def remove_instance(self, sock):
        if sock in self.uzbls:
            for plugin in self.plugins.values():
                plugin.free_uzbl(self.uzbls[sock])
            del self.uzbls[sock]
        if not self.uzbls and opts.auto_close:
            self.quit()

    def close_server_socket(self):
        '''Close and delete the server socket.'''

        try:
            self.listener.close()

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

        for uzbl in list(self.uzbls.values()):
            uzbl.close()

        if not self._quit:
            for plugin in self._plugin_instances:
                plugin.cleanup()
            del self.plugins  # to avoid cyclic links
            del self._plugin_instances

        del_pid_file(opts.pid_file)

        if not self._quit:
            logger.info('event manager shut down')
            self._quit = True
            raise SystemExit()


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

    listener = Listener(opts.server_socket)
    listener.start()
    plugind = PluginDirectory()
    daemon = UzblEventDaemon(listener, plugind)
    daemon.run()


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
    handler = logging.FileHandler(opts.log_file, 'a+', 'utf-8', 1)
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
