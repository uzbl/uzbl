## Packages

Uzbl is available through the package managers of Arch, Debian, Fedora, Gentoo,
Ubuntu and more. If you cannot find a package for your system you may find a
tutorial on [uzbl.org/wiki/howtos](http://www.uzbl.org/wiki/howtos)

## From source

You can pull the code from git or get a tagged tarball.

    $ git clone git://github.com/uzbl/uzbl.git
    $ cd uzbl
    [ $ git checkout origin/next ] # optional. see below
    $ make
    $ sudo make install

Persistent settings (`PREFIX`, GTK version, etc.) may be set in a `local.mk`
file. If you want to remove uzbl again, you can issue:

    $ make uninstall

Tarballs can be pulled from
[github.com/uzbl/uzbl/downloads](http://github.com/uzbl/uzbl/downloads)

Though you can only get tagged versions from the master branch, which may be
older then what you'll have through git.

If you want the specific subprojects, you can issue:

  $ sudo make install-uzbl-core
  $ sudo make install-uzbl-browser
  $ sudo make install-uzbl-tabbed

## Dependencies

Dependencies which are optional for `uzbl-core` are marked with an asterisk.
(i.e. these are needed for extra scripts)

* libwebkit 1.2.4 or higher
* libsoup 2.33.4 or higher (dep for webkit/gtk+)
* gtk 2.14 or higher
* libgnutls
* socat (for socket communication) (optional)
* dmenu (vertical patch recommended) (optional)
* zenity (optional)
* bash (optional)
* python (optional)
* xclip (optional)
* pygtk (optional)
* pygobject (optional)

## Make dependencies

* `git` (for downloading)
* `pkg-config` (for finding WebKit, GTK, and other dependencies)
* `python3-setuptools`

## Git Repo's & branches

* Main official repo:
  http://github.com/uzbl/uzbl
- master -> main development branch
- experimental -> intrusive/experimental stuff
- next -> future development
- <..>  -> specific topic branches that come and go. they are a place to work on specific features
