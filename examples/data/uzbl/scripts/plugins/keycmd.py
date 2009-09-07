class Keylet(object):
    def __init__(self):
        self.cmd = ""
        self.held = []
        self.modfirst = False

    def __repr__(self):
        fmt = "<Keycmd(%r)>"
        if not self.cmd and not self.held:
            return fmt % ""

        elif not len(self.held):
            return fmt % self.cmd

        helds = '+'.join(["<%s>" % key for key in self.held])
        if not self.cmd:
            return fmt % helds

        else:
            return fmt % ("%s+%s" % (helds, self.cmd))


keymap = {'period': '.'}

class KeycmdTracker(dict):
    def get_cmd(self, uzbl):
        '''Returns a tuple of the form (keys held, cmdstr)'''

        if uzbl not in self:
            return ([], [])

        return self[uzbl]


    def key_press(self, uzbl, key):

        if key.startswith("Shift_"):
            return

        t = self.get_keylet(uzbl)
        if key == "BackSpace":
            if t.cmd:
                t.cmd = t.cmd[:-1]

        elif key == "Escape":
            self.clear(uzbl)

        elif key == "space":
            if t.cmd:
                t.cmd += " "

        elif key in keymap:
            t.cmd += keymap[key]

        elif len(key) == 1:
            t.cmd += key

        elif key not in t.held:
            if not t.held and not t.cmd and len(key) != 1:
                t.modfirst = True

            t.held.append(key)

        self.raise_event(uzbl)


    def key_release(self, uzbl, key):

        #if key == "Return":
        # TODO: Something here

        t = self.get_keylet(uzbl)
        if key in t.held:
            t.held.remove(key)

        if key == "Return":
            self.clear(uzbl)

        if t.modfirst and not len(t.held):
            self.clear(uzbl)

        self.raise_event(uzbl)


    def get_keylet(self, uzbl):
        if uzbl not in self:
            self.add_instance(uzbl)

        return self[uzbl]


    def clear(self, uzbl):
        t = self.get_keylet(uzbl)
        t.cmd = ""
        t.modfirst = False


    def add_instance(self, uzbl):
        self[uzbl] = Keylet()


    def del_instance(self, uzbl):
        if uzbl in self:
            del uzbl


    def raise_event(self, uzbl):
        '''Raise a custom event.'''

        keylet = self.get_keylet(uzbl)
        uzbl.config['keycmd'] = keylet.cmd
        uzbl.event('KEYCMD_UPDATE', self.get_keylet(uzbl))


keycmd = KeycmdTracker()

def init(uzbl):

    uzbl.connect('INSTANCE_START', keycmd.add_instance)
    uzbl.connect('INSTANCE_STOP', keycmd.del_instance)
    uzbl.connect('KEY_PRESS', keycmd.key_press)
    uzbl.connect('KEY_RELEASE', keycmd.key_release)
