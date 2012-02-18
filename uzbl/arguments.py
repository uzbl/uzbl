'''
Arguments parser

provides argument parsing for event handlers
'''

import re


class Arguments(tuple):
    '''
    Given a argument line gives access to the split parts
    honoring common quotation and escaping rules

    >>> Arguments(r"simple 'quoted string'")
    (u'simple', u'quoted string')
    '''

    _splitquoted = re.compile("(\s+|\"(?:\\\\.|[^\"])*?\"|'(?:\\\\.|[^'])*?')")

    def __new__(cls, s):
        '''
        >>> Arguments(r"one two three")
        (u'one', u'two', u'three')
        >>> Arguments(r"spam 'escaping \\'works\\''")
        (u'spam', u"escaping 'works'")
        >>> # For testing purposes we can pass a preparsed tuple
        >>> Arguments((u'foo', u'bar', u'baz az'))
        (u'foo', u'bar', u'baz az')
        '''
        if isinstance(s, tuple):
            self = tuple.__new__(cls, s)
            self._raw, self._ref = s, range(len(s))
            return self
        raw = cls._splitquoted.split(s)
        ref = []
        self = tuple.__new__(cls, cls.parse(raw, ref))
        self._raw, self._ref = raw, ref
        return self

    @classmethod
    def parse(cls, raw, ref):
        '''
        Generator used to initialise the arguments tuple

        Indexes to where in source list the arguments start will be put in 'ref'
        '''
        c = None
        for i, part in enumerate(raw):
            if re.match('\s+', part):
                # Whitespace ends the current argument, leading ws is ignored
                if c is not None:
                    yield c
                    c = None
            else:
                f = unquote(part)
                if c is None:
                    # Mark the start of the argument in the raw input
                    if part != '':
                        ref.append(i)
                        c = f
                else:
                    c += f
        if c is not None:
            yield c

    def raw(self, frm=0, to=None):
        '''
        Returs the portion of the raw input that yielded arguments
        from 'frm' to 'to'

        >>> args = Arguments(r"'spam, spam' egg sausage   and 'spam'")
        >>> args
        (u'spam, spam', u'egg', u'sausage', u'and', u'spam')
        >>> args.raw(1)
        "egg sausage   and 'spam'"
        '''
        if len(self._ref) < 1:
            return ''
        rfrm = self._ref[frm]
        if to is None or len(self._ref) <= to+1:
            rto = len(self._raw)
        else:
            rto = self._ref[to+1]-1
        return ''.join(self._raw[rfrm:rto])

splitquoted = Arguments  # or define a function?

def unquote(s):
    '''Removes quotation marks around strings if any and interprets
    \\-escape sequences using `string_escape`'''
    if s and s[0] == s[-1] and s[0] in ['"', "'"]:
        s = s[1:-1]
    return s.encode('utf-8').decode('string_escape').decode('utf-8')
