#!/bin/sh
#
# action
# new:  add new profile template (creates file if not found), then edit
# edit: edit file (fall back to 'new' if file not found)
# load: load from file
# once: use temporary file to edit form once
# (empty): if file not available, new; otherwise, load
#

action=$1

. "$UZBL_UTIL_DIR/uzbl-dir.sh"
. "$UZBL_UTIL_DIR/editor.sh"

mkdir -p "$UZBL_FORMS_DIR" || exit

domain=${UZBL_URI#*://}
domain=${domain%%/*}

test "$domain" || exit

file=$UZBL_FORMS_DIR/$domain

GenForm ()
{
    echo 'js uzbl.formfiller.dump();' \
    | socat - unix-connect:"$UZBL_SOCKET" \
    | awk '
        /^formfillerstart$/ {
            while (getline) {
                if ( /^%!end/ ) exit
                print
            }
        }
    '
}

GetOption ()
{
    DMENU_SCHEME=formfiller

    # util/dmenu.sh doesn't handle spaces in DMENU_PROMPT. a proper fix will be
    # tricky.
    DMENU_PROMPT="choose_profile"
    DMENU_LINES=4

    . "$UZBL_UTIL_DIR/dmenu.sh"

    if [ $(grep -c '^!profile' "$1") -gt 1 ]
    then sed -n 's/^!profile=//p' "$1" | $DMENU
    else sed -n 's/^!profile=//p' "$1"
    fi
}

ParseProfile ()
{
    sed "/^>/d; /^!profile=$1$/,/^!/!d; /^!/d"
}

ParseFields ()
{
    awk '/^%/ {

        sub ( /%/, "" )
        gsub ( /\\/, "\\\\\\\\" )
        gsub ( /@/, "\\@" )
        gsub ( /"/, "\\\"" )

        split( $0, parts, /\(|\)|\{|\}/ )

        field = $0
        sub ( /[^:]*:/, "", field )

        if ( parts[2] ~ /^(checkbox|radio)$/ )
            printf( "js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",%s);\n",
                    parts[1], parts[2], parts[3], field )

        else if ( parts[2] == "textarea" ) {
            field = ""
            while (getline) {
                if ( /^%/ ) break
                sub ( /^\\/, "" )
                # JavaScript escape
                gsub ( /\\/, "\\\\\\\\" )
                gsub ( /"/, "\\\"" )
                # To support the possibility of the last line of the textarea
                # not being terminated by a newline, we add the newline here.
                # The "if (field)" is so that this does not happen in the first
                # iteration.
                if (field) field = field "\\n"
                field = field $0
            }
            # Uzbl escape
            gsub ( /\\/, "\\\\\\\\", field )
            gsub ( /@/, "\\@", field )
            printf( "js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",0);\n",
                parts[1], parts[2], field )
        }

        else
            printf( "js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",0);\n",
                    parts[1], parts[2], field )


    }'
}

New ()
{
    { echo '!profile=NAME_THIS_PROFILE'
      GenForm | sed 's/^!/\\!/'
      echo '!'
    } >> "$file"
    chmod 600 "$file"
    $UZBL_EDITOR "$file"
}

Edit ()
    if [ -e "$file" ]
    then $UZBL_EDITOR "$file"
    else New
    fi

Load ()
{
    test -e "$file" || exit

    option=$(GetOption "$file")

    case $option in *[!a-zA-Z0-9_-]*) exit 1; esac

    ParseProfile $option < "$file" \
    | ParseFields \
    > "$UZBL_FIFO"
}

Once ()
{
    tmpfile=/tmp/${0##*/}-$$-tmpfile
    trap 'rm -f "$tmpfile"' EXIT

    GenForm > "$tmpfile"
    chmod 600 "$tmpfile"

    $UZBL_EDITOR "$tmpfile"

    test -e "$tmpfile" &&
    ParseFields < "$tmpfile" \
    > "$UZBL_FIFO"
}

case $action in
    new) New; Load ;;
    edit) Edit; Load ;;
    load) Load ;;
    once) Once ;;
    '') if [ -e "$file" ]; then Load; else New; Load; fi ;;
    *) exit 1
esac
