'''Plugin provides arbitrary binding of uzbl events to uzbl commands.

Formatting options:
  %s = space separated string of the arguments
  %r = escaped and quoted version of %s
  %1 = argument 1
  %2 = argument 2
  %n = argument n

Usage:
  request ON_EVENT LINK_HOVER set selected_uri = $1
    --> LINK_HOVER http://uzbl.org/
    <-- set selected_uri = http://uzbl.org/

  request ON_EVENT CONFIG_CHANGED print Config changed: %1 = %2
    --> CONFIG_CHANGED selected_uri http://uzbl.org/
    <-- print Config changed: selected_uri = http://uzbl.org/
'''

import sys
import re

__export__ = ['get_on_events', 'on_event']

UZBLS = {}


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


def event_handler(uzbl, *args, **kargs):
    '''This function handles all the events being watched by various
    on_event definitions and responds accordingly.'''

    events = get_on_events(uzbl)
    event = kargs['on_event']
    if event not in events:
        return

    commands = events[event]
    cmd_expand = uzbl.cmd_expand
    for cmd in commands:
        cmd = cmd_expand(cmd, args)
        uzbl.send(cmd)


def on_event(uzbl, event, cmd):
    '''Add a new event to watch and respond to.'''

    event = event.upper()
    events = get_on_events(uzbl)
    if event not in events:
        uzbl.connect(event, event_handler, on_event=event)
        events[event] = []

    cmds = events[event]
    if cmd not in cmds:
        cmds.append(cmd)


def parse_on_event(uzbl, args):
    '''Parse ON_EVENT events and pass them to the on_event function.

    Syntax: "event ON_EVENT <EVENT_NAME> commands".'''

    if not args:
        return error("missing on_event arguments")

    split = args.split(' ', 1)
    if len(split) != 2:
        return error("invalid ON_EVENT syntax: %r" % args)

    event, cmd = split
    on_event(uzbl, event, cmd)


def init(uzbl):
    # Event handling hooks.
    uzbl.connect_dict({
        'INSTANCE_EXIT':    del_instance,
        'INSTANCE_START':   add_instance,
        'ON_EVENT':         parse_on_event,
    })

    # Function exports to the uzbl object, `function(uzbl, *args, ..)`
    # becomes `uzbl.function(*args, ..)`.
    uzbl.export_dict({
        'get_on_events':    get_on_events,
        'on_event':         on_event,
    })
