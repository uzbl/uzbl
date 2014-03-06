# UPGRADING TO UZBL 1.0

In uzbl 1.0, lots of APIs have changed around to be more consistent with
names, more logical in behaviors, and consolidated to minimize code
duplication. This document aims to help users in porting configurations
from previous uzbl releases to 1.0. Users using a stock setup should be
able to use the default configurations.

## COMMAND LINE

For `uzbl-core`, a URI must now use the `--uri` flag; the first non-flag
argument is no longer interpreted as a URI (this is to avoid confusion of what
should be loaded with `uzbl-core --uri page1 page2`).

The `--embed` flag is now removed and the `--socket` flag has been renamed to
`--xembed-socket`. If you were using `--embed` before, `--xembed-socket` is
enough now (and was before too).

## DEFAULT SETUP

The default setup has gained new functionality in 1.0. This is intended
as an overview of what has changed compared to previous releases.

### Configuration

The socket and fifo files for `uzbl-core` are now dropped in
`$XDG_RUNTIME_DIR/uzbl` if possible and `/tmp/uzbl-$USER` otherwise.

Some new bindings have been added:

* `<Ctrl>a`: "select all"
* `<Ctrl>c`: "copy selection to clipboard"
  - Adds basic support for select/paste.
* `<Shift><Ctrl>F`: `toggle frozen`
  - When `frozen` is set, all network communication is denied within
    `uzbl-core`. This is useful for blocking ads after the initial page
    load or rogue redirects.

In `follow.js`, the `uzbl.follow` function is now available as
`uzbl.follow.followLinks`. The code also tries to avoid breaking the
same-origin-policy to minimize spurious warnings from WebKit.

The `per-site-settings.py` script now supports directories and will
execute all `.pss` files in lexicographical order (a custom glob is
accepted as a second argument in this case). The file format also
supports `@` to break all current matches and `@@` to exit from the file
altogether.

The `insert_bookmark.sh` script now removes duplicate entries.

The `follow.sh` script now allows for copying links to the primary
selection, secondary selection, or clipboard through use of `primary`,
`secondary`, or `clipboard`.  The `primary` choice was previously
known as `clipboard`.  The default behavior is now to copy to the
actual clipboard, not the primary selection.

## COMMANDS

For 1.0, the commands in uzbl have been cleaned up. Some commands have
been removed, renamed, or changed to make things more consistent. For
the most part, a simple rename of the command is sufficient (`js` is the
major exception here).

* `add_cookie`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `cookie` command.
  - *Porting*: Use `cookie add`.
* `auth`
  - *Change*: Removed.
  - *Rationale*: Now that `request` is synchronous, the event manager
    can also return authentication results, so use a command string
    instead of a command which sets some magic variables.
  - *Porting*: Use the `authentication_handler` variable.
* `clear_cookies`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `cookie` command.
  - *Porting*: Use `cookie clear`.
* `delete_cookie`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `cookie` command.
  - *Porting*: Use `cookie remove`.
* `dehilight`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `search` command.
  - *Porting*: Use `search clear`.
* `js`
  - *Change*: Changed to be more flexible. Now supports running
    JavaScript code in different contexts, not just the page.
  - *Rationale*: The goal is to allow for multiple JavaScript contexts
    to avoid getting mixed up with the page's JavaScript where
    possible.
  - *Porting*: Use `js page string`.
* `js_file`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `js` command.
  - *Porting*: Use `js page file`.
* `menu_*`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the `menu` command.
  - *Porting*: Use `menu (add|add_separator|remove) (document|link|image|editable)`.
* `plugin_refresh`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `plugin` command.
  - *Porting*: Use `plugin refresh`.
* `plugin_toggle`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `plugin` command.
  - *Porting*: Use `plugin toggle`.
* `reload_ign_cache`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the `reload` command.
  - *Porting*: Use `reload full`.
* `request`
  - *Change*: This is now a command which communicates with event
    managers synchronously to allow getting responses back from them.
  - *Rationale*: The old command was a duplicate of `event` and
    `request` is just about the best name available for "synchronous
    event".
  - *Porting*: Rename all old `request` calls to be `event`.
* `search`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `search` command.
  - *Porting*: Use `search forward`.
* `search_clear`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `search` command.
  - *Porting*: Use `search reset`.
* `search_reverse`
  - *Change*: Removed.
  - *Rationale*: Consolidated into the new `search` command.
  - *Porting*: Use `search reverse`.
* `set`
  - *Change*: '`=`' is no longer used in the set command.
  - *Rationale*: No other command needed this, so it was changed for
    consistency.
  - *Porting*: Remove '`=`' from set commands.
* `sh`
  - *Change*: Removed.
  - *Rationale*: Renamed to have `spawn_` as a prefix.
  - *Porting*: Use `spawn_sh`.
* `show_inspector`
  - *Change*: Removed
  - *Rationale*: Consolidated into the `inspector` command.
  - *Porting*: Use `inspector show`.
* `spell_checker`
  - *Change*: Removed.
  - *Rationale*: Renamed to `spell` to make it more verb-like.
  - *Porting*: Use `spell`.
* `sync_sh`
  - *Change*: Removed.
  - *Rationale*: Renamed to have `spawn_` as a prefix.
  - *Porting*: Use `spawn_sh_sync`.
* `sync_spawn`
  - *Change*: Removed.
  - *Rationale*: Renamed to have `spawn_` as a prefix.
  - *Porting*: Use `spawn_sync`.
