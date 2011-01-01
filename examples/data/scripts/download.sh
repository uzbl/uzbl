#!/bin/sh
#
# uzbl's example configuration sets this script up as its download_handler.
# when uzbl starts a download it runs this script.
# if the script prints a file path to stdout, uzbl will save the download to
# that path.
# if nothing is printed to stdout, the download will be cancelled.

. $UZBL_UTIL_DIR/uzbl-dir.sh

# the URL that is being downloaded
uri=$1

# a filename suggested by the server or based on the URL
suggested_filename=${2:-$(echo "$uri" | sed 's/\W/-/g')}

# the mimetype of the file being downloaded
content_type=$3

# the size of the downloaded file in bytes. this is not always accurate, since
# the server might not have sent a size with its response headers.
total_size=$4

# just save the file to the default directory with the suggested name
echo $UZBL_DOWNLOAD_DIR/$suggested_filename
