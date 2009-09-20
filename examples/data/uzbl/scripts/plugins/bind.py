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
UZBLS = {}

# Commonly used regular expressions.
starts_with_mod = re.compile('^<([A-Z][A-Za-z0-9-_]+)>')
find_prompts = re.compile('<([^:>]*):>').split

# For accessing a bind glob stack.
MOD_CMD, ON_EXEC, HAS_ARGS, GLOB = range(4)


class BindParseError(Exception):
    pass


def echo(msg):
    if config['verbose']:
        print 'bind plugin:', msg


def error(msg):
    sys.stderr.write('bind plugin: error: %s\n' % msg)


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

    return '%s%s' % (''.join(sorted(mods)), glob)


def add_instance(uzbl, *args):
    UZBLS[uzbl] = {'binds': [], 'depth': 0, 'filter': [],
      'args': [], 'last_mode': ''}


def del_instance(uzbl, *args):
    if uzbl in UZBLS:
        del UZBLS[uzbl]


def get_bind_dict(uzbl):
    '''Return the bind dict for the uzbl instance.'''

    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


def get_binds(uzbl):
    '''Return the bind list for the uzbl instance.'''

    return get_bind_dict(uzbl)['binds']


def get_stack_depth(uzbl):
    '''Return the stack for the uzbl instance.'''

    return get_bind_dict(uzbl)['depth']


def get_filtered_binds(uzbl):
    '''Return the bind list for the uzbl instance or return the filtered
    bind list thats on the current stack.'''

    bind_dict = get_bind_dict(uzbl)
    if bind_dict['depth']:
        return list(bind_dict['filter'])

    return list(bind_dict['binds'])


def del_bind(uzbl, bind):
    '''Delete bind object if bind in the uzbl binds.'''

    binds = get_binds(uzbl)
    if bind in binds:
        binds.remove(bind)
        uzbl.event('DELETED_BIND', bind)
        return True

    return False


def del_bind_by_glob(uzbl, glob):
    '''Delete bind by glob if bind in the uzbl binds.'''

    binds = get_binds(uzbl)
    for bind in list(binds):
        if bind.glob == glob:
            binds.remove(bind)
            uzbl.event('DELETED_BIND', bind)
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
            raise ArgumentError('cannot supply kargs for uzbl commands')

        elif isiterable(handler):
            self.commands = handler

        else:
            self.commands = [handler,] + list(args)

        self.glob = glob
        self.bid = self.nextbid()

        self.split = split = find_prompts(glob)
        self.prompts = split[1::2]

        # Check that there is nothing like: fl*<int:>*
        for glob in split[:-1:2]:
            if glob.endswith('*'):
                msg = "token '*' not at the end of a prompt bind: %r" % split
                raise BindParseError(msg)

        # Check that there is nothing like: fl<prompt1:><prompt2:>_
        for glob in split[2::2]:
            if not glob:
                msg = 'found null segment after first prompt: %r' % split
                raise BindParseError(msg)

        self.stack = []

        for glob in split[::2]:
            # Is the binding a MODCMD or KEYCMD:
            mod_cmd = ismodbind(glob)

            # Execute the command on UPDATES or EXEC's:
            on_exec = True if glob.endswith('_') else False

            # Does the command store arguments:
            has_args = True if glob[-1] in ['*', '_'] else False
            glob = glob[:-1] if has_args else glob

            self.stack.append((mod_cmd, on_exec, has_args, glob))


    def __repr__(self):
        args = ['glob=%r' % self.glob, 'bid=%d' % self.bid]

        if self.callable:
            args.append('function=%r' % self.function)
            if self.args:
                args.append('args=%r' % self.args)

            if self.kargs:
                args.append('kargs=%r' % self.kargs)

        else:
            cmdlen = len(self.commands)
            cmds = self.commands[0] if cmdlen == 1 else self.commands
            args.append('command%s=%r' % ('s' if cmdlen-1 else '', cmds))

        return '<Bind(%s)>' % ', '.join(args)


def bind(uzbl, glob, handler, *args, **kargs):
    '''Add a bind handler object.'''

    # Mods come from the keycmd sorted so make sure the modkeys in the bind
    # command are sorted too.
    glob = sort_mods(glob)

    del_bind_by_glob(uzbl, glob)
    binds = get_binds(uzbl)

    bind = Bind(glob, handler, *args, **kargs)
    binds.append(bind)

    print bind
    uzbl.event('ADDED_BIND', bind)


