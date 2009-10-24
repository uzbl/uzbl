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
starts_with_mod = re.compile('^<([A-Z][A-Za-z0-9-_]*)>')
find_prompts = re.compile('<([^:>]*):(\"[^>]*\"|)>').split

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
        self.is_callable = iscallable(handler)
        self._repr_cache = None

        if not glob:
            raise ArgumentError('glob cannot be blank')

        if self.is_callable:
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
        self.prompts = zip(split[1::3],[x.strip('"') for x in split[2::3]])

        # Check that there is nothing like: fl*<int:>*
        for glob in split[:-1:3]:
            if glob.endswith('*'):
                msg = "token '*' not at the end of a prompt bind: %r" % split
                raise BindParseError(msg)

        # Check that there is nothing like: fl<prompt1:><prompt2:>_
        for glob in split[3::3]:
            if not glob:
                msg = 'found null segment after first prompt: %r' % split
                raise BindParseError(msg)

        stack = []
        for (index, glob) in enumerate(reversed(split[::3])):
            # Is the binding a MODCMD or KEYCMD:
            mod_cmd = ismodbind(glob)

            # Execute the command on UPDATES or EXEC's:
            on_exec = True if glob.endswith('_') else False

            # Does the command store arguments:
            has_args = True if glob[-1] in ['*', '_'] else False
            glob = glob[:-1] if has_args else glob

            stack.append((mod_cmd, on_exec, has_args, glob, index))

        self.stack = list(reversed(stack))
        self.is_global = len(self.stack) == 1


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
        if '%s' in cmd:
            if len(args) > 1:
                for arg in args:
                    cmd = cmd.replace('%s', arg, 1)

            elif len(args) == 1:
                cmd = cmd.replace('%s', args[0])

        uzbl.send(cmd)


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

    if not args:
        return error('missing bind arguments')

    split = map(unicode.strip, args.split('=', 1))
    if len(split) != 2:
        return error('missing "=" in bind definition: %r' % args)

    glob, command = split
    bind(uzbl, glob, command)


def set_stack_mode(uzbl, prompt):
    prompt,data = prompt
    if uzbl.get_mode() != 'stack':
        uzbl.set_mode('stack')

    if prompt:
        prompt = "%s: " % prompt

    uzbl.set('keycmd_prompt', prompt)

    if data:
		# go through uzbl-core to expand potential @-variables
		uzbl.send('event SET_KEYCMD %s' % data)


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
    (mod_cmd, on_exec, has_args, glob, more) = bind[depth]

    if has_args:
        if not keycmd.startswith(glob):
            if not mod_cmd:
                filter_bind(uzbl, bind_dict, bind)

            return False

        args = [keycmd[len(glob):],]

    elif keycmd != glob:
        if not mod_cmd:
            filter_bind(uzbl, bind_dict, bind)

        return False

    else:
        args = []

    if bind.is_global or (not more and depth == 0):
        exec_bind(uzbl, bind, *args)
        if not has_args:
            uzbl.clear_current()

        return True

    elif more:
        if bind_dict['depth'] == depth:
            globalcmds = [cmd for cmd in bind_dict['binds'] if cmd.is_global]
            bind_dict['filter'] = [bind,] + globalcmds
            bind_dict['args'] += args
            bind_dict['depth'] = depth + 1

        elif bind not in bind_dict['filter']:
            bind_dict['filter'].append(bind)

        set_stack_mode(uzbl, bind.prompts[depth])
        return False

    args = bind_dict['args'] + args
    exec_bind(uzbl, bind, *args)
    uzbl.set_mode()
    if not has_args:
        uzbl.clear_current()

    return True


def keycmd_update(uzbl, keylet):
    depth = get_stack_depth(uzbl)
    keycmd = keylet.get_keycmd()
    for bind in get_filtered_binds(uzbl):
        t = bind[depth]
        if t[MOD_CMD] or t[ON_EXEC]:
            continue

        if match_and_exec(uzbl, bind, depth, keycmd):
            return


def keycmd_exec(uzbl, keylet):
    depth = get_stack_depth(uzbl)
    keycmd = keylet.get_keycmd()
    for bind in get_filtered_binds(uzbl):
        t = bind[depth]
        if t[MOD_CMD] or not t[ON_EXEC]:
            continue

        if match_and_exec(uzbl, bind, depth, keycmd):
            return uzbl.clear_keycmd()


def modcmd_update(uzbl, keylet):
    depth = get_stack_depth(uzbl)
    keycmd = keylet.get_modcmd()
    for bind in get_filtered_binds(uzbl):
        t = bind[depth]
        if not t[MOD_CMD] or t[ON_EXEC]:
            continue

        if match_and_exec(uzbl, bind, depth, keycmd):
            return


def modcmd_exec(uzbl, keylet):
    depth = get_stack_depth(uzbl)
    keycmd = keylet.get_modcmd()
    for bind in get_filtered_binds(uzbl):
        t = bind[depth]
        if not t[MOD_CMD] or not t[ON_EXEC]:
            continue

        if match_and_exec(uzbl, bind, depth, keycmd):
            return uzbl.clear_modcmd()


def init(uzbl):
    connects = {'BIND': parse_bind_event,
      'KEYCMD_UPDATE': keycmd_update,
      'MODCMD_UPDATE': modcmd_update,
      'KEYCMD_EXEC': keycmd_exec,
      'MODCMD_EXEC': modcmd_exec,
      'MODE_CHANGED': clear_stack}

    uzbl.connect_dict(connects)
