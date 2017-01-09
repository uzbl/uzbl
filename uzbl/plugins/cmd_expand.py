import re

SIMPLE = re.compile('^[a-zA-Z]+$')


def escape(str):
    for (level, char) in [(1, '\\'), (1, "'"), (1, '"'), (1, '@')]:
        str = str.replace(char, (level * '\\') + char)

    return str


def cmd_expand(cmd, args):
    '''Exports a function that provides the following
    expansions in any uzbl command string:

        %s = replace('%s', ' '.join(args))
        %r = replace('%r', "'%s'" % escaped(' '.join(args)))
        %1 = replace('%1', arg[0])
        %2 = replace('%2', arg[1])
        %n = replace('%n', arg[n-1])
    '''

    # Ensure (1) all string representable and (2) correct string encoding.
    args = list(map(str, args))

    # Direct string replace.
    if '%s' in cmd:
        cmd = cmd.replace('%s', ' '.join(args))

    # Escaped and quoted string replace.
    if '%r' in cmd:
        cmd = cmd.replace('%r', "'%s'" % escape(' '.join(args)))

    # Arg index string replace.
    idx_list = list(enumerate(args))
    idx_list.reverse()
    for (index, arg) in idx_list:
        index += 1
        if '%%%d' % index in cmd:
            cmd = cmd.replace('%%%d' % index, str(arg))

    return cmd


def format_arg(a):
    if SIMPLE.match(a):
        return a
    return repr(a)


def send_user_command(uzbl, cmd, args):
    if cmd[0] == 'event':
        has_var = any('@' in x for x in cmd)
        event = cmd[1]
        args = cmd_expand(' '.join(format_arg(c) for c in cmd[2:]), args)
        if not has_var:
            # Bypass the roundtrip to uzbl and dispatch immediately
            uzbl.event(event, args)
        else:
            uzbl.send(' '.join(('event', event, args)))
    else:
        cmd = ' '.join((cmd[0],) + tuple(format_arg(c) for c in cmd[1:]))
        cmd = cmd_expand(cmd, args)
        uzbl.send(cmd)
