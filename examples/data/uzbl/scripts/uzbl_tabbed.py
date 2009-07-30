#!/usr/bin/env python

# Uzbl tabbing wrapper using a fifo socket interface
# Copyright (c) 2009, Tom Adams <tom@holizz.com>
# Copyright (c) 2009, Chris van Dijk <cn.vandijk@hotmail.com>
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


# Author(s):
#   Tom Adams <tom@holizz.com>
#       Wrote the original uzbl_tabbed.py as a proof of concept.
#
#   Chris van Dijk (quigybo) <cn.vandijk@hotmail.com>
#       Made signifigant headway on the old uzbl_tabbing.py script on the
#       uzbl wiki <http://www.uzbl.org/wiki/uzbl_tabbed>
#
#   Mason Larobina <mason.larobina@gmail.com>
#       Rewrite of the uzbl_tabbing.py script to use a fifo socket interface
#       and inherit configuration options from the user's uzbl config.
#
# Contributor(s):
#   mxey <mxey@ghosthacking.net>
#       uzbl_config path now honors XDG_CONFIG_HOME if it exists.
#
#   Romain Bignon <romain@peerfuse.org>
#       Fix for session restoration code.


# Dependencies:
#   pygtk - python bindings for gtk.
#   pango - python bindings needed for text rendering & layout in gtk widgets.
#   pygobject - GLib's GObject bindings for python.
#
# Optional dependencies:
#   simplejson - save uzbl_tabbed.py sessions & presets in json.
#
# Note: I haven't included version numbers with this dependency list because
# I've only ever tested uzbl_tabbed.py on the latest stable versions of these
# packages in Gentoo's portage. Package names may vary on different systems.


# Configuration:
# Because this version of uzbl_tabbed is able to inherit options from your main
# uzbl configuration file you may wish to configure uzbl tabbed from there.
# Here is a list of configuration options that can be customised and some
# example values for each:
#
# General tabbing options:
#   show_tablist            = 1
#   show_gtk_tabs           = 0
#   tablist_top             = 1
#   gtk_tab_pos             = (top|left|bottom|right)
#   switch_to_new_tabs      = 1
#   capture_new_windows     = 1
#
# Tab title options:
#   tab_titles              = 1
#   new_tab_title           = Loading
#   max_title_len           = 50
#   show_ellipsis           = 1
#
# Session options:
#   save_session            = 1
#   json_session            = 0
#   session_file            = $HOME/.local/share/uzbl/session
#
# Inherited uzbl options:
#   fifo_dir                = /tmp
#   socket_dir              = /tmp
#   icon_path               = $HOME/.local/share/uzbl/uzbl.png
#   status_background       = #303030
#
# Window options:
#   window_size             = 800,800
#
# And the key bindings:
#   bind_new_tab            = gn
#   bind_tab_from_clip      = gY
#   bind_tab_from_uri       = go _
#   bind_close_tab          = gC
#   bind_next_tab           = gt
#   bind_prev_tab           = gT
#   bind_goto_tab           = gi_
#   bind_goto_first         = g<
#   bind_goto_last          = g>
#   bind_clean_slate        = gQ
#
# Session preset key bindings:
#   bind_save_preset       = gsave _
#   bind_load_preset       = gload _
#   bind_del_preset        = gdel _
#   bind_list_presets      = glist
#
# And uzbl_tabbed.py takes care of the actual binding of the commands via each
# instances fifo socket.
#
# Custom tab styling:
#   tab_colours             = foreground = "#888" background = "#303030"
#   tab_text_colours        = foreground = "#bbb"
#   selected_tab            = foreground = "#fff"
#   selected_tab_text       = foreground = "green"
#   tab_indicate_https      = 1
#   https_colours           = foreground = "#888"
#   https_text_colours      = foreground = "#9c8e2d"
#   selected_https          = foreground = "#fff"
#   selected_https_text     = foreground = "gold"
#
# How these styling values are used are soley defined by the syling policy
# handler below (the function in the config section). So you can for example
# turn the tab text colour Firetruck-Red in the event "error" appears in the
# tab title or some other arbitrary event. You may wish to make a trusted
# hosts file and turn tab titles of tabs visiting trusted hosts purple.


# Issues:
#   - new windows are not caught and opened in a new tab.
#   - when uzbl_tabbed.py crashes it takes all the children with it.
#   - when a new tab is opened when using gtk tabs the tab button itself
#     grabs focus from its child for a few seconds.
#   - when switch_to_new_tabs is not selected the notebook page is
#     maintained but the new window grabs focus (try as I might to stop it).


# Todo:
#   - add command line options to use a different session file, not use a
#     session file and or open a uri on starup.
#   - ellipsize individual tab titles when the tab-list becomes over-crowded
#   - add "<" & ">" arrows to tablist to indicate that only a subset of the
#     currently open tabs are being displayed on the tablist.
#   - add the small tab-list display when both gtk tabs and text vim-like
#     tablist are hidden (I.e. [ 1 2 3 4 5 ])
#   - check spelling.
#   - pass a uzbl socketid to uzbl_tabbed.py and have it assimilated into
#     the collective. Resistance is futile!


import pygtk
import gtk
import subprocess
import os
import re
import time
import getopt
import pango
import select
import sys
import gobject
import socket
import random
import hashlib
import atexit

