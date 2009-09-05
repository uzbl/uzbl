#!/usr/bin/env python

# Event Manager for Uzbl
# Copyright (c) 2009, Mason Larobina <mason.larobina@gmail.com>
# Copyright (c) 2009, Dieter Plaetinck <diterer@plaetinck.be>
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

Some descriptive text here.

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
import pprint
import socket
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
    'verbose': True,
    'plugin_dir': "$XDG_DATA_HOME/uzbl/scripts/plugins/"
}


# ============================================================================
# ::: End of configuration section :::::::::::::::::::::::::::::::::::::::::::
# ============================================================================


# Define some globals.
_VALIDSETKEY = re.compile("^[a-zA-Z][a-zA-Z0-9_]*$").match
_SCRIPTNAME = os.path.basename(sys.argv[0])


def echo(msg):
    '''Prints only if the verbose flag has been set.'''

    if config['verbose']:
        sys.stderr.write("%s: %s\n" % (_SCRIPTNAME, msg))


def counter():
    '''Generate unique object id's.'''

    i = 0
    while True:
        i += 1
        yield i


class PluginManager(dict):
    def __init__(self):

        plugin_dir = os.path.expandvars(config['plugin_dir'])
        self.plugin_dir = os.path.realpath(plugin_dir)
        if not os.path.exists(self.plugin_dir):
            os.makedirs(self.plugin_dir)

        # Load all plugins in the plugin_dir.
        self.load_plugins()


    def _find_plugins(self):
        '''Find all python scripts in plugin dir and return a list of
        locations and imp moduleinfo's.'''

        dirlist = os.listdir(self.plugin_dir)
        pythonfiles = filter(lambda s: s.endswith('.py'), dirlist)

        plugins = []

        for filename in pythonfiles:
            plugins.append(filename[:-3])

        return plugins


    def _unload_plugin(self, plugin, remove_pyc=True):
        '''Unload specific plugin and remove all waste in sys.modules

        Notice: manual manipulation of sys.modules is very un-pythonic but I
        see no other way to make sure you have 100% unloaded the module. Also
        this allows us to implement a reload plugins function.'''

        allmodules = sys.modules.keys()
        allrefs = filter(lambda s: s.startswith("%s." % plugin), allmodules)

        for ref in allrefs:
            del sys.modules[ref]

        if plugin in sys.modules.keys():
            del sys.modules[plugin]

        if plugin in self.keys():
            dict.__delitem__(self, plugin)

        if remove_pyc:
            # Now remove bytecode.
            pyc = os.path.join(self.plugin_dir, '%s.pyc' % plugin)
            if os.path.exists(pyc):
                os.remove(pyc)


    def load_plugins(self):

        # Get a list of python files in the plugin_dir.
        pluginlist = self._find_plugins()

        # Load the plugins
        for name in pluginlist:
            try:
                # Make sure the plugin isn't already loaded.
                self._unload_plugin(name)

            except:
                print_exc()

            try:
                moduleinfo = imp.find_module(name, [self.plugin_dir,])
                plugin = imp.load_module(name, *moduleinfo)
                dict.__setitem__(self, name, plugin)

                # Check it has the init function.
                if not hasattr(plugin, 'init'):
                    raise ImportError('plugin missing main "init" function.')

                print "Loaded plugin: %r" % name

            except:
                print_exc()
                self._unload_plugin(name)


    def reload_plugins(self):
        '''Unload all loaded plugins then run load_plugins() again.

        IMPORTANT: It is crucial that the event handler be deleted if you
        are going to unload any modules because there is now way to track
        which module created wich handler.'''

        for plugin in self.keys():
            self._unload_plugin(plugin)

        self.load_plugins()


class UzblInstance:
    '''Event manager for a uzbl instance.'''

    # Singleton plugin manager.
    plugins = None

    def __init__(self):
        '''Initialise event manager.'''

        class ConfigDict(dict):
            def __init__(self, setcmd):
                self._setcmd = setcmd

            def __setitem__(self, key, value):
                '''Updates the config dict and relays any changes back to the
                uzbl instance via the set function.'''

                self._setcmd(key, value)
                dict.__setitem__(self, key, value)

        self._config = ConfigDict(self.set)

        # Keep track of keys typed.
        self.cmdbuffer = ""

        # Keep track of non-meta keys held.
        self.heldkeys = []

        # Keep track of meta keys held.
        self.metaheld = []

        # Hold classic bind commands.
        self.binds = {}

        # Keep track of the mode.
        self.mode = "command"

        # Event handlers
        self.handlers = {}

        # Handler object id generator
        self.nextid = counter().next

        # Fifo socket and socket file locations.
        self.fifo_socket = None
        self.socket_file = None

        # Outgoing socket
        self._socketout = []
        self._socket = None

        # Outgoing fifo
        self._fifoout = []

        # Default send method
        self.send = self._send_socket

        # Running flag
        self._running = None

        # Incoming message buffer
        self._buffer = ""

        # Initialise plugin manager
        if not self.plugins:
            self.plugins = PluginManager()

        # Call the init() function in every plugin.
        self._init_plugins()


    def _get_config(self):
        '''Return the uzbl config dictionary.'''

        return self._config

    # Set read-only config dict getter.
    config = property(_get_config)


    def _init_plugins(self):
        '''Call the init() function in every plugin.'''

        pprint.pprint(self.plugins)

        for plugin in self.plugins.keys():
            try:
                self.plugins[plugin].init(self)

            except:
                print_exc()


    def _flush(self):
        '''Flush messages from the outgoing queue to the uzbl instance.'''

        if len(self._fifoout) and self.fifo_socket:
            if os.path.exists(self.fifo_socket):
                h = open(self.fifo_socket, 'w')
                while len(self._fifoout):
                    msg = self._fifoout.pop(0)
                    print "Sending via fifo: %r" % msg
                    h.write("%s\n" % msg)
                h.close()

        if len(self._socketout) and self.socket_file:
            if not self._socket and os.path.exists(self.socket_file):
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                sock.connect(self.socket_file)
                self._socket = sock

            if self._socket:
                while len(self._socketout):
                    msg = self._socketout.pop(0)
                    print "Sending via socket: %r" % msg
                    self._socket.send("%s\n" % msg)


    def _send_fifo(self, msg):
        '''Send a command to the uzbl instance via the fifo socket.'''

        self._fifoout.append(msg)
        self._flush()


    def _send_socket(self, msg):
        '''Send a command to the uzbl instance via the socket file.'''

        self._socketout.append(msg)
        self._flush()


    def connect(self, event, handler, *args, **kargs):
        '''Connect event with handler and return unique handler id. It goes
        without saying that if you connect handlers with non-existent events
        nothing will happen so be careful.

        If you choose the handler may be a uzbl command and upon receiving the
        event the chosen command will be executed by the uzbl instance.'''

        if event not in self.handlers.keys():
            self.handlers[event] = {}

        id = self.nextid()
        d = {'handler': handler, 'args': args, 'kargs': kargs}

        self.handlers[event][id] = d
        print "Added handler:", event, d

        # The unique id is returned so that the newly created event handler can
        # be destroyed if need be.
        return id


    def remove(self, id):
        '''Remove connected event handler by unique handler id.'''

        for event in self.handlers.keys():
            if id in self.handlers[event].keys():
                print "Removed handler:", self.handlers[event][id]
                del self.handlers[event][id]


    def bind(self, glob, cmd=None):
        '''Support for classic uzbl binds.

        For example:
          bind ZZ = exit          -> bind('ZZ', 'exit')
          bind o _ = uri %s       -> bind('o _', 'uri %s')
          bind fl* = sh 'echo %s' -> bind('fl*', "sh 'echo %s'")
          bind fl* =              -> bind('fl*')

        And it is also possible to execute a function on activation:
          bind('DD', myhandler)

        NOTE: This wont work yet but the groundwork has been layed out.
        '''

        if not cmd:
            if glob in self.binds.keys():
                print "Deleted bind:", self.binds[glob]
                del self.binds[glob]

        d = {'glob': glob, 'once': True, 'hasargs': True, 'cmd': cmd}

        if glob.endswith('*'):
            d['pre'] = glob.rstrip('*')
            d['once'] = False

        elif glob.endswith('_'):
            d['pre'] = glob.rstrip('_')

        else:
            d['pre'] = glob
            d['hasargs'] = False

        self.binds[glob] = d
        print "Added bind:", d


    def set(self, key, value):
        '''Sets key "key" with value "value" in the uzbl instance.'''

        # TODO: Make a real escaping function.
        escape = str

        if not _VALIDSETKEY(key):
            raise KeyError("%r" % key)

        if '\n' in value:
            raise ValueError("invalid character: \\n")

        self.send('set %s = %s' % (key, escape(value)))


    def listen_from_fd(self, fd):
        '''Main loop reading event messages from stdin.'''

        self._running = True
        try:
            while self._running:

                # Poll for reading & errors from fd.
                if select.select([fd,], [], [], 1)[0]:
                    self.read_from_fd(fd)
                    continue

                # Check that all messages have been purged from the out queue.
                self._flush()

        except KeyboardInterrupt:
            print


    def read_from_fd(self, fd):
        '''Reads incoming event messages from fd.'''

        raw = fd.readline()
        if not raw:
            # Read null byte (i.e. uzbl closed).
            self._running = False
            return

        msg = raw.strip().split(' ', 3)

        if not msg or msg[0] != "EVENT":
            # Not an event message
            return

        event, args = msg[1], msg[3]
        self.handle_event(event, args)


    def handle_event(self, event, args):
        '''Handle uzbl events internally before dispatch.'''

        if event == 'VARIABLE_SET':
            l = args.split(' ', 1)
            if len(l) == 1:
                l.append("")

            key, value = l
            dict.__setitem__(self._config, key, value)

        elif event == 'FIFO_SET':
            self.fifo_socket = args

            # Workaround until SOCKET_SET is implemented.
            self.socket_file = args.replace("fifo", "socket")

        elif event == 'SOCKET_SET':
            self.socket_file = args

        # Now dispatch event to plugin's event handlers.
        self.dispatch_event(event, args)


    def dispatch_event(self, event, args):
        '''Now send the event to any event handlers added with the connect
        function. In other words: handle plugin's event hooks.'''
        unhandled = True

        if event in self.handlers.keys():
            for hid in self.handlers[event]:
                try:
                    unhandled = False
                    handler = self.handlers[event][hid]
                    print "Executing handler:", event, handler
                    self.exc_handler(handler, args)

                except:
                    print_exc()

        if unhandled:
            print "Unhandled event:", event, args


    def exc_handler(self, d, args):
        '''Handle handler.'''

        if type(d['handler']) == types.FunctionType:
            handler = d['handler']
            handler(self, args, *d['args'], **d['kargs'])

        else:
            cmd = d['handler']
            self.send(cmd)


if __name__ == "__main__":
    uzbl = UzblInstance().listen_from_fd(sys.stdin)
