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

# Commonly used regular expressions.
MOD_START = re.compile('^<([A-Z][A-Za-z0-9-_]*)>').match
# Matches <x:y>, <'x':y>, <:'y'>, <x!y>, <'x'!y>, ...
PROMPTS = '<(\"[^\"]*\"|\'[^\']*\'|[^:!>]*)(:|!)(\"[^\"]*\"|\'[^\']*\'|[^>]*)>'
FIND_PROMPTS = re.compile(PROMPTS).split
VALID_MODE = re.compile('^(-|)[A-Za-z0-9][A-Za-z0-9_]*$').match

# For accessing a bind glob stack.
ON_EXEC, HAS_ARGS, MOD_CMD, GLOB, MORE = range(5)


# Custom errors.
class ArgumentError(Exception): pass


class Bindlet(object):
    '''Per-instance bind status/state tracker.'''

    def __init__(self, uzbl):
        self.binds = {'global': {}}
        self.uzbl = uzbl
        self.depth = 0
        self.args = []
        self.last_mode = None
        self.after_cmds = None
        self.stack_binds = []

        # A subset of the global mode binds containing non-stack and modkey
        # activiated binds for use in the stack mode.
        self.globals = []


    def __getitem__(self, key):
        return self.get_binds(key)


    def reset(self):
        '''Reset the tracker state and return to last mode.'''

        self.depth = 0
        self.args = []
        self.after_cmds = None
        self.stack_binds = []

        if self.last_mode:
            mode, self.last_mode = self.last_mode, None
            self.uzbl.config['mode'] = mode

        del self.uzbl.config['keycmd_prompt']


    def stack(self, bind, args, depth):
        '''Enter or add new bind in the next stack level.'''

        if self.depth != depth:
            if bind not in self.stack_binds:
                self.stack_binds.append(bind)

            return

        mode = self.uzbl.config.get('mode', None)
        if mode != 'stack':
            self.last_mode = mode
            self.uzbl.config['mode'] = 'stack'

        self.stack_binds = [bind,]
        self.args += args
        self.depth += 1
        self.after_cmds = bind.prompts[depth]


    def after(self):
        '''If a stack was triggered then set the prompt and default value.'''

        if self.after_cmds is None:
            return

        (prompt, is_cmd, set), self.after_cmds = self.after_cmds, None

        self.uzbl.clear_keycmd()
        if prompt:
            self.uzbl.config['keycmd_prompt'] = prompt

        if set and is_cmd:
            self.uzbl.send(set)

        elif set and not is_cmd:
            self.uzbl.send('event SET_KEYCMD %s' % set)


    def get_binds(self, mode=None):
        '''Return the mode binds + globals. If we are stacked then return
        the filtered stack list and modkey & non-stack globals.'''

        if mode is None:
            mode = self.uzbl.config.get('mode', None)

        if not mode:
            mode = 'global'

        if self.depth:
            return self.stack_binds + self.globals

        globals = self.binds['global']
        if mode not in self.binds or mode == 'global':
            return filter(None, globals.values())

        binds = dict(globals.items() + self.binds[mode].items())
        return filter(None, binds.values())


    def add_bind(self, mode, glob, bind=None):
        '''Insert (or override) a bind into the mode bind dict.'''

        if mode not in self.binds:
            self.binds[mode] = {glob: bind}
            return

        binds = self.binds[mode]
        binds[glob] = bind

        if mode == 'global':
            # Regen the global-globals list.
            self.globals = []
            for bind in binds.values():
                if bind is not None and bind.is_global:
                    self.globals.append(bind)


def ismodbind(glob):
    '''Return True if the glob specifies a modbind.'''

    return bool(MOD_START(glob))


def split_glob(glob):
    '''Take a string of the form "<Mod1><Mod2>cmd _" and return a list of the
    modkeys in the glob and the command.'''

    mods = set()
    while True:
        match = MOD_START(glob)
        if not match:
            break

        end = match.span()[1]
        mods.add(glob[:end])
        glob = glob[end:]

    return (mods, glob)


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

        self.split = split = FIND_PROMPTS(glob)
        self.prompts = []
        for (prompt, cmd, set) in zip(split[1::4], split[2::4], split[3::4]):
            prompt, set = map(unquote, [prompt, set])
            cmd = True if cmd == '!' else False
            if prompt and prompt[-1] != ":":
                prompt = "%s:" % prompt

            self.prompts.append((prompt, cmd, set))

        # Check that there is nothing like: fl*<int:>*
        for glob in split[:-1:4]:
            if glob.endswith('*'):
                msg = "token '*' not at the end of a prompt bind: %r" % split
                raise SyntaxError(msg)

        # Check that there is nothing like: fl<prompt1:><prompt2:>_
        for glob in split[4::4]:
            if not glob:
                msg = 'found null segment after first prompt: %r' % split
                raise SyntaxError(msg)

        stack = []
        for (index, glob) in enumerate(reversed(split[::4])):
            # Is the binding a MODCMD or KEYCMD:
            mod_cmd = ismodbind(glob)

            # Do we execute on UPDATES or EXEC events?
            on_exec = True if glob[-1] in ['!', '_'] else False

            # Does the command take arguments?
            has_args = True if glob[-1] in ['*', '_'] else False

            glob = glob[:-1] if has_args or on_exec else glob
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
    cmd_expand = uzbl.cmd_expand
    for cmd in bind.commands:
        cmd = cmd_expand(cmd, args)
        uzbl.send(cmd)