* `sync_spawn_exec`
  - *Change*: Removed.
  - *Rationale*: Renamed to have `spawn_` as a prefix.
  - *Porting*: Use `spawn_sync_exec`.
* `toggle`
  - *Change*: Now supports toggling user variables with `0` or `1` as
    the value.
  - *Rationale*: Missing feature.
  - *Porting*: Check for `toggle` usage on user variables without extra
    arguments.
* `toggle_status`
  - *Change*: Removed.
  - *Rationale*: Unnecessary command.
  - *Porting*: Use `toggle show_status`.
* `toggle_zoom_type`
  - *Change*: Removed.
  - *Rationale*: Unnecessary command.
  - *Porting*: Use `toggle zoom_type`.
* `zoom_in`
  - *Change*: Removed
  - *Rationale*: Consolidated into the new `zoom` command.
  - *Porting*: Use `zoom in`.
* `zoom_out`
  - *Change*: Removed
  - *Rationale*: Consolidated into the new `zoom` command.
  - *Porting*: Use `zoom out`.

## VARIABLES

Multiple variables have been changed from integers to strings (for
easier understanding of the values) or changed into commands (usually
because manipulating them had weird semantics).

* `cookie_policy`
  - *Change*: This is now a string variable.
  - *Rationale*: Easier to understand.
  - *Porting*: Use `always` for `0`, `never` for `1`, and `first_party`
    for `2`.
* `editing_behavior`
  - *Change*: This is now a string variable.
  - *Rationale*: Easier to understand.
  - *Porting*: Use `mac` for `0`, `windows` for `1`, and `unix` for `2`.
* `geometry`
  - *Change*: Now read-only.
  - *Rationale*: Setting `geometry` triggered "magic" better attributed to a
    command.
  - *Porting*: Use the `geometry` command.
* `http_debug`
  - *Change*: This is now a string variable.
  - *Rationale*: Easier to understand.
  - *Porting*: Use `none` for `0`, `minimal` for `1`, `headers` for `2`,
    and `body` for `3`.
* `inject_html`
  - *Change*: Removed.
  - *Rationale*: Setting `inject_html` triggered "magic" better
    attributed to a command. Also, reading from it wasn't useful.
  - *Porting*: Use the `inject html` command.
* `inject_text`
  - *Change*: Removed.
  - *Rationale*: Setting `inject_text` triggered "magic" better
    attributed to a command. Also, reading from it wasn't useful.
  - *Porting*: Use the `inject text` command.
* `ssl_verify`
  - *Change*: Renamed to `ssl_policy` and uses strings.
  - *Rationale*: Easier to understand and more consistent.
  - *Porting*: Use `ignore` for `0` and `fail` for `1`.
* `stylesheet_uri`
  - *Change*: Removed.
  - *Rationale*: Setting `stylesheet_uri` triggered "magic" better
    attributed to a command.
  - *Porting*: Use the `css add` command.
* `uri`
  - *Change*: Now read-only.
  - *Rationale*: Setting `uri` triggered "magic" better attributed to a
    command.
  - *Porting*: Use the `uri` command.
* `view_mode`
  - *Change*: Removed.
  - *Rationale*: Renamed to `window_view_mode`.
  - *Porting*: Use `window_view_mode`.
* `view_source`
  - *Change*: Removed.
  - *Rationale*: Obsoleted by `page_view_mode`.
  - *Porting*: Set `page_view_mode` to `web` for `0` and `source` for
    `1`.
* `zoom_type`
  - *Change*: Removed.
  - *Rationale*: Redundant with `zoom_text_only`.
  - *Porting*: Use `zoom_text_only`.

## EXPANSION

In 1.0, new variable expansion rules apply. See the documentation for
more information.

* `@/ uzbl command /@`
* `@/+ uzbl command file /@`
* `@* uzbl js *@`
* `@*+ uzbl js file *@`
* `@- clean js -@`
* `@-+ clean js file -@`

## EVENTS

Some events have been modified to help support WebKit2 or for internal
changes. From the default configuration, check for usages of `@on_event`
for these events. Also check the event manager for built-in handlers.

* `AUTHENTICATE`
  - *Change*: Removed.
  - *Rationale*: The event manager can now communicate back to
    `uzbl-core` using the `request` command.
  - *Porting*: Use the `authentication_handler` variable.
* `DOWNLOAD_ERROR`
  - *Change*: The first argument is now the path to the destination file
    which failed and the error string is available as the third
    argument.
  - *Rationale*: It should allow event managers to clean up failed
    downloads properly since it can be correlated with the error event.
  - *Porting*: Change the argument expectation of any `DOWNLOAD_ERROR`
    handlers.
* `LINK_HOVER`
  - *Change*: The title of the hovered link is the second argument.
  - *Rationale*: The data is available and may be useful.
  - *Porting*: Expect a second argument in any `LINK_HOVER` event
    handlers.
* `SELECTION_CHANGED`
  - *Change*: Removed.
  - *Rationale*: This is no longer a builtin event. It was previously
    unused.
  - *Porting*: None.
* `VARIABLE_SET`
  - *Change*: The `float` type is now `double`. Also, events are not
    sent when setting the variable failed for whatever reason, so not
    all `set` commands have an associated `VARIABLE_SET` event.
  - *Rationale*: Internally, all varargs are `double` anyways, so
    there's no sense in truncating data.
  - *Porting*: Use `double` as a type and check assumptions with `set`
    commands.
