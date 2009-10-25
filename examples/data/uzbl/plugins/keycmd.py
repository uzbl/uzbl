import re

# Map these functions/variables in the plugins namespace to the uzbl object.
__export__ = ['clear_keycmd', 'set_keycmd', 'set_cursor_pos', 'get_keylet',
    'clear_current', 'clear_modcmd', 'add_modmap']

# Hold the keylets.
UZBLS = {}

# Keycmd format which includes the markup for the cursor.
KEYCMD_FORMAT = "%s<span @cursor_style>%s</span>%s"


def uzbl_escape(str):
    '''Prevent outgoing keycmd values from expanding inside the
    status_format.'''

    if not str:
        return ''

    for char in ['\\', '@']:
        if char in str:
            str = str.replace(char, '\\'+char)

    return "@[%s]@" % str


class Keylet(object):
    '''Small per-instance object that tracks all the keys held and characters
    typed.'''

    def __init__(self):
        # Modcmd tracking
        self.held = []
        self.modcmd = ''
        self.is_modcmd = False

        # Keycmd tracking
        self.keycmd = ''
        self.cursor = 0

        # Key modmaps.
        self.modmap = {}

        # Keylet string repr cache.
        self._repr_cache = None


    def get_keycmd(self):
        '''Get the keycmd-part of the keylet.'''

        return self.keycmd


    def get_modcmd(self):
        '''Get the modcmd-part of the keylet.'''

        if not self.is_modcmd:
            return ''

        return ''.join(['<%s>' % key for key in self.held]) + self.modcmd


    def key_modmap(self, key):
        '''Make some obscure names for some keys friendlier.'''

        if key in self.modmap:
            return self.modmap[key]

        elif key.endswith('_L') or key.endswith('_R'):
            # Remove left-right discrimination and try again.
            return self.key_modmap(key[:-2])

        else:
            return key


    def __repr__(self):
        '''Return a string representation of the keylet.'''

        if self._repr_cache:
            return self._repr_cache

        l = []
        if self.is_modcmd:
            l.append('modcmd=%r' % self.get_modcmd())

        elif self.held:
            l.append('held=%r' % ''.join(['<%s>'%key for key in self.held]))

        if self.keycmd:
            l.append('keycmd=%r' % self.get_keycmd())

        self._repr_cache = '<Keylet(%s)>' % ', '.join(l)
        return self._repr_cache


def add_modmap(uzbl, key, map=None):
    '''Add modmaps.'''

    keylet = get_keylet(uzbl)
    if not map:
        if key in keylet.modmap:
            map = keylet.modmap[key]
            del keylet.modmap[key]
            uzbl.event("DEL_MODMAP", key, map)

    else:
        keylet.modmap[key] = map
        uzbl.event("NEW_MODMAP", key, map)


def modmap_parse(uzbl, map):
    '''Parse a modmap definiton.'''

    split = [s.strip() for s in map.split(' ') if s.split()]

    if not split or len(split) > 2:
        raise Exception('Invalid modmap arugments: %r' % map)

    add_modmap(uzbl, *split)


def add_instance(uzbl, *args):
    '''Create the Keylet object for this uzbl instance.'''

    UZBLS[uzbl] = Keylet()


def del_instance(uzbl, *args):
    '''Delete the Keylet object for this uzbl instance.'''

    if uzbl in UZBLS:
        del UZBLS[uzbl]


def get_keylet(uzbl):
    '''Return the corresponding keylet for this uzbl instance.'''

    # Startup events are not correctly captured and sent over the uzbl socket
    # yet so this line is needed because the INSTANCE_START event is lost.
    if uzbl not in UZBLS:
        add_instance(uzbl)

    keylet = UZBLS[uzbl]
    keylet._repr_cache = False
    return keylet


def clear_keycmd(uzbl):
    '''Clear the keycmd for this uzbl instance.'''

    k = get_keylet(uzbl)
    k.keycmd = ''
    k.cursor = 0
    k._repr_cache = False
    config = uzbl.get_config()
    if 'keycmd' not in config or config['keycmd'] != '':
        uzbl.set('keycmd', '')
        uzbl.send('update_gui')

    uzbl.event('KEYCMD_CLEAR')


