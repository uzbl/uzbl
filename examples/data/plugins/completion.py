'''Keycmd completion.'''

import re

# Completion level
NONE, ONCE, LIST, COMPLETE = range(4)

# The reverse keyword finding re.
FIND_SEGMENT = re.compile("(\@[\w_]+|set[\s]+[\w_]+|[\w_]+)$").findall

# Formats
LIST_FORMAT = "<span> %s </span>"
ITEM_FORMAT = "<span @hint_style>%s</span>%s"

def escape(str):
    return str.replace("@", "\@")


def get_incomplete_keyword(uzbl):
    '''Gets the segment of the keycmd leading up to the cursor position and
    uses a regular expression to search backwards finding parially completed
    keywords or @variables. Returns a null string if the correct completion
    conditions aren't met.'''

    keylet = uzbl.keylet
    left_segment = keylet.keycmd[:keylet.cursor]
    partial = (FIND_SEGMENT(left_segment) + ['',])[0].lstrip()
    if partial.startswith('set '):
        return ('@%s' % partial[4:].lstrip(), True)

    return (partial, False)


def stop_completion(uzbl, *args):
    '''Stop command completion and return the level to NONE.'''

    uzbl.completion.level = NONE
    del uzbl.config['completion_list']


def complete_completion(uzbl, partial, hint, set_completion=False):
    '''Inject the remaining porition of the keyword into the keycmd then stop
    the completioning.'''

    if set_completion:
        remainder = "%s = " % hint[len(partial):]

    else:
        remainder = "%s " % hint[len(partial):]

    uzbl.inject_keycmd(remainder)
    stop_completion(uzbl)


def partial_completion(uzbl, partial, hint):
    '''Inject a common portion of the hints into the keycmd.'''

    remainder = hint[len(partial):]
    uzbl.inject_keycmd(remainder)


def update_completion_list(uzbl, *args):
    '''Checks if the user still has a partially completed keyword under his
    cursor then update the completion hints list.'''

    partial = get_incomplete_keyword(uzbl)[0]
    if not partial:
        return stop_completion(uzbl)

    if uzbl.completion.level < LIST:
        return

    hints = filter(lambda h: h.startswith(partial), uzbl.completion)
    if not hints:
        del uzbl.config['completion_list']
        return

    j = len(partial)
    l = [ITEM_FORMAT % (escape(h[:j]), h[j:]) for h in sorted(hints)]
    uzbl.config['completion_list'] = LIST_FORMAT % ' '.join(l)


def start_completion(uzbl, *args):

    comp = uzbl.completion
    if comp.locked:
        return

    (partial, set_completion) = get_incomplete_keyword(uzbl)
    if not partial:
        return stop_completion(uzbl)

    if comp.level < COMPLETE:
        comp.level += 1

    hints = filter(lambda h: h.startswith(partial), comp)
    if not hints:
        return

    elif len(hints) == 1:
        comp.lock()
        complete_completion(uzbl, partial, hints[0], set_completion)
        comp.unlock()
        return

    elif partial in hints and comp.level == COMPLETE:
        comp.lock()
        complete_completion(uzbl, partial, partial, set_completion)
        comp.unlock()
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
        comp.lock()
        partial_completion(uzbl, partial, partial+common)
        comp.unlock()

    update_completion_list(uzbl)


def add_builtins(uzbl, builtins):
    '''Pump the space delimited list of builtin commands into the
    builtin list.'''

    uzbl.completion.update(builtins.split())


def add_config_key(uzbl, key, value):
    '''Listen on the CONFIG_CHANGED event and add config keys to the variable
    list for @var<Tab> like expansion support.'''

    uzbl.completion.add("@%s" % key)


class Completions(set):
    def __init__(self):
        set.__init__(self)
        self.locked = False
        self.level = NONE

    def lock(self):
        self.locked = True

    def unlock(self):
        self.locked = False


def init(uzbl):
    '''Export functions and connect handlers to events.'''

    export_dict(uzbl, {
        'completion':       Completions(),
        'start_completion': start_completion,
    })

    connect_dict(uzbl, {
        'BUILTINS':         add_builtins,
        'CONFIG_CHANGED':   add_config_key,
        'KEYCMD_CLEARED':   stop_completion,
        'KEYCMD_EXEC':      stop_completion,
        'KEYCMD_UPDATE':    update_completion_list,
        'START_COMPLETION': start_completion,
        'STOP_COMPLETION':  stop_completion,
    })

    uzbl.send('dump_config_as_events')
