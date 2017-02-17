In order to configure the background color of selected text in the uzbl
UI status bar, add this into your
**\$XDG\_CONFIG\_HOME/gtk-3.0/gtk.css**

    #Uzbl label selection {
      background-color: #fdf6e3;
    }

The according CSS selectors can be found via the GTK inspector
(`GTK_DEBUG=interactive uzbl-browser`).

See https://github.com/uzbl/uzbl/pull/338 for further discussion.
