## Uzbl Keybindings

These keybindings are totally customizable. You can edit
`$XDG_CONFIG_HOME/uzbl/config` to modify them or add more.

### Navigating the Web

`o` opens a URL

`O` edits the current URL

`S` stops loading

`b` goes back

`m` goes forward

`U` searches the history

`r` reloads the current page

`fl` selects a link or form element using the keyboard

`Fl` selects a link to open in a new window using the keyboard

`w` opens a new window

`c` clones the current window

### Navigating the Page

`j` scrolls down

`k` scrolls up

`h` scrolls left

`l` scrolls right

`Page Up` or `Ctrl-b` scrolls up one screen

`Spacebar` or `Ctrl-f` scrolls down one screen

`<<` or `Home` scrolls to the top of the page

`>>` or `End` scrolls to the bottom of the page

`/` searches the current page

`?` searches the current page in reverse

`n` goes to next result further down the page

`N` goes to next result further up the page

### Modes

Uzbl's default configuration is modal (although a completely modeless
configuration should be possible).

In "command" mode, everything you type is interpreted by uzbl as a command.

In "insert" mode, everything you type is passed to the web page, for form input
or the web page's keybindings.

`i` or `Ctrl-i` switches to insert mode

`Esc` or `Ctrl-[` returns to command mode and clears the current command

### Web search

`ddg` searches [duckduckgo](http://duckduckgo.com/)

`gg` searches [Google](http://www.google.com/)

<!-- Broken, currently does not register as a command, see https://github.com/uzbl/uzbl/issues/294
`\wiki` searches [the English-language Wikipedia](http://en.wikipedia.org/)
-->

### uzbl-tabbed

Use these within `uzbl-tabbed`.

`gn` opens a new tab

<!-- Broken, currently acts the same as `gn`, see https://github.com/uzbl/uzbl/issues/295
`gN` opens a new tab and switches to it
-->

`go` opens a URL in a new tab

<!-- Broken, currently acts the same as `go`, see https://github.com/uzbl/uzbl/issues/295
`gO` opens a URL in a new tab and switches to it
-->

`gC` closes the current tab

`gQ` cleans tabs

`g<` goes to the first tab

`g>` goes to the last tab

`gt` goes to the next tab

`gT` goes to the previous tab

`gi` goes to a given tab index

### Clipboard

The terminology here is a bit confusing, please look at
[this article](http://en.wikipedia.org/wiki/X_Window_selection)
if you're not familiar with X selections.

For these commands to work, `xclip` must be installed.

`yu` copies the current URL to the primary selection

`yU` copies the URL of the hovered link to the primary selection

`yy` copies the page title to the primary selection

`p` goes to the URL in the primary selection

`P` goes to the URL in the clipboard selection

`'p` opens the URL in the primary selection in a new window

`Shift-Insert` pastes the primary selection into the status bar ("command"
mode) or active form ("insert" mode)

### Advanced Commands

`s` sets a variable

`:` issues an uzbl command

`!reload` reloads configuration file

`Ctrl-Mod1-t` opens a terminal that prints events and can issue commands to uzbl
