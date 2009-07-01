#!/usr/bin/python

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
#  Chris van Dijk (quigybo) <cn.vandijk@hotmail.com>
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


# Configuration:
# Because this version of uzbl_tabbed is able to inherit options from your main
# uzbl configuration file you may wish to configure uzbl tabbed from there.
# Here is a list of configuration options that can be customised and some
# example values for each:
#
#   set show_tablist        = 1
#   set show_gtk_tabs       = 0
#   set switch_to_new_tabs  = 1
#   set save_session        = 1
#   set gtk_tab_pos         = (left|bottom|top|right)
#   set max_title_len       = 50
#   set new_tab_title       = New tab
#   set status_background   = #303030
#   set session_file        = $HOME/.local/share/session
#   set tab_colours         = foreground = "#999"
#   set tab_text_colours    = foreground = "#444"
#   set selected_tab        = foreground = "#aaa" background="#303030"
#   set selected_tab_text   = foreground = "green" 
#   set window_size         = 800,800
#
# And the keybindings:
#
#   set bind_new_tab        = gn
#   set bind_tab_from_clip  = gY
#   set bind_close_tab      = gC
#   set bind_next_tab       = gt
#   set bind_prev_tab       = gT
#   set bind_goto_tab       = gi_
#   set bind_goto_first     = g<
#   set bind_goto_last      = g>
#
# And uzbl_tabbed.py takes care of the actual binding of the commands via each
# instances fifo socket. 


# Issues: 
#   - new windows are not caught and opened in a new tab.
#   - when uzbl_tabbed.py crashes it takes all the children with it.


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

pygtk.require('2.0')

def error(msg):
    sys.stderr.write("%s\n"%msg)


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
  'show_tablist':           True,
  'show_gtk_tabs':          False,
  'max_title_len':          50,
  'tablist_top':            True,
  'tab_titles':             True,
  'gtk_tab_pos':            'top', # (top|left|bottom|right)
  'new_tab_title':          'New tab',
  'switch_to_new_tabs':     True,
  
  # uzbl options
  'save_session':           True,
  'fifo_dir':               '/tmp',
  'socket_dir':             '/tmp',
  'icon_path':              os.path.join(data_dir, 'uzbl.png'),
  'session_file':           os.path.join(data_dir, 'session'),
  'status_background':      "#303030",
  'window_size':            "800,800", # in pixels
  'monospace_size':         10, 
  
  # Key bindings.
  'bind_new_tab':           'gn',
  'bind_tab_from_clip':     'gY', 
  'bind_close_tab':         'gC',
  'bind_next_tab':          'gt',
  'bind_prev_tab':          'gT',
  'bind_goto_tab':          'gi_',
  'bind_goto_first':        'g<',
  'bind_goto_last':         'g>',
  
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
    
    for (key, value) in findsets(rawconfig):
        key, value = key.strip(), value.strip()
        if key not in config.keys(): continue
        if isint(value): value = int(value)
        config[key] = value
    
    # Ensure that config keys that relate to paths are expanded.
    expand = ['fifo_dir', 'socket_dir', 'session_file', 'icon_path']
    for key in expand:
        config[key] = os.path.expandvars(config[key]) 


def rmkdir(path):
    '''Recursively make directories.
    I.e. `mkdir -p /some/nonexistant/path/`'''

    path, sep = os.path.realpath(path), os.path.sep
    dirs = path.split(sep)
    for i in range(2,len(dirs)+1):
        dir = os.path.join(sep,sep.join(dirs[:i]))
        if not os.path.exists(dir):
            os.mkdir(dir)


def counter():
    '''To infinity and beyond!'''

    i = 0
    while True:
        i += 1
        yield i


def gen_endmarker():
    '''Generates a random md5 for socket message-termination endmarkers.'''

    return hashlib.md5(str(random.random()*time.time())).hexdigest()


