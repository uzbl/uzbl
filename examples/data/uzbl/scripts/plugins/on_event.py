'''Plugin provides arbitrarily binding uzbl events to uzbl commands.
   You can use $1,$2 to refer to the arguments appearing in the relevant event messages

For example:
  request ON_EVENT LINK_HOVER 'set SELECTED_URI = $1'
  this will set the SELECTED_URI variable which you can display in your statusbar
'''

import sys
import re
from event_manager import config

__export__ = ['get_on_events', 'on_event']

UZBLS = {}

def echo(msg):
    if config['verbose']:
        print 'on_event plugin:', msg


def error(msg):
    sys.stderr.write('on_event plugin: error: %s\n' % msg)


def add_instance(uzbl, *args):
    UZBLS[uzbl] = {}


def del_instance(uzbl, *args):
    if uzbl in UZBLS:
        del UZBLS[uzbl]


def get_on_events(uzbl):
    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


def expand(cmd, args):
    '''Replaces "%s %s %s.." with "arg1 arg2 arg3..".

    This could be improved by specifing explicitly which argument to substitue
    for what by parsing "$@ $0 $1 $2 $3.." found in the command string.'''

    if '%s' not in cmd or not len(args):
        return cmd

    if len(args) > 1:
        for arg in args:
            cmd = cmd.replace('%s', str(arg), 1)

    else:
        cmd = cmd.replace('%s', str(args[0]))

    return cmd


def event_handler(uzbl, *args, **kargs):
    '''This function handles all the events being watched by various
    on_event definitions and responds accordingly.'''

    events = get_on_events(uzbl)
    event = kargs['on_event']
    if event not in events:
        return

    commands = events[event]
    for cmd in commands:
        cmd = expand(cmd, args)
        uzbl.send(cmd)


def on_event(uzbl, event, cmd):
    '''Add a new event to watch and respond to.'''

    events = get_on_events(uzbl)
    if event not in events:
        uzbl.connect(event, event_handler, on_event=event)
        events[event] = []

    cmds = events[event]
    if cmd not in cmds:
        cmds.append(cmd)


def parse_on_event(uzbl, args):
    '''Parse ON_EVENT events and pass them to the on_event function.

    Syntax: "event ON_EVENT <EVENT_NAME> commands"
    '''

    split = args.split(' ', 1)
    if len(split) != 2:
        return error("invalid ON_EVENT syntax: %r" % args)

    event, cmd = split
    on_event(uzbl, event, cmd)


def init(uzbl):
    connects = {'ON_EVENT': parse_on_event,
      'INSTANCE_START': add_instance,
      'INSTANCE_EXIT': del_instance}

    for (event, handler) in connects.items():
        uzbl.connect(event, handler)
