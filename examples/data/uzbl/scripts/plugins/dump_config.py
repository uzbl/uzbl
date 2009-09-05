import pprint

def dump_config(uzbl, args):
    '''Dump the config every time the page finishes loading.'''

    print "%s\n" % pprint.pformat(uzbl.config)


def init(uzbl):
    id = uzbl.connect('LOAD_FINISH', dump_config)
    print "Dump config id:", id
