UPDATES = 0

def update_progress(uzbl, progress=None):
    '''Updates the progress.output variable on LOAD_PROGRESS update.

    The current substitution options are:
        %d = done char * done
        %p = pending char * remaining
        %c = percent done
        %i = int done
        %s = -\|/ spinner
        %t = percent pending
        %o = int pending
        %r = sprites

    Default configuration options:
        progress.format  = [%d>%p]%c
        progress.width   = 8
        progress.done    = =
        progress.pending =
        progress.spinner = -\|/
        progress.sprites = loading
    '''

    global UPDATES

    if progress is None:
        UPDATES = 0
        progress = 100

    else:
        UPDATES += 1
        progress = int(progress)

    # Get progress config vars.
    format = uzbl.config.get('progress.format', '[%d>%p]%c')
    width = int(uzbl.config.get('progress.width', 8))
    done_symbol = uzbl.config.get('progress.done', '=')
    pend = uzbl.config.get('progress.pending', None)
    pending_symbol = pend if pend else ' '

    # Inflate the done and pending bars to stop the progress bar
    # jumping around.
    if '%c' in format or '%i' in format:
        count = format.count('%c') + format.count('%i')
        width += (3-len(str(progress))) * count

    if '%t' in format or '%o' in format:
        count = format.count('%t') + format.count('%o')
        width += (3-len(str(100-progress))) * count

    done = int(((progress/100.0)*width)+0.5)
    pending = width - done

    if '%d' in format:
        format = format.replace('%d', done_symbol * done)

    if '%p' in format:
        format = format.replace('%p', pending_symbol * pending)

    if '%c' in format:
        format = format.replace('%c', '%d%%' % progress)

    if '%i' in format:
        format = format.replace('%i', '%d' % progress)

    if '%t' in format:
        format = format.replace('%t', '%d%%' % (100-progress))

    if '%o' in format:
        format = format.replace('%o', '%d' % (100-progress))

    if '%s' in format:
        spinner = uzbl.config.get('progress.spinner', '-\\|/')
        index = 0 if progress == 100 else UPDATES % len(spinner)
        spin = '\\\\' if spinner[index] == '\\' else spinner[index]
        format = format.replace('%s', spin)

    if '%r' in format:
        sprites = uzbl.config.get('progress.sprites', 'loading')
        index = int(((progress/100.0)*len(sprites))+0.5)-1
        sprite = '\\\\' if sprites[index] == '\\' else sprites[index]
        format = format.replace('%r', sprite)

    if uzbl.config.get('progress.output', None) != format:
        uzbl.config['progress.output'] = format

# plugin init hook
def init(uzbl):
    connect_dict(uzbl, {
        'LOAD_COMMIT':      lambda uzbl, uri: update_progress(uzbl),
        'LOAD_PROGRESS':    update_progress,
    })
