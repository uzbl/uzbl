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

import re
import fnmatch

from uzbl.arguments import splitquoted
from .cmd_expand import cmd_expand

def match_args(pattern, args):
    if len(pattern) > len(args):
        return False
    for p, a in zip(pattern, args):
        if not fnmatch.fnmatch(a, p):
            return False
    return True


def event_handler(uzbl, *args, **kargs):
    '''This function handles all the events being watched by various
    on_event definitions and responds accordingly.'''

    # Could be connected to a EM internal event that can use anything as args
    if len(args) == 1 and isinstance(args[0], basestring):
        args = splitquoted(args[0])

    events = uzbl.on_events
    event = kargs['on_event']
    if event not in events:
        return

    commands = events[event]
    for cmd, pattern in commands.items():
        if not pattern or match_args(pattern, args):
            cmd = cmd_expand(cmd, args)
            uzbl.send(cmd)


def on_event(uzbl, event, pattern, cmd):
    '''Add a new event to watch and respond to.'''

    event = event.upper()
    events = uzbl.on_events
    logger.debug('new event handler %r %r %r', event, pattern, cmd)
    if event not in events:
        connect(uzbl, event, event_handler, on_event=event)
        events[event] = {}

    cmds = events[event]
    if cmd not in cmds:
        cmds[cmd] = pattern


def parse_on_event(uzbl, args):
    '''Parse ON_EVENT events and pass them to the on_event function.

    Syntax: "event ON_EVENT <EVENT_NAME> commands".'''

    args = splitquoted(args)
    assert args, 'missing on event arguments'

    # split arguments into event name, optional argument pattern and command
    event = args[0]
    pattern = []
    if args[1] == '[':
        for i, arg in enumerate(args[2:]):
            if arg == ']':
                break
            pattern.append(arg)
        command = args.raw(3+i)
    else:
        command = args.raw(1)

    assert event and command, 'missing on event command'
    on_event(uzbl, event, pattern, command)


# plugin init hook
def init(uzbl):
    '''Export functions and connect handlers to events.'''

    connect(uzbl, 'ON_EVENT', parse_on_event)

    export_dict(uzbl, {
        'on_event':     on_event,
        'on_events':    {},
    })

# plugin cleanup hook
def cleanup(uzbl):
    for handlers in uzbl.on_events.values():
        handlers.clear()

    uzbl.on_events.clear()

# vi: set et ts=4:
