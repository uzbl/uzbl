import random

from .on_set import OnSetPlugin
from .keycmd import KeyCmd
from uzbl.ext import GlobalPlugin, PerInstancePlugin

class SharedHistory(GlobalPlugin):

    def __init__(self, event_manager):
        super(SharedHistory, self).__init__(event_manager)
        self.history = {}  #TODO(tailhook) save and load from file

    def get_line_number(self, prompt):
        try:
            return len(self.history[prompt])
        except KeyError:
            return 0

    def addline(self, prompt, entry):
        lst = self.history.get(prompt)
        if lst is None:
            self.history[prompt] = [entry]
        else:
            lst.append(entry)

    def getline(self, prompt, index):
        try:
            return self.history[prompt][index]
        except KeyError:
            # not existent list is same as empty one
            raise IndexError()


class History(PerInstancePlugin):

    def __init__(self, uzbl):
        super(History, self).__init__(uzbl)
        self._tail = ''
        self.prompt = ''
        self.cursor = None
        self.search_key = None
        uzbl.connect('KEYCMD_EXEC', self.keycmd_exec)
        uzbl.connect('HISTORY_PREV', self.history_prev)
        uzbl.connect('HISTORY_NEXT', self.history_next)
        uzbl.connect('HISTORY_SEARCH', self.history_search)
        OnSetPlugin[uzbl].on_set('keycmd_prompt',
            lambda uzbl, k, v: self.change_prompt(v))

    def prev(self):
        shared = SharedHistory[self.uzbl]
        if self.cursor is None:
            self.cursor = shared.get_line_number(self.prompt) - 1
        else:
            self.cursor -= 1

        if self.search_key:
            while self.cursor >= 0:
                line = shared.getline(self.prompt, self.cursor)
                if self.search_key in line:
                    return line
                self.cursor -= 1

        if self.cursor >= 0:
            return shared.getline(self.prompt, self.cursor)

        self.cursor = -1
        return random.choice(end_messages)

    def __next__(self):
        if self.cursor is None:
            return ''
        shared = SharedHistory[self.uzbl]

        self.cursor += 1

        num = shared.get_line_number(self.prompt)
        if self.search_key:
            while self.cursor < num:
                line = shared.getline(self.prompt, self.cursor)
                if self.search_key in line:
                    return line
                self.cursor += 1

        if self.cursor >= num:
            self.cursor = None
            self.search_key = None
            if self._tail:
                value = self._tail
                self._tail = None
                return value
            return ''
        return shared.getline(self.prompt, self.cursor)

    # Python2 shenanigans
    next = __next__

    def change_prompt(self, prompt):
        self.prompt = prompt
        self._tail = None

    def search(self, key):
        self.search_key = key
        self.cursor = None

    def __str__(self):
        return "(History %s, %s)" % (self.cursor, self.prompt)

    def keycmd_exec(self, modstate, keylet):
        cmd = keylet.get_keycmd()
        if cmd:
            SharedHistory[self.uzbl].addline(self.prompt, cmd)
        self._tail = None
        self.cursor = None
        self.search_key = None

    def history_prev(self, _x):
        cmd = KeyCmd[self.uzbl].keylet.get_keycmd()
        if self.cursor is None and cmd:
            self._tail = cmd
        val = self.prev()
        KeyCmd[self.uzbl].set_keycmd(val)

    def history_next(self, _x):
        KeyCmd[self.uzbl].set_keycmd(next(self))

    def history_search(self, key):
        self.search(key)
        self.uzbl.send('event HISTORY_PREV')

end_messages = (
    'Look behind you, A three-headed monkey!',
    'error #4: static from nylon underwear.',
    'error #5: static from plastic slide rules.',
    'error #6: global warming.',
    'error #9: doppler effect.',
    'error #16: somebody was calculating pi on the server.',
    'error #19: floating point processor overflow.',
    'error #21: POSIX compliance problem.',
    'error #25: Decreasing electron flux.',
    'error #26: first Saturday after first full moon in Winter.',
    'error #64: CPU needs recalibration.',
    'error #116: the real ttys became pseudo ttys and vice-versa.',
    'error #229: wrong polarity of neutron flow.',
    'error #330: quantum decoherence.',
    'error #388: Bad user karma.',
    'error #407: Route flapping at the NAP.',
    'error #435: Internet shut down due to maintenance.',
    )


# vi: set et ts=4:
