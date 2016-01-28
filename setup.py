from setuptools import setup

setup(name='uzbl',
      version='201100808',
      description='Uzbl event daemon',
      url='http://uzbl.org',
      packages=['uzbl', 'uzbl.plugins'],
      entry_points={
          'console_scripts': [
             'uzbl-event-manager = uzbl.event_manager:main'
          ]
      })
