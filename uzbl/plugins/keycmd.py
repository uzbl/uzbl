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
    '''Small per-instance object that tracks characters typed.

        >>> k = Keylet()
        >>> k.set_keycmd('spam')
        >>> print k
        <keylet(keycmd='spam')>
        >>> k.append_keycmd(' and egg')
        >>> print k
        <keylet(keycmd='spam and egg')>
        >>> print k.cursor
        12
    '''

    def __init__(self):
        # Modcmd tracking
        self.modcmd = ''
        self.is_modcmd = False

        # Keycmd tracking
        self.keycmd = ''
        self.cursor = 0

    def get_keycmd(self):
        ''' Get the keycmd-part of the keylet. '''

        return self.keycmd

    def clear_keycmd(self):
        ''' Clears the keycmd part of the keylet '''

        self.keycmd = ''
        self.cursor = 0

    def get_modcmd(self):
        ''' Get the modcmd-part of the keylet. '''

        if not self.is_modcmd:
            return ''

        return self.modcmd

    def clear_modcmd(self):
        self.modcmd = ''
        self.is_modcmd = False

    def set_keycmd(self, keycmd):
        self.keycmd = keycmd
        self.cursor = len(keycmd)

    def insert_keycmd(self, s):
        ''' Inserts string at the current position

            >>> k = Keylet()
            >>> k.set_keycmd('spam')
            >>> k.cursor = 1
            >>> k.insert_keycmd('egg')
            >>> print k
            <keylet(keycmd='seggpam')>
            >>> print k.cursor
            4
        '''

        self.keycmd = inject_str(self.keycmd, self.cursor, s)
        self.cursor += len(s)

    def append_keycmd(self, s):
        ''' Appends string to to end of keycmd and moves the cursor

            >>> k = Keylet()
            >>> k.set_keycmd('spam')
            >>> k.cursor = 1
            >>> k.append_keycmd('egg')
            >>> print k
            <keylet(keycmd='spamegg')>
            >>> print k.cursor
            7
        '''

        self.keycmd += s
        self.cursor = len(self.keycmd)

    def backspace(self):
        ''' Removes the character at the cursor position. '''
        if not self.keycmd or not self.cursor:
            return False

        self.keycmd = self.keycmd[:self.cursor-1] + self.keycmd[self.cursor:]
        self.cursor -= 1
        return True

    def delete(self):
        ''' Removes the character after the cursor position. '''
        if not self.keycmd:
            return False

        self.keycmd = self.keycmd[:self.cursor] + self.keycmd[self.cursor+1:]
        return True

    def strip_word(self, seps=' '):
        ''' Removes the last word from the keycmd, similar to readline ^W
            returns the part removed or None

            >>> k = Keylet()
            >>> k.set_keycmd('spam and egg')
            >>> k.strip_word()
            'egg'
            >>> print k
            <keylet(keycmd='spam and ')>
            >>> k.strip_word()
            'and'
            >>> print k
            <keylet(keycmd='spam ')>
        '''
        if not self.keycmd:
            return None

        head, tail = self.keycmd[:self.cursor].rstrip(seps), self.keycmd[self.cursor:]
        rfind = -1
        for sep in seps:
            p = head.rfind(sep)
            if p >= 0 and rfind < p + 1:
                rfind = p + 1
        if rfind == len(head) and head[-1] in seps:
            rfind -= 1
        self.keycmd = head[:rfind] if rfind + 1 else '' + tail
        self.cursor = len(head)
        return head[rfind:]

    def set_cursor_pos(self, index):
        ''' Sets the cursor position, Supports negative indexing and relative
            stepping with '+' and '-'.
            Returns the new cursor position

            >>> k = Keylet()
            >>> k.set_keycmd('spam and egg')
            >>> k.set_cursor_pos(2)
            2
            >>> k.set_cursor_pos(-3)
            10
            >>> k.set_cursor_pos('+')
            11
        '''

        if index == '-':
            cursor = self.cursor - 1

        elif index == '+':
            cursor = self.cursor + 1

        else:
            cursor = int(index)
            if cursor < 0:
                cursor = len(self.keycmd) + cursor + 1

        if cursor < 0:
            cursor = 0

        if cursor > len(self.keycmd):
            cursor = len(self.keycmd)

        self.cursor = cursor
        return self.cursor

    def markup(self):
        ''' Returns the keycmd with the cursor in pango markup spliced in

        >>> k = Keylet()
        >>> k.set_keycmd('spam and egg')
        >>> k.set_cursor_pos(4)
        4
        >>> k.markup()
        '@[spam]@<span @cursor_style>@[ ]@</span>@[and egg]@'
    '''

        if self.cursor < len(self.keycmd):
            curchar = self.keycmd[self.cursor]
        else:
            curchar = ' '
        chunks = [self.keycmd[:self.cursor], curchar, self.keycmd[self.cursor+1:]]
        return KEYCMD_FORMAT % tuple(map(uzbl_escape, chunks))

    def __repr__(self):
        ''' Return a string representation of the keylet. '''

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
        self.modmaps = {}
        self.ignores = {}

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
        modmaps = self.modmaps

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
        ignores = self.ignores

        glob = "<%s>" % glob.strip("<> ")
        restr = glob.replace('*', '[^\s]*')
        match = re.compile(restr).match

        ignores[glob] = match
        self.uzbl.event('NEW_KEY_IGNORE', glob)

    def clear_keycmd(self, *args):
        '''Clear the keycmd for this uzbl instance.'''

        self.keylet.clear_keycmd()
        config = Config[self.uzbl]
        del config['keycmd']
        self.uzbl.event('KEYCMD_CLEARED')

    def clear_modcmd(self):
        '''Clear the modcmd for this uzbl instance.'''

        self.keylet.clear_modcmd()

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
            self.logger.debug('modcmd_update, %s', modcmd)
            self.uzbl.event('MODCMD_UPDATE', modstate, k)

        else:
            self.logger.debug('keycmd_update, %s', keycmd)
            self.uzbl.event('KEYCMD_UPDATE', modstate, k)

        config = Config[self.uzbl]
        if config.get('modcmd_updates', '1') == '1':
            new_modcmd = ''.join(modstate) + k.get_modcmd()
            if (not new_modcmd or not k.is_modcmd) and 'modcmd' in config:
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
            config['keycmd'] = unicode(k.markup())

    def parse_key_event(self, key):
        ''' Build a set from the modstate part of the event, and pass all keys through modmap '''

        modstate, key = splitquoted(key)
        modstate = set(['<%s>' % self.modmap_key(k) for k in modstate.split('|') if k])

        key = self.modmap_key(key)
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
        k.is_modcmd = any(not self.key_ignored(m) for m in modstate)

        self.logger.debug('key press modstate=%s', modstate)
        if key.lower() == 'space' and not k.is_modcmd and k.keycmd:
            k.insert_keycmd(' ')

        elif not k.is_modcmd and len(key) == 1:
            if config.get('keycmd_events', '1') != '1':
                # TODO, make a note on what's going on here
                k.keycmd = ''
                k.cursor = 0
                del config['keycmd']
                return

            k.insert_keycmd(key)

        elif len(key) == 1:
            k.modcmd += key

        else:
            if not self.key_ignored('<%s>' % key):
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

        self.keylet.set_keycmd(keycmd)
        self.update_event(set(), self.keylet, False)

    def inject_keycmd(self, keycmd):
        '''Allow injecting of a string into the keycmd at the cursor position.'''

        self.keylet.insert_keycmd(keycmd)
        self.update_event(set(), self.keylet, False)

    def append_keycmd(self, keycmd):
        '''Allow appening of a string to the keycmd.'''

        self.keylet.append_keycmd(keycmd)
        self.update_event(set(), self.keylet, False)

    def keycmd_strip_word(self, args):
        ''' Removes the last word from the keycmd, similar to readline ^W '''

        args = splitquoted(args)
        assert len(args) <= 1
        self.logger.debug('STRIPWORD %r %r', args, self.keylet)
        if self.keylet.strip_word(*args):
            self.update_event(set(), self.keylet, False)

    def keycmd_backspace(self, *args):
        '''Removes the character at the cursor position in the keycmd.'''

        if self.keylet.backspace():
            self.update_event(set(), self.keylet, False)

    def keycmd_delete(self, *args):
        '''Removes the character after the cursor position in the keycmd.'''

        if self.keylet.delete():
            self.update_event(set(), self.keylet, False)

    def keycmd_exec_current(self, *args):
        '''Raise a KEYCMD_EXEC with the current keylet and then clear the
        keycmd.'''

        self.uzbl.event('KEYCMD_EXEC', set(), self.keylet)
        self.clear_keycmd()

    def set_cursor_pos(self, args):
        '''Allow setting of the cursor position externally. Supports negative
        indexing and relative stepping with '+' and '-'.'''

        args = splitquoted(args)
        assert len(args) == 1

        self.keylet.set_cursor_pos(args[0])
        self.update_event(set(), self.keylet, False)

# vi: set et ts=4:
