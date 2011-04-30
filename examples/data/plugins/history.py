import random

shared_history = {'':[]}

class History(object):
    def __init__(self, uzbl):
        self.uzbl = uzbl
        self._temporary = []
        self.prompt = ''
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

        if self.cursor >= len(shared_history[self.prompt]):
            self.cursor = None
            self.search_key = None

            if self._temporary:
                return self._temporary.pop()
            return ''

        return self[self.cursor]

    def change_prompt(self, prompt):
        self.prompt = prompt
        self._temporary = []
        self.__temp_tail = False
        if prompt not in shared_history:
            shared_history[prompt] = []

    def search(self, key):
        self.search_key = key
        self.cursor = None

    def add(self, cmd):
        if self._temporary:
            self._temporary.pop()

        shared_history[self.prompt].append(cmd)
        self.cursor = None
        self.search_key = None

    def add_temporary(self, cmd):
        assert not self._temporary

        self._temporary.append(cmd)
        self.cursor = len(self) - 1

    def __getitem__(self, i):
        if i < len(shared_history[self.prompt]):
            return shared_history[self.prompt][i]
        return self._temporary[i-len(shared_history)+1]

    def __len__(self):
        return len(shared_history[self.prompt]) + len(self._temporary)

    def __str__(self):
        return "(History %s, %s)" % (self.cursor, self.prompt)

def keycmd_exec(uzbl, modstate, keylet):
    cmd = keylet.get_keycmd()
    if cmd:
        uzbl.history.add(cmd)

def history_prev(uzbl, _x):
    cmd = uzbl.keylet.get_keycmd()
    if uzbl.history.cursor is None and cmd:
        uzbl.history.add_temporary(cmd)

    uzbl.set_keycmd(uzbl.history.prev())
    uzbl.logger.debug('PREV %s' % uzbl.history)

def history_next(uzbl, _x):
    cmd = uzbl.keylet.get_keycmd()

    uzbl.set_keycmd(uzbl.history.next())
    uzbl.logger.debug('NEXT %s' % uzbl.history)

def history_search(uzbl, key):
    uzbl.history.search(key)
    uzbl.send('event HISTORY_PREV')
    uzbl.logger.debug('SEARCH %s %s' % (key, uzbl.history))

end_messages = ('Look behind you, A three-headed monkey!', 'error #4: static from nylon underwear.', 'error #5: static from plastic slide rules.', 'error #6: global warming.', 'error #9: doppler effect.', 'error #16: somebody was calculating pi on the server.', 'error #19: floating point processor overflow.', 'error #21: POSIX compliance problem.', 'error #25: Decreasing electron flux.', 'error #26: first Saturday after first full moon in Winter.', 'error #64: CPU needs recalibration.', 'error #116: the real ttys became pseudo ttys and vice-versa.', 'error #229: wrong polarity of neutron flow.', 'error #330: quantum decoherence.', 'error #388: Bad user karma.', 'error #407: Route flapping at the NAP.', 'error #435: Internet shut down due to maintenance.')

# plugin init hook
def init(uzbl):
    connect_dict(uzbl, {
        'KEYCMD_EXEC': keycmd_exec,
        'HISTORY_PREV': history_prev,
        'HISTORY_NEXT': history_next,
        'HISTORY_SEARCH': history_search
    })

    export_dict(uzbl, {
        'history' : History(uzbl)
    })

# plugin after hook
def after(uzbl):
    uzbl.on_set('keycmd_prompt', lambda uzbl, k, v: uzbl.history.change_prompt(v))

# vi: set et ts=4:
