""" XDG constants """

import os

__all__ = ('xdg_data_home',)

default_xdg_data_home = os.path.join(os.environ['HOME'], '.local/share')

xdg_data_home = os.environ.get('XDG_DATA_HOME', default_xdg_data_home)
