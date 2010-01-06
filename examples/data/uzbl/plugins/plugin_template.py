'''Plugin template.'''

# Holds the per-instance data dict.
UZBLS = {}

# The default instance dict.
DEFAULTS = {}


def add_instance(uzbl, *args):
    '''Add a new instance with default config options.'''

    UZBLS[uzbl] = dict(DEFAULTS)


def del_instance(uzbl, *args):
    '''Delete data stored for an instance.'''

    if uzbl in UZBLS:
        del UZBLS[uzbl]


def get_myplugin_dict(uzbl):
    '''Get data stored for an instance.'''

    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


def myplugin_function(uzbl, *args, **kargs):
    '''Custom plugin function which is exported by the __export__ list at the
    top of the file for use by other functions/callbacks.'''

    print "My plugin function arguments:", args, kargs

    # Get the per-instance data object.
    data = get_myplugin_dict(uzbl)

    # Function logic goes here.


def myplugin_event_parser(uzbl, args):
    '''Parses MYPLUGIN_EVENT raised by uzbl or another plugin.'''

    print "Got MYPLUGIN_EVENT with arguments: %r" % args

    # Parsing logic goes here.


def init(uzbl):
    '''The main function of the plugin which is used to attach all the event
    hooks that are going to be used throughout the plugins life. This function
    is called each time a UzblInstance() object is created in the event
    manager.'''

    # Make a dictionary comprising of {"EVENT_NAME": handler, ..} to the event
    # handler stack:
    uzbl.connect_dict({
        # event name       function
        'INSTANCE_START':  add_instance,
        'INSTANCE_EXIT':   del_instance,
        'MYPLUGIN_EVENT':  myplugin_event_parser,
    })

    # Or connect a handler to an event manually and supply additional optional
    # arguments:
    #uzbl.connect("MYOTHER_EVENT", myother_event_parser, True, limit=20)

    # Function exports to the uzbl object, `function(uzbl, *args, ..)`
    # becomes `uzbl.function(*args, ..)`.
    uzbl.connect_dict({
        # external name      function
        'myplugin_function': myplugin_function,
    })
