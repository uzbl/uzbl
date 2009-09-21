import sys

UZBLS = {}


DEFAULTS = {'width': 8,
  'done': '=',
  'pending': '.',
  'arrow': '>',
  'format': '[%d%a%p]%c',
  'spinner': '-\\|/',
  'updates': 0,
  'progress': 100}


def error(msg):
    sys.stderr.write("progress_bar plugin: error: %s\n" % msg)


def add_instance(uzbl, *args):
    UZBLS[uzbl] = dict(DEFAULTS)


def del_instance(uzbl, *args):
    if uzbl in UZBLS:
        del UZBLS[uzbl]


def get_progress_config(uzbl):
    if uzbl not in UZBLS:
        add_instance(uzbl)

    return UZBLS[uzbl]


def update_progress(uzbl, prog=None):
    '''Updates the progress_format variable on LOAD_PROGRESS update.

    The current substitution options are:
        %d = done char * done
        %p = pending char * remaining
        %a = arrow
        %c = percent done
        %i = int done
        %s = -\|/ spinner
        %t = percent pending
        %o = int pending
    '''

    prog_config = get_progress_config(uzbl)
    config = uzbl.get_config()

    if prog is None:
        prog = prog_config['progress']

    prog = int(prog)
    if prog < prog_config['progress']:
        prog_config['updates'] = 0

    prog_config['updates'] += 1
    prog_config['progress'] = prog
    format = prog_config['format']
    width = prog_config['width']

    # Inflate the done and pending bars to stop the progress bar
    # jumping around.
    if '%c' in format or '%i' in format:
        count = format.count('%c') + format.count('%i')
        width += (3-len(str(prog))) * count

    if '%t' in format or '%o' in format:
        count = format.count('%t') + format.count('%o')
        width += (3-len(str(100-prog))) * count

    done = int(((prog/100.0)*width)+0.5)
    pending = width - done

    if '%d' in format:
        format = format.replace('%d', prog_config['done']*done)

    if '%p' in format:
        format = format.replace('%p', prog_config['pending']*pending)

    if '%a' in format:
        format = format.replace('%a', prog_config['arrow'])

    if '%c' in format:
        format = format.replace('%c', '%d%%' % prog)

    if '%i' in format:
        format = format.replace('%i', '%d' % prog)

    if '%t' in format:
        format = format.replace('%t', '%d%%' % (100-prog))

    if '%o' in format:
        format = format.replace('%o', '%d' % (100-prog))

    if '%s' in format:
        spin = '-' if not prog_config['spinner'] else prog_config['spinner']
        index = 0 if prog == 100 else prog_config['updates'] % len(spin)
        char = '\\\\' if spin[index] == '\\' else spin[index]
        format = format.replace('%s', char)

    if 'progress_format' not in config or config['progress_format'] != format:
        config['progress_format'] = format


def progress_config(uzbl, args):
    split = args.split('=', 1)
    if len(split) != 2:
        return error("invalid syntax: %r" % args)

    key, value = map(str.strip, split)
    prog_config = get_progress_config(uzbl)

    if key not in prog_config:
        return error("key error: %r" % args)

    if type(prog_config[key]) == type(1):
        try:
            value = int(value)

        except:
            return error("invalid type: %r" % args)

    elif not value:
        value = ' '

    prog_config[key] = value
    update_progress(uzbl)


def init(uzbl):

    connects = {'LOAD_PROGRESS': update_progress,
      'INSTANCE_START': add_instance,
      'INSTANCE_EXIT': del_instance,
      'PROGRESS_CONFIG': progress_config}

    for (event, handler) in connects.items():
        uzbl.connect(event, handler)
