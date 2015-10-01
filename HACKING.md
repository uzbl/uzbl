## Source Tree

The source tree is laid out as follows:

  * `bin`: top-level uzbl tools and scripts
  * `docs`: obsolete
  * `examples`: default configuration and scripts
  * `extras`: Vim syntax and other miscellaneous things
  * `misc`: utilities for uzbl
  * `src`: main source code
  * `uzbl`: the event manager

Source files are broken down as follows:

  * `comm`: formatting function for communication with the outside world
  * `commands`: command API and command implementations
  * `config`: default baked-in configuration
  * `cookie-jar`: WebKit1 cookie management
  * `events`: event API and built-in event definitions
  * `gui`: GUI-related code
  * `inspector`: web inspector callback handlers
  * `io`: I/O API and command queue thread
  * `js`: JavaScript utility functions
  * `menu`: menu structure definition
  * `requests`: request API
  * `scheme-request`: WebKit1 custom scheme implementation
  * `scheme`: main scheme handler implementation
  * `setup`: internal API
  * `soup`: WebKit1 libsoup code
  * `status-bar`: the status bar widget
  * `type`: I/O type enumeration
  * `util`: miscellaneous functions
  * `uzbl-core`: main structures and setup/teardown
  * `variables`: variable API and builtin variable implementations
  * `webkit`: WebKit1/WebKit2 API unifier