from gobject import io_add_watch, source_remove, timeout_add, IO_IN, IO_HUP
from signal import signal, SIGTERM, SIGINT
from optparse import OptionParser, OptionGroup

pygtk.require('2.0')

_scriptname = os.path.basename(sys.argv[0])
def error(msg):
    sys.stderr.write("%s: %s\n" % (_scriptname, msg))

def echo(msg):
    print "%s: %s" % (_scriptname, msg)


# ============================================================================
# ::: Default configuration section ::::::::::::::::::::::::::::::::::::::::::
# ============================================================================


# Location of your uzbl data directory.
if 'XDG_DATA_HOME' in os.environ.keys() and os.environ['XDG_DATA_HOME']:
    data_dir = os.path.join(os.environ['XDG_DATA_HOME'], 'uzbl/')
else:
    data_dir = os.path.join(os.environ['HOME'], '.local/share/uzbl/')
if not os.path.exists(data_dir):
    error("Warning: uzbl data_dir does not exist: %r" % data_dir)

# Location of your uzbl configuration file.
if 'XDG_CONFIG_HOME' in os.environ.keys() and os.environ['XDG_CONFIG_HOME']:
    uzbl_config = os.path.join(os.environ['XDG_CONFIG_HOME'], 'uzbl/config')
else:
    uzbl_config = os.path.join(os.environ['HOME'],'.config/uzbl/config')
if not os.path.exists(uzbl_config):
    error("Warning: Cannot locate your uzbl_config file %r" % uzbl_config)

# All of these settings can be inherited from your uzbl config file.
config = {
  # Tab options
  'show_tablist':           True,   # Show text uzbl like statusbar tab-list
  'show_gtk_tabs':          False,  # Show gtk notebook tabs
  'tablist_top':            True,   # Display tab-list at top of window
  'gtk_tab_pos':            'top',  # Gtk tab position (top|left|bottom|right)
  'switch_to_new_tabs':     True,   # Upon opening a new tab switch to it
  'capture_new_windows':    True,   # Use uzbl_tabbed to catch new windows

  # Tab title options
  'tab_titles':             True,   # Display tab titles (else only tab-nums)
  'new_tab_title':          'Loading', # New tab title
  'max_title_len':          50,     # Truncate title at n characters
  'show_ellipsis':          True,   # Show ellipsis when truncating titles

  # Session options
  'save_session':           True,   # Save session in file when quit
  'json_session':           False,   # Use json to save session.
  'saved_sessions_dir':     os.path.join(data_dir, 'sessions/'),
  'session_file':           os.path.join(data_dir, 'session'),

  # Inherited uzbl options
  'fifo_dir':               '/tmp', # Path to look for uzbl fifo.
  'socket_dir':             '/tmp', # Path to look for uzbl socket.
  'icon_path':              os.path.join(data_dir, 'uzbl.png'),
  'status_background':      "#303030", # Default background for all panels.

  # Window options
  'window_size':            "800,800", # width,height in pixels.

  # Key bindings
  'bind_new_tab':           'gn',   # Open new tab.
  'bind_tab_from_clip':     'gY',   # Open tab from clipboard.
  'bind_tab_from_uri':      'go _', # Open new tab and goto entered uri.
  'bind_close_tab':         'gC',   # Close tab.
  'bind_next_tab':          'gt',   # Next tab.
  'bind_prev_tab':          'gT',   # Prev tab.
  'bind_goto_tab':          'gi_',  # Goto tab by tab-number (in title).
  'bind_goto_first':        'g<',   # Goto first tab.
  'bind_goto_last':         'g>',   # Goto last tab.
  'bind_clean_slate':       'gQ',   # Close all tabs and open new tab.

  # Session preset key bindings
  'bind_save_preset':       'gsave _', # Save session to file %s.
  'bind_load_preset':       'gload _', # Load preset session from file %s.
  'bind_del_preset':        'gdel _',  # Delete preset session %s.
  'bind_list_presets':      'glist',   # List all session presets.

  # Add custom tab style definitions to be used by the tab colour policy
  # handler here. Because these are added to the config dictionary like
  # any other uzbl_tabbed configuration option remember that they can
  # be superseeded from your main uzbl config file.
  'tab_colours':            'foreground = "#888" background = "#303030"',
  'tab_text_colours':       'foreground = "#bbb"',
  'selected_tab':           'foreground = "#fff"',
  'selected_tab_text':      'foreground = "green"',
  'tab_indicate_https':     True,
  'https_colours':          'foreground = "#888"',
  'https_text_colours':     'foreground = "#9c8e2d"',
  'selected_https':         'foreground = "#fff"',
  'selected_https_text':    'foreground = "gold"',

} # End of config dict.

# This is the tab style policy handler. Every time the tablist is updated
# this function is called to determine how to colourise that specific tab
# according the simple/complex rules as defined here. You may even wish to
# move this function into another python script and import it using:
#   from mycustomtabbingconfig import colour_selector
# Remember to rename, delete or comment out this function if you do that.

