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

# Export these functions to uzbl.<name>
__export__ = ['bind', 'del_bind', 'del_bind_by_glob', 'get_binds']

# Hold the bind dicts for each uzbl instance.
UZBLS = {}
DEFAULTS = {'binds': [], 'depth': 0, 'stack': [], 'args': [],
    'last_mode': '', 'after': None}

# Commonly used regular expressions.
starts_with_mod = re.compile('^<([A-Z][A-Za-z0-9-_]*)>')
find_prompts = re.compile('<([^:>]*):(\"[^\"]*\"|\'[^\']*\'|[^>]*)>').split

# For accessing a bind glob stack.
ON_EXEC, HAS_ARGS, MOD_CMD, GLOB, MORE = range(5)


class ArgumentError(Exception):
    pass


def ismodbind(glob):
    '''Return True if the glob specifies a modbind.'''

    return bool(starts_with_mod.match(glob))


def split_glob(glob):
    '''Take a string of the form "<Mod1><Mod2>cmd _" and return a list of the
    modkeys in the glob and the command.'''

    mods = set()
    while True:
        match = starts_with_mod.match(glob)
        if not match:
            break

        end = match.span()[1]
        mods.add(glob[:end])
        glob = glob[end:]

    return (mods, glob)


def add_instance(uzbl, *args):
    UZBLS[uzbl] = dict(DEFAULTS)


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


def get_filtered_binds(uzbl, bd):
    '''Return the bind list for the uzbl instance or return the filtered
    bind list thats on the current stack.'''

    return bd['stack'] if bd['depth'] else bd['binds']


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

    # Class attribute to hold the number of Bind classes created.
    counter = [0,]

    def __init__(self, glob, handler, *args, **kargs):
        self.is_callable = callable(handler)
        self._repr_cache = None

        if not glob:
            raise ArgumentError('glob cannot be blank')

        if self.is_callable:
            self.function = handler
            self.args = args
            self.kargs = kargs

        elif kargs:
            raise ArgumentError('cannot supply kargs for uzbl commands')

        elif hasattr(handler, '__iter__'):
            self.commands = handler

        else:
            self.commands = [handler,] + list(args)

        self.glob = glob

        # Assign unique id.
        self.counter[0] += 1
        self.bid = self.counter[0]

        self.split = split = find_prompts(glob)
        self.prompts = []
        for (prompt, set) in zip(split[1::3], split[2::3]):
            if set and set[0] == set[-1] and set[0] in ['"', "'"]:
                # Remove quotes around set.
                set = set[1:-1]

            self.prompts.append((prompt, set))

        # Check that there is nothing like: fl*<int:>*
        for glob in split[:-1:3]:
            if glob.endswith('*'):
                msg = "token '*' not at the end of a prompt bind: %r" % split
                raise SyntaxError(msg)

        # Check that there is nothing like: fl<prompt1:><prompt2:>_
        for glob in split[3::3]:
            if not glob:
                msg = 'found null segment after first prompt: %r' % split
                raise SyntaxError(msg)

        stack = []
        for (index, glob) in enumerate(reversed(split[::3])):
            # Is the binding a MODCMD or KEYCMD:
            mod_cmd = ismodbind(glob)

            # Execute the command on UPDATES or EXEC's:
            on_exec = True if glob.endswith('_') else False

            # Does the command store arguments:
            has_args = True if glob[-1] in ['*', '_'] else False
            glob = glob[:-1] if has_args else glob

            mods, glob = split_glob(glob)
            stack.append((on_exec, has_args, mods, glob, index))

        self.stack = list(reversed(stack))
        self.is_global = (len(self.stack) == 1 and self.stack[0][MOD_CMD])


    def __getitem__(self, depth):
        '''Get bind info at a depth.'''

        if self.is_global:
            return self.stack[0]

        return self.stack[depth]


    def __repr__(self):
        if self._repr_cache:
            return self._repr_cache

        args = ['glob=%r' % self.glob, 'bid=%d' % self.bid]

        if self.is_callable:
            args.append('function=%r' % self.function)
            if self.args:
                args.append('args=%r' % self.args)

            if self.kargs:
                args.append('kargs=%r' % self.kargs)

        else:
            cmdlen = len(self.commands)
            cmds = self.commands[0] if cmdlen == 1 else self.commands
            args.append('command%s=%r' % ('s' if cmdlen-1 else '', cmds))

        self._repr_cache = '<Bind(%s)>' % ', '.join(args)
        return self._repr_cache


def expand(cmd, args):
    '''Replaces "%s %1 %2 %3..." with "<all args> <arg 0> <arg 1>...".'''

    if '%s' in cmd:
        cmd = cmd.replace('%s', ' '.join(map(unicode, args)))

    for (index, arg) in enumerate(args):
        index += 1
        if '%%%d' % index in cmd:
            cmd = cmd.replace('%%%d' % index, unicode(arg))

    return cmd


def exec_bind(uzbl, bind, *args, **kargs):
    '''Execute bind objects.'''

    uzbl.event("EXEC_BIND", bind, args, kargs)

    if bind.is_callable:
        args += bind.args
        kargs = dict(bind.kargs.items()+kargs.items())
        bind.function(uzbl, *args, **kargs)
        return

    if kargs:
        raise ArgumentError('cannot supply kargs for uzbl commands')

    commands = []
    for cmd in bind.commands:
        cmd = expand(cmd, args)
        uzbl.send(cmd)


