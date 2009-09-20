#!/usr/bin/env python

# Event Manager for Uzbl
# Copyright (c) 2009, Mason Larobina <mason.larobina@gmail.com>
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

Usage
=====

  uzbl | $XDG_DATA_HOME/uzbl/scripts/event_manager.py

Todo
====

 - Command line options including supplying a list of plugins to load or not
   load (default is load all plugins in the plugin_dir).
 - Spell checking.


'''

import imp
import os
import sys
import select
import re
import types
import socket
import pprint
import time
from optparse import OptionParser
from traceback import print_exc


# ============================================================================
# ::: Default configuration section ::::::::::::::::::::::::::::::::::::::::::
# ============================================================================


def xdghome(key, default):
    '''Attempts to use the environ XDG_*_HOME paths if they exist otherwise
    use $HOME and the default path.'''

    xdgkey = "XDG_%s_HOME" % key
    if xdgkey in os.environ.keys() and os.environ[xdgkey]:
        return os.environ[xdgkey]

    return os.path.join(os.environ['HOME'], default)

# Setup xdg paths.
DATA_DIR = os.path.join(xdghome('DATA', '.local/share/'), 'uzbl/')

# Config dict (NOT the same as the uzbl.config).
config = {
    'verbose': False,
    'plugin_dir': "$XDG_DATA_HOME/uzbl/scripts/plugins/",
    'plugins_load': [],
    'plugins_ignore': [],
}


# ============================================================================
# ::: End of configuration section :::::::::::::::::::::::::::::::::::::::::::
# ============================================================================


# Define some globals.
_SCRIPTNAME = os.path.basename(sys.argv[0])
_RE_FINDSPACES = re.compile("\s+")

def echo(msg):
    '''Prints only if the verbose flag has been set.'''

    if config['verbose']:
        sys.stdout.write("%s: %s\n" % (_SCRIPTNAME, msg))


def error(msg):
    '''Prints error messages to stderr.'''

    sys.stderr.write("%s: error: %s\n" % (_SCRIPTNAME, msg))


def counter():
    '''Generate unique object id's.'''

    i = 0
    while True:
        i += 1
        yield i


def iscallable(obj):
    '''Return true if the object is callable.'''

    return hasattr(obj, "__call__")


def isiterable(obj):
    '''Return true if you can iterate over the item.'''

    return hasattr(obj, "__iter__")


class PluginManager(dict):
    def __init__(self):

        plugin_dir = os.path.expandvars(config['plugin_dir'])
        self.plugin_dir = os.path.realpath(plugin_dir)
        if not os.path.exists(self.plugin_dir):
            os.makedirs(self.plugin_dir)

        self.load_plugins()


    def _find_all_plugins(self):
        '''Find all python scripts in plugin dir and return a list of
        locations and imp moduleinfo's.'''

        dirlist = os.listdir(self.plugin_dir)
        pythonfiles = filter(lambda s: s.endswith('.py'), dirlist)

        plugins = []

        for filename in pythonfiles:
            plugins.append(filename[:-3])

        return plugins


    def _unload_plugin(self, name, remove_pyc=True):
        '''Unload specific plugin and remove all waste in sys.modules

        Notice: manual manipulation of sys.modules is very un-pythonic but I
        see no other way to make sure you have 100% unloaded the module. Also
        this allows us to implement a reload plugins function.'''

        allmodules = sys.modules.keys()
        allrefs = filter(lambda s: s.startswith("%s." % name), allmodules)

        for ref in allrefs:
            del sys.modules[ref]

        if name in sys.modules.keys():
            del sys.modules[name]

        if name in self:
            del self[name]

        if remove_pyc:
            pyc = os.path.join(self.plugin_dir, '%s.pyc' % name)
            if os.path.exists(pyc):
                os.remove(pyc)


    def load_plugins(self):

        if config['plugins_load']:
            pluginlist = config['plugins_load']

        else:
            pluginlist = self._find_all_plugins()
            for name in config['plugins_ignore']:
                if name in pluginlist:
                    pluginlist.remove(name)

        for name in pluginlist:
            # Make sure the plugin isn't already loaded.
            self._unload_plugin(name)

            try:
                moduleinfo = imp.find_module(name, [self.plugin_dir,])
                plugin = imp.load_module(name, *moduleinfo)
                self[name] = plugin

            except:
                raise

        if self.keys():
            echo("loaded plugin(s): %s" % ', '.join(self.keys()))


    def reload_plugins(self):
        '''Unload all loaded plugins then run load_plugins() again.

        IMPORTANT: It is crucial that the event handler be deleted if you
        are going to unload any modules because there is now way to track
        which module created wich handler.'''

        for plugin in self.keys():
            self._unload_plugin(plugin)

        self.load_plugins()