def mode_bind(uzbl, modes, glob, handler=None, *args, **kargs):
    '''Add a mode bind.'''

    bindlet = uzbl.bindlet

    if not hasattr(modes, '__iter__'):
        modes = unicode(modes).split(',')

    # Sort and filter binds.
    modes = filter(None, map(unicode.strip, modes))

    if callable(handler) or (handler is not None and handler.strip()):
        bind = Bind(glob, handler, *args, **kargs)

    else:
        bind = None

    for mode in modes:
        if not VALID_MODE(mode):
            raise NameError('invalid mode name: %r' % mode)

    for mode in modes:
        if mode[0] == '-':
            mode, bind = mode[1:], None

        bindlet.add_bind(mode, glob, bind)
        uzbl.event('ADDED_MODE_BIND', mode, glob, bind)


def bind(uzbl, glob, handler, *args, **kargs):
    '''Legacy bind function.'''

    mode_bind(uzbl, 'global', glob, handler, *args, **kargs)


def parse_mode_bind(uzbl, args):
    '''Parser for the MODE_BIND event.

    Example events:
        MODE_BIND <mode>         <bind>        = <command>
        MODE_BIND command        o<location:>_ = uri %s
        MODE_BIND insert,command <BackSpace>   = ...
        MODE_BIND global         ...           = ...
        MODE_BIND global,-insert ...           = ...
    '''

    if not args:
        raise ArgumentError('missing bind arguments')

    split = map(unicode.strip, args.split(' ', 1))
    if len(split) != 2:
        raise ArgumentError('missing mode or bind section: %r' % args)

    modes, args = split[0].split(','), split[1]
    split = map(unicode.strip, args.split('=', 1))
    if len(split) != 2:
        raise ArgumentError('missing delimiter in bind section: %r' % args)

    glob, command = split
    mode_bind(uzbl, modes, glob, command)


def parse_bind(uzbl, args):
    '''Legacy parsing of the BIND event and conversion to the new format.

    Example events:
        request BIND <bind>        = <command>
        request BIND o<location:>_ = uri %s
        request BIND <BackSpace>   = ...
        request BIND ...           = ...
    '''

    parse_mode_bind(uzbl, "global %s" % args)


def mode_changed(uzbl, mode):
    '''Clear the stack on all non-stack mode changes.'''

    if mode != 'stack':
        uzbl.bindlet.reset()


def match_and_exec(uzbl, bind, depth, modstate, keylet, bindlet):
    (on_exec, has_args, mod_cmd, glob, more) = bind[depth]
    cmd = keylet.modcmd if mod_cmd else keylet.keycmd

    if mod_cmd and modstate != mod_cmd:
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
        bindlet.stack(bind, args, depth)
        (on_exec, has_args, mod_cmd, glob, more) = bind[depth+1]
        if not on_exec and has_args and not glob and not more:
            exec_bind(uzbl, bind, *(args+['',]))

        return False

    args = bindlet.args + args
    exec_bind(uzbl, bind, *args)
    if not has_args or on_exec:
        del uzbl.config['mode']
        bindlet.reset()

    return True


def key_event(uzbl, modstate, keylet, mod_cmd=False, on_exec=False):
    bindlet = uzbl.bindlet
    depth = bindlet.depth
    for bind in bindlet.get_binds():
        t = bind[depth]
        if (bool(t[MOD_CMD]) != mod_cmd) or (t[ON_EXEC] != on_exec):
            continue

        if match_and_exec(uzbl, bind, depth, modstate, keylet, bindlet):
            return

    bindlet.after()

    # Return to the previous mode if the KEYCMD_EXEC keycmd doesn't match any
    # binds in the stack mode.
    if on_exec and not mod_cmd and depth and depth == bindlet.depth:
        del uzbl.config['mode']


# plugin init hook
def init(uzbl):
    '''Export functions and connect handlers to events.'''

    connect_dict(uzbl, {
        'BIND':             parse_bind,
        'MODE_BIND':        parse_mode_bind,
        'MODE_CHANGED':     mode_changed,
    })

    # Connect key related events to the key_event function.
    events = [['KEYCMD_UPDATE', 'KEYCMD_EXEC'],
              ['MODCMD_UPDATE', 'MODCMD_EXEC']]

    for mod_cmd in range(2):
        for on_exec in range(2):
            event = events[mod_cmd][on_exec]
            connect(uzbl, event, key_event, bool(mod_cmd), bool(on_exec))

    export_dict(uzbl, {
        'bind':         bind,
        'mode_bind':    mode_bind,
        'bindlet':      Bindlet(uzbl),
    })

# vi: set et ts=4:
