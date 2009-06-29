#!/usr/bin/python

import string, pygtk, gtk, sys, subprocess
pygtk.require('2.0')

def new_tab(nothing):
    socket = gtk.Socket()
    socket.show()
    notebook.append_page(socket, gtk.Label('title goes here'))
    sid = socket.get_id()
    subprocess.call(['sh', '-c', 'uzbl -s %s &'%sid])


window = gtk.Window()
window.show()

vbox = gtk.VBox()
vbox.show()
window.add(vbox)

button = gtk.Button(stock=gtk.STOCK_ADD)
button.connect('clicked', new_tab)
button.show()
vbox.add(button)

notebook = gtk.Notebook()
vbox.add(notebook)
notebook.show()

window.connect("destroy", lambda w: gtk.main_quit())

#def plugged_event(widget):
#    print "I (", widget, ") have just had a plug inserted!"

#socket.connect("plug-added", plugged_event)
#socket.connect("plug-removed", plugged_event)

if len(sys.argv) == 2:
    socket.add_id(long(sys.argv[1]))

gtk.main()