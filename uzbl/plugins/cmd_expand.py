def escape(str):
    for (level, char) in [(3, '\\'), (2, "'"), (2, '"'), (1, '@')]:
        str = str.replace(char, (level * '\\') + char)

    return str


def cmd_expand(uzbl, cmd, args):
    '''Exports a function that provides the following
    expansions in any uzbl command string:

        %s = replace('%s', ' '.join(args))
        %r = replace('%r', "'%s'" % escaped(' '.join(args)))
        %1 = replace('%1', arg[0])
        %2 = replace('%2', arg[1])
        %n = replace('%n', arg[n-1])
    '''

    # Ensure (1) all string representable and (2) correct string encoding.
    args = map(unicode, args)

    # Direct string replace.
    if '%s' in cmd:
        cmd = cmd.replace('%s', ' '.join(args))

    # Escaped and quoted string replace.
    if '%r' in cmd:
        cmd = cmd.replace('%r', "'%s'" % escape(' '.join(args)))

    # Arg index string replace.
    for (index, arg) in enumerate(args):
        index += 1
        if '%%%d' % index in cmd:
            cmd = cmd.replace('%%%d' % index, unicode(arg))

    return cmd

# plugin init hook
def init(uzbl):
    export(uzbl, 'cmd_expand', cmd_expand)