def colour_selector(tabindex, currentpage, uzbl):
    '''Tablist styling policy handler. This function must return a tuple of
    the form (tab style, text style).'''

    # Just as an example:
    # if 'error' in uzbl.title:
    #     if tabindex == currentpage:
    #         return ('foreground="#fff"', 'foreground="red"')
    #     return ('foreground="#888"', 'foreground="red"')

    # Style tabs to indicate connected via https.
    if config['tab_indicate_https'] and uzbl.uri.startswith("https://"):
        if tabindex == currentpage:
            return (config['selected_https'], config['selected_https_text'])
        return (config['https_colours'], config['https_text_colours'])

    # Style to indicate selected.
    if tabindex == currentpage:
        return (config['selected_tab'], config['selected_tab_text'])

    # Default tab style.
    return (config['tab_colours'], config['tab_text_colours'])


# ============================================================================
# ::: End of configuration section :::::::::::::::::::::::::::::::::::::::::::
# ============================================================================


def readconfig(uzbl_config, config):
    '''Loads relevant config from the users uzbl config file into the global
    config dictionary.'''

    if not os.path.exists(uzbl_config):
        error("Unable to load config %r" % uzbl_config)
        return None

    # Define parsing regular expressions
    isint = re.compile("^(\-|)[0-9]+$").match
    findsets = re.compile("^set\s+([^\=]+)\s*\=\s*(.+)$",\
      re.MULTILINE).findall

    h = open(os.path.expandvars(uzbl_config), 'r')
    rawconfig = h.read()
    h.close()

    configkeys, strip = config.keys(), str.strip
    for (key, value) in findsets(rawconfig):
        key, value = strip(key), strip(value)
        if key not in configkeys: continue
        if isint(value): value = int(value)
        config[key] = value

    # Ensure that config keys that relate to paths are expanded.
    expand = ['fifo_dir', 'socket_dir', 'session_file', 'icon_path']
    for key in expand:
        config[key] = os.path.expandvars(config[key])


def counter():
    '''To infinity and beyond!'''

    i = 0
    while True:
        i += 1
        yield i


def escape(s):
    '''Replaces html markup in tab titles that screw around with pango.'''

    for (split, glue) in [('&','&amp;'), ('<', '&lt;'), ('>', '&gt;')]:
        s = s.replace(split, glue)
    return s


def gen_endmarker():
    '''Generates a random md5 for socket message-termination endmarkers.'''

    return hashlib.md5(str(random.random()*time.time())).hexdigest()