def bind(uzbl, glob, handler, *args, **kargs):
    '''Add a bind handler object.'''

    # Mods come from the keycmd sorted so make sure the modkeys in the bind
    # command are sorted too.

    del_bind_by_glob(uzbl, glob)
    binds = get_binds(uzbl)

    bind = Bind(glob, handler, *args, **kargs)
    binds.append(bind)

    uzbl.event('ADDED_BIND', bind)


def parse_bind_event(uzbl, args):
    '''Break "event BIND fl* = js follownums.js" into (glob, command).'''

    if not args:
        raise ArgumentError('missing bind arguments')

    split = map(unicode.strip, args.split('=', 1))
    if len(split) != 2:
        raise ArgumentError('missing delimiter in bind: %r' % args)

    glob, command = split
    bind(uzbl, glob, command)


def mode_changed(uzbl, mode):
    '''Clear the stack on all non-stack mode changes.'''

    if mode != 'stack':
        clear_stack(uzbl)


def clear_stack(uzbl, bd=None):
    '''Clear everything related to stacked binds.'''

    if bd is None:
        bd = get_bind_dict(uzbl)

    bd['stack'] = []
    bd['depth'] = 0
    bd['args'] = []
    bd['after'] = None
    if bd['last_mode']:
        mode, bd['last_mode'] = bd['last_mode'], ''
        uzbl.set_mode(mode)

    uzbl.set('keycmd_prompt')


def stack_bind(uzbl, bind, args, depth, bd):
    '''Increment the stack depth in the bind dict, generate filtered bind
    list for stack mode and set keycmd prompt.'''

    if bd['depth'] != depth:
        if bind not in bd['stack']:
            bd['stack'].append(bind)

        return

    if uzbl.get_mode() != 'stack':
        bd['last_mode'] = uzbl.get_mode()
        uzbl.set_mode('stack')

    globalcmds = [cmd for cmd in bd['binds'] if cmd.is_global]
    bd['stack'] = [bind,] + globalcmds
    bd['args'] += args
    bd['depth'] = depth + 1
    bd['after'] = bind.prompts[depth]


def after_bind(uzbl, bd):
    '''Check if there are afte-actions to perform.'''

    if bd['after'] is None:
        return

    (prompt, set), bd['after'] = bd['after'], None
    if prompt:
        uzbl.set('keycmd_prompt', '%s:' % prompt)

    else:
        uzbl.set('keycmd_prompt')

    if set:
        uzbl.send('event SET_KEYCMD %s' % set)

    else:
        uzbl.clear_keycmd()

    uzbl.send('event BIND_STACK_LEVEL %d' % bd['depth'])


def match_and_exec(uzbl, bind, depth, keylet, bd):

    (on_exec, has_args, mod_cmd, glob, more) = bind[depth]

    held = keylet.held
    cmd = keylet.modcmd if mod_cmd else keylet.keycmd

    if mod_cmd and held != mod_cmd:
        return False

    if has_args:
        if not cmd.startswith(glob):
            return False

        args = [cmd[len(glob):],]

    elif cmd != glob:
        return False

    else:
        args = []

    if bind.is_global or (not more and depth == 0):
        exec_bind(uzbl, bind, *args)
        if not has_args:
            uzbl.clear_current()

        return True

    elif more:
        stack_bind(uzbl, bind, args, depth, bd)
        return False

    args = bd['args'] + args
    exec_bind(uzbl, bind, *args)
    uzbl.set_mode()
    if not has_args:
        clear_stack(uzbl, bd)
        uzbl.clear_current()

    return True


def keycmd_update(uzbl, keylet):
    bd = get_bind_dict(uzbl)
    depth = bd['depth']
    for bind in get_filtered_binds(uzbl, bd):
        t = bind[depth]
        if t[MOD_CMD] or t[ON_EXEC]:
            continue

        if match_and_exec(uzbl, bind, depth, keylet, bd):
            return

    after_bind(uzbl, bd)


def keycmd_exec(uzbl, keylet):
    bd = get_bind_dict(uzbl)
    depth = bd['depth']
    for bind in get_filtered_binds(uzbl, bd):
        t = bind[depth]
        if t[MOD_CMD] or not t[ON_EXEC]:
            continue

        if match_and_exec(uzbl, bind, depth, keylet, bd):
            return uzbl.clear_keycmd()

    after_bind(uzbl, bd)


def modcmd_update(uzbl, keylet):
    bd = get_bind_dict(uzbl)
    depth = bd['depth']
    for bind in get_filtered_binds(uzbl, bd):
        t = bind[depth]
        if not t[MOD_CMD] or t[ON_EXEC]:
            continue

        if match_and_exec(uzbl, bind, depth, keylet, bd):
            return

    after_bind(uzbl, bd)


def modcmd_exec(uzbl, keylet):
    bd = get_bind_dict(uzbl)
    depth = bd['depth']
    for bind in get_filtered_binds(uzbl, bd):
        t = bind[depth]
        if not t[MOD_CMD] or not t[ON_EXEC]:
            continue

        if match_and_exec(uzbl, bind, depth, keylet, bd):
            return uzbl.clear_modcmd()

    after_bind(uzbl, bd)


def init(uzbl):
    connects = {'BIND': parse_bind_event,
      'KEYCMD_UPDATE': keycmd_update,
      'MODCMD_UPDATE': modcmd_update,
      'KEYCMD_EXEC': keycmd_exec,
      'MODCMD_EXEC': modcmd_exec,
      'MODE_CHANGED': mode_changed}

    uzbl.connect_dict(connects)
