import mock
import logging
from uzbl.event_manager import Uzbl

class EventManagerMock(object):
	def __init__(self, global_plugins=(), instance_plugins=()):
		self.uzbls = {}
		self.plugins = {}
		self.instance_plugins = instance_plugins
		for plugin in global_plugins:
			self.plugins[plugin] = plugin(self)

	def add(self):
		u = mock.Mock(spec=Uzbl)
		u.parent = self
		u.logger = logging.getLogger('debug')
		u.plugins = {}
		for plugin in self.instance_plugins:
			u.plugins[plugin] = plugin(u)
		self.uzbls[mock.Mock()] = u
		return u
