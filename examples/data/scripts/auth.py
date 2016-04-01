#!/usr/bin/env python

from __future__ import print_function

import os
import gtk
import sys
import argparse


def responseToDialog(entry, dialog, response):
    dialog.response(response)


def getText(authHost, authRealm):
    dialog = gtk.MessageDialog(
        None,
        gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
        gtk.MESSAGE_QUESTION,
        gtk.BUTTONS_OK_CANCEL,
        None)
    dialog.set_markup('%s at %s' % (authRealm, authHost))

    login = gtk.Entry()
    password = gtk.Entry()
    password.set_visibility(False)

    login.connect("activate", responseToDialog, dialog, gtk.RESPONSE_OK)
    password.connect("activate", responseToDialog, dialog, gtk.RESPONSE_OK)

    hbox = gtk.HBox()

    vbox_entries = gtk.VBox()
    vbox_labels = gtk.VBox()

    vbox_labels.pack_start(gtk.Label("Login:"), False, 5, 5)
    vbox_labels.pack_end(gtk.Label("Password:"), False, 5, 5)

    vbox_entries.pack_start(login)
    vbox_entries.pack_end(password)

    dialog.format_secondary_markup("Please enter username and password:")
    hbox.pack_start(vbox_labels, True, True, 0)
    hbox.pack_end(vbox_entries, True, True, 0)

    dialog.vbox.pack_start(hbox)
    dialog.show_all()
    rv = dialog.run()

    output = {
        'username': login.get_text(),
        'password': password.get_text()
    }
    dialog.destroy()
    return rv, output


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('host')
    parser.add_argument('realm')
    parser.add_argument('extra', nargs='*')
    args = parser.parse_args()

    rv, output = getText(args.host, args.realm)
    if rv == gtk.RESPONSE_OK:
        print('AUTH')
        print(output['username'])
        print(output['password'])
    else:
        print('IGNORE')
        exit(1)