def clear_modcmd(uzbl, clear_held=False):
    '''Clear the modcmd for this uzbl instance.'''

    k = get_keylet(uzbl)
    k.modcmd = ''
    k.is_modcmd = False
    k._repr_cache = False
    if clear_held:
        k.held = []

    config = uzbl.get_config()
    if 'modcmd' not in config or config['modcmd'] != '':
        uzbl.set('modcmd', '')
        uzbl.send('update_gui')

    uzbl.event('MODCMD_CLEAR')


def clear_current(uzbl):
    '''Clear the modcmd if is_modcmd else clear keycmd.'''

    k = get_keylet(uzbl)
    if k.is_modcmd:
        clear_modcmd(uzbl)

    else:
        clear_keycmd(uzbl)


def focus_changed(uzbl, *args):
    '''Focus to the uzbl instance has now been lost which means all currently
    held keys in the held list will not get a KEY_RELEASE event so clear the
    entire held list.'''

    clear_modcmd(uzbl, clear_held=True)


def update_event(uzbl, k, execute=True):
    '''Raise keycmd & modcmd update events.'''

    config = uzbl.get_config()
    keycmd, modcmd = k.get_keycmd(), k.get_modcmd()

    if k.is_modcmd:
        uzbl.event('MODCMD_UPDATE', k)

    else:
        uzbl.event('KEYCMD_UPDATE', k)

    if 'modcmd_updates' not in config or config['modcmd_updates'] == '1':
        new_modcmd = k.get_modcmd()
        if not new_modcmd or new_modcmd == modcmd:
            uzbl.set('modcmd', uzbl_escape(new_modcmd))

    if 'keycmd_events' in config and config['keycmd_events'] != '1':
        return uzbl.send('update_gui')

    new_keycmd = k.get_keycmd()
    if not new_keycmd or new_keycmd != keycmd:
        uzbl.set('keycmd', '')
        return uzbl.send('update_gui')


    # Generate the pango markup for the cursor in the keycmd.
    curchar = keycmd[k.cursor] if k.cursor < len(keycmd) else ' '
    chunks = [keycmd[:k.cursor], curchar, keycmd[k.cursor+1:]]
    uzbl.set('keycmd', KEYCMD_FORMAT % tuple(map(uzbl_escape, chunks)))
    uzbl.send('update_gui')


def inject_char(str, index, char):
    '''Inject character into string at at given index.'''

    assert len(char) == 1
    return "%s%s%s" % (str[:index], char, str[index:])


def key_press(uzbl, key):
    '''Handle KEY_PRESS events. Things done by this function include:

    1. Ignore all shift key presses (shift can be detected by capital chars)
    3. In non-modcmd mode:
         a. append char to keycmd
    4. If not in modcmd mode and a modkey was pressed set modcmd mode.
    5. If in modcmd mode the pressed key is added to the held keys list.
    6. Keycmd is updated and events raised if anything is changed.'''

    if key.startswith('Shift_'):
        return

    k = get_keylet(uzbl)
    key = k.key_modmap(key.strip())
    print 'KEY', key
    if key.startswith("<ISO_"):
        return

    if key == 'Space' and not k.held and k.keycmd:
        k.keycmd = inject_char(k.keycmd, k.cursor, ' ')
        k.cursor += 1

    elif not k.held and len(key) == 1:
        config = uzbl.get_config()
        if 'keycmd_events' not in config or config['keycmd_events'] == '1':
            k.keycmd = inject_char(k.keycmd, k.cursor, key)
            k.cursor += 1

        elif k.keycmd:
            k.keycmd = ''
            k.cursor = 0

    elif len(key) > 1:
        k.is_modcmd = True
        if key == 'Shift-Tab' and 'Tab' in k.held:
            k.held.remove('Tab')

        if key not in k.held:
            k.held.append(key)
            k.held.sort()

    else:
        k.is_modcmd = True
        k.modcmd += key

    update_event(uzbl, k)


