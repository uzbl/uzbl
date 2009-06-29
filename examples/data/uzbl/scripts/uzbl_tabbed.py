#!/usr/bin/python

# Uzbl tabbing wrapper using a fifo socket interface
# Copywrite (c) 2009, Tom Adams <tom@holizz.com>
# Copywrite (c) 2009, quigybo <?>
# Copywrite (c) 2009, Mason Larobina <mason.larobina@gmail.com>
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
#   quigybo <?>
#       Made signifigant headway on the uzbl_tabbing.py script on the 
#       uzbl wiki <http://www.uzbl.org/wiki/uzbl_tabbed> 
#
#   Mason Larobina <mason.larobina@gmail.com>
#       Rewrite of the uzbl_tabbing.py script to use a fifo socket interface
#       and inherit configuration options from the user's uzbl config.
#
# Contributor(s):
#   (None yet)


# Issues: 
#   - status_background colour is not honoured (reverts to gtk default).
#   - new windows are not caught and opened in a new tab.
#   - need an easier way to read a uzbl instances window title instead of 
#     spawning a shell to spawn uzblctrl to communicate to the uzbl 
#     instance via socket to dump the window title to then pipe it to 
#     the tabbing managers fifo socket.
#   - probably missing some os.path.expandvars somewhere. 


# Todo: 
#   - add command line options to use a different session file, not use a
#     session file and or open a uri on starup. 
#   - ellipsize individual tab titles when the tab-list becomes over-crowded
#   - add "<" & ">" arrows to tablist to indicate that only a subset of the 
#     currently open tabs are being displayed on the tablist.
#   - probably missing some os.path.expandvars somewhere and other 
#     user-friendly.. things, this is still a very early version. 
#   - fix status_background issues & style tablist. 
#   - add the small tab-list display when both gtk tabs and text vim-like
#     tablist are hidden (I.e. [ 1 2 3 4 5 ])
#   - check spelling.


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

pygtk.require('2.0')

def error(msg):
    sys.stderr.write("%s\n"%msg)

if 'XDG_DATA_HOME' in os.environ.keys() and os.environ['XDG_DATA_HOME']:
    data_dir = os.path.join(os.environ['XDG_DATA_HOME'], 'uzbl/')

else:
    data_dir = os.path.join(os.environ['HOME'], '.local/share/uzbl/')

# === Default Configuration ====================================================

# Location of your uzbl configuration file.
uzbl_config = os.path.join(os.environ['HOME'],'.config/uzbl/config')

# All of these settings can be inherited from your uzbl config file.
config = {'show_tabs': True,
  'show_gtk_tabs': False,
  'switch_to_new_tabs': True,
  'save_session': True,
  'fifo_dir': '/tmp',
  'icon_path': os.path.join(data_dir, 'uzbl.png'),
  'session_file': os.path.join(data_dir, 'session'),
  'tab_colours': 'foreground = "#999"',
  'tab_text_colours': 'foreground = "#444"',
  'selected_tab': 'foreground = "#aaa" background="#303030"',
  'selected_tab_text': 'foreground = "green"',
  'window_size': "800,800",
  'monospace_size': 10, 
  'bind_new_tab': 'gn',
  'bind_tab_from_clipboard': 'gY', 
  'bind_close_tab': 'gC',
  'bind_next_tab': 'gt',
  'bind_prev_tab': 'gT',
  'bind_goto_tab': 'gi_',
  'bind_goto_first': 'g<',
  'bind_goto_last':'g>'}

# === End Configuration =======================================================

def readconfig(uzbl_config, config):
    '''Loads relevant config from the users uzbl config file into the global
    config dictionary.'''

    if not os.path.exists(uzbl_config):
        error("Unable to load config %r" % uzbl_config)
        return None
    
    # Define parsing regular expressions
    isint = re.compile("^[0-9]+$").match
    findsets = re.compile("^set\s+([^\=]+)\s*\=\s*(.+)$",\
      re.MULTILINE).findall

    h = open(os.path.expandvars(uzbl_config), 'r')
    rawconfig = h.read()
    h.close()
    
    for (key, value) in findsets(rawconfig):
        key = key.strip()
        if key not in config.keys(): continue
        if isint(value): value = int(value)
        config[key] = value


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


