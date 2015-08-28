# Event Manager

The default event manager provided with uzbl ships with behavior to help create
a "standard" browser experience. The base plugins which are shipped handle
things as fundumental as keyboard bindings, cookie preservation, providing a
progress bar for loading, and more.

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

If `prompt`, `value`, or `command` contains spaces, it must be quoted. `prompt`
may also be an empty string.

### commandspec

The command is an uzbl command which may use format string replacement:

* `%s`: The string as given.
* `%r`: The string, escaped and quoted for uzbl.
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

Cookies are stored in the following files (in decreasing precedence):

* `$UZBL_COOKIE_FILE`
* `$XDG_DATA_HOME/uzbl/cookies.txt`
* `$HOME/.local/share/uzbl/cookies.txt`

Session cookies are stored in the following files (in decreasing precedence):

* `$UZBL_SESSION_COOKIE_FILE`
* `$XDG_DATA_HOME/uzbl/session-cookies.txt`
* `$HOME/.local/share/uzbl/session-cookies.txt`

These paths are determined at the daemon's startup, not on a per-uzbl basis.

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
