'''Keycmd completion.'''

# A list of functions this plugin exports to be used via uzbl object.
__export__ = ['start_completion', 'get_completion_dict']

import re

# Holds the per-instance completion dicts.
UZBLS = {}

# Completion level
NONE, ONCE, LIST, COMPLETE = range(4)

# Default instance dict.
DEFAULTS = {'completions': [], 'level': NONE, 'lock': False}

# The reverse keyword finding re.
FIND_SEGMENT = re.compile("(\@[\w_]+|set[\s]+[\w_]+|[\w_]+)$").findall

# Formats
LIST_FORMAT = "<span> %s </span>"
ITEM_FORMAT = "<span @hint_style>%s</span>%s"


def escape(str):
    return str.replace("@", "\@")


def add_instance(uzbl, *args):
    UZBLS[uzbl] = dict(DEFAULTS)

    # Make sure the config keys for all possible completions are known.
    uzbl.send('dump_config_as_events')


def del_instance(uzbl, *args):
    if uzbl in UZBLS:
        del UZBLS[uzbl]


def get_completion_dict(uzbl):
    '''Get data stored for an instance.'''

    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


def get_incomplete_keyword(uzbl):
    '''Gets the segment of the keycmd leading up to the cursor position and
    uses a regular expression to search backwards finding parially completed
    keywords or @variables. Returns a null string if the correct completion
    conditions aren't met.'''

    keylet = uzbl.get_keylet()
    left_segment = keylet.keycmd[:keylet.cursor]
    partial = (FIND_SEGMENT(left_segment) + ['',])[0].lstrip()
    if partial.startswith('set '):
        return ('@%s' % partial[4:].lstrip(), True)

    return (partial, False)


def stop_completion(uzbl, *args):
    '''Stop command completion and return the level to NONE.'''

    d = get_completion_dict(uzbl)
    d['level'] = NONE
    uzbl.set('completion_list')


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

    d = get_completion_dict(uzbl)
    if d['level'] < LIST:
        return

    hints = [h for h in d['completions'] if h.startswith(partial)]
    if not hints:
        return uzbl.set('completion_list')

    j = len(partial)
    l = [ITEM_FORMAT % (escape(h[:j]), h[j:]) for h in sorted(hints)]
    uzbl.set('completion_list', LIST_FORMAT % ' '.join(l))


def start_completion(uzbl, *args):

    d = get_completion_dict(uzbl)
    if d['lock']:
        return

    (partial, set_completion) = get_incomplete_keyword(uzbl)
    if not partial:
        return stop_completion(uzbl)

    if d['level'] < COMPLETE:
        d['level'] += 1

    hints = [h for h in d['completions'] if h.startswith(partial)]
    if not hints:
        return

    elif len(hints) == 1:
        d['lock'] = True
        complete_completion(uzbl, partial, hints[0], set_completion)
        d['lock'] = False
        return

    elif partial in hints and d['level'] == COMPLETE:
        d['lock'] = True
        complete_completion(uzbl, partial, partial, set_completion)
        d['lock'] = False
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
        d['lock'] = True
        partial_completion(uzbl, partial, partial+common)
        d['lock'] = False

    update_completion_list(uzbl)


def add_builtins(uzbl, args):
    '''Pump the space delimited list of builtin commands into the
    builtin list.'''

    completions = get_completion_dict(uzbl)['completions']
    builtins = filter(None, map(unicode.strip, args.split(" ")))
    for builtin in builtins:
        if builtin not in completions:
            completions.append(builtin)


def add_config_key(uzbl, key, value):
    '''Listen on the CONFIG_CHANGED event and add config keys to the variable
    list for @var<Tab> like expansion support.'''

    completions = get_completion_dict(uzbl)['completions']
    key = "@%s" % key
    if key not in completions:
        completions.append(key)


def init(uzbl):
    # Event handling hooks.
    uzbl.connect_dict({
      'BUILTINS':          add_builtins,
      'CONFIG_CHANGED':    add_config_key,
      'INSTANCE_EXIT':     del_instance,
      'INSTANCE_START':    add_instance,
      'KEYCMD_CLEARED':    stop_completion,
      'KEYCMD_EXEC':       stop_completion,
      'KEYCMD_UPDATE':     update_completion_list,
      'START_COMPLETION':  start_completion,
      'STOP_COMPLETION':   stop_completion,
    })

    # Function exports to the uzbl object, `function(uzbl, *args, ..)`
    # becomes `uzbl.function(*args, ..)`.
    uzbl.export_dict({
        'get_completion_dict':  get_completion_dict,
        'start_completion':     start_completion,
    })
