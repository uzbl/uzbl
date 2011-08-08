from distutils.core import setup

setup(name='uzbl',
      version='201100808',
      description='Uzbl event daemon',
      url='http://uzbl.org',
      packages=['uzbl', 'uzbl.plugins'],
      scripts=[
        'bin/uzbl-event-manager',
        ],
    )




