import os.path

plugin_path = os.environ.get("UZBL_PLUGIN_PATH",
    "~/.local/share/uzbl/plugins:/usr/share/uzbl/site-plugins",
    ).split(":")
if plugin_path:
    __path__ = list(map(os.path.expanduser, plugin_path)) + __path__

