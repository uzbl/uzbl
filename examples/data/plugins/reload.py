from threading import Timer


UZBL_PIDS = {}


def reload_unwatch(uzbl, _=''):
    if uzbl.pid in UZBL_PIDS:
        del UZBL_PIDS[uzbl.pid]
        uzbl.config['reload.interval'] = ''


def reload_thread(uzbl, interval):
    uzbl.send('reload')
    if uzbl.pid in UZBL_PIDS:
        reload_unwatch(uzbl)
        uzbl.send('event RELOAD_WATCH %s' % interval)


def reload_watch(uzbl, interval):
    try:
        t = float(interval)
        UZBL_PIDS[uzbl.pid] = uzbl
        Timer(t, reload_thread, [uzbl, interval]).start()
        uzbl.config['reload.interval'] = interval
    except:
        pass


def init(uzbl):
    '''Export functions and connect handlers to events.'''

    connect_dict(uzbl, {
        'RELOAD_WATCH':   reload_watch,
        'RELOAD_UNWATCH': reload_unwatch,
    })

    export_dict(uzbl, {
        'reload_watch':   reload_watch,
        'reload_unwatch': reload_unwatch,
    })
