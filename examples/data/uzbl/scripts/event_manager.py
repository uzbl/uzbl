#!/usr/bin/env python

# Uzbl sample event manager
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
The Python Event Manager
========================

Sample event manager written in python

Usage
====
uzbl | <path to event_manager.py>

'''

import sys
import os

# config dir. needed for bindings config
if 'XDG_CONFIG_HOME' in os.environ.keys() and os.environ['XDG_CONFIG_HOME']:
    CONFIG_DIR = os.path.join(os.environ['XDG_CONFIG_HOME'], 'uzbl/')
else:
    CONFIG_DIR = os.path.join(os.environ['HOME'], '.config/uzbl/')


# Default config
config = {

  'uzbl_fifo': '',
  'verbose': True,

} # End of config dictionary.

# buffer for building up commands
keycmd = ''


_SCRIPTNAME = os.path.basename(sys.argv[0])
def echo(msg):
    '''Prints only if the verbose flag has been set.'''

    if config['verbose']:
        sys.stderr.write("%s: %s\n" % (_SCRIPTNAME, msg))


def error(msg):
    '''Prints error message and exits.'''

    sys.stderr.write("%s: error: %s\n" % (_SCRIPTNAME, msg))
    sys.exit(1)

def fifo(msg):
    '''Writes commands to uzbl's fifo, if the fifo path is known'''

    echo ('Fifo msg: ' + msg + '(fifo path: ' + config['uzbl_fifo'] + ')')
    if config['uzbl_fifo']:
        fd = os.open(config['uzbl_fifo'], os.O_WRONLY)
        os.write(fd, msg)
        os.close(fd)

def submit_keycmd():
    '''Sends the updated keycmd to uzbl, which can render it and stuff'''

    fifo ('set keycmd = ' + keycmd)


def main():
    '''Main function.'''

    echo ("Init eventhandler")

    for line in sys.stdin:
        line = line.strip()
        data = line.partition('EVENT ')
        if (data[0] == ""):
            line = data[2]
            echo ("Got event: " + line)
            data = line.partition(' ')
            event_name = data[0]
            event_data = data[2]
        else:
            echo ("Non-event: " + line)
            continue
        
        if (event_name == 'FIFO_SET'):
            config['uzbl_fifo'] = event_data.split()[-1]
        elif (event_name == 'KEY_PRESS'):
            # todo: keep a table of pressed modkeys. do we work with Mod[1-4] here or Alt_L and such?
            key = event_data.split()[-1]
            if (key == 'Escape'):
                keycmd = ''
            submit_keycmd
        elif (event_name == 'KEY_RELEASE'):
           #todo : update table of pressed modkeys
           submit_keycmd

if __name__ == "__main__":
    main()
 