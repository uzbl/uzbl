### DEFAULT CONFIGURATION

The default configuration sets up uzbl with functionality similar to a
"typical" browser. The major difference is that uzbl uses a modal
interface by default (like `vi` or `vim`). Since uzbl-core itself is
fairly small, a number of scripts are shipped by default to help fill in
functionality.

### DIRECTORIES

* `UZBL_UTIL_DIR` (set by `uzbl-browser`)
  - If unset, defaults to `$XDG_DATA_HOME/uzbl/scripts/uzbl/util` if it
    exists and `@PREFIX@/share/uzbl/examples/data/scripts/util` as a
    fallback.

### DEFAULT SCRIPT UTILITIES

Uzbl uses shell scripts to implement quite a bit of functionality. To
simplify much of the code, a number of utilities from files in
`$UZBL_UTIL_DIR/util`.

#### dmenu.sh

Builds a command line to help call dmenu. Before sourcing it, the
following variables may be set:

* `DMENU_SCHEME`
  - Sets the color scheme to use. These schemes are defined in the
    `dmenu.sh` script. The default schemes include "wmii", "formfiller",
    "bookmarks", "history", and "temps".
* `DMENU_PROMPT`
  - The prompt string.
* `DMENU_LINES` (default 10)
  - The number of lines to show. Only used if the "vertical" feature is
    requested.
* `DMENU_FONT`
  - The font to use.
* `DMENU_OPTIONS`
  - A space-separated list of features to enable if available.
    Understood features include:
    + xmms: Use XMMS-style filtering if available (the input is
      interpreted as a space-separated list of search terms rather than
      a single search string).
    + vertical: List entries vertically rather than horizontally.
    + resize: Shrink dmenu when fewer than `$DMENU_LINES` are available.
      Only used if "vertical" is requested.

#### editor.sh

Finds a suitable editor to use. By default `$VISUAL` is used. If that is
not set, `$VTERM -e $EDITOR` is used. `$VTERM` defaults to `xterm` and
`$EDITOR` defaults to `vim`. The command is stored in `$UZBL_EDITOR`.

#### uzbl-dir.sh

Sets directories for use in scripts. The set variables and their default
values are:

* `UZBL_DATA_DIR`
  - `$XDG_DATA_HOME/uzbl` with a fallback to `$HOME/.local/share/uzbl`
    if `$XDG_DATA_HOME` is empty.
* `UZBL_CONFIG_DIR`
  - `$XDG_CONFIG_HOME/uzbl` with a fallback to `$HOME/.config/uzbl` if
    `$XDG_CONFIG_HOME` is empty.
* `UZBL_FIFO_DIR` and `UZBL_SOCKET_DIR`
  - If `$XDG_RUNTIME_DIR` is set, `$XDG_RUNTIME_DIR/uzbl`, otherwise,
    `/tmp/uzbl-$USER`.
* `UZBL_DOWNLOAD_DIR`
  - `$XDG_DOWNLOAD_DIR` with a fallback to `$HOME` if empty.
* `UZBL_FORMS_DIR`
  - If unset externally, `$UZBL_DATA_DIR/dforms`.
* `UZBL_CONFIG_DIR`
  - If unset externally, `$UZBL_CONFIG_DIR/config`.
* `UZBL_COOKIE_FILE`
  - If unset externally, `$UZBL_DATA_DIR/cookies.txt`.
* `UZBL_BOOKMARKS_FILE`
  - If unset externally, `$UZBL_DATA_DIR/bookmarks`.
* `UZBL_TEMPS_FILE`
  - If unset externally, `$UZBL_DATA_DIR/temps`.
* `UZBL_HISTORY_FILE`
  - If unset externally, `$UZBL_DATA_DIR/history`.
* `UZBL_SESSION_FILE`
  - If unset externally, `$UZBL_DATA_DIR/browser-session`.

#### uzbl-util.sh

Provides shell functions to help keep boilerplate from scripts.

* `print`
  - Safely print the input string. May include escape sequences.
* `uzbl_send`
  - Sends standard input to uzbl.
* `uzbl_control`
  - Send a command to uzbl.
* `uzbl_escape`
  - Escape a string for uzbl's consumption.

#### uzbl-window.sh

Sets variables with the window dimensions of the associated uzbl
instance. It assumes that `$UZBL_XID` is set (which `uzbl-core` puts
into the environment for external scripts.

* `UZBL_WIN_POS_X` and `UZBL_WIN_POS_Y`
  - The ordinates of the uzbl window on the screen (`UZBL_WIN_POS` is
    set to "`$UZBL_WIN_POS_X $UZBL_WIN_POS_Y`")
* `UZBL_WIN_WIDTH` and `UZBL_WIN_HEIGHT`
  - The size of the uzbl window on the screen (`UZBL_WIN_SIZE` is
    set to "`$UZBL_WIN_WIDTH $UZBL_WIN_HEIGHT`")

### DEFAULT SCRIPTS

The default scripts shipped with uzbl are installed into
`@PREFIX@/share/uzbl/examples/data/scripts`.

#### auth.py

By default, this is set up as the `authentication_handler`. It provides
a dialog box which asks the user for a username/password to login to a
page. Requires PyGTK.

#### download.sh

This is the default `download_handler`. By default, it places all files
into `$UZBL_DOWNLOAD_DIR`. The switch statement may be extended to
download other specific filetypes into different locations.

#### follow.js and follow.sh

These scripts support labelling links within a webpage with a string and
then using the selected URI to do some action. The `follow.js`

#### formfiller.sh

Example config entries for formfiller script

    set formfiller = spawn @scripts_dir/formfiller.sh
    @cbind    za        = @formfiller add
    @cbind    ze        = @formfiller edit
    @cbind    zn        = @formfiller new
    @cbind    zl        = @formfiller load
    @cbind    zo        = @formfiller once

NEW action generates new file with formfields for current domain. Note that it
will overwrite existing file.

EDIT action calls an external editor with curent domain profiles.

ADD action adds another profile to current domain. Remember to name the
profiles, by default the have a name with random numbers.

LOAD action loads form data. If there is more then one profile, it will call
dmenu first to choose the profile you want to fill in the form.

ONCE action generates a temp file with formfields including the textareas and
after closing the editor, it will load the data into the formfields. The temp
file is removed

#### go\_input.js and go\_input.sh

#### history.sh

#### insert\_bookmark.sh

#### insert\_temp.sh

#### instance-select-wmii.sh

#### load\_cookies.sh

#### load\_url\_from\_bookmarks.sh

#### load\_url\_from\_history.sh

#### load\_url\_from\_temps.sh

#### per-site-settings.py

#### pipermail.js

#### scheme.py

#### session.sh

#### uzblcat