class UzblTabbed:
    '''A tabbed version of uzbl using gtk.Notebook'''

    class UzblInstance:
        '''Uzbl instance meta-data/meta-action object.'''

        def __init__(self, parent, socket, fifo, pid, url='', switch=True):
            self.parent = parent
            self.socket = socket # the gtk socket
            self.fifo = fifo
            self.pid = pid
            self.title = "New tab"
            self.url = url
            self.timers = {}
            self._lastprobe = 0
            self._switch_on_config = switch
            self._outgoing = []
            self._configured = False

            # When notebook tab deleted the kill switch is raised.
            self._kill = False
            
            # Queue binds for uzbl child
            self.parent.config_uzbl(self)


        def flush(self, timer_call=False):
            '''Flush messages from the queue.'''
            
            if self._kill:
                error("Flush called on dead page.")
                return False

            if os.path.exists(self.fifo):
                h = open(self.fifo, 'w')
                while len(self._outgoing):
                    msg = self._outgoing.pop(0)
                    h.write("%s\n" % msg)
                h.close()

            elif not timer_call and self._configured:
                # TODO: I dont know what to do here. A previously thought
                # alright uzbl client fifo socket has now gone missing.
                # I think this should be fatal (at least for the page in
                # question). I'll wait until this error appears in the wild. 
                error("Error: fifo %r lost in action." % self.fifo)
            
            if not len(self._outgoing) and timer_call:
                self._configured = True

                if timer_call in self.timers.keys():
                    gobject.source_remove(self.timers[timer_call])
                    del self.timers[timer_call]

                if self._switch_on_config:
                    notebook = list(self.parent.notebook)
                    try:
                        tabid = notebook.index(self.socket)
                        self.parent.goto_tab(tabid)

                    except ValueError:
                        pass
                
            return len(self._outgoing)


        def probe(self):
            '''Probes the client for information about its self.'''
            
            # Ugly way of getting the socket path. Screwed if fifo is in any
            # other part of the fifo socket path.

            socket = 'socket'.join(self.fifo.split('fifo'))
            
            # I feel so dirty
            subcmd = 'print title %s @<document.title>@' % self.pid
            cmd = 'uzblctrl -s "%s" -c "%s" > "%s" &' % (socket, subcmd, \
              self.parent.fifo_socket)

            subprocess.Popen([cmd], shell=True)

            self._lastprobe = time.time()
            
        
        def send(self, msg):
            '''Child fifo write function.'''

            self._outgoing.append(msg)
            # Flush messages from the queue if able.
            return self.flush()


    def __init__(self):
        '''Create tablist, window and notebook.'''
        
        self.pages = {}
        self._pidcounter = counter()
        self.next_pid = self._pidcounter.next
        self._watchers = {}
        self._timers = {}
        self._buffer = ""

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
        if config['show_tabs']:
            vbox = gtk.VBox()
            self.window.add(vbox)

            self.tablist = gtk.Label()
            self.tablist.set_use_markup(True)
            self.tablist.set_justify(gtk.JUSTIFY_LEFT)
            self.tablist.set_line_wrap(False)
            self.tablist.set_selectable(False)
            self.tablist.set_padding(0,2)
            self.tablist.set_alignment(0,0)
            self.tablist.set_ellipsize(pango.ELLIPSIZE_END)
            self.tablist.set_text(" ")
            self.tablist.show()
            vbox.pack_start(self.tablist, False, False, 0)
        
        # Create notebook
        self.notebook = gtk.Notebook()
        self.notebook.set_show_tabs(config['show_gtk_tabs'])
        self.notebook.set_show_border(False)
        self.notebook.connect("page-removed", self.tab_closed)
        self.notebook.connect("switch-page", self.tab_changed)
        self.notebook.show()
        if config['show_tabs']:
            vbox.pack_end(self.notebook, True, True, 0)
            vbox.show()
        else:
            self.window.add(self.notebook)
        
        self.window.show()
        self.wid = self.notebook.window.xid
        # Fifo socket definition
        self._refindfifos = re.compile('^uzbl_fifo_%s_[0-9]+$' % self.wid)
        fifo_filename = 'uzbltabbed_%d' % os.getpid()
        self.fifo_socket = os.path.join(config['fifo_dir'], fifo_filename)

        self._watchers = {}
        self._buffer = ""
        self._create_fifo_socket(self.fifo_socket)
        self._setup_fifo_watcher(self.fifo_socket)


    def run(self):
        
        # Update tablist timer
        timer = "update-tablist"
        timerid = gobject.timeout_add(500, self.update_tablist,timer)
        self._timers[timer] = timerid

        # Due to the hackish way in which the window titles are read 
        # too many window will cause the application to slow down insanely
        timer = "probe-clients"
        timerid = gobject.timeout_add(1000, self.probe_clients, timer)
        self._timers[timer] = timerid

        gtk.main()


    def _find_fifos(self, fifo_dir):
        '''Find all child fifo sockets in fifo_dir.'''
        
        dirlist = '\n'.join(os.listdir(fifo_dir))
        allfifos = self._refindfifos.findall(dirlist)
        return sorted(allfifos)


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


    def _setup_fifo_watcher(self, fifo_socket, fd=None):
        '''Open fifo socket fd and setup gobject IO_IN & IO_HUP watchers.
        Also log the creation of a fd and store the the internal
        self._watchers dictionary along with the filename of the fd.'''
        
        #TODO: Convert current self._watcher dict manipulation to the better 
        # IMHO self._timers handling by using "timer-keys" as the keys instead
        # of the fifo fd's as keys.

        if fd:
            os.close(fd)
            if fd in self._watchers.keys():
                d = self._watchers[fd]
                watchers = d['watchers']
                for watcher in list(watchers):
                    gobject.source_remove(watcher)
                    watchers.remove(watcher)
                del self._watchers[fd]         
        
        fd = os.open(fifo_socket, os.O_RDONLY | os.O_NONBLOCK)
        self._watchers[fd] = {'watchers': [], 'filename': fifo_socket}
            
        watcher = self._watchers[fd]['watchers'].append
        watcher(gobject.io_add_watch(fd, gobject.IO_IN, self.read_fifo))
        watcher(gobject.io_add_watch(fd, gobject.IO_HUP, self.fifo_hangup))
        

    def probe_clients(self, timer_call):
        '''Load balance probe all uzbl clients for up-to-date window titles 
        and uri's.'''
        
        p = self.pages 
        probetimes = [(s, p[s]._lastprobe) for s in p.keys()]
        socket, lasttime = sorted(probetimes, key=lambda t: t[1])[0]

        if (time.time()-lasttime) > 5:
            # Probe a uzbl instance at most once every 10 seconds
            self.pages[socket].probe()

        return True


    def fifo_hangup(self, fd, cb_condition):
        '''Handle fifo socket hangups.'''
        
        # Close fd, re-open fifo_socket and watch.
        self._setup_fifo_watcher(self.fifo_socket, fd)

        # And to kill any gobject event handlers calling this function:
        return False


    def read_fifo(self, fd, cb_condition):
        '''Read from fifo socket and handle fifo socket hangups.'''

        self._buffer = os.read(fd, 1024)
        temp = self._buffer.split("\n")
        self._buffer = temp.pop()

        for cmd in [s.strip().split() for s in temp if len(s.strip())]:
            try:
                #print cmd
                self.parse_command(cmd)

            except:
                #raise
                error("Invalid command: %s" % ' '.join(cmd))
        
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
        # url {pid} {document-location}
         
        # WARNING SOME OF THESE COMMANDS MIGHT NOT BE WORKING YET OR FAIL.

        if cmd[0] == "new":
            if len(cmd) == 2:
                self.new_tab(cmd[1])

            else:
                self.new_tab()

        elif cmd[0] == "newfromclip":
            url = subprocess.Popen(['xclip','-selection','clipboard','-o'],\
              stdout=subprocess.PIPE).communicate()[0]
            if url:
                self.new_tab(url)

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

        elif cmd[0] in ["title", "url"]:
            if len(cmd) > 2:
                uzbl = self.get_uzbl_by_pid(int(cmd[1]))
                if uzbl:
                    setattr(uzbl, cmd[0], ' '.join(cmd[2:]))
                else:
                    error("Cannot find uzbl instance with pid %r" % int(cmd[1]))
        else:
            error("Unknown command: %s" % ' '.join(cmd))

    
    def get_uzbl_by_pid(self, pid):
        '''Return uzbl instance by pid.'''

        for socket in self.pages.keys():
            if self.pages[socket].pid == pid:
                return self.pages[socket]
        return False
   

    def new_tab(self,url='', switch=True):
        '''Add a new tab to the notebook and start a new instance of uzbl.
        Use the switch option to negate config['switch_to_new_tabs'] option 
        when you need to load multiple tabs at a time (I.e. like when 
        restoring a session from a file).'''
       
        pid = self.next_pid()
        socket = gtk.Socket()
        socket.show()
        self.notebook.append_page(socket)
        sid = socket.get_id()
        
        if url:
            url = '--uri %s' % url
        
        fifo_filename = 'uzbl_fifo_%s_%0.2d' % (self.wid, pid)
        fifo_socket = os.path.join(config['fifo_dir'], fifo_filename)
        uzbl = self.UzblInstance(self, socket, fifo_socket, pid,\
          url=url, switch=switch)
        self.pages[socket] = uzbl
        cmd = 'uzbl -s %s -n %s_%0.2d %s &' % (sid, self.wid, pid, url)
        subprocess.Popen([cmd], shell=True)        
        
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
        bind(config['bind_tab_from_clipboard'], 'newfromclip')
        bind(config['bind_close_tab'], 'close')
        bind(config['bind_next_tab'], 'next')
        bind(config['bind_prev_tab'], 'prev')
        bind(config['bind_goto_tab'], 'goto %s')
        bind(config['bind_goto_first'], 'goto 0')
        bind(config['bind_goto_last'], 'goto -1')

        uzbl.send("\n".join(binds))


    def goto_tab(self, n):
        '''Goto tab n (supports negative indexing).'''
        
        notebook = list(self.notebook)
        
        try: 
            page = notebook[n]
            i = notebook.index(page)
            self.notebook.set_current_page(i)
        
        except IndexError:
            pass

        self.update_tablist()


    def next_tab(self, n=1):
        '''Switch to next tab or n tabs right.'''
        
        if n >= 1:
            numofpages = self.notebook.get_n_pages()
            pagen = self.notebook.get_current_page() + n
            self.notebook.set_current_page( pagen % numofpages ) 

        self.update_tablist()


    def prev_tab(self, n=1):
        '''Switch to prev tab or n tabs left.'''
        
        if n >= 1:
            numofpages = self.notebook.get_n_pages()
            pagen = self.notebook.get_current_page() - n
            while pagen < 0: 
                pagen += numofpages
            self.notebook.set_current_page(pagen)

        self.update_tablist()


    def close_tab(self, tabid=None):
        '''Closes current tab. Supports negative indexing.'''
        
        if not tabid: 
            tabid = self.notebook.get_current_page()
        
        try: 
            socket = list(self.notebook)[tabid]

        except IndexError:
            error("Invalid index. Cannot close tab.")
            return False

        uzbl = self.pages[socket]
        # Kill timers:
        for timer in uzbl.timers.keys():
            error("Removing timer %r %r" % (timer, uzbl.timers[timer]))
            gobject.source_remove(uzbl.timers[timer])

        uzbl._outgoing = []
        uzbl._kill = True
        del self.pages[socket]
        self.notebook.remove_page(tabid)

        self.update_tablist()


    def tab_closed(self, notebook, socket, page_num):
        '''Close the window if no tabs are left. Called by page-removed 
        signal.'''
        
        if socket in self.pages.keys():
            uzbl = self.pages[socket]
            for timer in uzbl.timers.keys():
                error("Removing timer %r %r" % (timer, uzbl.timers[timer]))
                gobject.source_remove(uzbl.timers[timer])

            uzbl._outgoing = []
            uzbl._kill = True
            del self.pages[socket]
        
        if self.notebook.get_n_pages() == 0:
            self.quit()

        self.update_tablist()


    def tab_changed(self, notebook, page, page_num):
        '''Refresh tab list. Called by switch-page signal.'''

        self.update_tablist()


    def update_tablist(self, timer_call=None):
        '''Upate tablist status bar.'''

        pango = ""

        normal = (config['tab_colours'], config['tab_text_colours'])
        selected = (config['selected_tab'], config['selected_tab_text'])
        
        tab_format = "<span %s> [ %d <span %s> %s</span> ] </span>"
        
        title_format = "%s - Uzbl Browser"

        uzblkeys = self.pages.keys()
        curpage = self.notebook.get_current_page()

        for index, socket in enumerate(self.notebook):
            if socket not in uzblkeys:
                #error("Theres a socket in the notebook that I have no uzbl "\
                #  "record of.")
                continue
            uzbl = self.pages[socket]
            
            if index == curpage:
                colours = selected
                self.window.set_title(title_format % uzbl.title)

            else:
                colours = normal
            
            pango += tab_format % (colours[0], index, colours[1], uzbl.title)

        self.tablist.set_markup(pango)

        return True


    #def quit(self, window, event):
    def quit(self, *args):
        '''Cleanup the application and quit. Called by delete-event signal.'''

        for fd in self._watchers.keys():
            d = self._watchers[fd]
            watchers = d['watchers']
            for watcher in list(watchers):
                gobject.source_remove(watcher)
        
        for timer in self._timers.keys():
            gobject.source_remove(self._timers[timer])

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
                h.close()
                for socket in list(self.notebook):
                    if socket not in self.pages.keys(): continue
                    uzbl = self.pages[socket]
                    uzbl.send('sh "echo $6 >> %s"' % session_file)
                    time.sleep(0.05)

            else:
                # Notebook has no pages so delete session file if it exists.
                # Its better to not exist than be blank IMO. 
                if os.path.isfile(session_file):
                    os.remove(session_file)

        gtk.main_quit() 


if __name__ == "__main__":
    
    # Read from the uzbl config into the global config dictionary. 
    readconfig(uzbl_config, config)
     
    uzbl = UzblTabbed()
    
    if os.path.isfile(os.path.expandvars(config['session_file'])):
        h = open(os.path.expandvars(config['session_file']),'r')
        urls = [s.strip() for s in h.readlines()]
        h.close()
        current = 0
        for url in urls:
            if url.startswith("current"):
                current = int(url.split()[-1])
            else:
                uzbl.new_tab(url, False)
    else:
        uzbl.new_tab()

    uzbl.run()