class UzblTabbed:
    '''A tabbed version of uzbl using gtk.Notebook'''

    class UzblInstance: 
        '''Uzbl instance meta-data/meta-action object.'''

        def __init__(self, parent, tab, fifo_socket, socket_file, pid,\
          uri, switch):

            self.parent = parent
            self.tab = tab 
            self.fifo_socket = fifo_socket
            self.socket_file = socket_file
            self.pid = pid
            self.title = config['new_tab_title']
            self.uri = uri
            self.timers = {}
            self._lastprobe = 0
            self._fifoout = []
            self._socketout = []
            self._socket = None
            self._buffer = ""
            # Switch to tab after connection
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
                        gobject.source_remove(self.timers[timer_call])
                        del self.timers[timer_call]

                    if self._switch:
                        tabs = list(self.parent.notebook)
                        tabid = tabs.index(self.tab)
                        self.parent.goto_tab(tabid)
                
            return len(self._fifoout + self._socketout)


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
        
        self._fifos = {}
        self._timers = {}
        self._buffer = ""
        
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
        self.window.connect("delete-event", self.quit)
        
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
        self.notebook.connect("page-removed", self.tab_closed)
        self.notebook.connect("switch-page", self.tab_changed)
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
        
        # Create the uzbl_tabbed fifo
        fifo_filename = 'uzbltabbed_%d' % os.getpid()
        self.fifo_socket = os.path.join(config['fifo_dir'], fifo_filename)
        self._create_fifo_socket(self.fifo_socket)
        self._setup_fifo_watcher(self.fifo_socket)


    def _create_fifo_socket(self, fifo_socket):
        '''Create interprocess communication fifo socket.''' 

        if os.path.exists(fifo_socket):
            if not os.access(fifo_socket, os.F_OK | os.R_OK | os.W_OK):
                os.mkfifo(fifo_socket)

        else:
            basedir = os.path.dirname(self.fifo_socket)
            if not os.path.exists(basedir):
                rmkdir(basedir)
            os.mkfifo(self.fifo_socket)
        
        print "Listening on %s" % self.fifo_socket


    def _setup_fifo_watcher(self, fifo_socket):
        '''Open fifo socket fd and setup gobject IO_IN & IO_HUP watchers.
        Also log the creation of a fd and store the the internal
        self._watchers dictionary along with the filename of the fd.'''

        if fifo_socket in self._fifos.keys():
            fd, watchers = self._fifos[fifo_socket]
            os.close(fd)
            for watcherid in watchers.keys():
                gobject.source_remove(watchers[watcherid])
                del watchers[watcherid]

            del self._fifos[fifo_socket]
        
        # Re-open fifo and add listeners.
        fd = os.open(fifo_socket, os.O_RDONLY | os.O_NONBLOCK)
        watchers = {}
        self._fifos[fifo_socket] = (fd, watchers)
        watcher = lambda key, id: watchers.__setitem__(key, id)
        
        # Watch for incoming data.
        gid = gobject.io_add_watch(fd, gobject.IO_IN, self.main_fifo_read)
        watcher('main-fifo-read', gid)
        
        # Watch for fifo hangups.
        gid = gobject.io_add_watch(fd, gobject.IO_HUP, self.main_fifo_hangup)
        watcher('main-fifo-hangup', gid)
        

    def run(self):
        '''UzblTabbed main function that calls the gtk loop.'''

        # Update tablist timer
        #timer = "update-tablist"
        #timerid = gobject.timeout_add(500, self.update_tablist,timer)
        #self._timers[timer] = timerid
        
        # Probe clients every second for window titles and location
        timer = "probe-clients"
        timerid = gobject.timeout_add(1000, self.probe_clients, timer)
        self._timers[timer] = timerid

        gtk.main()


    def probe_clients(self, timer_call):
        '''Probe all uzbl clients for up-to-date window titles and uri's.'''
        
        sockd = {}

        for tab in self.tabs.keys():
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


    def main_fifo_hangup(self, fd, cb_condition):
        '''Handle main fifo socket hangups.'''
        
        # Close fd, re-open fifo_socket and watch.
        self._setup_fifo_watcher(self.fifo_socket)

        # And to kill any gobject event handlers calling this function:
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
        else:
            error("parse_command: unknown command %r" % ' '.join(cmd))

    
    def get_tab_by_pid(self, pid):
        '''Return uzbl instance by pid.'''

        for tab in self.tabs.keys():
            if self.tabs[tab].pid == pid:
                return self.tabs[tab]

        return False
   

    def new_tab(self, uri='', switch=True):
        '''Add a new tab to the notebook and start a new instance of uzbl.
        Use the switch option to negate config['switch_to_new_tabs'] option 
        when you need to load multiple tabs at a time (I.e. like when 
        restoring a session from a file).'''
       
        pid = self.next_pid()
        tab = gtk.Socket()
        tab.show()
        self.notebook.append_page(tab)
        sid = tab.get_id()
        
        fifo_filename = 'uzbl_fifo_%s_%0.2d' % (self.wid, pid)
        fifo_socket = os.path.join(config['fifo_dir'], fifo_filename)
        socket_filename = 'uzbl_socket_%s_%0.2d' % (self.wid, pid)
        socket_file = os.path.join(config['socket_dir'], socket_filename)
        
        # Create meta-instance and spawn child
        if uri: uri = '--uri %s' % uri
        uzbl = self.UzblInstance(self, tab, fifo_socket, socket_file, pid,\
          uri, switch)
        self.tabs[tab] = uzbl
        cmd = 'uzbl -s %s -n %s_%0.2d %s &' % (sid, self.wid, pid, uri)
        subprocess.Popen([cmd], shell=True) # TODO: do i need close_fds=True ?
        
        # Add gobject timer to make sure the config is pushed when fifo socket
        # has been created. 
        timerid = gobject.timeout_add(100, uzbl.flush, "flush-initial-config")
        uzbl.timers['flush-initial-config'] = timerid
    
        self.update_tablist()


    def config_uzbl(self, uzbl):
        '''Send bind commands for tab new/close/next/prev to a uzbl 
        instance.'''

        binds = []
        bind_format = 'bind %s = sh "echo \\\"%s\\\" > \\\"%s\\\""'
        bind = lambda key, action: binds.append(bind_format % (key, action, \
          self.fifo_socket))
        
        # Keys are defined in the config section
        # bind ( key , command back to fifo ) 
        bind(config['bind_new_tab'], 'new')
        bind(config['bind_tab_from_clip'], 'newfromclip')
        bind(config['bind_close_tab'], 'close')
        bind(config['bind_next_tab'], 'next')
        bind(config['bind_prev_tab'], 'prev')
        bind(config['bind_goto_tab'], 'goto %s')
        bind(config['bind_goto_first'], 'goto 0')
        bind(config['bind_goto_last'], 'goto -1')

        # uzbl.send via socket or uzbl.write via fifo, I'll try send. 
        uzbl.send("\n".join(binds))


    def goto_tab(self, n):
        '''Goto tab n (supports negative indexing).'''
        
        if 0 <= n < self.notebook.get_n_pages():
            self.notebook.set_current_page(n)
            self.update_tablist()
            return None

        try: 
            tabs = list(self.notebook)
            tab = tabs[n]
            i = tabs.index(tab)
            self.notebook.set_current_page(i)
            self.update_tablist()
        
        except IndexError:
            pass


    def next_tab(self, step=1):
        '''Switch to next tab or n tabs right.'''
        
        if step < 1:
            error("next_tab: invalid step %r" % step)
            return None
                
        ntabs = self.notebook.get_n_pages()
        tabn = self.notebook.get_current_page() + step
        self.notebook.set_current_page(tabn % ntabs)
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
        
        try: 
            tab = list(self.notebook)[tabn]
        

        except IndexError:
            error("close_tab: invalid index %r" % tabn)
            return None

        self.notebook.remove_page(tabn)


    def tab_closed(self, notebook, tab, page_num):
        '''Close the window if no tabs are left. Called by page-removed 
        signal.'''
        
        if tab in self.tabs.keys():
            uzbl = self.tabs[tab]
            for timer in uzbl.timers.keys():
                error("tab_closed: removing timer %r" % timer)
                gobject.source_remove(uzbl.timers[timer])
            
            if uzbl._socket:
                uzbl._socket.close()
                uzbl._socket = None

            uzbl._fifoout = []
            uzbl._socketout = []
            uzbl._kill = True
            del self.tabs[tab]
        
        if self.notebook.get_n_pages() == 0:
            self.quit()

        self.update_tablist()


    def tab_changed(self, notebook, page, page_num):
        '''Refresh tab list. Called by switch-page signal.'''

        self.update_tablist()


    def update_tablist(self):
        '''Upate tablist status bar.'''
    
        show_tablist = config['show_tablist']
        show_gtk_tabs = config['show_gtk_tabs']
        tab_titles = config['tab_titles']
        if not show_tablist and not show_gtk_tabs:
            return True

        tabs = self.tabs.keys()
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

            if show_gtk_tabs:
                if tab_titles:
                    self.notebook.set_tab_label_text(tab, \
                      gtk_tab_format % (index, uzbl.title[:max_title_len]))
                else:
                    self.notebook.set_tab_label_text(tab, str(index))

            if show_tablist:
                style = colour_selector(index, curpage, uzbl)
                (tabc, textc) = style
                if tab_titles:
                    pango += tab_format % (tabc, index, textc,\
                      uzbl.title[:max_title_len])
                else:
                    pango += tab_format % (tabc, textc, index)
        
        if show_tablist:
            self.tablist.set_markup(pango)

        return True


    def quit(self, *args):
        '''Cleanup the application and quit. Called by delete-event signal.'''
        
        for fifo_socket in self._fifos.keys():
            fd, watchers = self._fifos[fifo_socket]
            os.close(fd)
            for watcherid in watchers.keys():
                gobject.source_remove(watchers[watcherid])
                del watchers[watcherid]

            del self._fifos[fifo_socket]
        
        for timerid in self._timers.keys():
            gobject.source_remove(self._timers[timerid])
            del self._timers[timerid]

        if os.path.exists(self.fifo_socket):
            os.unlink(self.fifo_socket)
            print "Unlinked %s" % self.fifo_socket

        if config['save_session']:
            session_file = os.path.expandvars(config['session_file'])
            if self.notebook.get_n_pages():
                if not os.path.isfile(session_file):
                    dirname = os.path.dirname(session_file)
                    if not os.path.isdir(dirname):
                        rmkdir(dirname)

                h = open(session_file, 'w')
                h.write('current = %s\n' % self.notebook.get_current_page())
                tabs = self.tabs.keys()
                for tab in list(self.notebook):                       
                    if tab not in tabs: continue
                    uzbl = self.tabs[tab]
                    h.write("%s\n" % uzbl.uri)
                h.close()
                
            else:
                # Notebook has no pages so delete session file if it exists.
                if os.path.isfile(session_file):
                    os.remove(session_file)

        gtk.main_quit() 


if __name__ == "__main__":
    
    # Read from the uzbl config into the global config dictionary. 
    readconfig(uzbl_config, config)
     
    uzbl = UzblTabbed()
    
    if os.path.isfile(os.path.expandvars(config['session_file'])):
        h = open(os.path.expandvars(config['session_file']),'r')
        lines = [line.strip() for line in h.readlines()]
        h.close()
        current = 0
        for line in lines:
            if line.startswith("current"):
                current = int(line.split()[-1])

            else:
                uzbl.new_tab(line, False)

        if not len(lines):
            self.new_tab()

    else:
        uzbl.new_tab()

    uzbl.run()


