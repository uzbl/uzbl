#!/bin/sh
# uzbl's example configuration sets this script up as its download_handler.
# when uzbl starts a download it runs this script.
# if the script prints a file path to stdout, uzbl will save the download to
# that path.
# if nothing is printed to stdout, the download will be cancelled.

. "$UZBL_UTIL_DIR/uzbl-dir.sh"

# the URL that is being downloaded
uri="$1"
shift

safe_uri="$( echo "$uri" | sed -e 's/\W/-/g' )"

# a filename suggested by the server or based on the URL
suggested_filename="${1:-$safe_uri}"
shift

# the mimetype of the file being downloaded
content_type="$1"
shift

# the size of the downloaded file in bytes. this is not always accurate, since
# the server might not have sent a size with its response headers.
total_size="$1"
shift

case "$suggested_filename" in
    # example: save torrents to a separate directory
    #*.torrent)
    #    path="$UZBL_DOWNLOAD_DIR/torrents/$suggested_filename"
    #    ;;
    # Default case
    *)
        path="$UZBL_DOWNLOAD_DIR/$suggested_filename"
        ;;
esac

# Do nothing if we don't want to save the file
[ -z "$path" ] && exit 0

# Check if the file exists
if [ ! -e "$path" ]; then
    echo "$path"
    exit 0
fi

# Try to make a unique filename
count=1
while [ -e "$path.$count" ]; do
    count=$(( $count + 1 ))
done

echo "$path.$count"
