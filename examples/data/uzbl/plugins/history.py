__export__ = ['get_history']
UZBLS = {}

import random

shared_history = []

class History(object):
    def __init__(self):
        self._temporary = []
        self.cursor = None
        self.__temp_tail = False
        self.search_key = None

    def prev(self):
        if self.cursor is None:
            self.cursor = len(self) - 1
        else:
            self.cursor -= 1

        if self.search_key:
            while self.cursor >= 0 and self.search_key not in self[self.cursor]:
                self.cursor -= 1

        if self.cursor < 0 or len(self) == 0:
            self.cursor = -1
            return random.choice(end_messages)

        return self[self.cursor]

    def next(self):
        if self.cursor is None:
            return ''

        self.cursor += 1

        if self.search_key:
            while self.cursor < len(self) and self.search_key not in self[self.cursor]:
                self.cursor += 1

        if self.cursor >= len(shared_history):
            self.cursor = None
            self.search_key = None

            if self._temporary:
                print 'popping temporary'
                return self._temporary.pop()
            return ''

        return self[self.cursor]

    def search(self, key):
        self.search_key = key
        return self.prev()

    def add(self, cmd):
        if self._temporary:
            self._temporary.pop()

        shared_history.append(cmd)
        self.cursor = None
        self.search_key = None

    def add_temporary(self, cmd):
        assert not self._temporary

        self._temporary.append(cmd)
        self.cursor = len(self) - 1

        print 'adding temporary', self

    def __getitem__(self, i):
        if i < len(shared_history):
            return shared_history[i]
        return self._temporary[i-len(shared_history)+1]

    def __len__(self):
        return len(shared_history) + len(self._temporary)

    def __str__(self):
        return "(History %s)" % (self.cursor)

def get_history(uzbl):
    return UZBLS[uzbl]

def add_instance(uzbl, *args):
    UZBLS[uzbl] = History()

def del_instance(uzbl, *args):
    if uzbl in UZBLS:
        del UZBLS[uzbl]

def keycmd_exec(uzbl, keylet):
    history = get_history(uzbl)
    cmd = keylet.get_keycmd()
    if cmd:
        history.add(cmd)

def history_prev(uzbl, _x):
    history = get_history(uzbl)
    cmd = uzbl.get_keylet().get_keycmd()
    if history.cursor is None and cmd:
        history.add_temporary(cmd)

    uzbl.set_keycmd(history.prev())
    print 'PREV', history

def history_next(uzbl, _x):
    history = get_history(uzbl)
    cmd = uzbl.get_keylet().get_keycmd()

    uzbl.set_keycmd(history.next())
    print 'NEXT', history

def history_search(uzbl, key):
    history = get_history(uzbl)
    uzbl.set_keycmd(history.search(key))
    print 'SEARCH', history

end_messages = ('Look behind you, A three-headed monkey!', 'error #4: static from nylon underwear.', 'error #5: static from plastic slide rules.', 'error #6: global warming.', 'error #9: doppler effect.', 'error #16: somebody was calculating pi on the server.', 'error #19: floating point processor overflow.', 'error #21: POSIX compliance problem.', 'error #25: Decreasing electron flux.', 'error #26: first Saturday after first full moon in Winter.', 'error #64: CPU needs recalibration.', 'error #116: the real ttys became pseudo ttys and vice-versa.', 'error #229: wrong polarity of neutron flow.', 'error #330: quantum decoherence.', 'error #388: Bad user karma.', 'error #407: Route flapping at the NAP.', 'error #435: Internet shut down due to maintenance.')

def init(uzbl):
    connects = {'INSTANCE_START': add_instance,
        'INSTANCE_EXIT': del_instance,
        'KEYCMD_EXEC': keycmd_exec,
        'HISTORY_PREV': history_prev,
        'HISTORY_NEXT': history_next,
        'HISTORY_SEARCH': history_search
    }

    uzbl.connect_dict(connects)
