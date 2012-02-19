# Network communication classes
# vi: set et ts=4:
import asyncore
import asynchat
import socket
import os
import logging

logger = logging.getLogger('uzbl.net')


class NoTargetSet(Exception):
    pass


class TargetAlreadySet(Exception):
    pass


class WithTarget(object):
    '''
        Mixin that adds a property 'target' than can only be set once and
        raises an exception if not set when accesed
    '''

    @property
    def target(self):
        try:
            return self._target
        except AttributeError as e:
            raise NoTargetSet("No target for %r" % self, e)

    @target.setter
    def target(self, value):
        if hasattr(self, '_target') and self._target is not None:
            raise TargetAlreadySet(
                "target of listener already set (%r)" % self._target
            )
        self._target = value


class Listener(asyncore.dispatcher, WithTarget):
    ''' Waits for new connections and accept()s them '''

    def __init__(self, addr, target=None):
        asyncore.dispatcher.__init__(self)
        self.addr = addr
        self.target = target
        self.create_socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.set_reuse_addr()
        self.bind(addr)
        self.listen(5)

    def writable(self):
        return False

    def handle_accept(self):
        try:
            sock, addr = self.accept()
        except socket.error:
            return
        else:
            self.target.add_instance(sock)

    def close(self):
        super(Listener, self).close()
        if os.path.exists(self.addr):
            logger.info('unlinking %r', self.addr)
            os.unlink(self.addr)

    def handle_error(self):
        raise


class Protocol(asynchat.async_chat):
    ''' A connection with a single client '''

    def __init__(self, socket, target=None):
        asynchat.async_chat.__init__(self, socket)
        self.socket = socket
        self.target = target
        self.buffer = bytearray()
        self.set_terminator(b'\n')

    def collect_incoming_data(self, data):
        self.buffer += data

    def found_terminator(self):
        val = self.buffer.decode('utf-8')
        del self.buffer[:]
        self.target.parse_msg(val)

    def handle_error(self):
        raise
