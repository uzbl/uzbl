'''
Arguments paser

provides argument parsing for event handlers
'''

import re

class Arguments(list):
    _splitquoted = re.compile("( +|\"(?:\\\\.|[^\"])*?\"|'(?:\\\\.|[^'])*?')")

    def __init__(self, s):
        self._raw = self._splitquoted.split(s)
        self._ref = []
        self[:] = self.parse()

    def parse(self):
        c = None
        for i, part in enumerate(self._raw):
            if re.match(' +', part):
                if c is not None:
                    yield c
                    c = None
            else:
                f = unquote(part)
                if c == None:
                    if part != '':
                        self._ref.append(i)
                        c = f
                else:
                    c += f
        yield c

    def raw(self, frm=0, to=None):
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
