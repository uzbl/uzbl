import re

SIMPLE = re.compile('[a-zA-Z0-9._]+$')
UZBL_EXPAND = re.compile('(@[({<*/-].*[)}>*/-]@)')
ARG_EXPAND = re.compile('%[sr0-9]')


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


def format_arg(a, args):
    v = cmd_expand(a, args)
    if SIMPLE.match(a) or UZBL_EXPAND.match(a) or ARG_EXPAND.match(a):
        return v
    # Uzbl expands are evaluated before quotes so don't escape them
    return '"%s"' % ''.join(
        p if p.startswith('@') else escape(p)
        for p in UZBL_EXPAND.split(v)
    )


def send_user_command(uzbl, cmd, args):
    if cmd[0] == 'event':
        has_var = any('@' in x for x in cmd)
        event = cmd[1]
        args = ' '.join(format_arg(c, args) for c in cmd[2:])
        if not has_var:
            # Bypass the roundtrip to uzbl and dispatch immediately
            uzbl.event(event, args)
        else:
            uzbl.send(' '.join(('event', event, args)))
    else:
        cmd = ' '.join((cmd_expand(cmd[0], args),) +
                       tuple(format_arg(c, args) for c in cmd[1:]))
        uzbl.send(cmd)