class UzblTabbed:
    '''A tabbed version of uzbl using gtk.Notebook'''

    class UzblInstance:
        '''Uzbl instance meta-data/meta-action object.'''

        def __init__(self, parent, tab, fifo_socket, socket_file, pid,\
          uri, title, switch):

            self.parent = parent
            self.tab = tab
            self.fifo_socket = fifo_socket
            self.socket_file = socket_file
            self.pid = pid
            self.title = title
            self.uri = uri
            self.timers = {}
            self._lastprobe = 0
            self._fifoout = []
            self._socketout = []
            self._socket = None
            self._buffer = ""
            # Switch to tab after loading
            self._switch = switch
            # fifo/socket files exists and socket connected.
            self._connected = False
            # The kill switch
            self._kill = False

            # Message termination endmarker.
            self._marker = gen_endmarker()

            # Gen probe commands string
            probes = []
            probe = probes.append
            probe('print uri %d @uri %s' % (self.pid, self._marker))
            probe('print title %d @<document.title>@ %s' % (self.pid,\
              self._marker))
            self._probecmds = '\n'.join(probes)

            # Enqueue keybinding config for child uzbl instance
            self.parent.config_uzbl(self)


        def flush(self, timer_call=False):
            '''Flush messages from the socket-out and fifo-out queues.'''

            if self._kill:
                if self._socket:
                    self._socket.close()
                    self._socket = None

                error("Flush called on dead tab.")
                return False

            if len(self._fifoout):
                if os.path.exists(self.fifo_socket):
                    h = open(self.fifo_socket, 'w')
                    while len(self._fifoout):
                        msg = self._fifoout.pop(0)
                        h.write("%s\n"%msg)
                    h.close()

            if len(self._socketout):
                if not self._socket and os.path.exists(self.socket_file):
                    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                    sock.connect(self.socket_file)
                    self._socket = sock

                if self._socket:
                    while len(self._socketout):
                        msg = self._socketout.pop(0)
                        self._socket.send("%s\n"%msg)

            if not self._connected and timer_call:
                if not len(self._fifoout + self._socketout):
                    self._connected = True

                    if timer_call in self.timers.keys():
                        source_remove(self.timers[timer_call])
                        del self.timers[timer_call]

                    if self._switch:
                        self.grabfocus()

            return len(self._fifoout + self._socketout)


        def grabfocus(self):
            '''Steal parent focus and switch the notebook to my own tab.'''

            tabs = list(self.parent.notebook)
            tabid = tabs.index(self.tab)
            self.parent.goto_tab(tabid)


        def probe(self):
            '''Probes the client for information about its self.'''

            if self._connected:
                self.send(self._probecmds)
                self._lastprobe = time.time()


        def write(self, msg):
            '''Child fifo write function.'''

            self._fifoout.append(msg)
            # Flush messages from the queue if able.
            return self.flush()


        def send(self, msg):
            '''Child socket send function.'''

            self._socketout.append(msg)
            # Flush messages from queue if able.
            return self.flush()


    def __init__(self):
        '''Create tablist, window and notebook.'''

        # Store information about the applications fifo_socket.
        self._fifo = None

        self._timers = {}
        self._buffer = ""
        self._killed = False

        # A list of the recently closed tabs
        self._closed = []

        # Holds metadata on the uzbl childen open.
        self.tabs = {}

        # Generates a unique id for uzbl socket filenames.
        self.next_pid = counter().next

        # Create main window
        self.window = gtk.Window()
        try:
            window_size = map(int, config['window_size'].split(','))
            self.window.set_default_size(*window_size)

        except:
            error("Invalid value for default_size in config file.")

        self.window.set_title("Uzbl Browser")
        self.window.set_border_width(0)

        # Set main window icon
        icon_path = config['icon_path']
        if os.path.exists(icon_path):
            self.window.set_icon(gtk.gdk.pixbuf_new_from_file(icon_path))

        else:
            icon_path = '/usr/share/uzbl/examples/data/uzbl/uzbl.png'
            if os.path.exists(icon_path):
                self.window.set_icon(gtk.gdk.pixbuf_new_from_file(icon_path))

        # Attach main window event handlers
        self.window.connect("delete-event", self.quitrequest)

        # Create tab list
        if config['show_tablist']:
            vbox = gtk.VBox()
            self.window.add(vbox)
            ebox = gtk.EventBox()
            self.tablist = gtk.Label()
            self.tablist.set_use_markup(True)
            self.tablist.set_justify(gtk.JUSTIFY_LEFT)
            self.tablist.set_line_wrap(False)
            self.tablist.set_selectable(False)
            self.tablist.set_padding(2,2)
            self.tablist.set_alignment(0,0)
            self.tablist.set_ellipsize(pango.ELLIPSIZE_END)
            self.tablist.set_text(" ")
            self.tablist.show()
            ebox.add(self.tablist)
            ebox.show()
            bgcolor = gtk.gdk.color_parse(config['status_background'])
            ebox.modify_bg(gtk.STATE_NORMAL, bgcolor)

        # Create notebook
        self.notebook = gtk.Notebook()
        self.notebook.set_show_tabs(config['show_gtk_tabs'])

        # Set tab position
        allposes = {'left': gtk.POS_LEFT, 'right':gtk.POS_RIGHT,
          'top':gtk.POS_TOP, 'bottom':gtk.POS_BOTTOM}
        if config['gtk_tab_pos'] in allposes.keys():
            self.notebook.set_tab_pos(allposes[config['gtk_tab_pos']])

        self.notebook.set_show_border(False)
        self.notebook.set_scrollable(True)
        self.notebook.set_border_width(0)

        self.notebook.connect("page-removed", self.tab_closed)
        self.notebook.connect("switch-page", self.tab_changed)
        self.notebook.connect("page-added", self.tab_opened)

        self.notebook.show()
        if config['show_tablist']:
            if config['tablist_top']:
                vbox.pack_start(ebox, False, False, 0)
                vbox.pack_end(self.notebook, True, True, 0)

            else:
                vbox.pack_start(self.notebook, True, True, 0)
                vbox.pack_end(ebox, False, False, 0)

            vbox.show()

        else:
            self.window.add(self.notebook)

        self.window.show()
        self.wid = self.notebook.window.xid

        # Generate the fifo socket filename.
        fifo_filename = 'uzbltabbed_%d' % os.getpid()
        self.fifo_socket = os.path.join(config['fifo_dir'], fifo_filename)

        # Now initialise the fifo socket at self.fifo_socket
        self.init_fifo_socket()

        # If we are using sessions then load the last one if it exists.
        if config['save_session']:
            self.load_session()


    def run(self):
        '''UzblTabbed main function that calls the gtk loop.'''

        if not len(self.tabs):
            self.new_tab()

        # Update tablist timer
        #timer = "update-tablist"
        #timerid = timeout_add(500, self.update_tablist,timer)
        #self._timers[timer] = timerid

        # Probe clients every second for window titles and location
        timer = "probe-clients"
        timerid = timeout_add(1000, self.probe_clients, timer)
        self._timers[timer] = timerid

        # Make SIGTERM act orderly.
        signal(SIGTERM, lambda signum, stack_frame: self.terminate(SIGTERM))

        # Catch keyboard interrupts
        signal(SIGINT, lambda signum, stack_frame: self.terminate(SIGINT))

        try:
            gtk.main()

        except:
            error("encounted error %r" % sys.exc_info()[1])

            # Unlink fifo socket
            self.unlink_fifo_socket()

            # Attempt to close all uzbl instances nicely.
            self.quitrequest()

            # Allow time for all the uzbl instances to quit.
            time.sleep(1)

            raise


    def terminate(self, termsig=None):
        '''Handle termination signals and exit safely and cleanly.'''

        # Not required but at least it lets the user know what killed his
        # browsing session.
        if termsig == SIGTERM:
            error("caught SIGTERM signal")

        elif termsig == SIGINT:
            error("caught keyboard interrupt")

        else:
            error("caught unknown signal")

        error("commencing infanticide!")

        # Sends the exit signal to all uzbl instances.
        self.quitrequest()


    def init_fifo_socket(self):
        '''Create interprocess communication fifo socket.'''

        if os.path.exists(self.fifo_socket):
            if not os.access(self.fifo_socket, os.F_OK | os.R_OK | os.W_OK):
                os.mkfifo(self.fifo_socket)

        else:
            basedir = os.path.dirname(self.fifo_socket)
            if not os.path.exists(basedir):
                os.makedirs(basedir)

            os.mkfifo(self.fifo_socket)

        # Add event handlers for IO_IN & IO_HUP events.
        self.setup_fifo_watchers()

        echo("listening at %r" % self.fifo_socket)

        # Add atexit register to destroy the socket on program termination.
        atexit.register(self.unlink_fifo_socket)


    def unlink_fifo_socket(self):
        '''Unlink the fifo socket. Note: This function is called automatically
        on exit by an atexit register.'''

        # Make sure the fifo_socket fd is closed.
        self.close_fifo()

        # And unlink if the real fifo_socket exists.
        if os.path.exists(self.fifo_socket):
            os.unlink(self.fifo_socket)
            echo("unlinked %r" % self.fifo_socket)


    def close_fifo(self):
        '''Remove all event handlers watching the fifo and close the fd.'''

        # Already closed
        if self._fifo is None: return

        (fd, watchers) = self._fifo
        os.close(fd)

        # Stop all gobject io watchers watching the fifo.
        for gid in watchers:
            source_remove(gid)

        self._fifo = None


    def setup_fifo_watchers(self):
        '''Open fifo socket fd and setup gobject IO_IN & IO_HUP event
        handlers.'''

        # Close currently open fifo_socket fd and kill all watchers
        self.close_fifo()

        fd = os.open(self.fifo_socket, os.O_RDONLY | os.O_NONBLOCK)

        # Add gobject io event handlers to the fifo socket.
        watchers = [io_add_watch(fd, IO_IN, self.main_fifo_read),\
          io_add_watch(fd, IO_HUP, self.main_fifo_hangup)]

        self._fifo = (fd, watchers)


    def main_fifo_hangup(self, fd, cb_condition):
        '''Handle main fifo socket hangups.'''

        # Close old fd, open new fifo socket and add io event handlers.
        self.setup_fifo_watchers()

        # Kill the gobject event handler calling this handler function.
        return False


    def main_fifo_read(self, fd, cb_condition):
        '''Read from main fifo socket.'''

        self._buffer = os.read(fd, 1024)
        temp = self._buffer.split("\n")
        self._buffer = temp.pop()
        cmds = [s.strip().split() for s in temp if len(s.strip())]

        for cmd in cmds:
            try:
                #print cmd
                self.parse_command(cmd)

            except:
                error("parse_command: invalid command %s" % ' '.join(cmd))
                raise

        return True


    def probe_clients(self, timer_call):
        '''Probe all uzbl clients for up-to-date window titles and uri's.'''

        save_session = config['save_session']

        sockd = {}
        tabskeys = self.tabs.keys()
        notebooklist = list(self.notebook)

        for tab in notebooklist:
            if tab not in tabskeys: continue
            uzbl = self.tabs[tab]
            uzbl.probe()
            if uzbl._socket:
                sockd[uzbl._socket] = uzbl

        sockets = sockd.keys()
        (reading, _, errors) = select.select(sockets, [], sockets, 0)

        for sock in reading:
            uzbl = sockd[sock]
            uzbl._buffer = sock.recv(1024).replace('\n',' ')
            temp = uzbl._buffer.split(uzbl._marker)
            self._buffer = temp.pop()
            cmds = [s.strip().split() for s in temp if len(s.strip())]
            for cmd in cmds:
                try:
                    #print cmd
                    self.parse_command(cmd)

                except:
                    error("parse_command: invalid command %s" % ' '.join(cmd))
                    raise

        return True


    def parse_command(self, cmd):
        '''Parse instructions from uzbl child processes.'''

        # Commands ( [] = optional, {} = required )
        # new [uri]
        #   open new tab and head to optional uri.
        # close [tab-num]
        #   close current tab or close via tab id.
        # next [n-tabs]
        #   open next tab or n tabs down. Supports negative indexing.
        # prev [n-tabs]
        #   open prev tab or n tabs down. Supports negative indexing.
        # goto {tab-n}
        #   goto tab n.
        # first
        #   goto first tab.
        # last
        #   goto last tab.
        # title {pid} {document-title}
        #   updates tablist title.
        # uri {pid} {document-location}

        if cmd[0] == "new":
            if len(cmd) == 2:
                self.new_tab(cmd[1])

            else:
                self.new_tab()

        elif cmd[0] == "newfromclip":
            uri = subprocess.Popen(['xclip','-selection','clipboard','-o'],\
              stdout=subprocess.PIPE).communicate()[0]
            if uri:
                self.new_tab(uri)

        elif cmd[0] == "close":
            if len(cmd) == 2:
                self.close_tab(int(cmd[1]))

            else:
                self.close_tab()

        elif cmd[0] == "next":
            if len(cmd) == 2:
                self.next_tab(int(cmd[1]))

            else:
                self.next_tab()

        elif cmd[0] == "prev":
            if len(cmd) == 2:
                self.prev_tab(int(cmd[1]))

            else:
                self.prev_tab()

        elif cmd[0] == "goto":
            self.goto_tab(int(cmd[1]))

        elif cmd[0] == "first":
            self.goto_tab(0)

        elif cmd[0] == "last":
            self.goto_tab(-1)

        elif cmd[0] in ["title", "uri"]:
            if len(cmd) > 2:
                uzbl = self.get_tab_by_pid(int(cmd[1]))
                if uzbl:
                    old = getattr(uzbl, cmd[0])
                    new = ' '.join(cmd[2:])
                    setattr(uzbl, cmd[0], new)
                    if old != new:
                        self.update_tablist()

                else:
                    error("parse_command: no uzbl with pid %r" % int(cmd[1]))

        elif cmd[0] == "preset":
            if len(cmd) < 3:
                error("parse_command: invalid preset command")

            elif cmd[1] == "save":
                path = os.path.join(config['saved_sessions_dir'], cmd[2])
                self.save_session(path)

            elif cmd[1] == "load":
                path = os.path.join(config['saved_sessions_dir'], cmd[2])
                self.load_session(path)

            elif cmd[1] == "del":
                path = os.path.join(config['saved_sessions_dir'], cmd[2])
                if os.path.isfile(path):
                    os.remove(path)

                else:
                    error("parse_command: preset %r does not exist." % path)

            elif cmd[1] == "list":
                uzbl = self.get_tab_by_pid(int(cmd[2]))
                if uzbl:
                    if not os.path.isdir(config['saved_sessions_dir']):
                        js = "js alert('No saved presets.');"
                        uzbl.send(js)

                    else:
                        listdir = os.listdir(config['saved_sessions_dir'])
                        listdir = "\\n".join(listdir)
                        js = "js alert('Session presets:\\n\\n%s');" % listdir
                        uzbl.send(js)

                else:
                    error("parse_command: unknown tab pid.")

            else:
                error("parse_command: unknown parse command %r"\
                  % ' '.join(cmd))

        elif cmd[0] == "clean":
            self.clean_slate()

        else:
            error("parse_command: unknown command %r" % ' '.join(cmd))


    def get_tab_by_pid(self, pid):
        '''Return uzbl instance by pid.'''

        for (tab, uzbl) in self.tabs.items():
            if uzbl.pid == pid:
                return uzbl

        return False


    def new_tab(self, uri='', title='', switch=None):
        '''Add a new tab to the notebook and start a new instance of uzbl.
        Use the switch option to negate config['switch_to_new_tabs'] option
        when you need to load multiple tabs at a time (I.e. like when
        restoring a session from a file).'''

        pid = self.next_pid()
        tab = gtk.Socket()
        tab.show()
        self.notebook.append_page(tab)
        sid = tab.get_id()
        uri = uri.strip()

        fifo_filename = 'uzbl_fifo_%s_%0.2d' % (self.wid, pid)
        fifo_socket = os.path.join(config['fifo_dir'], fifo_filename)
        socket_filename = 'uzbl_socket_%s_%0.2d' % (self.wid, pid)
        socket_file = os.path.join(config['socket_dir'], socket_filename)

        if switch is None:
            switch = config['switch_to_new_tabs']

        if not title:
            title = config['new_tab_title']

        uzbl = self.UzblInstance(self, tab, fifo_socket, socket_file, pid,\
          uri, title, switch)

        if len(uri):
            uri = "--uri %r" % uri

        self.tabs[tab] = uzbl
        cmd = 'uzbl -s %s -n %s_%0.2d %s &' % (sid, self.wid, pid, uri)
        subprocess.Popen([cmd], shell=True) # TODO: do i need close_fds=True ?

        # Add gobject timer to make sure the config is pushed when fifo socket
        # has been created.
        timerid = timeout_add(100, uzbl.flush, "flush-initial-config")
        uzbl.timers['flush-initial-config'] = timerid

        self.update_tablist()


    def clean_slate(self):
        '''Close all open tabs and open a fresh brand new one.'''

        self.new_tab()
        tabs = self.tabs.keys()
        for tab in list(self.notebook)[:-1]:
            if tab not in tabs: continue
            uzbl = self.tabs[tab]
            uzbl.send("exit")


    def config_uzbl(self, uzbl):
        '''Send bind commands for tab new/close/next/prev to a uzbl
        instance.'''

        binds = []
        bind_format = r'bind %s = sh "echo \"%s\" > \"%s\""'
        bind = lambda key, action: binds.append(bind_format % (key, action,\
          self.fifo_socket))

        sets = []
        set_format = r'set %s = sh \"echo \\"%s\\" > \\"%s\\""'
        set = lambda key, action: binds.append(set_format % (key, action,\
          self.fifo_socket))

        # Bind definitions here
        # bind(key, command back to fifo)
        bind(config['bind_new_tab'], 'new')
        bind(config['bind_tab_from_clip'], 'newfromclip')
        bind(config['bind_tab_from_uri'], 'new %s')
        bind(config['bind_close_tab'], 'close')
        bind(config['bind_next_tab'], 'next')
        bind(config['bind_prev_tab'], 'prev')
        bind(config['bind_goto_tab'], 'goto %s')
        bind(config['bind_goto_first'], 'goto 0')
        bind(config['bind_goto_last'], 'goto -1')
        bind(config['bind_clean_slate'], 'clean')
        bind(config['bind_save_preset'], 'preset save %s')
        bind(config['bind_load_preset'], 'preset load %s')
        bind(config['bind_del_preset'], 'preset del %s')
        bind(config['bind_list_presets'], 'preset list %d' % uzbl.pid)

        # Set definitions here
        # set(key, command back to fifo)
        if config['capture_new_windows']:
            set("new_window", r'new $8')

        # Send config to uzbl instance via its socket file.
        uzbl.send("\n".join(binds+sets))


    def goto_tab(self, index):
        '''Goto tab n (supports negative indexing).'''

        tabs = list(self.notebook)
        if 0 <= index < len(tabs):
            self.notebook.set_current_page(index)
            self.update_tablist()
            return None

        try:
            tab = tabs[index]
            # Update index because index might have previously been a
            # negative index.
            index = tabs.index(tab)
            self.notebook.set_current_page(index)
            self.update_tablist()

        except IndexError:
            pass


    def next_tab(self, step=1):
        '''Switch to next tab or n tabs right.'''

        if step < 1:
            error("next_tab: invalid step %r" % step)
            return None

        ntabs = self.notebook.get_n_pages()
        tabn = (self.notebook.get_current_page() + step) % ntabs
        self.notebook.set_current_page(tabn)
        self.update_tablist()


    def prev_tab(self, step=1):
        '''Switch to prev tab or n tabs left.'''

        if step < 1:
            error("prev_tab: invalid step %r" % step)
            return None

        ntabs = self.notebook.get_n_pages()
        tabn = self.notebook.get_current_page() - step
        while tabn < 0: tabn += ntabs
        self.notebook.set_current_page(tabn)
        self.update_tablist()


    def close_tab(self, tabn=None):
        '''Closes current tab. Supports negative indexing.'''

        if tabn is None:
            tabn = self.notebook.get_current_page()

        else:
            try:
                tab = list(self.notebook)[tabn]

            except IndexError:
                error("close_tab: invalid index %r" % tabn)
                return None

        self.notebook.remove_page(tabn)


    def tab_opened(self, notebook, tab, index):
        '''Called upon tab creation. Called by page-added signal.'''

        if config['switch_to_new_tabs']:
            self.notebook.set_focus_child(tab)

        else:
            oldindex = self.notebook.get_current_page()
            oldtab = self.notebook.get_nth_page(oldindex)
            self.notebook.set_focus_child(oldtab)


    def tab_closed(self, notebook, tab, index):
        '''Close the window if no tabs are left. Called by page-removed
        signal.'''

        if tab in self.tabs.keys():
            uzbl = self.tabs[tab]
            for (timer, gid) in uzbl.timers.items():
                error("tab_closed: removing timer %r" % timer)
                source_remove(gid)
                del uzbl.timers[timer]

            if uzbl._socket:
                uzbl._socket.close()
                uzbl._socket = None

            uzbl._fifoout = []
            uzbl._socketout = []
            uzbl._kill = True
            self._closed.append((uzbl.uri, uzbl.title))
            self._closed = self._closed[-10:]
            del self.tabs[tab]

        if self.notebook.get_n_pages() == 0:
            if not self._killed and config['save_session']:
                if len(self._closed):
                    d = {'curtab': 0, 'tabs': [self._closed[-1],]}
                    self.save_session(session=d)

            self.quit()

        self.update_tablist()

        return True


    def tab_changed(self, notebook, page, index):
        '''Refresh tab list. Called by switch-page signal.'''

        tab = self.notebook.get_nth_page(index)
        self.notebook.set_focus_child(tab)
        self.update_tablist(index)
        return True


    def update_tablist(self, curpage=None):
        '''Upate tablist status bar.'''

        show_tablist = config['show_tablist']
        show_gtk_tabs = config['show_gtk_tabs']
        tab_titles = config['tab_titles']
        show_ellipsis = config['show_ellipsis']
        if not show_tablist and not show_gtk_tabs:
            return True

        tabs = self.tabs.keys()
        if curpage is None:
            curpage = self.notebook.get_current_page()

        title_format = "%s - Uzbl Browser"
        max_title_len = config['max_title_len']

        if show_tablist:
            pango = ""
            normal = (config['tab_colours'], config['tab_text_colours'])
            selected = (config['selected_tab'], config['selected_tab_text'])
            if tab_titles:
                tab_format = "<span %s> [ %d <span %s> %s</span> ] </span>"
            else:
                tab_format = "<span %s> [ <span %s>%d</span> ] </span>"

        if show_gtk_tabs:
            gtk_tab_format = "%d %s"

        for index, tab in enumerate(self.notebook):
            if tab not in tabs: continue
            uzbl = self.tabs[tab]

            if index == curpage:
                self.window.set_title(title_format % uzbl.title)

            tabtitle = uzbl.title[:max_title_len]
            if show_ellipsis and len(tabtitle) != len(uzbl.title):
                tabtitle = "%s\xe2\x80\xa6" % tabtitle[:-1] # Show Ellipsis

            if show_gtk_tabs:
                if tab_titles:
                    self.notebook.set_tab_label_text(tab,\
                      gtk_tab_format % (index, tabtitle))
                else:
                    self.notebook.set_tab_label_text(tab, str(index))

            if show_tablist:
                style = colour_selector(index, curpage, uzbl)
                (tabc, textc) = style

                if tab_titles:
                    pango += tab_format % (tabc, index, textc,\
                      escape(tabtitle))
                else:
                    pango += tab_format % (tabc, textc, index)

        if show_tablist:
            self.tablist.set_markup(pango)

        return True


    def save_session(self, session_file=None, session=None):
        '''Save the current session to file for restoration on next load.'''

        strip = str.strip

        if session_file is None:
            session_file = config['session_file']

        if session is None:
            tabs = self.tabs.keys()
            state = []
            for tab in list(self.notebook):
                if tab not in tabs: continue
                uzbl = self.tabs[tab]
                if not uzbl.uri: continue
                state += [(uzbl.uri, uzbl.title),]

            session = {'curtab': self.notebook.get_current_page(),
              'tabs': state}

        if config['json_session']:
            raw = json.dumps(session)

        else:
            lines = ["curtab = %d" % session['curtab'],]
            for (uri, title) in session['tabs']:
                lines += ["%s\t%s" % (strip(uri), strip(title)),]

            raw = "\n".join(lines)

        if not os.path.isfile(session_file):
            dirname = os.path.dirname(session_file)
            if not os.path.isdir(dirname):
                os.makedirs(dirname)

        h = open(session_file, 'w')
        h.write(raw)
        h.close()


    def load_session(self, session_file=None):
        '''Load a saved session from file.'''

        default_path = False
        strip = str.strip
        json_session = config['json_session']

        if session_file is None:
            default_path = True
            session_file = config['session_file']

        if not os.path.isfile(session_file):
            return False

        h = open(session_file, 'r')
        raw = h.read()
        h.close()
        if json_session:
            if sum([1 for s in raw.split("\n") if strip(s)]) != 1:
                error("Warning: The session file %r does not look json. "\
                  "Trying to load it as a non-json session file."\
                  % session_file)
                json_session = False

        if json_session:
            try:
                session = json.loads(raw)
                curtab, tabs = session['curtab'], session['tabs']

            except:
                error("Failed to load jsonifed session from %r"\
                  % session_file)
                return None

        else:
            tabs = []
            strip = str.strip
            curtab, tabs = 0, []
            lines = [s for s in raw.split("\n") if strip(s)]
            if len(lines) < 2:
                error("Warning: The non-json session file %r looks invalid."\
                  % session_file)
                return None

            try:
                for line in lines:
                    if line.startswith("curtab"):
                        curtab = int(line.split()[-1])

                    else:
                        uri, title = line.split("\t",1)
                        tabs += [(strip(uri), strip(title)),]

            except:
                error("Warning: failed to load session file %r" % session_file)
                return None

            session = {'curtab': curtab, 'tabs': tabs}

        # Now populate notebook with the loaded session.
        for (index, (uri, title)) in enumerate(tabs):
            self.new_tab(uri=uri, title=title, switch=(curtab==index))

        # There may be other state information in the session dict of use to
        # other functions. Of course however the non-json session object is
        # just a dummy object of no use to no one.
        return session


    def quitrequest(self, *args):
        '''Called by delete-event signal to kill all uzbl instances.'''

        self._killed = True

        if config['save_session']:
            if len(list(self.notebook)):
                self.save_session()

            else:
                # Notebook has no pages so delete session file if it exists.
                if os.path.isfile(config['session_file']):
                    os.remove(config['session_file'])

        for (tab, uzbl) in self.tabs.items():
            uzbl.send("exit")

        # Add a gobject timer to make sure the application force-quits after a period.
        timer = "force-quit"
        timerid = timeout_add(5000, self.quit, timer)
        self._timers[timer] = timerid


    def quit(self, *args):
        '''Cleanup and quit. Called by delete-event signal.'''

        # Close the fifo socket, remove any gobject io event handlers and
        # delete socket.
        self.unlink_fifo_socket()

        # Remove all gobject timers that are still ticking.
        for (timerid, gid) in self._timers.items():
            source_remove(gid)
            del self._timers[timerid]

        try:
            gtk.main_quit()

        except:
            pass


if __name__ == "__main__":

    # Read from the uzbl config into the global config dictionary.
    readconfig(uzbl_config, config)

    # Build command line parser
    parser = OptionParser()
    parser.add_option('-n', '--no-session', dest='nosession',\
      action='store_true', help="ignore session saving a loading.")
    group = OptionGroup(parser, "Note", "All other command line arguments are "\
      "interpreted as uris and loaded in new tabs.")
    parser.add_option_group(group)

    # Parse command line options
    (options, uris) = parser.parse_args()

    if options.nosession:
        config['save_session'] = False

    if config['json_session']:
        try:
            import simplejson as json

        except:
            error("Warning: json_session set but cannot import the python "\
              "module simplejson. Fix: \"set json_session = 0\" or "\
              "install the simplejson python module to remove this warning.")
            config['json_session'] = False

    uzbl = UzblTabbed()

    # All extra arguments given to uzbl_tabbed.py are interpreted as
    # web-locations to opened in new tabs.
    lasturi = len(uris)-1
    for (index,uri) in enumerate(uris):
        uzbl.new_tab(uri, switch=(index==lasturi))

    uzbl.run()
