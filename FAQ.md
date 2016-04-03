### $browser\_plugin is not working properly! Help!

There is a known issue where WebKit1 and GTK3 does not work with plugins (e.g.,
Flash). If you need Netscape-compatible plugins to work, use GTK2 or WebKit2.

### WebKit2 isn't working!

More development is needed to get WebKit2 support to be feature compatible in
uzbl. Currently, WebKit1 is the best option. If you would like to use WebKit2,
a non-exhaustive list of things that is known to not work at the moment
includes:

* Scrolling
* Injecting JavaScript into the page
* Cookie management

For more specifics, search the [README](README.md) file for 'WebKit1' and
'WebKit2'.

### The python package fails to install

If trying to install uzbl into a location that is not supported by the system
python you will get an error like this.

```
TEST FAILED: /usr/local/lib/python3.4/site-packages/ does NOT support .pth files
error: bad install directory or PYTHONPATH
```

You can either install into a prefix that is supported by python

```
make install PREFIX=/usr
```

Or, enable the path as a site dir for python by adding or modifying the file
`sitecustomize.py` in the system site-packages directory (e.g
"/usr/lib/python3.4/site-packages/sitecustomize.py"). The file should contain a
addsitedir directive for the path you're trying to install to.

```
import site
site.addsitedir("/usr/local/lib/python3.4/site-packages/")
```

### Starting the event manager gives "ImportError: No module named 'six'"

Install the `six` module from your package manager.