def parse_bind_event(uzbl, args):
    '''Break "event BIND fl* = js follownums.js" into (glob, command).'''

    split = map(str.strip, args.split('=', 1))
    if len(split) != 2:
        return error('missing "=" in bind definition: %r' % args)

    glob, command = split
    bind(uzbl, glob, command)


def set_stack_mode(uzbl, prompt):
    if uzbl.get_mode() != 'stack':
        uzbl.set_mode('stack')

    if prompt:
        prompt = "%s: " % prompt

    uzbl.set('keycmd_prompt', prompt)


def clear_stack(uzbl, mode):
    bind_dict = get_bind_dict(uzbl)
    if mode != "stack" and bind_dict['last_mode'] == "stack":
        uzbl.set('keycmd_prompt', '')

    if mode != "stack":
        bind_dict = get_bind_dict(uzbl)
        bind_dict['filter'] = []
        bind_dict['depth'] = 0
        bind_dict['args'] = []

    bind_dict['last_mode'] = mode


def filter_bind(uzbl, bind_dict, bind):
    '''Remove a bind from the stack filter list.'''

    if bind in bind_dict['filter']:
        bind_dict['filter'].remove(bind)

        if not bind_dict['filter']:
            uzbl.set_mode()


def match_and_exec(uzbl, bind, depth, keycmd):
    bind_dict = get_bind_dict(uzbl)
    mode_cmd, on_exec, has_args, glob = bind.stack[depth]

    if has_args:
        if not keycmd.startswith(glob):
            filter_bind(uzbl, bind_dict, bind)
            return False

        args = [keycmd[len(glob):],]

    elif keycmd != glob:
        filter_bind(uzbl, bind_dict, bind)
        return False

    else:
        args = []

    execindex = len(bind.stack)-1
    if execindex == depth == 0:
        uzbl.exec_handler(bind, *args)
        if not has_args:
            uzbl.clear_keycmd()

        return True

    elif depth != execindex:
        if bind_dict['depth'] == depth:
            bind_dict['filter'] = [bind,]
            bind_dict['args'] += args
            bind_dict['depth'] = depth + 1

        else:
            if bind not in bind_dict['filter']:
                bind_dict['filter'].append(bind)

        set_stack_mode(uzbl, bind.prompts[depth])
        return False

    args = bind_dict['args'] + args
    uzbl.exec_handler(bind, *args)
    if on_exec:
        uzbl.set_mode()

    return True


def keycmd_update(uzbl, keylet):
    depth = get_stack_depth(uzbl)
    keycmd = keylet.to_string()
    for bind in get_filtered_binds(uzbl):
        t = bind.stack[depth]
        if t[MOD_CMD] or t[ON_EXEC]:
            continue

        match_and_exec(uzbl, bind, depth, keycmd)


def keycmd_exec(uzbl, keylet):
    depth = get_stack_depth(uzbl)
    keycmd = keylet.to_string()
    for bind in get_filtered_binds(uzbl):
        t = bind.stack[depth]
        if t[MOD_CMD] or not t[ON_EXEC]:
            continue

        match_and_exec(uzbl, bind, depth, keycmd)


def modcmd_update(uzbl, keylet):
    depth = get_stack_depth(uzbl)
    keycmd = keylet.to_string()
    for bind in get_filtered_binds(uzbl):
        t = bind.stack[depth]
        if not t[MOD_CMD] or t[ON_EXEC]:
            continue

        match_and_exec(uzbl, bind, depth, keycmd)


def modcmd_exec(uzbl, keylet):
    depth = get_stack_depth(uzbl)
    keycmd = keylet.to_string()
    for bind in get_filtered_binds(uzbl):
        t = bind.stack[depth]
        if not t[MOD_CMD] or not t[ON_EXEC]:
            continue

        match_and_exec(uzbl, bind, depth, keycmd)


def init(uzbl):
    connects = {'BIND': parse_bind_event,
      'KEYCMD_UPDATE': keycmd_update,
      'MODCMD_UPDATE': modcmd_update,
      'KEYCMD_EXEC': keycmd_exec,
      'MODCMD_EXEC': modcmd_exec,
      'MODE_CHANGED': clear_stack}

    for (event, handler) in connects.items():
        uzbl.connect(event, handler)
