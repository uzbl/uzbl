def echo_keys(uzbl, key, print_meta=True):
    '''Prints key-presses to the terminal.'''

    keys_pressed = int(uzbl.config['keys_pressed']) + 1
    print "Total keys pressed:", keys_pressed
    uzbl.config['keys_pressed'] = str(keys_pressed)


def init(uzbl):
    '''In this function attach all your event hooks using uzbl.connect and
    uzbl.bind functions.'''

    uzbl.connect('KEY_PRESS', echo_keys)
    uzbl.connect('KEY_PRESS', "sh %r" % ("echo %r" % "You just pressed %s"))

    # Start a running counter of all keys pressed.
    uzbl.config['keys_pressed'] = str(0)
