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
