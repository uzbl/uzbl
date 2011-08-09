import time
import logging
from collections import defaultdict

def ascii(u):
    '''Convert unicode strings into ascii for transmission over
    ascii-only streams/sockets/devices.'''
    # TODO(tailhook) docstring is misleading
    # TODO(tailhook) name clashes with python3's builtin
    return u.encode('utf-8')


class Uzbl(object):
    def __init__(self, parent, child_socket, options):
        self.opts = options
        self.parent = parent
        self.child_socket = child_socket
        self.child_buffer = []
        self.time = time.time()
        self.pid = None
        self.name = None

        # Flag if the instance has raised the INSTANCE_START event.
        self.instance_start = False

        # Use name "unknown" until name is discovered.
        self.logger = logging.getLogger('uzbl-instance[]')

        # Track plugin event handlers and exported functions.
        self.exports = {}
        self.handlers = defaultdict(list)

        # Internal vars
        self._depth = 0
        self._buffer = ''

    def __repr__(self):
        return '<uzbl(%s)>' % ', '.join([
            'pid=%s' % (self.pid if self.pid else "Unknown"),
            'name=%s' % ('%r' % self.name if self.name else "Unknown"),
            'uptime=%f' % (time.time() - self.time),
            '%d exports' % len(self.exports.keys()),
            '%d handlers' % sum([len(l) for l in self.handlers.values()])])

    def init_plugins(self):
        '''Call the init and after hooks in all loaded plugins for this
        instance.'''
        from uzbl.ext import per_instance_registry

        self._plugin_instances = []
        for plugin in per_instance_registry:
            self._plugin_instances.append(plugin(self))

        # Initialise each plugin with the current uzbl instance.
        for plugin in self.parent.plugins.values():
            if plugin.init:
                self.logger.debug('calling %r plugin init hook', plugin.name)
                plugin.init(self)

        # Allow plugins to use exported features of other plugins by calling an
        # optional `after` function in the plugins namespace.
        for plugin in self.parent.plugins.values():
            if plugin.after:
                self.logger.debug('calling %r plugin after hook', plugin.name)
                plugin.after(self)

    def send(self, msg):
        '''Send a command to the uzbl instance via the child socket
        instance.'''

        msg = msg.strip()
        assert self.child_socket, "socket inactive"

        if self.opts.print_events:
            print(ascii(u'%s<-- %s' % ('  ' * self._depth, msg)))

        self.child_buffer.append(ascii("%s\n" % msg))

    def do_send(self):
        data = ''.join(self.child_buffer)
        try:
            bsent = self.child_socket.send(data)
        except socket.error as e:
            if e.errno in (errno.EAGAIN, errno.EINTR):
                self.child_buffer = [data]
                return
            else:
                self.logger.error('failed to send', exc_info=True)
                return self.close()
        else:
            if bsent == 0:
                self.logger.debug('write end of connection closed')
                self.close()
            elif bsent < len(data):
                self.child_buffer = [data[bsent:]]
            else:
                del self.child_buffer[:]

    def read(self):
        '''Read data from the child socket and pass lines to the parse_msg
        function.'''

        try:
            raw = unicode(self.child_socket.recv(8192), 'utf-8', 'ignore')
            if not raw:
                self.logger.debug('read null byte')
                return self.close()

        except:
            self.logger.error('failed to read', exc_info=True)
            return self.close()

        lines = (self._buffer + raw).split('\n')
        self._buffer = lines.pop()

        for line in filter(None, map(unicode.strip, lines)):
            try:
                self.parse_msg(line.strip())

            except Exception:
                self.logger.exception('erroneous event: %r' % line)

    def parse_msg(self, line):
        '''Parse an incoming message from a uzbl instance. Event strings
        will be parsed into `self.event(event, args)`.'''

        # Split by spaces (and fill missing with nulls)
        elems = (line.split(' ', 3) + [''] * 3)[:4]

        # Ignore non-event messages.
        if elems[0] != 'EVENT':
            logger.info('non-event message: %r', line)
            if self.opts.print_events:
                print('--- %s' % ascii(line))
            return

        # Check event string elements
        (name, event, args) = elems[1:]
        assert name and event, 'event string missing elements'
        if not self.name:
            self.name = name
            self.logger = logging.getLogger('uzbl-instance%s' % name)
            self.logger.info('found instance name %r', name)

        assert self.name == name, 'instance name mismatch'

        # Handle the event with the event handlers through the event method
        self.event(event, args)

    def event(self, event, *args, **kargs):
        '''Raise an event.'''

        event = event.upper()

        if not self.opts.daemon_mode and self.opts.print_events:
            elems = [event]
            if args:
                elems.append(unicode(args))
            if kargs:
                elems.append(unicode(kargs))
            print(ascii(u'%s--> %s' % ('  ' * self._depth, ' '.join(elems))))

        if event == "INSTANCE_START" and args:
            assert not self.instance_start, 'instance already started'

            self.pid = int(args[0])
            self.logger.info('found instance pid %r', self.pid)

            self.init_plugins()

        elif event == "INSTANCE_EXIT":
            self.logger.info('uzbl instance exit')
            self.close()

        if event not in self.handlers:
            return

        for handler in self.handlers[event]:
            self._depth += 1
            try:
                if hasattr(handler, 'call'):
                    # temporary solution, will migrate all events later
                    handler.call(self, *args, **kargs)
                else:
                    handler(*args, **kargs)

            except Exception:
                self.logger.error('error in handler', exc_info=True)

            self._depth -= 1

    def close_connection(self, child_socket):
        '''Close child socket and delete the uzbl instance created for that
        child socket connection.'''

    def close(self):
        '''Close the client socket and call the plugin cleanup hooks.'''

        self.logger.debug('called close method')

        # Remove self from parent uzbls dict.
        if self.child_socket in self.parent.uzbls:
            self.logger.debug('removing self from uzbls list')
            del self.parent.uzbls[self.child_socket]

        try:
            if self.child_socket:
                self.logger.debug('closing child socket')
                self.child_socket.close()

        except:
            self.logger.error('failed to close socket', exc_info=True)

        finally:
            self.child_socket = None

        # Call plugins cleanup hooks.
        for plugin in self.parent.plugins.values():
            if plugin.cleanup:
                self.logger.debug('calling %r plugin cleanup hook',
                    plugin.name)
                plugin.cleanup(self)

        for plugin in self._plugin_instances:
            plugin.cleanup()
        del self._plugin_instances # to avoid cyclic links

        logger.info('removed %r', self)

    def connect(self, name, handler):
        """Attach event handler

        No extra arguments added. Use bound methods and partials to have
        extra arguments.
        """
        self.handlers[name].append(handler)

