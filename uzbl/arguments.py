'''
Arguments parser

provides argument parsing for event handlers
'''

import re

sglquote = re.compile("^'")
dblquote = re.compile('^"')
exp = re.compile('^(@[({<*/-]|[)}>*/-]@)')
space = re.compile('^\\s+')
escape = re.compile('^\\\\.')
str = re.compile('^[^\'"\\s@<>\\\\]+')
special = re.compile('^[@<>]')

patterns = (sglquote, dblquote, exp, space, escape, str, special)


def match(s):
    for p in patterns:
        m = p.search(s)
        if m:
            return p, m


def lex(s):
    i, l = 0, len(s)
    while i < l:
        p, m = match(s[i:])
        i += m.end()
        yield p, m.group(0)


def parse(l):
    raw = []
    ref = []
    args = []
    s = ''
    close = None
    start = None
    for i, (p, t) in enumerate(l):
        raw.append(t)
        if close:
            if p is close:
                if p is exp:
                    s += t
                close = None
            elif p is escape:
                s += t[1:]
            else:
                s += t
        else:
            if p is space:
                if start is not None:
                    ref.append(start)
                    args.append(s)
                s = ''
                start = None
                continue
            elif start is None:
                start = i

            if p in (sglquote, dblquote):
                close = p
            elif p is exp:
                close = p
                s += t
            elif p is escape:
                s += t[1:]
            else:
                s += t
    if s:
        ref.append(start)
        args.append(s)
    return args, raw, ref


class Arguments(tuple):
    '''
    Given a argument line gives access to the split parts
    honoring common quotation and escaping rules

    >>> Arguments(r"simple 'quoted string'")
    ('simple', 'quoted string')
    '''

    def __new__(cls, s):
        '''
        >>> Arguments(r"one two three")
        ('one', 'two', 'three')
        >>> Arguments(r"spam 'escaping \\'works\\''")
        ('spam', "escaping 'works'")
        >>> # For testing purposes we can pass a preparsed tuple
        >>> Arguments(('foo', 'bar', 'baz az'))
        ('foo', 'bar', 'baz az')
        '''
        if isinstance(s, tuple):
            self = tuple.__new__(cls, s)
            self._raw, self._ref = s, list(range(len(s)))
            return self

        args, raw, ref = parse(lex(s))
        self = tuple.__new__(cls, args)
        self._raw, self._ref = raw, ref
        return self

    def raw(self, frm=0, to=None):
        '''
        Returns the portion of the raw input that yielded arguments
        from 'frm' to 'to'

        >>> args = Arguments(r"'spam, spam' egg sausage   and 'spam'")
        >>> args
        ('spam, spam', 'egg', 'sausage', 'and', 'spam')
        >>> args.raw(1)
        "egg sausage   and 'spam'"
        '''
        if len(self._ref) < 1:
            return ''
        rfrm = self._ref[frm]
        if to is None or len(self._ref) <= to + 1:
            rto = len(self._raw)
        else:
            rto = self._ref[to + 1] - 1
        return ''.join(self._raw[rfrm:rto])

    def safe_raw(self, frm=0, to=None):
        '''
        Returns the portion of the raw input that yielded arguments
        from 'frm' to 'to'

        >>> args = Arguments(r"'spam, spam' egg sausage   and 'spam'")
        >>> args
        ('spam, spam', 'egg', 'sausage', 'and', 'spam')
        >>> args.raw(1)
        "egg sausage   and 'spam'"
        '''
        return self.raw(frm, to).replace('@', '\\@')

splitquoted = Arguments  # or define a function?


def is_quoted(s):
    return s and s[0] == s[-1] and s[0] in "'\""


Unescape = re.compile('\\\\(.)')


def unquote(s):
    '''
        Returns the input string without quotations and with
        escape sequences interpreted
    '''

    if is_quoted(s):
        s = s[1:-1]
    return Unescape.sub('\\1', s)