class CallPrepender(object):
    '''Execution argument modifier. Takes (arg, function) then modifies the
    function call:

    -> function(*args, **kargs) ->  function(arg, *args, **kargs) ->'''

    def __init__(self, uzbl, function):
        self.function = function
        self.uzbl = uzbl

    def call(self, *args, **kargs):
        return self.function(self.uzbl, *args, **kargs)


class Handler(object):

    nexthid = counter().next

    def __init__(self, event, handler, *args, **kargs):
        self.callable = iscallable(handler)
        if self.callable:
            self.function = handler
            self.args = args
            self.kargs = kargs

        elif kargs:
            raise ArgumentError("cannot supply kargs with a uzbl command")

        elif isiterable(handler):
            self.commands = handler

        else:
            self.commands = [handler,] + list(args)

        self.event = event
        self.hid = self.nexthid()


    def __repr__(self):
        args = ["event=%s" % self.event, "hid=%d" % self.hid]

        if self.callable:
            args.append("function=%r" % self.function)
            if self.args:
                args.append("args=%r" % self.args)

            if self.kargs:
                args.append("kargs=%r" % self.kargs)

        else:
            cmdlen = len(self.commands)
            cmds = self.commands[0] if cmdlen == 1 else self.commands
            args.append("command%s=%r" % ("s" if cmdlen-1 else "", cmds))

        return "<EventHandler(%s)>" % ', '.join(args)


