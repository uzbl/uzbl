#!/bin/sh
# script to determine git hash and latest tag of current source tree

# set a variable when running `git archive <hash/tag>` (this is what github does)
# alternatively, you could also git get-tar-commit-id < tarball (but that's a bit dirtier)

# the `%` expansions possible here are described in `man git-log`
readonly tarball_check='$Format:$'

# ... but try to use whatever git tells us if there is a .git folder
if [ -d .git ] && [ -r .git ]; then
    git describe --tags --always
elif [ -z "$tarball_check" ]; then
    readonly tarball_tag="$(echo '$Format:%D$' | sed -n 's/.*tag: \(v[0-9]\.[0-9.]*\).*/\1/p')"
    if [ -n "$tarball_tag" ]; then
        echo "$tarball_tag"
    else
        echo '$Format:%h$'
    fi
else
    echo >&2 "Commit hash detection fail. Dear packager, please figure out what goes wrong or get in touch with us"
    echo UNKNOWN
    exit 2
fi
