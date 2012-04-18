'''Keycmd completion.'''

import re

from uzbl.arguments import splitquoted
from uzbl.ext import PerInstancePlugin
from .config import Config
from .keycmd import KeyCmd

# Completion level
NONE, ONCE, LIST, COMPLETE = list(range(4))

# The reverse keyword finding re.
FIND_SEGMENT = re.compile("(\@[\w_]+|set[\s]+[\w_]+|[\w_]+)$").findall


def escape(str):
    return str.replace("@", "\@")


class Completions(set):
    def __init__(self):
        set.__init__(self)
        self.locked = False
        self.level = NONE

    def lock(self):
        self.locked = True

    def unlock(self):
        self.locked = False

    def add_var(self, var):
        self.add('@' + var)


class CompletionListFormatter(object):
    LIST_FORMAT = "<span> %s </span>"
    ITEM_FORMAT = "<span @hint_style>%s</span>%s"

    def format(self, partial, completions):
        p = len(partial)
        completions.sort()
        return self.LIST_FORMAT % ' '.join(
            [self.ITEM_FORMAT % (escape(h[:p]), h[p:]) for h in completions]
        )


class CompletionPlugin(PerInstancePlugin):
    def __init__(self, uzbl):
        '''Export functions and connect handlers to events.'''
        super(CompletionPlugin, self).__init__(uzbl)

        self.completion = Completions()
        self.listformatter = CompletionListFormatter()

        uzbl.connect('BUILTINS', self.add_builtins)
        uzbl.connect('CONFIG_CHANGED', self.add_config_key)
        uzbl.connect('KEYCMD_CLEARED', self.stop_completion)
        uzbl.connect('KEYCMD_EXEC', self.stop_completion)
        uzbl.connect('KEYCMD_UPDATE', self.update_completion_list)
        uzbl.connect('START_COMPLETION', self.start_completion)
        uzbl.connect('STOP_COMPLETION', self.stop_completion)

        uzbl.send('dump_config_as_events')

    def get_incomplete_keyword(self):
        '''Gets the segment of the keycmd leading up to the cursor position and
        uses a regular expression to search backwards finding parially completed
        keywords or @variables. Returns a null string if the correct completion
        conditions aren't met.'''

        keylet = KeyCmd[self.uzbl].keylet
        left_segment = keylet.keycmd[:keylet.cursor]
        partial = (FIND_SEGMENT(left_segment) + ['', ])[0].lstrip()
        if partial.startswith('set '):
            return ('@' + partial[4:].lstrip(), True)

        return (partial, False)

    def stop_completion(self, *args):
        '''Stop command completion and return the level to NONE.'''

        self.completion.level = NONE
        if 'completion_list' in Config[self.uzbl]:
            del Config[self.uzbl]['completion_list']

    def complete_completion(self, partial, hint, set_completion=False):
        '''Inject the remaining porition of the keyword into the keycmd then stop
        the completioning.'''

        if set_completion:
            remainder = "%s = " % hint[len(partial):]

        else:
            remainder = "%s " % hint[len(partial):]

        KeyCmd[self.uzbl].inject_keycmd(remainder)
        self.stop_completion()

    def partial_completion(self, partial, hint):
        '''Inject a common portion of the hints into the keycmd.'''

        remainder = hint[len(partial):]
        KeyCmd[self.uzbl].inject_keycmd(remainder)

    def update_completion_list(self, *args):
        '''Checks if the user still has a partially completed keyword under his
        cursor then update the completion hints list.'''

        partial = self.get_incomplete_keyword()[0]
        if not partial:
            return self.stop_completion()

        if self.completion.level < LIST:
            return

        config = Config[self.uzbl]

        hints = [h for h in self.completion if h.startswith(partial)]
        if not hints:
            del config['completion_list']
            return

        config['completion_list'] = self.listformatter.format(partial, hints)

    def start_completion(self, *args):
        if self.completion.locked:
            return

        (partial, set_completion) = self.get_incomplete_keyword()
        if not partial:
            return self.stop_completion()

        if self.completion.level < COMPLETE:
            self.completion.level += 1

        hints = [h for h in self.completion if h.startswith(partial)]
        if not hints:
            return

        elif len(hints) == 1:
            self.completion.lock()
            self.complete_completion(partial, hints[0], set_completion)
            self.completion.unlock()
            return

        elif partial in hints and completion.level == COMPLETE:
            self.completion.lock()
            self.complete_completion(partial, partial, set_completion)
            self.completion.unlock()
            return

        smalllen, smallest = sorted([(len(h), h) for h in hints])[0]
        common = ''
        for i in range(len(partial), smalllen):
            char, same = smallest[i], True
            for hint in hints:
                if hint[i] != char:
                    same = False
                    break

            if not same:
                break

            common += char

        if common:
            self.completion.lock()
            self.partial_completion(partial, partial + common)
            self.completion.unlock()

        self.update_completion_list()

    def add_builtins(self, builtins):
        '''Pump the space delimited list of builtin commands into the
        builtin list.'''

        builtins = splitquoted(builtins)
        self.completion.update(builtins)

    def add_config_key(self, key, value):
        '''Listen on the CONFIG_CHANGED event and add config keys to the variable
        list for @var<Tab> like expansion support.'''

        self.completion.add_var(key)
