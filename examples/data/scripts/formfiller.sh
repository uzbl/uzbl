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
shift $#

keydir=$HOME/etc/uzbl/dforms

mkdir -p "$keydir" || exit

Ed () { "${VTERM:-xterm}" -e "${VISUAL:-${EDITOR:-vi}}" "$@"; }

Dmenu ()
{
    dmenu -p "choose profile" \
        ${DMENU_FONT+-fn "$DMENU_FONT"} \
        -nb "#0f0f0f" -nf "#4e7093" -sb "#003d7c" -sf "#3a9bff" \
        -l 4 \
        "$@"
}

domain=${UZBL_URI#*://}
domain=${domain%%/*}

test "$domain" || exit

file=$keydir/$domain

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
    if [ $(grep -c '^!profile' "$1") -gt 1 ]
    then sed -n 's/^!profile=//p' "$1" | Dmenu
    else sed -n 's/^!profile=//p' "$1"
    fi

ParseProfile ()
{
    sed "/^>/d; /^!profile=$1$/,/^!/!d; /^!/d"
}

ParseFields ()
{
    awk '/^%/ {

        sub ( /%/, "" )

        split( $0, parts, /\(|\)|\{|\}/ )

        field = $0
        sub ( /[^:]*:/, "", field )

        if ( parts[2] ~ /(text|password|search)/ )
            printf( "js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",0);\n",
                    parts[1], parts[2], field )

        else if ( parts[2] ~ /(checkbox|radio)/ )
            printf( "js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",%s);\n",
                    parts[1], parts[2], parts[3], field )

        else if ( parts[2] == "textarea" ) {
            field = ""
            while (getline) {
                if ( /^%/ ) break
                sub ( /^\\/, "" )
                gsub ( /"/, "\\\"" )
                gsub ( /\\/, "\\\\" )
                field = field $0 "\\n"
            }
            printf( "js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",0);\n",
                parts[1], parts[2], field )
        }

    }'
}

New ()
{
    { echo '!profile=NAME_THIS_PROFILE'
      GenForm | sed 's/^!/\\!/'
      echo '!'
    } >> "$file"
    chmod 600 "$file"
    Ed "$file"
}

Edit ()
    if [ -e "$file" ]
    then Ed "$file"
    else New
    fi

Load ()
{
    test -e "$file" || exit

    option=$(GetOption "$file")

    case $option in *[!a-zA-Z0-9_-]*) exit 1; esac

    ParseProfile $option < "$file" \
    | ParseFields \
    | sed 's/@/\\@/' \
    > "$UZBL_FIFO"
}

Once ()
{
    tmpfile=/tmp/${0##*/}-$$-tmpfile
    trap 'rm -f "$tmpfile"' EXIT

    GenForm > "$tmpfile"
    chmod 600 "$tmpfile"

    Ed "$tmpfile"

    test -e "$tmpfile" &&
    ParseFields < "$tmpfile" \
    | sed 's/@/\\@' \
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
