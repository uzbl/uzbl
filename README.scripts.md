# per-site-settings

This script may be used to apply settings based on the current URL. It is
usually placed in the configuration as:

```uzbl
@on_event LOAD_COMMIT spawn @scripts_dir/per-site-settings.py @data_home/uzbl/per-site-settings
```

The `per-site-settings` may be any of:

  - a file
  - a directory
  - an executable

If it is a directory, all files within the directory are read an concatenated
together. An executable is run without arguments and its output is read.

However it is structured, the output is parsed as such:

```
<url>
    <path>
        <command>
```

Where any level may be repeated multiple times.

The `url` section may be a regular expression or a literal string. If the value
is the *end* of the current URL, it is matched, otherwise if it matches as a
regular expression, it is matched.

If a `url` line matches, the next `path` entries will be tried. Each `path` may
also be either a regular expression or a literal. If the value is the *start*
of the current path, it is matched, otherwise if it matches as a regular
expression, it is matched.

If the last `url` and `path` sections both match, any `command` read until the
next `path` or `url` directive is passed to `uzbl`.

There are two special "commands":

  - `@`: clears the `url` and `path` matching flags; may be used to implement
    "all except `<path>`" semantics; and
  - `@@`: stops processing of the entire file.

## Example

For example, to disallow JavaScript on all sites except for anything at
`example.net` and only `example.com/allow-scripts`, the following file may be
used:

```
.
    /
        set enable_scripts 0
example.net
    /
        set enable_scripts 1
example.com
    /allow-scripts
        set enable_scripts 1
```