def key_release(uzbl, key):
    '''Respond to KEY_RELEASE event. Things done by this function include:

    1. Remove the key from the keylet held list.
    2. If in a mod-command then raise a MODCMD_EXEC.
    3. Check if any modkey is held, if so set modcmd mode.
    4. Update the keycmd uzbl variable if anything changed.'''

    k = get_keylet(uzbl)
    key = k.key_modmap(key)

    if key in ['Shift', 'Tab'] and 'Shift-Tab' in k.held:
        key = 'Shift-Tab'

    elif key in ['Shift', 'Alt'] and 'Meta' in k.held:
        key = 'Meta'

    if key in k.held:
        if k.is_modcmd:
            uzbl.event('MODCMD_EXEC', k)

        k.held.remove(key)
        clear_modcmd(uzbl)


def set_keycmd(uzbl, keycmd):
    '''Allow setting of the keycmd externally.'''

    k = get_keylet(uzbl)
    k.keycmd = keycmd
    k._repr_cache = None
    k.cursor = len(keycmd)
    update_event(uzbl, k, False)


def keycmd_strip_word(uzbl, sep):
    ''' Removes the last word from the keycmd, similar to readline ^W '''

    sep = sep or ' '
    k = get_keylet(uzbl)
    if not k.keycmd:
        return

    head, tail = k.keycmd[:k.cursor].rstrip(sep), k.keycmd[k.cursor:]
    rfind = head.rfind(sep)
    head = head[:rfind] if rfind + 1 else ''
    k.keycmd = head + tail
    k.cursor = len(head)
    update_event(uzbl, k, False)


def keycmd_backspace(uzbl, *args):
    '''Removes the character at the cursor position in the keycmd.'''

    k = get_keylet(uzbl)
    if not k.keycmd:
        return

    k.keycmd = k.keycmd[:k.cursor-1] + k.keycmd[k.cursor:]
    k.cursor -= 1
    update_event(uzbl, k, False)


def keycmd_delete(uzbl, *args):
    '''Removes the character after the cursor position in the keycmd.'''

    k = get_keylet(uzbl)
    if not k.keycmd:
        return

    k.keycmd = k.keycmd[:k.cursor] + k.keycmd[k.cursor+1:]
    update_event(uzbl, k, False)


def keycmd_exec_current(uzbl, *args):
    '''Raise a KEYCMD_EXEC with the current keylet and then clear the
    keycmd.'''

    k = get_keylet(uzbl)
    uzbl.event('KEYCMD_EXEC', k)
    clear_keycmd(uzbl)


def set_cursor_pos(uzbl, index):
    '''Allow setting of the cursor position externally. Supports negative
    indexing and relative stepping with '+' and '-'.'''

    k = get_keylet(uzbl)
    if index == '-':
        cursor = k.cursor - 1

    elif index == '+':
        cursor = k.cursor + 1

    else:
        cursor = int(index.strip())
        if cursor < 0:
            cursor = len(k.keycmd) + cursor + 1

    if cursor < 0:
        cursor = 0

    if cursor > len(k.keycmd):
        cursor = len(k.keycmd)

    k.cursor = cursor
    update_event(uzbl, k, False)


def init(uzbl):
    '''Connect handlers to uzbl events.'''

    connects = {'INSTANCE_START': add_instance,
      'INSTANCE_EXIT': del_instance,
      'KEY_PRESS': key_press,
      'KEY_RELEASE': key_release,
      'SET_KEYCMD': set_keycmd,
      'KEYCMD_STRIP_WORD': keycmd_strip_word,
      'KEYCMD_BACKSPACE': keycmd_backspace,
      'KEYCMD_DELETE': keycmd_delete,
      'KEYCMD_EXEC_CURRENT': keycmd_exec_current,
      'SET_CURSOR_POS': set_cursor_pos,
      'FOCUS_LOST': focus_changed,
      'FOCUS_GAINED': focus_changed,
      'MODMAP': modmap_parse}

    uzbl.connect_dict(connects)
