### INTRODUCTION

Any program can only be really useful if it complies with the Unix
philosophy. Web browsers (and other tools that work with HTML, such as feed
readers) are frequent violators of this principle:

* They build in way too much things into one (complex) program, dramatically
  decreasing the options to do things the way you want.
* They store things in way too fancy formats (XML, RDF, SQLite, etc.) which are
  hard to store under version control, reuse in other scripts, and so on.

The Uzbl project was started as an attempt to resolve this.

### EDITIONS

"Uzbl" is an umbrella project consisting of different flavors. In the future
more things may come, but for now:

#### uzbl-core: main component meant for integration with other tools and scripts

* Uses WebKitGtk+ for rendering and network interaction (libsoup). CSS,
  JavaScript, and plugin support come for free.
* Provides interfaces to get data in (commands/configuration) and out (events):
  stdin/stdout/fifo/Unix sockets.
* You see a WebKit view and (optionally) a status bar which gets populated
  externally.
* No built-in means for URL changing, loading/saving of bookmarks, saving
  history, keybindings, downloads, etc.
* Extra functionality: many sample scripts come with it. More are available on
  the [Uzbl wiki](http://www.uzbl.org/wiki/scripts) or you can write them
  yourself.
* Entire configuration/state can be changed at runtime.
* Uzbl keeps it simple, and puts **you** in charge.

#### uzbl-browser: a complete browser experience based on uzbl-core

* Uses a set of scripts (mostly Python) that will fit most people, so things
  work out of the box; yet plenty of room for customization.
* Brings everything you expect: URL changing, history, downloads, form filling,
  link navigation, cookies, event management, etc.
* Advanced, customizable keyboard interface with support for modes, modkeys,
  multichars, variables (keywords) etc. (e.g., you can tweak the interface to
  be Vi-like, Emacs-like or any-other-program-like).
* Adequate default configuration.
* Focus on plaintext storage for your data and configuration files in simple,
  easy-to-parse formats and adherence to the [XDG basedir
  spec](http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html).
* Visually, similar to `uzbl-core` except that the status bar contains useful
  information.
* One window per page.

#### uzbl-tabbed: wraps around uzbl-browser and multiplexes it

* Spawns one window containing multiple tabs, each tab containing a full
  embedded `uzbl-browser`.
* Ideal as a quick and simple solution to manage multiple `uzbl-browser`
  instances without getting lost.

Throughout the documentation, when referring to `uzbl` we mean `uzbl-core`,
unless otherwise specified.

### CONFIGURATION / CONTROL:

The general idea is that Uzbl by default is very bare bones. You can send it
commands to update settings and perform actions, through various
interfaces. There is a limited, built-in default configuration. It sets a basic
status bar, title format, and network connection settings. By default, there
are *no* keybindings defined at all (default keybindings would be
counterproductive when you try to customize). For examples of the possibilities
what you can do, please see the sample configurations, and [wiki
page](http://www.uzbl.org/wiki/config).

There are several interfaces to interact with Uzbl:

* `uzbl --config <filename>`: `<filename>` will be read line by line, and the
  commands in it will be executed. Useful to configure Uzbl at startup. If you
  have a file in `$XDG_CONFIG_HOME/uzbl/config` (this expands to
  `~/.config/uzbl/config` on most systems), it will be automatically recognized.
* `stdin`: to write commands into `stdin`, use `--config -` (or `-c -`).
* Interactive: you can enter commands (and bind them to shortcuts, even at
  runtime). By default, the behaviour is modal (vi-like):

  - command mode: every keystroke is interpreted to run commands
  - insert mode: keystrokes are not interpreted so you can enter text into html
    forms

  There is also support for "chained" commands (multiple characters long), and
  keyworded commands. Also you can trigger incremental matching on commands and
  variables after pressing return. See the sample configuration file for more
  information.

  By default, copy and paste works when typing commands:

  - `insert` (paste X cliboard)
  - `shift insert` (paste primary selection buffer)

* FIFO & socket files: If enabled by setting their paths through one of the
  above means, you can have socket and FIFO files available which are very
  useful to control `uzbl` using external programs.

  - The advantage of the FIFO is you can write directly to it, but it's half
    duplex only (`uzbl` cannot send a response to you).
  - The socket is full duplex but a socket-compatible wrapper such as `socat`
    is needed to work with it. For example:

      echo <command> | socat - unix-connect:<socketfile>

When `uzbl` forks a new instance (e.g., "open in new window") it will use the
same command line arguments (e.g., the same `--config <file>`), except `--uri`
and `--named`. If you made changes to the configuration at runtime, these are
not passed on to the child.

#### Uzbl-browser

A minimal browser which encompasses a default configuration file and event
manager which gives a comparable browsing experience to the more common
browsers. For more, see [its documentation](README.browser.md).

#### Uzbl-tabbed

A flavor of `uzbl` built on top of `uzbl-browser` which adds tabbed browsing.
For more, see [its documentation](README.tabbed.md).

#### WebKit1 vs. WebKit2

Multiple commands and variables mention that they are not supported in WebKit2.
WebKit2 changes many things compared to WebKit1, some of which impact `uzbl`
greatly. It *works*, but many of the core features are not supported (cookie
management, the `scroll` command, HTML5 database control, and more). As such,
using WebKit2 is not recommended for day-to-day usage currently. For a list of
what is missing, see [this
bug](https://bugs.webkit.org/show_bug.cgi?id=113663).

### COMMAND SYNTAX

`Uzbl` will read commands via standard input, named FIFO pipe (once `fifo_dir`
is set) and Unix socket (once `socket_dir` is set). For convenience, `uzbl` can
also be instructed to read commands from a file on startup by using the
`--config` option. Indeed, the config file is nothing more than a list of
commands with support for comments (using the `#` character).

Each command starts with the name of a command or a `uzbl` variable that expands
to it. A command is terminated by a newline. Empty lines and lines that start
with the hash sign (ignoring leading whitespace) are ignored by the parser.
Command names are always written in lowercase.

A list of commands supported by `uzbl` is given here. Optional arguments are
given in square brackets (`[]`) with literal strings options in lowercase
separated by a pipe (`|`) character. `N` indicates an integer value and other
uppercase values are placeholders describing what the value should be. Required
options are given in angle brackets (`<>`). Arguments on which argument
splitting is not performed are given in curly braces (`{}`). Variable arguments
are indicated with ellipses (`...`). As an example:

    `example1 <a|b> <N> [URI...]`

means that the command `example` requires its first argument which must either
be a literal `a` or `b`, a second argument which is an integer, and any
remaining arguments are treated as URIs. Another example:

    `example2 <KEY> {VALUE}`

means that the first argument is split off used as a key and the remaining
string is used verbatim. The command `example2 key many args` would have `key`
as `KEY` and `many args` as `VALUE`.

#### Navigation

* `back [N]`
  - Navigate to the Nth (default: 1) previous URI in the instance history.
* `forward [N]`
  - Navigate to the Nth (default: 1) next URI in the instance history.
* `reload [cached|full]`
  - Reload the current page. If `cached` is given (the default), the cache is
    used. If `full` is given, the cache is ignored.
* `stop`
  - Stop loading the current page. This usually means that no new network
    requests are made, but JavaScript can still run.
* `uri {URI}`
  - Tell `uzbl` to navigate to the given URI.
* `download <URI> [DESTINATION]`
  - Tell WebKit to download a URI. In WebKit2, the destination parameter is
    ignored (the `download_handler` mechanism is used instead).

#### Page

* `load <COMMAND>`
  - Load content into the page. Supported subcommands include:
    + `html <URI> <CONTENT>`
      * Load content as HTML relative to a URI.
    + `text <CONTENT>`
      * Load content as plain text.
    + `error_html <URI> <BASE_URI> <CONTENT>`
      * Load content an error page (which does not appear in the forward/back
        list) at the given URI with the given
* `save [FORMAT] [PATH]` (WebKit2 >= 1.9.90)
  - Save the current page to a file in the given format. If no path is given,
    WebKit determines the output path. Currently supported formats include:
    mhtml (the default).
* `frame <list|focus|reload|stop|dump|inject|get|set> [NAME]` (WebKit1 only)
  - Operate on frames in the current page. If not name is given, the `_current`
    frame is used. Supported subcommands include:
    + `list` (Unimplemented)
      * Returns a JSON object representing the of frames in the frame.
    + `focus` (Unimplemented)
      * Give focus to the frame.
    + `reload`
      * Reload the frame.
    + `stop`
      * Stop loading the frame.
    + `save` (Unimplemented)
      * Save the content frame.
    + `load` (Unimplemented)
      * Load content into the frame.
    + `get <VARIABLE>` (Unimplemented)
      * Get information about a frame.
    + `set <VARIABLE> <VALUE>` (Unimplemented)
      * Set information about a frame.

#### Cookie

* `cookie <add|delete|clear>`
  - Manage cookies in `uzbl`. The subcommands work as follows:
    + `add <HOST> <PATH> <NAME> <VALUE> <SCHEME> <EXPIRATION>` (WebKit1 only)
      * Manually add a cookie.
    + `delete <DOMAIN> <PATH> <NAME> <VALUE>` (WebKit1 only)
      * Delete a cookie from the cookie jar.
    + `clear all`
      * Delete all cookies.
    + `clear domain [DOMAIN...]`
      * Delete all cookies matching the given domains.

#### Display

* `scroll <horizontal|vertical> <VALUE>` (WebKit2 broken)
  - Scroll either the horizontal or vertical scrollbar for the page. The value
    may be one of:
    + `begin`
      * Scrolls to the beginning of the page (either left or up).
    + `end`
      * Scrolls to the end of the page (either right or down).
    + `[-]N`
      * Scrolls N "units" as defined by the scrollbar (seems to be pixels).
    + `[-]N%`
      * Scrolls N% of a single page.
    + `N!`
      * Scrolls to position N on the scrollbar (also in "units").
    + `N%!`
      * Scrolls to position N% on the scrollbar.
* `zoom <in|out|set> [VALUE]`
  - Zoom either in or out by the given amount. When setting, the value is
    required. If no value is given, the variable `zoom_step` is used.
* `hardcopy [REGION]`
  - Print the given region. Supported regions include:
    + `page` (the default)
      * Prints the entire page.
    + `frame [NAME]`
      * Prints the frame with the given name or the current frame if no name is
        given.
* `geometry <SIZE>`
  - Set the size of the `uzbl` window. This is subject to window manager
    policy. If the size is `maximized`, the `uzbl` window will try to maximize
    itself. Otherwise, the size is parsed using the
    `WIDTHxHEIGHT±XOFFSET±YOFFSET` pattern.
* `snapshot <PATH> <FORMAT> <REGION> [FLAG...]` (WebKit2 >= 1.11.92 or WebKit1 >= 1.9.6)
  - Saves the current page as an image to the given path. Currently supported
    formats include: `png`. Acceptable regions include:
    + `visible`
      * Only includes the regions of the page which are currently visible.
    + `document` (WebKit2 only)
      * Includes the entire page.

    Accepted flags include:
    + `selection` (WebKit2 only)
      * Include the selection highlighting in the image.

#### Content

* `plugin <COMMAND>`
  - Exposes control of plugin-related information. Supported subcommands include:
    + `search <DIRECTORY>` (WebKit2 only)
      * Add the directory to the search path for plugins.
    + `refresh` (WebKit1 only)
      * Refresh the plugin database.
    + `toggle [NAME...]` (WebKit1 only)
      * Toggle whether the given plugins are enabled (or all if none are given).
* `remove_all_db` (WebKit1 only)
  - Remove all web databases.
* `spell <COMMAND>` (WebKit1 >= 1.5.1)
  - Use WebKit's spell check logic. Supported subcommands include:
    + `ignore [WORD...]`
      * Add the given words to the list of words to ignore.
    + `learn [WORD...]`
      * Teaches that the given words are spelled correctly.
    + `autocorrect <WORD>`
      * Returns the word as autocorrected by the checker.
    + `guesses <WORD>`
      * Returns the guesses for the word given by the checker as a JSON list.
* `cache <COMMAND>` (WebKit2 only)
  - Controls the cache used by WebKit. Supported subcommands include:
    + `clear`
      * Clears all cache.
* `favicon <COMMAND>`
  - Controls the favicon database. Supported subcommands include:
    + `clear`
      * Clears all cached favicons.
    + `uri <URI>`
      * Returns the URI of the favicon for the given URI.
    + `save <URI> <PATH>` (Unimplemented)
      * Saves the favicon for the given URI to a path.
* `css <COMMAND>`
  - Controls CSS settings in web pages. Supported subcommands include:
    + `add <URI> <LOCATION> [BASE_URI] [WHITELIST] [BLACKLIST]`
      * Adds a CSS file to the given page applied at the given location in the
        page. WebKit1 only uses the URI parameter and only supports one extra
        stylesheet at a time. For WebKit2, the location can be one of `all` or
        `top_only`. For `top_only`, child frames do not use the stylesheet. The
        base URI, if given, is used during CSS processing. The white and
        blacklists are comma-separated URI patterns which must match the format
        `protocol://host/path` and may use `*` as a wildcard.
    + `clear`
      Clears all user-supplied stylesheets.
* `scheme <SCHEME> {COMMAND}`
  - Registers a custom scheme handler for `uzbl`. The handler should accept a
    single argument for the URI to load and return HTML.
* `menu <COMMAND>`
  - Controls the context menu shown in `uzbl`. Supported subcommands include:
    + `add <OBJECT> <NAME> <COMMAND>`
      * Appends a new entry to the menu with the given name and runs the given
        command when used.
    + `add_separator <OBJECT> <NAME>`
      * Appends a new separator to the menu with the given name.
    + `remove <NAME>`
      * Remove items which have the given name.
    + `query <NAME>`
      * Returns the command for the item with the given name.
    + `list`
      * Returns the names of items in the menu as a JSON list.

#### Search

* `search <COMMAND>`
  - Controls WebKit's search mechanisms. Supported subcommands include:
    + `option <OPTION...>`
      * Manage options for searching. Options may be prefixed with `+` to set,
        `-` to unset, `!` to toggle, and `~` to reset to its default. The
        default options have `wrap` enabled. Recognized options include:
        - `wrap`
          + If set, searches will wrap around when the start or end of the
            document is reached.
        - `case_insensitive`
          + If set, searches are case-insensitive.
        - `word_start` (WebKit2 only)
          + If set, searches will only match the start of words.
        - `camel_case` (WebKit2 only)
          + If set, capital letters in the middle of words are considered the
            start of a word.
    + `options`
      * Returns the current options as a JSON list.
    + `clear`
      * Turn off searching. The current options and search string are
        preserved.
    + `reset`
      * Turn off searching, setting the options back to the default and
        forgetting the search string.
    + `find {STRING}`
      * Search forward in the document for the string. If no string is given,
        the previous search string is used.
    + `rfind {STRING}`
      * As `find`, but searches backwards in the document.
    + `next`
      * Jumps to the next instance of the search string in the document in the
        same direction as the last `find` or `rfind` command.
    + `prev`
      * As `next`, but searches in the opposite direction from the last `find`
        or `rfind` command.
* `security <SCHEME> <COMMAND> <OPTION>` (WebKit2 or WebKit2 >= 1.11.1)
  - Controls security options for a scheme. Recognized commands include:
    + `get`
      * Returns `true` if the option is set for the scheme, `false` otherwise.
    + `set`
      * Sets the option for the scheme.With WebKit2, be careful setting options
        since they cannot (currently) be unset.
    + `unset` (WebKit1 only)
      * Unsets the option for the scheme.

    Supported options include ("scheme" here is used as "content referenced
    with this scheme"):

    + `local`
      * If set, non-local schemes cannot link or access pages this scheme.
    + `no_access`
      * If set, the scheme cannot access other schemes.
    + `display_isolated`
      * If set, the scheme can only be displayed in the same scheme.
    + `secure`
      * If set, the scheme is considered "secure" and will not generate
        warnings when referenced by an HTTPS page.
    + `cors_enabled`
      * If set, the scheme is allowed to make cross-origin resource sharing.
    + `empty_document`
      * If set, the scheme is allowed to be committed synchronously.
* `dns <COMMAND>` (WebKit2 only)
  - Manage DNS settings. Supported subcommands include:
    + `fetch <HOSTNAME>`
      * Prefetch the DNS entry for the given hostname.
* `inspector <COMMAND>`
  - Control the web inspector. Supported subcommands include:
    + `show`
      * Show the web inspector.
    + `close`
      * Close the web inspector.
    + `attach` (WebKit2 only)
      * Request that the inspector be attached to the current page.
    + `detach` (WebKit2 only)
      * Request that the inspector be detached to the current page.
    + `coord <X> <Y>` (WebKit1 only)
      * Inspect the page at the given coordinate. The coordinate is relative to
        the current viewport.
    + `node <NODE>` (WebKit1 >= 1.3.17) (Unimplemented)
      * Inspect the node which matches the given CSS selector string. If
        multiple nodes match, the first one is used.

#### Execution

* `js <CONTEXT> <file|string> <VALUE>`
  - Run JavaScript code. If `file` is given, the value is interpreted as a path
    to a file to run, otherwise the value is executed as JavaScript code.
    Currently supported contexts include:
    + `uzbl`
      * Run the code in the `uzbl` context. This context does not (currently)
        have access to the page's contents.
    + `clean`
      * Create a new context just for this command. This is the most secure
        option and should be preferred where possible.
    + `frame` (WebKit1 only)
      * Run the code in the current frame's context.
    + `page` (WebKit2 broken)
      * Run the code in the current page's context. Be careful putting code
        into this context since anything in here may be hijacked by a malicious
        webpage. The context is cleared on each page load.
* `spawn <COMMAND> [ARGUMENT...]` (WILL CHANGE)
  - Spawn a command on the system with the given arguments. This ignore the
    stdout of the command. Will become `spawn async` in the future.
* `spawn_sync <COMMAND> [ARGUMENT...]` (DEPRECATED)
  - Spawn a command on the system synchronously with the given arguments and
    return its stdout. Will become `spawn async` in the future.
* `spawn_sync_exec <COMMAND> [ARGUMENT...]` (DEPRECATED)
  - Spawn a command synchronously and then execute the output as `uzbl`
    commands. This will disappear in the future and the command should
    communicate to `uzbl` over either the FIFO or the socket.
* `spawn_sh <COMMAND> [ARGUMENT...]` (DEPRECATED)
  - Spawn a command using the default shell. This is deprecated for `spawn
    @shell_cmd ...`.
* `spawn_sh_sync <COMMAND> [ARGUMENT...]` (DEPRECATED)
  - Spawn a command using the default shell. This is deprecated for `spawn_sync
    @shell_cmd ...`.

#### Uzbl

* `chain <COMMAND...>`
  - Execute a chain of commands. This is useful for commands which should not
    have any interruption between them.
* `include {PATH}`
  - Execute a file as a list of uzbl commands.
* `exit`
  - Closes `uzbl`.

#### Variable

* `set <NAME> {VALUE}`
  - Set a variable to the given value. Unsetting a variable is not possible
    (currently). Set it to the empty string (behavior is the same).
* `toggle <VARIABLE> [OPTION...]`
  - Toggles a variable. If any options are given, a value is chosen from the
    list, the option following the current value is used, defaulting to the
    first value if the current value is not found. Without any options,
    non-strings are toggled between 0 and 1 (if it is neither of these values,
    0 is set). User values are set to the empty string unless they are `0` or
    `1` in which case the other is set. If the variable does not exist, it is
    set to `1`.
* `print {STRING}`
  - Performs variable expansion on the string and returns the value.
* `dump_config`
  - Dumps the current config (which may have been changed at runtime) to
    stdout. Uses a format which can be piped into `uzbl` again or saved as a
    config file.
* `dump_config_as_events`
  - Dump the current config as a series of `VARIABLE_SET` events, which can be
    handled by an event manager.
* `event <NAME> [ARGUMENTS...]`
  - Send a custom event.
* `request <NAME> <COOKIE> [ARGUMENTS...]`
  - Send a synchronous request and returns the result of the request. This is
    meant to be used for synchronous communication between the event manager
    and `uzbl` since `spawn_sync` is not usable to talk to the event manager.

### VARIABLES AND CONSTANTS

`Uzbl` has a lot of internal variables and constants. You can get the values
(see the `print` command), and for variables you can also change the value at
runtime. Some of the values can be passed at start up through command line
arguments, others need to be set by using commands (e.g., in the configuration
file).

* Some variables have callback functions which will get called after setting
  the variable to perform some additional logic (see below).
* Besides the builtin variables you can also define your own ones and use them
  in the exact same way as the builtin ones.

Integer variables may be negative, boolean values should be either `0` or `1`.
Commands are variables intended to be executed as uzbl commands. All user
variables are treated as strings.

#### Uzbl

* `verbose` (integer) (default: 0)
  - Controls the verbosity of `uzbl` as it runs.
* `frozen` (boolean) (default: 0)
  - If non-zero, all network requests and navigation are blocked. This is
    useful for blocking pages from redirecting or loading any content after the
    core content is loaded. Network connections which have already started will
    still complete.
* `print_events` (boolean) (default: 0)
  - If non-zero, events will be printed to stdout.
* `handle_multi_button` (boolean) (default: 0)
  - If non-zero, `uzbl` will intercept all double and triple clicks and the
    page will not see them.

#### Communication

* `fifo_dir` (string) (no default)
  - Sets the directory for the FIFO. If set previously, the old FIFO is
    removed.
* `socket_dir` (string) (no default)
  - Sets the directory for the socket. If set previously, the old socket is
    removed.

#### Handler

* `scheme_handler` (command) (no default)
  - The command to use when determining what to do when navigating to a new
    URI. It is passed the URI as an extra argument. If the command returns a
    string with the first line containing only the word `USED`, the navigation
    is ignored.
* `request_handler` (command) (no default)
  - The command to use when a new network request is about to be initiated. The
    URI is passed as an argument. If the command returns a non-empty string,
    the first line of the result is used as the new command. To cancel a
    request, use the URI `about:blank`.
* `download_handler` (command) (no default)
  - The command to use when determining where to save a downloaded file. It is
    passed the URI, suggested filename, content type, and total size as
    arguments. If a destination is known, it is passed as well. The result is
    used as the final destination. If it is empty, the download is cancelled.
* `mime_handler` (command) (no default) (WebKit1 only)
  - The command to use when determining what to do with content based on its
    mime type. It is passed the mime type and disposition as arguments.
* `authentication_handler` (command) (no default) (WebKit1 only)
  - The command to use when requesting authentication.
* `shell_cmd` (string) (default: `sh -c`)
  - The command to use as a shell. This is used in the `spawn_sh` and `sync_sh`
    commands as well as `@()@` expansion.
* `enable_builtin_auth` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, WebKit will handle HTTP authentication dialogs. WebKit2 acts
    as if this were always `1`.

#### Window

* `icon` (string) (no default)
  - The path to an image to use as an icon for `uzbl`. Overrides `icon_name`
    when set.
* `icon_name` (string) (no default)
  - The name of an icon use as an icon for `uzbl`. Overrides `icon` when set.
* `window_role` (string) (no default)
  - Sets the role of the window. This is used to hint to the window manager how
    to handle the window.
* `auto_resize_window` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, WebKit will try to honor JavaScript moving the window. May
    still be blocked by window manager policy.

#### UI


* `show_status` (boolean) (default: 1)
  - If non-zero, the status bar is shown.
* `status_top` (boolean) (default: 0)
  - If non-zero, the status bar is placed above the page, otherwise it is
    placed below.
* `status_format` (string) (default: `<b>@[@TITLE]@</b> - @[@uri]@ - <span foreground="#bbb">@NAME</span>`)
  - The format string of the content in the status bar.
* `status_format_right` (string) (no default)
  - The format string of the content in right side of the status bar. The right
    side is elided if the left side overflows.
* `status_background` (string) (no default)
  - The background color of the status bar. Most common formats supported.
* `title_format_long` (string (default: `@keycmd @TITLE - Uzbl browser <@NAME> > @SELECTED_URI`)
  - `The title to use when no status bar is shown.
* `title_format_short` (string (default: `@TITLE - Uzbl browser <@NAME>`)
  - `The title to use when the status bar is shown.
* `enable_compositing_debugging` (boolean) (default: 0) (WebKit2 only)
  - If non-zero, draw borders  when using accelerated compositing.

#### Customization

* `default_context_menu` (boolean) (default: 0) (WebKit2 or WebKit2 >= 1.9.0)
  - If non-zero, display the default context menu, ignoring any custom menu
    items.

#### Printing

* `print_backgrounds` (boolean) (default: 1)
  - If non-zero, print backgrounds when printing pages.

#### Network

* `proxy_url` (string) (no default) (WebKit1 only)
  - If set, the value is used as a network proxy.
* `max_conns` (integer) (default: 100) (WebKit1 only)
  - The maximum number of connections to initialize total.
* `max_conns_host` (integer) (default: 6) (WebKit1 only)
  - The maximum number of connections to use per host.
* `http_debug` (enumeration) (default: `none`) (WebKit1 only)
  - Debugging level for HTTP connections. Acceptable values include:
    + `none`
    + `minimal`
    + `headers`
    + `body`
* `ssl_ca_file` (string) (no default) (WebKit1 only)
  - The path to the CA certificate file in PEM format to use for trusting SSL
    certificates. By default, the system store is used.
* `ssl_policy` (enumeration) (default: `fail`)
  - The policy to use for handling SSL failures. Acceptable values include:
    + `ignore`
    + `fail`
* `cache_model` (enumeration) (default: `unknown`)
  - The model to use for caching content. Acceptable values include:
    + `document_viewer`
      * Disables all caching.
    + `web_browser`
      * Caches heavily to attempt to minimize network usage.
    + `document_browser`
      * Caches moderately. This is optimized for navigation of local resources.

#### Security

* `enable_private` (boolean) (default: 0)
  - If non-zero, enables WebKit's private browsing mode. Also sets the
    `UZBL_PRIVATE` environment variable for external plugins. DNS prefetching
    is separate from this; see the `enable_dns_prefetch` variable. It is
    currently experimental in WebKitGtk itself.
* `enable_universal_file_access` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, content accessed via the `file` scheme will be allowed to
    access all other content.
* `enable_cross_file_access` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, content accessed via the `file` scheme will be allowed to
    access other `file` content.
* `enable_hyperlink_auditing` (boolean) (default: 0)
  - If non-zero, `<a ping>` will be allowed in WebKit.
* `enable_xss_auditing` (boolean) (default: 0) (WebKit2 only)
  - If non-zero, WebKit will attempt to filter reflecting XSS attacks.
* `cookie_policy` (enumeration) (default: `always`) (reading the value is broken in WebKit2)
  - Sets the cookie policy for WebKit. Acceptable values include:
    + `always`
    + `never`
    + `first_party`
      * Blocks third-party cookies.
* `enable_dns_prefetch` (boolean) (default: 1) (WebKit >= 1.3.13)
  - If non-zero, WebKit will prefetch domain names while browsing.
* `display_insecure_content` (boolean) (default: 1) (WebKit1 >= 1.11.2)
  - If non-zero, HTTPS content will be allowed to display content served over
    HTTP.
* `run_insecure_content` (boolean) (default: 1) (WebKit1 >= 1.11.2)
  - If non-zero, HTTPS content will be allowed to run content served over HTTP.
* `maintain_history` (boolean) (default: 1) (WebKit1 only)
  - If non-zero, WebKit will maintain a back/forward list.

#### Inspector

* `profile_js` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, the inspector will profile running JavaScript. Profiles are
    available as the `Console.profiles` variable in JavaScript.
* `profile_timeline` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, the inspector will profile network activity.

#### Page

* `forward_keys` (boolean) (default: 0)
  - If non-zero, `uzbl` will pass keypresses to WebKit.
* `useragent` (string) (no default)
  - The string to send as the `User-Agent` HTTP header. Setting this variable
    does not update the useragent in JavaScript as seen by the
    `navigator.userAgent` variable.
* `accept_languages` (string) (no default)
  - The list of languages to send with the `Accept-Language` HTTP header. In
    WebKit1, if set to `auto`, the list will be constructed from the system.
* `zoom_level` (float) (default: 1.0)
  - The current zoom level of the page.
* `zoom_step` (float) (default: 0.1)
  - The amount to step by default with the `zoom in` and `zoom out` commands.
    Must be greater than 0.
* `zoom_text_only` (boolean) (default: 1 in WebKit1; 0 in WebKit2) (WebKit2 >= 1.7.91 or WebKit1)
  - If non-zero, only text will be zoomed on a page.
* `caret_browsing` (boolean) (default: 0)
  - If non-zero, pages may be navigated using the arrows to move a cursor
    (similar to a word processor).
* `enable_frame_flattening` (boolean) (default: 0) (WebKit >= 1.3.5)
  - If non-zero, frames will be expanded and collapsed into a single scrollable
    area.
* `enable_smooth_scrolling` (boolean) (default: 0) (WebKit >= 1.9.0)
  - If non-zero, scrolling the page will be smoothed.
* `page_view_mode` (enumeration) (default: `web`)
  - How to render content on a page. Acceptable values include:
    + `web`
      * Render content as a web page.
    + `source`
      * Display the source of a page as plain text.
* `transparent` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, the web page will have a transparent background rather than a
    solid color.
* `window_view_mode` (enumeration) (default: `windowed`) (WebKit1 >= 1.3.4)
  - Sets the value web pages will see for how the content is presented to the
    user. Acceptable values include:
    + `windowed`
    + `floating`
    + `fullscreen`
    + `maximized`
    + `minimized`
* `enable_fullscreen` (boolean) (default: 0) (WebKit >= 1.3.4)
  - If non-zero, Mozilla-style JavaScript APIs will be enabled for JavaScript.
* `editable` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, web pages will be editable similar to a WYSIWYG editor.

#### JavaScript

* `enable_scripts` (boolean) (default: 1)
  - If non-zero, JavaScript will be allowed to execute on web pages.
* `javascript_windows` (boolean) (default: 0)
  - If non-zero, JavaScript in a page will be allowed to open windows without
    user intervention.
* `javascript_modal_dialogs` (boolean) (default: 0) (WebKit2 only)
  - If non-zero, JavaScript in a page will be allowed to create modal windows
    via the `window.showModalDialog` API.
* `javascript_dom_paste` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, JavaScript in a page will be able to paste content from the
    system clipboard.
* `javascript_clipboard` (boolean) (default: 0) (WebKit >= 1.3.0)
  - If non-zero, JavaScript in a page will be able to access content in the
    system clipboard directly.
* `javascript_console_to_stdout` (boolean) (default: 0) (WebKit2 >= 2.1.1)
  - If non-zero, JavaScript's console output will be put on stdout as well.

#### Image

* `autoload_images` (boolean) (default: 1)
  - If non-zero, images will automatically be loaded.
* `autoload_icons` (boolean) (default: 0) (WebKit2 only)
  - If non-zero, favicons will be loaded regardless of the `autoload_images`
    variable.
* `autoshrink_images` (boolean) (default: 1) (WebKit1 only)
  - If non-zero, stand-alone images will be resized to fit within the window.
* `use_image_orientation` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, image orientation tags will be used to reorient images once
    loaded.

#### Spell checking

* `enable_spellcheck` (boolean) (default: 0)
  - If non-zero, spell checking will be enabled in text entries.
* `spellcheck_languages` (string) (no default)
  - A comma-separated list of languages to use for spell checking. Must be set
    for `enable_spellcheck` to actually mean anything.

#### Form

* `resizable_text_areas` (boolean) (default: 1)
  - If non-zero, text area form elements will be resizable with a drag handle.
* `enable_spatial_navigation` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, the arrow keys will navigate between form elements.
* `editing_behavior` (enumeration) (default: `unix`) (WebKit1 only)
  - Sets the behavior of text area elements to behave closer to native widgets
    on different platforms. Acceptable values include:
    + `mac`
    + `unix`
    + `windows`
* `enable_tab_cycle` (boolean) (default: 1)
  - If non-zero, the tab key will navigate between form elements, otherwise it
    will always be interpreted as an insertion of a tab character.

#### Text

* `default_encoding` (string) (default: `iso-8859-1`)
  - The encoding to assume pages use if not indicated in HTTP headers.
* `custom_encoding` (string) (no default)
  - If non-empty, the value is used to override any other encoding used for the
    page. Causes the page to be reloaded. Set to the empty string to clear any
    custom encoding.
* `enforce_96_dpi` (boolean) (default: 0) (WebKit1 only)
  - If non-zero, the display will be assumed to be 96-dpi.

#### Font

* `default_font_family` (string) (default: `sans-serif`)
  - The font family to use by default for all text.
* `monospace_font_family` (string) (default: `monospace`)
  - The font family to use by default for monospace text.
* `sans_serif_font_family` (string) (default: `sans-serif`)
  - The font family to use by default for sans serif text.
* `serif_font_family` (string) (default: `serif`)
  - The font family to use by default for serif text.
* `cursive_font_family` (string) (default: `serif`)
  - The font family to use by default for any cursive text.
* `fantasy_font_family` (string) (default: `serif`)
  - The font family to use by default for any fantasy text.
* `pictograph_font_family` (string) (default: `serif`) (WebKit2 only)
  - The font family to use by default for any pictographic text.

#### Font size

Note: In WebKit2, font sizes are in pixels, whereas WebKit1 uses points.

* `minimum_font_size` (integer) (default: 5) (WebKit1 only)
  - The minimum font size to use for text.
* `minimum_logical_font_size` (integer) (default: 5) (WebKit1 only)
  - The minimum logical font size to use for text. This is the size after any
    transforms or "smaller" CSS specifications.
* `font_size` (integer) (default: 12)
  - The default font size for text.
* `monospace_size` (integer) (default: 10)
  - The default font size for monospace text.

#### Features

* `enable_plugins` (boolean) (default: 1)
  - If non-zero, plugins will be enabled on the page.
* `enable_java_applet` (boolean) (default: 1)
  - If non-zero, Java will be enabled for the page. This includes the `applet`
    HTML tag which is not controlled by the `enable_plugins` variable.
* `enable_webgl` (boolean) (default: 0) (WebKit >= 1.3.14)
  - If non-zero, WebGL will be enabled.
* `enable_webaudio` (boolean) (default: 0) (WebKit >= 1.7.5)
  - If non-zero, the WebAudio API will be enabled.
* `enable_3d_acceleration` (boolean) (default: 0) (WebKit1 >= 1.7.90)
  - If non-zero, the GPU will be used to render animations and 3D CSS
    transforms.
* `enable_2d_acceleration` (boolean) (default: 0) (WebKit2 >= 2.1.1)
  - If non-zero, 2D canvas operations will be accelerated by hardware.
* `enable_inline_media` (boolean) (default: 1) (WebKit >= 1.9.3)
  - If non-zero, media playback within a page will be enabled, otherwise, only
    fullscreen playback is allowed.
* `require_click_to_play` (boolean) (default: 0) (WebKit >= 1.9.3)
  - If non-zero, media may automatically play on a page, otherwise a user
    interaction is required to even load the media.
* `enable_css_shaders` (boolean) (default: 0) (WebKit1 >= 1.11.1)
  - If non-zero, CSS shaders will be enabled.
* `enable_media_stream` (boolean) (default: 0) (WebKit1 >= 1.11.1)
  - If non-zero, the Media Stream API will be enabled.

#### HTML5 Database

* `enable_database` (boolean) (default: 1)
  - If non-zero, the HTML5 client-side SQL API for JavaScript is enabled.
* `enable_local_storage` (boolean) (default: 1)
  - If non-zero, the HTML5 local storage API is enabled.
* `enable_pagecache` (boolean) (default: 0)
  - If non-zero, WebKit will "freeze" pages when navigating to optimize
    back/forward navigation. See
    [this article](http://webkit.org/blog/427/webkit-page-cache-i-the-basics/)
    for more information.
* `enable_offline_app_cache` (boolean) (default: 1)
  - If non-zero, HTML5 web application cache support will be enabled. This
    allows web applications to run locally without a network connection.
* `app_cache_size` (size) (default: [no quota]) (WebKit1 >= 1.3.13)
  - The maximum size (in bytes) of the local application cache.
* `web_database_directory` (string) (default: `$XDG_DATA_HOME/webkit/databases`) (WebKit1 only)
  - Where to store web databases.
* `web_database_quota` (size) (default: 5242880) (WebKit1 only)
  - The maximum size (in bytes) of web databases. (Note: It is unclear if this
    is a total quota or per-database quota.)
* `local_storage_path` (string) (default: `$XDG_DATA_HOME/webkit/databases`) (WebKit1 >= 1.5.2)
  - Where to store HTML5 `localStorage` data.
* `disk_cache_directory` (string) (no default) (WebKit2 >= 1.11.92)
  - Where to store cache files.. Must be set before loading any pages to have
    an effect.
* `web_extensions_directory` (string) (no default) (WebKit2 only)
  - Where to look for web extension libraries. Must be set before loading any
    pages.

#### Hacks

* `enable_site_workarounds` (boolean) (default: 0)
  - If non-zero, WebKit will use site-specific rules to workaround known broken
    behavior.

#### Constants

Constants may not be assigned to. When dumping variables using `dump_config`,
they are written as comments.

* `inspected_uri` (string) (WebKit >= 1.3.17)
  - The URI which the inspector is currently focusing on.
* `current_encoding` (string) (WebKit1 only)
  - The encoding of the current page.
* `geometry` (string)
  - A string giving the current window geometry of `uzbl` in
    `<width>x<height>+<xoffset>+<yoffset>` format.
* `plugin_list` (string) (WebKit2 >= 1.11.4 or WebKit1 >= 1.3.8) (WebKit2 broken)
  - A JSON-formatted list describing loaded plugins.
* `app_cache_directory` (string) (WebKit1 >= 1.3.13)
  - Currently always `$XDG_CACHE_HOME/webkitgtk/applications`.
* `uri` (string)
* `WEBKIT_MAJOR` (integer)
  - The major version of WebKit at runtime.
* `WEBKIT_MINOR` (integer)
  - The minor version of WebKit at runtime.
* `WEBKIT_MICRO` (integer)
  - The micro version of WebKit at runtime.
* `WEBKIT_UA_MAJOR` (integer) (WebKit2 broken)
  - The major useragent version of WebKit at compile time.
* `WEBKIT_UA_MINOR` (integer) (WebKit2 broken)
  - The minor useragent version of WebKit at compile time.
* `HAS_WEBKIT2` (boolean)
  - If non-zero, `uzbl` is using WebKit2.
* `ARCH_UZBL` (string)
  - The architecture `uzbl` is compiled for.
* `COMMIT` (string)
  - The commit string of `uzbl` at compiled time.
* `TITLE` (string)
  - The title of the current page.
* `SELECTED_URI` (string)
  - The URI of the link is hovering over; empty if there is no link.
* `NAME` (string)
  - The name of the `uzbl` instance (defaults to the pid).
* `PID` (integer)
  - The process ID of `uzbl`.
* `_` (string)
  - The last result from a command.

### VARIABLE EXPANSION

Variable expansion works similarly to shell interpreters (sh, bash, etc.). This
means you can construct strings with uzbl variables in them and have uzbl
replace the variable name with its contents.

In order to let uzbl know what to expand you'll need to prepend @ to the
variable name:

    print The variable \@show_status contains @show_status

The above example demonstrates two things:

* `\` is treated as escape character and will use the character immediately
  following it literally this means `\@show_status` will not expand to the
  variable content but be rather printed as `@show_status`
* prepending the variable with `@` will expand to its contents

As in the shell you can use `@{uzbl_var}` to denote the beginning/end of the
variable name in cases where it is not obvious what belongs to the name and
what not, e.g., `print @{show_status}foobar`

There are multiple constructs for more complex expansion.

#### Shell expansion

* `@(+command arg1 arg2)@`
  - This is equivalent to running `spawn_sync command arg1 arg2` and is
    replaced by the first line of its stdout.
* `@(command arg1 arg2)@`
  - This is equivalent to running `spawn_sh_sync command arg1 arg2` and is
    replaced by the first line of its stdout.

#### Uzbl command expansion

* `@/+file/@`
  - This is equivalent to running `include file` and is replaced by its result.
* `@/command arg1 arg2/@`
  - This is equivalent to running `command arg1 arg2` and is replaced by its
    result.

#### JavaScript expansion

* `@*+javascript_file*@`
  - This is equivalent to running `js uzbl file javascript_file` and is
    replaced by its result.
* `@*javascript*@`
  - This is equivalent to running `js uzbl string javascript` and is replaced
    by its result.
* `@<+javascript_file>@`
  - This is equivalent to running `js page file javascript_file` and is
    replaced by its result.
* `@<javascript>@`
  - This is equivalent to running `js page string javascript` and is replaced
    by its result.

#### XML escaping

* `@[string]@`
  - This escapes `string` for XML entities. This is most useful when using
    page-defined strings in the `status_bar` variable.

### JavaScript API

Uzbl has a JavaScript API available via the `js uzbl` command. There is a
global `uzbl` object can control `uzbl` through two properties: `variables` and
`commands`. For example, to set the `verbose` variable:

    uzbl.variables.verbose = 1

To call the `back` command:

    uzbl.commands.back()

All arguments to commands be converted to strings, so `{}` will appear to
commands as `[object Object]`, not JavaScript objects.

#### Accessing the web page

Currently, access to the webpage is not available through the `uzbl` context.
This is currently impossible with WebKit2 (since even `uzbl` does not have
access to the page's JavaScript context) and crashes WebKit1.

### TITLE AND STATUS BAR EVALUATION

The contents of the status bar can be customized by setting the `status_format`
variable. The contents of the window title can be customized by setting the
`title_format_short` variable (which is used when the status bar is displayed)
and the `title_format_long` variable (which is used when the status bar is not
displayed). Their values can be set using the expansion and substitution
techniques described above.

These variables are expanded in multiple stages; once when the variable is set,
and again every time that the status bar or window title are updated. Expansions
that should be evaluated on every update need to be escaped:

    set title_format_short = @(date)@
    # this expansion will be evaluated when the variable is set.
    # the title will stay constant with the date that the variable was set.

    set title_format_short = \@(date)\@
    # this expansion will be evaluated when the window title is updated.
    # the date in the title will change when you change pages, for example.

    set title_format_short = \\\@(date)\\\@
    # the title will stay constant as a literal "@(date)@"

The `status_format` and `status_format_right` variables can contain
[Pango](http://library.gnome.org/devel/pango/stable/PangoMarkupFormat.html)
markup . In these variables, expansions that might produce the characters `<`,
`&` or `>` should be wrapped in `@[]@` substitutions so that they don't
interfere with the status bar's markup; see the sample config for examples.

### EXTERNAL SCRIPTS

You can use external scripts with Uzbl the following ways:

* Let `uzbl` call them. These scripts are called "handlers" in the `uzbl`
  config. These are typically used for policy requests so that behavior can be
  controlled with external logic.
* Call them yourself from inside `uzbl`. You can bind keys with this. Examples:
  add new bookmark, load new URL.
* You could also use `xbindkeys` or your WM config to trigger scripts if `uzbl`
  does not have focus.

Scripts called by `uzbl` (the commands starting with `spawn`) have access to
the following environment variables:

* `$UZBL_CONFIG`
  - The configuration file loaded by this `uzbl` instance.
* `$UZBL_PID`
  - The process ID of this `uzbl` instance.
* `$UZBL_XID`
  - The X Windows ID of the process.
* `$UZBL_FIFO`
  - The filename of the FIFO being used, if any.
* `$UZBL_SOCKET`
  - The filename of the Unix socket being used, if any.
* `$UZBL_URI`
  - The URI of the current page.
* `$UZBL_TITLE`
  - The current page title.
* `$UZBL_PRIVATE`
  - Set if the `enable_private` variable is non-zero, unset otherwise.

Handler scripts (`download_handler`, `scheme_handler`, `request_handler`,
`mime_handler`, and `authentication_handler`) are called with special
arguments:

* download handler

  1. `url`
    - The URL of the item to be downloaded.
  2. `suggested_filename`
    - A filename suggested by the server or based on the URL.
  3. `content_type`
    - The mimetype of the file to be downloaded.
  4. `total_size`
    - The size of the file to be downloaded in bytes. This may be inaccurate.
  5. `destination_path`
    - This is only present if the download was started explicitly using the
      `download` command. If it is present, this is the path that the file
      should be saved to. A download handler using WebKit's internal downloader
      can just echo this path and exit when this argument is present.

* scheme handler

  1. `uri`
    - The URI of the page to be navigated to.

* request handler

  1. `uri`
    - The URI of the resource which is being requested.

* mime handler

  1. `mime_type`
    - The mime type of the resource.
  2. `disposition`
    - The disposition of the resource. Empty if unknown.

* authentication handler

  1. `host`
    - The host requesting authentication.
  2. `realm`
    - The realm requesting authentication.
  2. `retry`
    - Either `retrying` or `initial` depending on whether this request is a
      retrial of a previous request.

### WINDOW MANAGER INTEGRATION

As mentined before, the contents of the window title can be customized by
setting the `title_format_short` variable and the `title_format_long` variable
(see above to figure out when each of them is used). You can also set `icon`
variable to path of the icon file. Some advanced window managers can also take
`WM_WINDOW_ROLE` in account, which can be set by modifying `window_role`
variable.

Uzbl does not automatically use the current page's favicon as its icon. This
can, however, be done with external scripts.

Uzbl sets the `UZBL_URI` property on its window which is always the uri of the
page loaded into uzbl, except for web inspector windows which does not have the
property.

### EVENTS

Unlike commands, events are not handled in `uzbl` itself, but are dispatched
asynchronously through a text stream on `stdout` and/or through a socket.
You'll usually use `uzbl` by connecting it to a so-called "event manager" (EM).

An EM is a privileged communicator with `uzbl`. EM sockets are given on the
command line via the `--connect-socket` argument. EM sockets receive requests
in addition to events. All sockets receive event output.

An example EM is shipped with `uzbl` called `uzbl-event-manager`. See [its
documentation](README.event-manager.md) for more.

Many of the features of larger browsers are intended to be implemented via an
EM. The EM receives all events and requests from `uzbl` and can implement
history, keybindings, cookie management, and more. For more on the EM shipped
with `uzbl`, see [its documentation](README.event-manager.md)

#### Format

Events are reported in the following format.

     EVENT <NAME> <EVENT_NAME> [DETAILS...]

Events are line-oriented, so newlines are not supported within events.

Requests use the following format:

    REQUEST-<COOKIE> <REQUEST_NAME> [DETAILS...]

Only EM sockets receive REQUEST lines. Currently, if a reply is not received
within one second from `uzbl` sending a request, `uzbl` will continue without a
reply. This is because the request blocks the GUI thread. Replies to a request
use the following format:

    REPLY-<COOKIE> <REPLY>

If the cookie does not match the cookie from the request, `uzbl` will ignore it.

#### Built-in events

Uzbl will report various events by default. All of these events are 

##### Navigation

* `LOAD_START <URI>`
  - Sent when a main page navigation is requested.
* `LOAD_REDIRECTED <URI>`
  - Sent when the main page navigation has been redirected to a new URI.
* `LOAD_COMMIT <URI>`
  - Sent when the main page navigation has been committed and the network
    request has been sent to the server.
* `LOAD_PROGRESS <PROGRESS>`
  - Sent when the load percentage changes. The progress is in percentage.
* `LOAD_ERROR <URI> <CODE> <MESSAGE>`
  - Sent when WebKit has failed to load a page.
* `LOAD_FINISH <URI>`
  - Sent when a page navigation is complete. All of the top-level resources
    have been retrieved by this point.
* `REQUEST_QUEUED <URI>` (WebKit1 only)
  - Sent when a request is queued for the network.
* `REQUEST_STARTING <URI>`
  - Sent when a request has been sent to the server.
* `REQUEST_FINISHED <URI>`
  - Sent when a request has completed.

##### Input

* `KEY_PRESS <MODIFIERS> <KEY>`
  - Sent when a key (e.g., Shift, Ctrl, etc.) is pressed. The key is in UTF-8.
    This event is also used for mouse clicks which use the format
    `<REP>Button<VALUE>` which would be `2Button1` for a double click of the
    main mouse button (rep is empty for single clicks). Multiple clicks usually
    only show as a `KEY_RELEASE` event due to GDK implementation details.
* `KEY_RELEASE <MODIFIERS> <KEY>`
  - Sent when a key (e.g., Shift, Ctrl, etc.) is released. The key is in UTF-8.
* `MOD_PRESS <MODIFIERS> <MODIFIER>`
  - Sent when a modifier (e.g., Shift, Ctrl, etc.) is pressed.
* `MOD_RELEASE <MODIFIERS> <MODIFIER>`
  - Sent when a modifier (e.g., Shift, Ctrl, etc.) is released.

##### Commands

* `BUILTINS <COMMANDS>`
  - On startup, `uzbl` will emit this event with a JSON list of the names of
    all commands it understands.
* `COMMAND_ERROR <REASON>`
  - Sent when `uzbl` cannot execute a command.
* `COMMAND_EXECUTED <NAME> [ARGS...]`
  - Sent after a command has executed.
* `FILE_INCLUDED <PATH>`
  - Sent *after* a file is included.

##### Uzbl

* `FIFO_SET`
  - Sent `uzbl` opens a communication FIFO.
* `SOCKET_SET <PATH>`
  - Sent `uzbl` opens a communication socket.
* `INSTANCE_START <PID>`
  - Sent on startup.
* `PLUG_CREATED <ID>`
  - Sent if `uzbl` is started in plug mode. The ID is for the Xembed socket.
* `INSTANCE_EXIT <PID>`
  - Sent before `uzbl` quits. When this is sent, `uzbl` has already stopped
    listening on all sockets.
* `VARIABLE_SET <NAME> <str|int|ull|float> {VALUE}`
  - Sent when a variable has been set. Not all variable changes cause a
    `VARIABLE_SET` event to occur (e.g., any variable managed by WebKit behind
    the scenes does not trigger this event).

##### Page

* `BLUR_ELEMENT <NAME>` (WebKit1 only)
  - Sent when an element loses focus.
* `FOCUS_ELEMENT <NAME>` (WebKit1 only)
  - Sent when an element gains focus.
* `INSECURE_CONTENT <REASON>` (WebKit2 >= 1.11.4)
  - Sent when insecure content is used on a secure page.
* `LINK_HOVER <URI> <TITLE>`
   - Sent when a link is hovered over using the mouse.
* `LINK_UNHOVER <URI>`
   - Sent when a link is hovered over using the mouse. The URI is the
     previously hovered link URI.
* `FORM_ACTIVE <BUTTON>`
  - Sent when a form element has gained focus because of a mouse click.
* `ROOT_ACTIVE <BUTTON>`
  - Sent when the background page has been clicked.
* `SCROLL_HORIZ <VALUE> <MIN> <MAX> <PAGE>`
  - Sent when the page horizontal scroll bar changes. The min and max values
    are the bounds for scrolling, page is the size that fits in the viewport
    and the value is the current position of the scrollbar.
* `SCROLL_VERT <VALUE> <MIN> <MAX> <PAGE>`
  - Similar to `SCROLL_HORIZ`, but for the vertical scrollbar.
* `TITLE_CHANGED <TITLE>`
  - Sent when the page title changes.
* `WEB_PROCESS_CRASHED` (WebKit2 only)
  - Sent when the main rendering process crashed.

##### Window

* `GEOMETRY_CHANGED <GEOMETRY>`
  - Sent when the geometry changes.
* `WEBINSPECTOR <open|close>`
  - Sent when the web inspector window is opened or closed.
* `NEW_WINDOW <URI>`
  - Sent when a new window is being requested.
* `CLOSE_WINDOW`
  - Sent when `uzbl` is closing its window.
* `FOCUS_LOST`
  - Sent when `uzbl` loses the keyboard focus.
* `FOCUS_GAINED`
  - Sent when `uzbl` gains the keyboard focus.

##### Download

* `DOWNLOAD_STARTED <DESTINATION>`
  - Sent when a download to the given URI has started.
* `DOWNLOAD_PROGRESS <DESTINATION> <PROGRESS>`
  - Sent when progress for a download has been updated. The progress is a value
    between 0 and 1.
* `DOWNLOAD_ERROR <DESTINATION> <REASON> <CODE> <MESSAGE>`
  - Sent when a download has an error.
* `DOWNLOAD_COMPLETE <DESTINATION>`
  - Sent when a download has completed.

##### Cookie

* `ADD_COOKIE <DOMAIN> <PATH> <NAME> <VALUE> <SCHEME> <EXPIRATION>`
  - Sent when a cookie is added.
* `DELETE_COOKIE <DOMAIN> <PATH> <NAME> <VALUE> <SCHEME> <EXPIRATION>`
  - Sent when a cookie is deleted.

### COMMAND LINE ARGUMENTS

`uzbl` is invoked as

    uzbl-core [ARGUMENTS] [URI]

where `arguments` and `uri` are both optional. `arguments` can be:

* `-u`, `--uri=URI`
  - URI to load at startup. Equivalent to `uzbl-core <uri>` or `uri URI` after
    `uzbl` has launched. This overrides the optional URI without the flag.
* `-v`, `--verbose`
  - Sets `verbose` to be non-zero.
* `-n`, `--named=NAME`
  - Name of the current instance (defaults to Xorg window id or random for
    GtkSocket mode).
* `-c`, `--config=FILE`
  - Path to config file or `-` for stdin.
* `-s`, `--socket=SOCKET`
  - Xembed socket ID.
* `--connect-socket=CSOCKET`
  - Connect to server socket for event managing.
* `-p`, `--print-events`
  - Sets `print_events` to be non-zero.
* `-g`, `--geometry=GEOMETRY`
  - Runs `geometry GEOMETRY` with the given value on startup. Ignored if `uzbl` is
    embedded.
* `-V`, `--version`
  - Print the version and exit.
* `--display=DISPLAY`
  - X display to use.
* `--help`
  - Display help.

`uzbl-core scheme://address` will work as you expect. If you don't provide the
`scheme://` part, it will check if the argument is an existing file in the
filesystem, if it is, it will prepend `file://`, if not, it will prepend
`http://`.

### BUGS

Please report new issues to the [Uzbl bugtracker](http://uzbl.org/bugs).
