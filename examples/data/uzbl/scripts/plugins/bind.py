'''Plugin provides support for classic uzbl binds.

For example:
  bind ZZ = exit          -> bind('ZZ', 'exit')
  bind o _ = uri %s       -> bind('o _', 'uri %s')
  bind fl* = sh 'echo %s' -> bind('fl*', "sh 'echo %s'")
  bind fl* =              -> bind('fl*')

And it is also possible to execute a function on activation:
  bind('DD', myhandler)
'''

__export__ = ['bind',]

uzbls = {}


def bind(uzbl, glob, cmd=None):

    if uzbl not in uzbls:
        uzbls[uzbl] = {}
    binds = uzbls[uzbl]

    if not cmd:
        if glob in binds.keys():
            echo("deleted bind: %r" % self.binds[glob])
            del binds[glob]

    d = {'glob': glob, 'once': True, 'hasargs': True, 'cmd': cmd}

    if glob.endswith('*'):
        d['pre'] = glob.rstrip('*')
        d['once'] = False

    elif glob.endswith('_'):
        d['pre'] = glob.rstrip('_')

    else:
        d['pre'] = glob
        d['hasargs'] = False

    binds[glob] = d
    print "added bind: %r" % d


def init(uzbl):

    uzbl.bind("test", lambda _: True)

def cleanup(uzbl):
    if uzbl in uzbls:
        del uzbl
