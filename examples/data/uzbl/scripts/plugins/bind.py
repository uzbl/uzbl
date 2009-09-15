'''Plugin provides support for binds in uzbl.

For example:
  event BIND ZZ = exit          -> bind('ZZ', 'exit')
  event BIND o _ = uri %s       -> bind('o _', 'uri %s')
  event BIND fl* = sh 'echo %s' -> bind('fl*', "sh 'echo %s'")

And it is also possible to execute a function on activation:
  bind('DD', myhandler)
'''

import sys
import re
from event_manager import config, counter, iscallable, isiterable

# Export these variables/functions to uzbl.<name>
__export__ = ['bind', 'del_bind', 'del_bind_by_glob', 'get_binds']

# Hold the bind lists per uzbl instance.
_UZBLS = {}

# Commonly used regular expressions.
starts_with_mod = re.compile('^<([A-Za-z0-9-_]+|.)>')


def echo(msg):
    if config['verbose']:
        print "plugin: bind:", msg


def error(msg):
    sys.stderr.write("plugin: bind: error: %s" % msg)


def ismodbind(glob):
    '''Return True if the glob specifies a modbind.'''

    return bool(starts_with_mod.match(glob))


def sort_mods(glob):
    '''Mods are sorted in the keylet.to_string() result so make sure that
    bind commands also have their mod keys sorted.'''

    mods = []
    while True:
        match = starts_with_mod.match(glob)
        if not match:
            break

        end = match.span()[1]
        mods.append(glob[:end])
        glob = glob[end:]

    return "%s%s" % (''.join(sorted(mods)), glob)


def add_instance(uzbl, *args):
    _UZBLS[uzbl] = []


def del_instance(uzbl, *args):
    if uzbl in _UZBLS:
        del _UZBLS[uzbl]


def get_binds(uzbl):
    '''Return the bind list for the uzbl instance.'''

    if uzbl not in _UZBLS:
        add_instance(uzbl)

    return _UZBLS[uzbl]


def del_bind(uzbl, bind):
    '''Delete bind object if bind in the uzbl binds.'''

    binds = get_binds(uzbl)
    if bind in binds:
        binds.remove(bind)
        uzbl.event("DELETED_BIND", bind)
        return True

    return False


def del_bind_by_glob(uzbl, glob):
    '''Delete bind by glob if bind in the uzbl binds.'''

    binds = get_binds(uzbl)
    for bind in list(binds):
        if bind.glob == glob:
            binds.remove(bind)
            uzbl.event("DELETED_BIND", bind)
            return True

    return False


class Bind(object):

    nextbid = counter().next

    def __init__(self, glob, handler, *args, **kargs):
        self.callable = iscallable(handler)

        if not glob:
            raise ArgumentError('glob cannot be blank')

        if self.callable:
            self.function = handler
            self.args = args
            self.kargs = kargs

        elif kargs:
            raise ArgumentError("cannot supply kargs for uzbl commands")

        elif isiterable(handler):
            self.commands = handler

        else:
            self.commands = [handler,] + list(args)

        self.glob = glob
        self.bid = self.nextbid()

        # Is the binding a MODCMD or KEYCMD.
        self.mod_bind = ismodbind(glob)

        # Execute the command on UPDATES or EXEC's.
        self.on_exec = True if glob.endswith('_') else False

        if glob[-1] in ['*', '_']:
            self.has_args = True
            glob = glob[:-1]

        else:
            self.has_args = False

        self.match = glob


    def __repr__(self):
        args = ["glob=%r" % self.glob, "bid=%d" % self.bid]

        if self.callable:
            args.append("function=%r" % self.function)
            if self.args:
                args.append("args=%r" % self.args)

            if self.kargs:
                args.append("kargs=%r" % self.kargs)

        else:
            cmdlen = len(self.commands)
            cmds = self.commands[0] if cmdlen == 1 else self.commands
            args.append("command%s=%r" % ("s" if cmdlen-1 else "", cmds))

        return "<Bind(%s)>" % ', '.join(args)


def bind(uzbl, glob, handler, *args, **kargs):
    '''Add a bind handler object.'''

    # Mods come from the keycmd sorted so make sure the modkeys in the bind
    # command are sorted too.
    glob = sort_mods(glob)

    del_bind_by_glob(uzbl, glob)
    binds = get_binds(uzbl)

    bind = Bind(glob, handler, *args, **kargs)
    binds.append(bind)

    uzbl.event('ADDED_BIND', bind)


def parse_bind_event(uzbl, args):
    '''Parse "event BIND fl* = js follownums.js" commands.'''

    if len(args.split('=', 1)) != 2:
        error('invalid bind format: %r' % args)

    glob, command = map(str.strip, args.split('=', 1))
    bind(uzbl, glob, command)


def match_and_exec(uzbl, bind, keylet):

    keycmd = keylet.to_string()
    if bind.has_args:
        if not keycmd.startswith(bind.match):
            return False

        args = [keycmd[len(bind.match):],]

    elif keycmd != bind.match:
        return False

    else:
        args = []

    uzbl.exec_handler(bind, *args)

    if not bind.has_args:
        uzbl.clear_keycmd()

    return True


def keycmd_update(uzbl, keylet):
    for bind in get_binds(uzbl):
        if bind.mod_bind or bind.on_exec:
            continue

        match_and_exec(uzbl, bind, keylet)


def keycmd_exec(uzbl, keylet):
    for bind in get_binds(uzbl):
        if bind.mod_bind or not bind.on_exec:
            continue

        match_and_exec(uzbl, bind, keylet)


def modcmd_update(uzbl, keylet):
    for bind in get_binds(uzbl):
        if not bind.mod_bind or bind.on_exec:
            continue

        match_and_exec(uzbl, bind, keylet)


def modcmd_exec(uzbl, keylet):
    for bind in get_binds(uzbl):
        if not bind.mod_bind or not bind.on_exec:
            continue

        match_and_exec(uzbl, bind, keylet)


def init(uzbl):

    connects = {'BIND': parse_bind_event,
      'KEYCMD_UPDATE': keycmd_update,
      'MODCMD_UPDATE': modcmd_update,
      'KEYCMD_EXEC': keycmd_exec,
      'MODCMD_EXEC': modcmd_exec}

    for (event, handler) in connects.items():
        uzbl.connect(event, handler)
