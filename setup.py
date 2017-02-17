import os
from setuptools import setup

VERSION = os.getenv('VERSION', '0+devel')

setup(name='uzbl',
      version=VERSION,
      description='Uzbl event daemon',
      url='http://uzbl.org',
      packages=['uzbl', 'uzbl.plugins'],
      entry_points={
          'console_scripts': [
             'uzbl-event-manager = uzbl.event_manager:main'
          ]
      })