class UzblInstance(object):
    '''Event manager for a uzbl instance.'''

    # Singleton plugin manager.
    plugins = None

    def __init__(self):
        '''Initialise event manager.'''

        # Hold functions exported by plugins.
        self._exports = {}
        self._running = None
        self._buffer = ''

        self._handlers = {}

        # Variables needed for fifo & socket communication with uzbl.
        self.uzbl_fifo = None
        self.uzbl_socket = None
        self._fifo_cmd_queue = []
        self._socket_cmd_queue = []
        self._socket = None
        self.send = self._send_socket

        if not self.plugins:
            self.plugins = PluginManager()

        # Call the init() function in every plugin which then setup their
        # respective hooks (event handlers, binds or timers).
        self._init_plugins()


    def __getattribute__(self, name):
        '''Expose any exported functions before class functions.'''

        if not name.startswith('_'):
            exports = object.__getattribute__(self, '_exports')
            if name in exports:
                return exports[name]

        return object.__getattribute__(self, name)


    def _init_plugins(self):
        '''Call the init() function in every plugin and expose all exposable
        functions in the plugins root namespace.'''

        # Map all plugin exports
        for (name, plugin) in self.plugins.items():
            if not hasattr(plugin, '__export__'):
                continue

            for export in plugin.__export__:
                if export in self._exports:
                    orig = self._exports[export]
                    raise KeyError("already exported attribute: %r" % export)

                obj = getattr(plugin, export)
                if iscallable(obj):
                    # Wrap the function in the CallPrepender object to make
                    # the exposed functions act like instance methods.
                    obj = CallPrepender(self, obj).call

                self._exports[export] = obj

        echo("exposed attribute(s): %s" % ', '.join(self._exports.keys()))

        # Now call the init function in all plugins.
        for (name, plugin) in self.plugins.items():
            try:
                plugin.init(self)

            except:
                #print_exc()
                raise


    def _init_uzbl_socket(self, uzbl_socket=None, timeout=None):
        '''Store socket location and open socket connection to uzbl socket.'''

        if uzbl_socket is None:
            uzbl_socket = self.uzbl_socket

        if not uzbl_socket:
            error("no socket location.")
            return

        if not os.path.exists(uzbl_socket):
            if timeout is None:
                error("uzbl socket doesn't exist: %r" % uzbl_socket)
                return

            waitlimit = time.time() + timeout
            echo("waiting for uzbl socket: %r" % uzbl_socket)
            while not os.path.exists(uzbl_socket):
                time.sleep(0.25)
                if time.time() > waitlimit:
                    error("timed out waiting for socket: %r" % uzbl_socket)
                    return

        self.uzbl_socket = uzbl_socket
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(self.uzbl_socket)
        self._socket = sock


    def _close_socket(self):
        '''Close the socket used for communication with the uzbl instance.
        This function is normally called upon receiving the INSTANCE_EXIT
        event.'''

        if self._socket:
            self._socket.close()

        self.uzbl_socket = self._socket = None


    def _flush(self):
        '''Flush messages from the outgoing queue to the uzbl instance.'''

        if len(self._fifo_cmd_queue) and self.uzbl_fifo:
            if os.path.exists(self.uzbl_fifo):
                h = open(self.uzbl_fifo, 'w')
                while len(self._fifo_cmd_queue):
                    msg = self._fifo_cmd_queue.pop(0)
                    print '<-- %s' % msg
                    h.write("%s\n" % msg)

                h.close()

        if len(self._socket_cmd_queue) and self.uzbl_socket:
            if not self._socket and os.path.exists(self.uzbl_socket):
                self._init_uzbl_socket()

            if self._socket:
                while len(self._socket_cmd_queue):
                    msg = self._socket_cmd_queue.pop(0)
                    print '<-- %s' % msg
                    self._socket.send("%s\n" % msg)


    def _send_fifo(self, msg):
        '''Send a command to the uzbl instance via the fifo socket.'''

        self._fifo_cmd_queue.append(msg)
        self._flush()


    def _send_socket(self, msg):
        '''Send a command to the uzbl instance via the socket file.'''

        self._socket_cmd_queue.append(msg)
        self._flush()


    def connect(self, event, handler, *args, **kargs):
        '''Connect event with handler and return the newly created handler.
        Handlers can either be a function or a uzbl command string.'''

        if event not in self._handlers.keys():
            self._handlers[event] = []

        handler = Handler(event, handler, *args, **kargs)
        self._handlers[event].append(handler)

        print handler
        return handler


    def remove_by_id(self, hid):
        '''Remove connected event handler by unique handler id.'''

        for (event, handlers) in self._handlers.items():
            for handler in list(handlers):
                if hid != handler.hid:
                    continue

                echo("removed %r" % handler)
                handlers.remove(handler)
                return

        echo('unable to find & remove handler with id: %d' % handler.hid)


    def remove(self, handler):
        '''Remove connected event handler.'''

        for (event, handlers) in self._handlers.items():
            if handler in handlers:
                echo("removed %r" % handler)
                handlers.remove(handler)
                return

        echo('unable to find & remove handler: %r' % handler)


    def listen_from_fd(self, fd):
        '''Polls for event messages from fd.'''

        try:
            self._running = True
            while self._running:
                if select.select([fd,], [], [], 1)[0]:
                    self.read_from_fd(fd)
                    continue

                self._flush()

        except KeyboardInterrupt:
            print

        except:
            #print_exc()
            raise


    def read_from_fd(self, fd):
        '''Reads event messages from a single fd.'''

        raw = fd.readline()
        if not raw:
            # Read null byte (i.e. uzbl closed).
            self._running = False
            return

        msg = raw.strip().split(' ', 3)

        if not msg or msg[0] != "EVENT":
            # Not an event message
            print "---", raw.rstrip()
            return

        event, args = msg[1], msg[3]
        self.handle_event(event, args)


    def listen_from_uzbl_socket(self, uzbl_socket):
        '''Polls for event messages from a single uzbl socket.'''

        self._init_uzbl_socket(uzbl_socket, 10)

        if not self._socket:
            error("failed to init socket: %r" % uzbl_socket)
            return

        self._flush()
        try:
            self._running = True
            while self._running:
                if select.select([self._socket], [], [], 1):
                    self.read_from_uzbl_socket()
                    continue

                self._flush()

        except KeyboardInterrupt:
            print

        except:
            #print_exc()
            raise


    def read_from_uzbl_socket(self):
        '''Reads event messages from a uzbl socket.'''

        raw = self._socket.recv(1024)
        if not raw:
            # Read null byte
            self._running = False
            return

        self._buffer += raw
        msgs = self._buffer.split("\n")
        self._buffer = msgs.pop()

        for msg in msgs:
            msg = msg.rstrip()
            if not msg:
                continue

            cmd = _RE_FINDSPACES.split(msg, 3)
            if not cmd or cmd[0] != "EVENT":
                # Not an event message
                print msg.rstrip()
                continue

            event, args = cmd[2], cmd[3]
            try:
                self.handle_event(event, args)

            except:
                #print_exc()
                raise


    def handle_event(self, event, args):
        '''Handle uzbl events internally before dispatch.'''

        if event == 'FIFO_SET':
            self.uzbl_fifo = args
            self._flush()

        elif event == 'SOCKET_SET':
            if not self.uzbl_socket or not self._socket:
                self._init_uzbl_socket(args)
                self._flush()

        elif event == 'INSTANCE_EXIT':
            self._close_socket()
            self._running = False
            for (name, plugin) in self.plugins.items():
                if hasattr(plugin, "cleanup"):
                    plugin.cleanup(uzbl)

        # Now handle the event "publically".
        self.event(event, args)


    def exec_handler(self, handler, *args, **kargs):
        '''Execute either the handler function or send the handlers uzbl
        commands via the socket.'''

        if handler.callable:
            args = args + handler.args
            kargs = dict(handler.kargs.items()+kargs.items())
            handler.function(uzbl, *args, **kargs)

        else:
            if kargs:
                raise ArgumentError('cannot supply kargs for uzbl commands')

            for command in handler.commands:
                if '%s' in command:
                    if len(args) > 1:
                        for arg in args:
                            command = command.replace('%s', arg, 1)

                    elif len(args) == 1:
                        command = command.replace('%s', args[0])

                uzbl.send(command)


    def event(self, event, *args, **kargs):
        '''Raise a custom event.'''

        # Silence _printing_ of geo events while still debugging.
        if event != "GEOMETRY_CHANGED":
            print "--> %s %s %s" % (event, args, '' if not kargs else kargs)

        if event in self._handlers:
            for handler in self._handlers[event]:
                try:
                    self.exec_handler(handler, *args, **kargs)

                except:
                    #print_exc()
                    raise


