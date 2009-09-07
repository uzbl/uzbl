def init(uzbl):
    commands = ['dump_config', 'dump_config_as_events']
    handler = uzbl.connect('LOAD_FINISH', commands)
    print "Added handler with id", handler.hid
