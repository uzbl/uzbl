import re

from uzbl.arguments import splitquoted
from uzbl.ext import PerInstancePlugin
from .config import Config

# Keycmd format which includes the markup for the cursor.
KEYCMD_FORMAT = "%s<span @cursor_style>%s</span>%s"
MODCMD_FORMAT = "<span> %s </span>"


# FIXME, add utility functions to shared module
def escape(str):
    for char in ['\\', '@']:
        str = str.replace(char, '\\'+char)

    return str


def uzbl_escape(str):
    return "@[%s]@" % escape(str) if str else ''


def inject_str(str, index, inj):
    '''Inject a string into string at at given index.'''

    return "%s%s%s" % (str[:index], inj, str[index:])


class Keylet(object):
    '''Small per-instance object that tracks characters typed.'''

    def __init__(self):
        # Modcmd tracking
        self.modcmd = ''
        self.is_modcmd = False

        # Keycmd tracking
        self.keycmd = ''
        self.cursor = 0

        # Move these to plugin instance ?
        self.modmaps = {}
        self.ignores = {}


    def get_keycmd(self):
        '''Get the keycmd-part of the keylet.'''

        return self.keycmd


    def get_modcmd(self):
        '''Get the modcmd-part of the keylet.'''

        if not self.is_modcmd:
            return ''

        return self.modcmd


    def modmap_key(self, key):
        '''Make some obscure names for some keys friendlier.'''

        if key in self.modmaps:
            return self.modmaps[key]

        elif key.endswith('_L') or key.endswith('_R'):
            # Remove left-right discrimination and try again.
            return self.modmap_key(key[:-2])

        else:
            return key


    def key_ignored(self, key):
        '''Check if the given key is ignored by any ignore rules.'''

        for (glob, match) in self.ignores.items():
            if match(key):
                return True

        return False


    def __repr__(self):
        '''Return a string representation of the keylet.'''

        l = []
        if self.is_modcmd:
            l.append('modcmd=%r' % self.get_modcmd())

        if self.keycmd:
            l.append('keycmd=%r' % self.get_keycmd())

        return '<keylet(%s)>' % ', '.join(l)