if __name__ == "__main__":
    #uzbl = UzblInstance().listen_from_fd(sys.stdin)

    parser = OptionParser()
    parser.add_option('-s', '--uzbl-socket', dest='socket',
      action="store", metavar="SOCKET",
      help="read event messages from uzbl socket.")

    parser.add_option('-v', '--verbose', dest='verbose', action="store_true",
      help="print verbose output.")

    parser.add_option('-d', '--plugin-dir', dest='plugin_dir', action="store",
      metavar="DIR", help="change plugin directory.")

    parser.add_option('-p', '--load-plugins', dest="load", action="store",
      metavar="PLUGINS", help="comma separated list of plugins to load")

    parser.add_option('-i', '--ignore-plugins', dest="ignore", action="store",
      metavar="PLUGINS", help="comma separated list of plugins to ignore")

    parser.add_option('-l', '--list-plugins', dest='list', action='store_true',
      help="list all the plugins in the plugin dir.")

    (options, args) = parser.parse_args()

    if len(args):
        for arg in args:
            error("unknown argument: %r" % arg)

        raise ArgumentError

    if options.verbose:
        config['verbose'] = True

    if options.plugin_dir:
        plugin_dir = os.path.expandvars(options.plugin_dir)
        if not os.path.isdir(plugin_dir):
            error("%r is not a directory" % plugin_dir)
            sys.exit(1)

        config['plugin_dir'] = plugin_dir
        echo("changed plugin dir: %r" % plugin_dir)

    if options.load and options.ignore:
        error("you can't load and ignore at the same time.")
        sys.exit(1)

    elif options.load:
        plugins_load = config['plugins_load']
        for plugin in options.load.split(','):
            if plugin.strip():
                plugins_load.append(plugin.strip())

        echo('only loading plugin(s): %s' % ', '.join(plugins_load))

    elif options.ignore:
        plugins_ignore = config['plugins_ignore']
        for plugin in options.ignore.split(','):
            if plugin.strip():
                plugins_ignore.append(plugin.strip())

        echo('ignoring plugin(s): %s' % ', '.join(plugins_ignore))


    if options.list:
        plugin_dir = os.path.expandvars(config['plugin_dir'])
        if not os.path.isdir(plugin_dir):
            error("not a directory: %r" % plugin_dir)
            sys.exit(1)

        dirlist = filter(lambda p: p.endswith('.py'), os.listdir(plugin_dir))
        print ', '.join([p[:-3] for p in dirlist])

    else:
        uzbl = UzblInstance()
        if options.socket:
            echo("listen from uzbl socket: %r" % options.socket)
            uzbl.listen_from_uzbl_socket(options.socket)

        else:
            uzbl.listen_from_fd(sys.stdin)
