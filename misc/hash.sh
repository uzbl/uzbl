#!/bin/sh
# script to determine git hash and latest tag of current source tree

# set a variable when running `git archive <hash/tag>` (this is what github does)
# alternatively, you could also git get-tar-commit-id < tarball (but that's a bit dirtier)

# the `%` expansions possible here are described in `man git-log`
FROM_ARCHIVE=$Format:%h$

# ... but try to use whatever git tells us if there is a .git folder
if [ -d .git -a -r .git ]
then
	hash=$(git describe --tags)
  echo $hash
elif [ "$FROM_ARCHIVE" != ':%h$' ]
then
	echo $FROM_ARCHIVE
else
	echo "commit hash detection fail.  Dear packager, please figure out what goes wrong or get in touch with us" >&2
	echo UNKNOWN
	exit 2
fi
exit 0