class KeyCmd(PerInstancePlugin):
    def __init__(self, uzbl):
        '''Export functions and connect handlers to events.'''
        super(KeyCmd, self).__init__(uzbl)

        self.keylet = Keylet()

        uzbl.connect('APPEND_KEYCMD', self.append_keycmd)
        uzbl.connect('IGNORE_KEY', self.add_key_ignore)
        uzbl.connect('INJECT_KEYCMD', self.inject_keycmd)
        uzbl.connect('KEYCMD_BACKSPACE', self.keycmd_backspace)
        uzbl.connect('KEYCMD_DELETE', self.keycmd_delete)
        uzbl.connect('KEYCMD_EXEC_CURRENT', self.keycmd_exec_current)
        uzbl.connect('KEYCMD_STRIP_WORD', self.keycmd_strip_word)
        uzbl.connect('KEYCMD_CLEAR', self.clear_keycmd)
        uzbl.connect('KEY_PRESS', self.key_press)
        uzbl.connect('KEY_RELEASE', self.key_release)
        uzbl.connect('MOD_PRESS', self.key_press)
        uzbl.connect('MOD_RELEASE', self.key_release)
        uzbl.connect('MODMAP', self.modmap_parse)
        uzbl.connect('SET_CURSOR_POS', self.set_cursor_pos)
        uzbl.connect('SET_KEYCMD', self.set_keycmd)

    def add_modmap(self, key, map):
        '''Add modmaps.

        Examples:
            set modmap = request MODMAP
            @modmap <Control> <Ctrl>
            @modmap <ISO_Left_Tab> <Shift-Tab>
            ...

        Then:
            @bind <Shift-Tab> = <command1>
            @bind <Ctrl>x = <command2>
            ...

        '''

        assert len(key)
        modmaps = self.keylet.modmaps

        modmaps[key.strip('<>')] = map.strip('<>')
        self.uzbl.event("NEW_MODMAP", key, map)

    def modmap_parse(self, map):
        '''Parse a modmap definiton.'''

        split = splitquoted(map)

        if not split or len(split) > 2:
            raise Exception('Invalid modmap arugments: %r' % map)

        self.add_modmap(*split)

    def add_key_ignore(self, glob):
        '''Add an ignore definition.

        Examples:
            set ignore_key = request IGNORE_KEY
            @ignore_key <Shift>
            @ignore_key <ISO_*>
            ...
        '''

        assert len(glob) > 1
        ignores = self.keylet.ignores

        glob = "<%s>" % glob.strip("<> ")
        restr = glob.replace('*', '[^\s]*')
        match = re.compile(restr).match

        ignores[glob] = match
        self.uzbl.event('NEW_KEY_IGNORE', glob)

    def clear_keycmd(self, *args):
        '''Clear the keycmd for this uzbl instance.'''

        k = self.keylet
        k.keycmd = ''
        k.cursor = 0
        config = Config[self.uzbl]
        del config['keycmd']
        self.uzbl.event('KEYCMD_CLEARED')

    def clear_modcmd(self):
        '''Clear the modcmd for this uzbl instance.'''

        k = self.keylet
        k.modcmd = ''
        k.is_modcmd = False

        config = Config[self.uzbl]
        del config['modcmd']
        self.uzbl.event('MODCMD_CLEARED')

    def clear_current(self):
        '''Clear the modcmd if is_modcmd else clear keycmd.'''

        if self.keylet.is_modcmd:
            self.clear_modcmd()

        else:
            self.clear_keycmd()

    def update_event(self, modstate, k, execute=True):
        '''Raise keycmd & modcmd update events.'''

        keycmd, modcmd = k.get_keycmd(), ''.join(modstate) + k.get_modcmd()

        if k.is_modcmd:
            logger.debug('modcmd_update, %s' % modcmd)
            self.uzbl.event('MODCMD_UPDATE', modstate, k)

        else:
            logger.debug('keycmd_update, %s' % keycmd)
            self.uzbl.event('KEYCMD_UPDATE', modstate, k)

        config = Config[self.uzbl]
        if config.get('modcmd_updates', '1') == '1':
            new_modcmd = ''.join(modstate) + k.get_modcmd()
            if not new_modcmd or not k.is_modcmd:
                del config['modcmd']

            elif new_modcmd == modcmd:
                config['modcmd'] = MODCMD_FORMAT % uzbl_escape(modcmd)

        if config.get('keycmd_events', '1') != '1':
            return

        new_keycmd = k.get_keycmd()
        if not new_keycmd:
            del config['keycmd']

        elif new_keycmd == keycmd:
            # Generate the pango markup for the cursor in the keycmd.
            curchar = keycmd[k.cursor] if k.cursor < len(keycmd) else ' '
            chunks = [keycmd[:k.cursor], curchar, keycmd[k.cursor+1:]]
            value = KEYCMD_FORMAT % tuple(map(uzbl_escape, chunks))

            config['keycmd'] = value

    def parse_key_event(self, key):
        ''' Build a set from the modstate part of the event, and pass all keys through modmap '''
        keylet = self.keylet

        modstate, key = splitquoted(key)
        modstate = set(['<%s>' % keylet.modmap_key(k) for k in modstate.split('|') if k])

        key = keylet.modmap_key(key)
        return modstate, key

    def key_press(self, key):
        '''Handle KEY_PRESS events. Things done by this function include:

        1. Ignore all shift key presses (shift can be detected by capital chars)
        2. In non-modcmd mode:
             a. append char to keycmd
        3. If not in modcmd mode and a modkey was pressed set modcmd mode.
        4. Keycmd is updated and events raised if anything is changed.'''

        k = self.keylet
        config = Config[self.uzbl]
        modstate, key = self.parse_key_event(key)
        k.is_modcmd = any(not k.key_ignored(m) for m in modstate)

        logger.debug('key press modstate=%s' % str(modstate))
        if key.lower() == 'space' and not k.is_modcmd and k.keycmd:
            k.keycmd = inject_str(k.keycmd, k.cursor, ' ')
            k.cursor += 1

        elif not k.is_modcmd and len(key) == 1:
            if config.get('keycmd_events', '1') != '1':
                # TODO, make a note on what's going on here
                k.keycmd = ''
                k.cursor = 0
                del config['keycmd']
                return

            k.keycmd = inject_str(k.keycmd, k.cursor, key)
            k.cursor += 1

        elif len(key) == 1:
            k.modcmd += key

        else:
            if not k.key_ignored('<%s>' % key):
                modstate.add('<%s>' % key)
                k.is_modcmd = True

        self.update_event(modstate, k)

    def key_release(self, key):
        '''Respond to KEY_RELEASE event. Things done by this function include:

        1. If in a mod-command then raise a MODCMD_EXEC.
        2. Update the keycmd uzbl variable if anything changed.'''
        k = self.keylet
        modstate, key = self.parse_key_event(key)

        if len(key) > 1:
            if k.is_modcmd:
                self.uzbl.event('MODCMD_EXEC', modstate, k)

            self.clear_modcmd()

    def set_keycmd(self, keycmd):
        '''Allow setting of the keycmd externally.'''

        k = self.keylet
        k.keycmd = keycmd
        k.cursor = len(keycmd)
        self.update_event(set(), k, False)

    def inject_keycmd(self, keycmd):
        '''Allow injecting of a string into the keycmd at the cursor position.'''

        k = self.keylet
        k.keycmd = inject_str(k.keycmd, k.cursor, keycmd)
        k.cursor += len(keycmd)
        self.update_event(set(), k, False)

    def append_keycmd(self, keycmd):
        '''Allow appening of a string to the keycmd.'''

        k = self.keylet
        k.keycmd += keycmd
        k.cursor = len(k.keycmd)
        self.update_event(set(), k, False)

    def keycmd_strip_word(self, seps):
        ''' Removes the last word from the keycmd, similar to readline ^W '''

        seps = seps or ' '
        k = self.keylet
        if not k.keycmd:
            return

        head, tail = k.keycmd[:k.cursor].rstrip(seps), k.keycmd[k.cursor:]
        rfind = -1
        for sep in seps:
            p = head.rfind(sep)
            if p >= 0 and rfind < p + 1:
                rfind = p + 1
        if rfind == len(head) and head[-1] in seps:
            rfind -= 1
        head = head[:rfind] if rfind + 1 else ''
        k.keycmd = head + tail
        k.cursor = len(head)
        self.update_event(set(), k, False)

    def keycmd_backspace(self, *args):
        '''Removes the character at the cursor position in the keycmd.'''

        k = self.keylet
        if not k.keycmd or not k.cursor:
            return

        k.keycmd = k.keycmd[:k.cursor-1] + k.keycmd[k.cursor:]
        k.cursor -= 1
        self.update_event(set(), k, False)

    def keycmd_delete(self, *args):
        '''Removes the character after the cursor position in the keycmd.'''

        k = self.keylet
        if not k.keycmd:
            return

        k.keycmd = k.keycmd[:k.cursor] + k.keycmd[k.cursor+1:]
        self.update_event(set(), k, False)

    def keycmd_exec_current(self, *args):
        '''Raise a KEYCMD_EXEC with the current keylet and then clear the
        keycmd.'''

        self.uzbl.event('KEYCMD_EXEC', set(), self.keylet)
        self.clear_keycmd()

    def set_cursor_pos(self, index):
        '''Allow setting of the cursor position externally. Supports negative
        indexing and relative stepping with '+' and '-'.'''

        k = self.keylet
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
        self.update_event(set(), k, False)

# vi: set et ts=4:
