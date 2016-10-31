# Event Manager

The default event manager provided with uzbl ships with behavior to help create
a "standard" browser experience. The base plugins which are shipped handle
things as fundumental as keyboard bindings, cookie preservation, providing a
progress bar for loading, and more.

The event manager accepts the following command line arguments:

* `-c`, `--config` `CONFIG`
  - Specifies the configuration file. Defaults to
    `$XDG_CONFIG_HOME/uzbl/event-manager.conf`
* `-s`, `--server-socket` `SOCKET`
  - The location of the socket the event manager will create for uzbl instances
    to connect to. Defaults to `$XDG_CACHE_HOME/uzbl/event_daemon`.
* `-p`, `--pid-file` `FILE`
  - The location of the pid file for use when the event manager is restarted or
    stopped. Defaults to the socket's location with a `.pid` extension.
* `-o`, `--log-file` `FILE`
  - The log file to write to. Defaults to the socket's location with a `.log`
    extension.
* `-n`, `--no-daemon`
  - Run in the foreground instead of forking into the background.
* `-a`, `--auto-close`
  - Shutdown the event manager when the last uzbl instance disconnects.
* `-v`, `--verbose`
  - Increases verbosity. May be specified multiple times.
* `-q`, `--quiet-events`
  - Turns off printing of events to stdout.

## bind

The `bind` plugin implements keybindings via the following events:

* `MODE_BIND <modespec> <bindspec> = <commandspec>`
  - Adds a binding to the `<modespec>` modes to execute `<commandspec>`. See
    below for details on what each means.
* `BIND <bindspec> = <command>`
  - Deprecated alias for `MODE_BIND global <bind> = <commandspec>`.
* `MODE_CHANGED <mode>`
  - Changes the expected mode for keybindings. Also clears the current bind
    parsing state.

The plugin stores the current mode in the `@mode` variable. Writes to this
variable are ignored; use the `MODE_CHANGED` event instead.

### modespec

A `modespec` is a comma-separated list of modes. Special modes include `global`
which is active in all contexts, and `stack` which is used during multi-key
parsing. In addition, an `-` may prefix a mode to exclude it during that mode
(when combined with `global`). For example, `global,-insert` will activate the
binding for every mode except for `insert`. Modes may be selected using the
`MODE_CHANGED` event.

### bindspec

A `bindspec` represents the series of keys which must be pressed to trigger a
command. A `bindspec` may use the following characters to change the behavior
of the binding to disambiguate bindings or to prompt for strings to use in the
command:

* `_`: The command will only be invoked after confirmation (return or enter).
  Any text is matched and available to the command via the expansion rules.
  These may be chained to ask for multiple values. May be prefixed with a
  `promptspec` to change the prompt text.
* `*`: The command will be invoked for each additional character. Any text is
  matched and available to the command via the expansion rules.
* `!`: The command will only be invoked after confirmation. This may be used to
  allow bindings which are prefixes of others (e.g., bind `x` when `xx` is also
  bound).
* Any other character: The command will be invoked upon matching.

Bindings may be prefixed with modifiers which apply to the entire keybinding
(up to a `_`). Modifier order does matter and when combined will only match if
the modifiers are given in alphabetical order.

* `Ctrl`
* `Mod1` (`Alt`)
* `Mod5` (`RightAlt`)
* `Shift`

Mouse buttons are represented as `<XButtonY>` where `X` is the number of
presses for multiple clicks (i.e., `2` for a double click) and `Y` is the
button number. Common button values are `1` for a left click, `2` for a middle
click, and `3` for a right click.

### promptspec

A prompt text may be specified for the `_` control character. It may use any of
the following specifications:

* `<prompt:>`: Sets `@keycmd_prompt` to `prompt:`.
* `<prompt:value>`: Sets `@keycmd_prompt` to `prompt:` and `@keycmd` to
  `value`.
* `<prompt!command>`: Sets `@keycmd_prompt` to `prompt` and executes `command`.

If `prompt`, `value`, or `command` contains spaces, it may be quoted. `prompt`
may also be an empty string.

### commandspec

The command is an uzbl command which may use format string replacement:

* `%s`: The string as given.
* `%r`: The string, escaped and quoted for uzbl. The specifics of escaping has
       changed to only be one level in all cases.
* `%1`: The first prompt value or argument as parsed by uzbl (space-separated).
* `%2`: The first prompt value or argument as parsed by uzbl (space-separated).
* `%n`: The `n`th prompt value or argument as parsed by uzbl (space-separated).

### Examples

* `event MODE_BIND command o_ = uri %s`: In `command` mode, typing
  `ouzbl.org<Enter>`, executes `uri uzbl.org`.
* `event MODE_BIND command <Ctrl>xy = exit`: In `command` mode, pressing
  `<Ctrl>x` followed by `<Ctrl>y` will exit uzbl. Releasing `<Ctrl>` cancels
  the binding.
* `event MODE_BIND global /* = search find %s`: In `global` mode, pressing `/`
  followed by any text will incrementally search for the text. Pressing
  `<Enter>` will end the search.
* `event MODE_BIND global <Ctrl>/<search:>_ = search find %s`: In `global`
  mode, pressing `<Ctrl>/` followed by any text followed by `<Enter>` will
  search for the text.

## completion

Provides tab-completion in `command` mode. It completes variable names and
command names. It uses the following commands:

* `START_COMPLETION`
  - Sets `@completion_list` to a space-separated list of span elements
    containing potential values (or empty if there are fewer than two matches).
    The matching portion uses `@hint_style` for that section.
* `STOP_COMPLETION`
  - Clears `@completion_list`.

## config

Provides a view into uzbl's current configuration values to other plugins. It
uses the `CONFIG_CHANGED` event to indicate when a variable's value has
changed:

* `CONFIG_CHANGED <name> <value>`
  - Sent when the variable `name` has been set to a new value.

## cookies

Provides persistence of cookie data between uzbl instances. Provides the
following event listeners:

* `BLACKLIST_COOKIE <cookiespec>`
  - Blacklists matching cookies. Due to the way uzbl works, the cookies are
    deleted after adding rather than blocked in the first place.
* `WHITELIST_COOKIE <cookiespec>`
  - Whitelists matching cookies.

If not `WHITELIST_COOKIE` rules are added, all cookies, not matching a
`BLACKLIST_COOKIE` rule will be allowed. Cookies which match a
`BLACKLIST_COOKIE` will always be denied.

There are multiple backends for cookie storage:

* `null`
* `memory`
* `text`

The `null` store does not remember any cookies between sessions. The `memory`
store only stores cookies in the current instance. The `file` store uses a file
using the Mozilla cookie format to preserve cookies.

Cookies are stored in the following files (in decreasing precedence):

* `$UZBL_COOKIE_FILE`
* `$XDG_DATA_HOME/uzbl/cookies.txt`
* `$HOME/.local/share/uzbl/cookies.txt`

Session cookies are stored in the following files (in decreasing precedence):

* `$UZBL_SESSION_COOKIE_FILE`
* `$XDG_DATA_HOME/uzbl/session-cookies.txt`
* `$HOME/.local/share/uzbl/session-cookies.txt`

These paths are determined at the daemon's startup, not on a per-uzbl basis.

The backend and paths may be configured in the configuration file:

```ini
[cookies]
global.type = text
global.path = <default global cookie path>
session.type = text
session.path = <default session cookie path>
```

Any cookies added or removed in one instance are shared with or deleted from
all other instances sharing the same event manager.

### cookiespec

A `cookiespec` is an argument list of keyworded regular expressions. The
supported keywords:

* `domain`: The domain name of the cookie source.
* `path`: The path the cookie applies to.
* `name`: The name of the cookie.
* `value`: The content of the cookie.
* `scheme`: Whether the cookie is for `http` or `https`. May end in `Only` if
  it is specific to that scheme.
* `expires`: The Unix timestamp of when the cookie expires.

As an example, `event WHITELIST_COOKIE domain 'github\.com$'` would allow any
cookie set by `github.com` (as well as any subdomains).

## downloads

Watches for download-related events and fills in `@downloads` with path and
progress information.

## history

Provides a readline-like history for the `command`-mode prompt. Uses the
following events:

* `HISTORY_PREV`
  - Selects the previous history item matching the current search.
* `HISTORY_NEXT`
  - Selects the next history item matching the current search.
* `HISTORY_SEARCH`
  - Sets the history search string and triggers `HISTORY_PREV`.

## keycmd

Manages a prompt for use in the status bar. Uses the following events:

* `APPEND_KEYCMD <keycmd>`
  - Appends to the current keycmd state.
* `IGNORE_KEY <glob>`
  - Ignores keys matching the glob. May be used to filter out keys such as
    `<Shift>` from the display (since it is implicit based on the character).
* `INJECT_KEYCMD <keycmd>`
  - Inserts `keycmd` into the current keycmd at the current cursor position.
* `KEYCMD_BACKSPACE`
  - Removes the character at the cursor position.
* `KEYCMD_DELETE`
  - Removes the character after the cursor position.
* `KEYCMD_EXEC_CURRENT`
  - Requests execution of the current command.
* `KEYCMD_STRIP_WORD [separators]`
  - Deletes a word from the current cursor position using `separators` to
    define word boundaries. Acts like `delete-word` in editors or shells. By
    default, only a space character is considered a word separator.
* `KEYCMD_CLEAR`
  - Clear the current keycmd state.
* `MODMAP <old> <new>`
  - Replaces `old` with `new` for modifiers. For example, it may be used to
    rename `<Control>` to `<Ctrl>`.
* `SET_CURSOR_POS <position>`
  - Sets the current cursor position. Negative numbers are relative to the end
    of the string. May also be the literal `-` or `+` to move the cursor
    relative to its current position.
* `SET_KEYCMD <keycmd>`
  - Sets the current keycmd state.

The following events are used as notification for actions:

* `MODCMD_UPDATE <modstate> <key>`
  - ???
* `KEYCMD_UPDATE <modstate> <key>`
  - ???
* `NEW_KEY_IGNORE <glob>`
  - Sent when a key glob is ignored.
* `KEYCMD_CLEARED`
  - Sent to indicate the `@keycmd` variable has been cleared.
* `MODCMD_CLEARED`
  - Sent to indicate the `@modcmd` variable has been cleared.
* `KEYCMD_EXEC <modcmd> <keycmd>`
  - Sent to indicate that execution of a command has been requested.
* `NEW_MODMAP <old> <new>`
  - Sent to notify that a new modifier mapping has been added.

### Configuration

The `@modcmd_updates` and `@keycmd_events` may be set to `0` to disable
updating the `@modcmd` and `@keycmd` variables. The `@keycmd` variable is HTML
markup using `@cursor_style` to indicate the current cursor position.

## mode

Implements a modal interface for uzbl. Uses the following events:

* `MODE_CONFIG <mode> <variable> <value>`
  - When `mode` is entered, sets `variable` to `value`.
* `MODE_CONFIRM <mode>`
  - Used internally.

The following events are used as notification for actions:

* `MODE_CHANGED <mode>`
  - Sent when a mode change is complete.

The plugin also manages the following variables:

- `@mode`: The current mode.
- `@default_mode`: The mode to use if `@mode` is ever set to the empty string.

## on\_event

Implements event chaining and reactions. Uses the following events:

* `ON_EVENT <event> <commandspec>`
  - Registers `command` to be executed when an event named `event` is seen. The
    command may be formatted using the arguments the original event received.

## on\_set

Implements reactions to variable set events. Uses the following events:

* `ON_SET <glob> <commandspec>`
  - When a variable matching `glob` is set to a new value, execute `command`
    with the new value as an argument.

## progress\_bar

Implements a progress bar for page loading. Uses the following variables:

* `@progress.format`: The format string for the `@progress.output` variable.
  Defaults to `[%d>%p]%c`.
* `@progress.done`: The character to use for filling the `done` section of the
  progress bar. Defaults to `=`.
* `@progress.pending`: The character to use for filling the `pending` section
  of the progress bar. Defaults to ` ` (space).
* `@progress.spinner`: The characters to use for the spinner. Defaults to
  `-\|/`.
* `@progress.sprites`: The characters to use for the sprite. Defaults to
  `loading`.
* `@progress.width`: The width of the progress bar. Defaults to 8.

The current progress output is 

### Progress format

* `%d`: The `done` character repeated to fill its space within the progress
  bar.
* `%p`: The `pending` character repeated to fill its space within the progress
  bar.
* `%s`: Current spinner character.
* `%r`: Current sprite character.
* `%c`: The percentage complete (with `%`).
* `%i`: The percentage complete as an integer.
* `%o`: The percentage pending as an integer.
* `%t`: The percentage pending (with `%`).
* `%%`: A literal `%`.
