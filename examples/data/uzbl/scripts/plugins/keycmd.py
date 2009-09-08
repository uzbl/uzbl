#TODO: Comment code.

import re

# Regex build cache.
_RE_CACHE = {}

def get_regex(regex):
    if regex not in _RE_CACHE:
        _RE_CACHE[regex] = re.compile(regex).match

    return _RE_CACHE[regex]


class Keylet(object):
    def __init__(self):
        self.cmd = ""
        self.held = []

        # to_string() string building cache.
        self._to_string = None

        self._modcmd = False
        self._wasmod = True


    def __repr__(self):
        return "<Keycmd(%r)>" % self.to_string()


    def _clear(self):
        self.cmd = ""
        self._to_string = None
        if self._modcmd:
            self._wasmod = True

        self._modcmd = False


    def to_string(self):
        '''Always of the form <Modkey1><ModKey2>+command'''

        if self._to_string is not None:
            return self._to_string

        if not self.held:
            self._to_string = self.cmd

        else:
            self._to_string = ''.join(["<%s>" % key for key in self.held])
            if self.cmd:
                self._to_string += "+%s" % self.cmd

        return self._to_string


    def match(self, regex):
        return bool(get_regex(regex)(self.to_string()))


_SIMPLEKEYS = {'Control': 'Ctrl', 'ISO_Left_Tab': 'Shift-Tab',}

def makesimple(key):
    if key.endswith("_L") or key.endswith("_R"):
        key = key[:-2]

    if key in _SIMPLEKEYS:
        key = _SIMPLEKEYS[key]

    return key


class KeycmdTracker(dict):
    def key_press(self, uzbl, key):
        if key.startswith("Shift_"):
            return

        if len(key) > 1:
            key = makesimple(key)

        k = self.get_keylet(uzbl)
        cmdmod = False

        if k.held and k._wasmod:
            k._modcmd = True
            k._wasmod = False
            cmdmod = True

        if key == "space":
            if k.cmd:
                k.cmd += " "
                cmdmod = True

        elif not k._modcmd and key in ['BackSpace', 'Return', 'Escape']:
            if key == "BackSpace":
                if k.cmd:
                    k.cmd = k.cmd[:-1]
                    if not k.cmd:
                        self.clear(uzbl)

                    else:
                        cmdmod = True

            elif key == "Return":
                uzbl.event("KEYCMD_EXEC", k)
                self.clear(uzbl)

            elif key == "Escape":
                self.clear(uzbl)

        elif not k.held and not k.cmd:
            k._modcmd = True if len(key) > 1 else False
            k.held.append(key)
            k.held.sort()
            cmdmod = True
            if not k._modcmd:
                k.cmd += key

        elif k._modcmd:
            cmdmod = True
            if len(key) > 1:
                if key not in k.held:
                    k.held.append(key)
                    k.held.sort()

            else:
                k.cmd += key

        else:
            cmdmod = True
            if len(key) == 1:
                if key not in k.held:
                    k.held.append(key)
                    k.held.sort()

                k.cmd += key

        if cmdmod:
            self.update(uzbl, k)


    def key_release(self, uzbl, key):
        if len(key) > 1:
            key = makesimple(key)

        k = self.get_keylet(uzbl)
        cmdmod = False
        if k._modcmd and key in k.held:
            uzbl.event("MODCMD_EXEC", k)
            k.held.remove(key)
            k.held.sort()
            self.clear(uzbl)

        elif not k._modcmd and key in k.held:
            k.held.remove(key)
            k.held.sort()
            cmdmod = True

        if not k.held and not k.cmd and k._wasmod:
            k._wasmod = False

        if cmdmod:
            self.update(uzbl, k)


    def update(self, uzbl, keylet):
        if keylet._modcmd:
            uzbl.config['keycmd'] = keylet.to_string()
            uzbl.event("MODCMD_UPDATE", keylet)

        else:
            uzbl.config['keycmd'] = keylet.cmd
            uzbl.event("KEYCMD_UPDATE", keylet)


    def get_keylet(self, uzbl):
        if uzbl not in self:
            self.add_instance(uzbl)
        keylet = self[uzbl]
        keylet._to_string = None
        return self[uzbl]


    def clear(self, uzbl):
        self.get_keylet(uzbl)._clear()
        uzbl.config['keycmd'] = ""
        uzbl.event("KEYCMD_CLEAR")


    def add_instance(self, uzbl, *args):
        self[uzbl] = Keylet()


    def del_instance(self, uzbl, *args):
        if uzbl in self:
            del uzbl


keycmd = KeycmdTracker()
export_clear_keycmd = keycmd.clear


def init(uzbl):

    uzbl.connect('INSTANCE_START', keycmd.add_instance)
    uzbl.connect('INSTANCE_STOP', keycmd.del_instance)
    uzbl.connect('KEY_PRESS', keycmd.key_press)
    uzbl.connect('KEY_RELEASE', keycmd.key_release)
