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
_TYPECONVERT = {'int': int, 'float': float, 'str': str}

def echo(msg):
    '''Prints only if the verbose flag has been set.'''

    if config['verbose']:
        sys.stdout.write("%s: %s\n" % (_SCRIPTNAME, msg))


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
            pyc = os.path.join(self.plugin_dir, '%s.pyc' % plugin)
            if os.path.exists(pyc):
                os.remove(pyc)


    def load_plugins(self):

        pluginlist = self._find_plugins()

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

            except:
                print_exc()
                self._unload_plugin(name)

        if len(self.keys()):
            echo("loaded plugin(s): %s" % ', '.join(self.keys()))


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

                if type(value) == types.BooleanType:
                    value = int(value)

                if key in self.keys() and type(value) != type(self[key]):
                    raise TypeError("%r for %r" % (type(value), key))

                else:
                    # All custom variables are strings.
                    value = "" if value is None else str(value)

                self._setcmd(key, value)
                dict.__setitem__(self, key, value)

        self._config = ConfigDict(self.set)
        self._running = None

        self._cmdbuffer = []
        self.keysheld = []
        self.metaheld = []
        self.mode = "command"

        self.binds = {}
        self.handlers = {}
        self.nexthid = counter().next

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


    def _get_config(self):
        '''Return the uzbl config dictionary.'''

        return self._config

    config = property(_get_config)


    def _init_plugins(self):
        '''Call the init() function in every plugin.'''

        for plugin in self.plugins.keys():
            try:
                self.plugins[plugin].init(self)

            except:
                print_exc()


    def _flush(self):
        '''Flush messages from the outgoing queue to the uzbl instance.'''

        if len(self._fifo_cmd_queue) and self.uzbl_fifo:
            if os.path.exists(self.uzbl_fifo):
                h = open(self.uzbl_fifo, 'w')
                while len(self._fifo_cmd_queue):
                    msg = self._fifo_cmd_queue.pop(0)
                    print "Sending via fifo: %r" % msg
                    h.write("%s\n" % msg)
                h.close()

        if len(self._socket_cmd_queue) and self.uzbl_socket:
            if not self._socket and os.path.exists(self.uzbl_socket):
                sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                sock.connect(self.uzbl_socket)
                self._socket = sock

            if self._socket:
                while len(self._socket_cmd_queue):
                    msg = self._socket_cmd_queue.pop(0)
                    print "Sending via socket: %r" % msg
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
        '''Connect event with handler and return unique handler id. It goes
        without saying that if you connect handlers with non-existent events
        nothing will happen so be careful.

        If you choose the handler may be a uzbl command and upon receiving the
        event the chosen command will be executed by the uzbl instance.'''

        if event not in self.handlers.keys():
            self.handlers[event] = {}

        id = self.nexthid()
        d = {'handler': handler, 'args': args, 'kargs': kargs}

        self.handlers[event][id] = d
        echo("added handler for %s: %r" % (event, d))

        return id


    def remove(self, id):
        '''Remove connected event handler by unique handler id.'''

        for event in self.handlers.keys():
            if id in self.handlers[event].keys():
                echo("removed handler %d" % id)
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
                echo("deleted bind: %r" % self.binds[glob])
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
        echo("added bind: %r" % d)


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
        while self._running:
            try:
                if select.select([fd,], [], [], 1)[0]:
                    self.read_from_fd(fd)
                    continue

                self._flush()

            except KeyboardInterrupt:
                self._running = False
                print

            except:
                print_exc()


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

        print event, args

        if event == 'VARIABLE_SET':
            l = args.split(' ', 2)
            if len(l) == 2:
                l.append("")

            key, type, value = l
            dict.__setitem__(self._config, key, _TYPECONVERT[type](value))

        elif event == 'FIFO_SET':
            self.uzbl_fifo = args

        elif event == 'SOCKET_SET':
            self.uzbl_socket = args

        self.dispatch_event(event, args)


    def dispatch_event(self, event, args):
        '''Now send the event to any event handlers added with the connect
        function. In other words: handle plugin's event hooks.'''

        if event in self.handlers.keys():
            for hid in self.handlers[event]:
                try:
                    handler = self.handlers[event][hid]
                    print "Executing handler:", event, handler
                    self.exc_handler(handler, args)

                except:
                    print_exc()


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
