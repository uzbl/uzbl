#!/bin/sh
#
# action
# new:  add new profile template (creates file if not found), then edit
# edit: edit file (fall back to 'new' if file not found)
# load: load from file
# once: use temporary file to edit form once
# (empty): if file not available, new; otherwise, load
#

. "$UZBL_UTIL_DIR/editor.sh"
. "$UZBL_UTIL_DIR/uzbl-dir.sh"

mkdir -p "$UZBL_FORMS_DIR" || exit

domain=${UZBL_URI#*://}
domain=${domain%%/*}

test "$domain" || exit

basefile="$UZBL_FORMS_DIR/$domain"
default_profile="default"

action=$1
shift

GenForm ()
{
    printf "js uzbl.formfiller.dump()\n" \
    | socat - "unix-connect:$UZBL_SOCKET" \
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
    DMENU_SCHEME="formfiller"
    DMENU_PROMPT="profile"

    . "$UZBL_UTIL_DIR/dmenu.sh"

    count=""

    for x in "$basefile"*; do
        [ "$x" = "$basefile*" ] && continue

        count="1$count"
    done

    case "$count" in
        11*)
            ls "$basefile"* | sed -e 's!^'"$basefile"'\.!!' | $DMENU
            ;;
        1)
            echo "$basefile"*
            ;;
        *)
            ;;
    esac
}

ParseFields ()
{
    awk '/^%/ {

        sub ( /%/, "" )

        split( $0, parts, /\(|\)|\{|\}/ )

        field = $0
        sub ( /[^:]*:/, "", field )

        if ( parts[2] ~ /^(checkbox|radio)$/ ) {
            printf( "js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",%s)\n",
                    parts[1], parts[2], parts[3], field )
        } else if ( parts[2] ~ /^textarea$/ ) {
            field = ""
            while (getline) {
                if ( /^%/ ) break
                sub ( /^\\/, "" )
                gsub ( /"/, "\\\"" )
                gsub ( /\\/, "\\\\" )
                field = field $0 "\\\\n"
            }
            printf( "js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",0)\n",
                parts[1], parts[2], field )
        } else {
            printf( "js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",0)\n",
                    parts[1], parts[2], field )
        }

    }'
}

New () {
    file="$1"
    shift

    if [ -z "$file" ]; then
        profile="$default_profile"

        while true; do
            profile="$( Xdialog --stdout --title "New profile for $domain" --inputbox "Profile name:" 0 0 "$profile" )"
            exitstatus="$?"

            [ "$exitstatus" -eq 0 ] || exit "$exitstatus"

            [ -z "$profile" ] && profile="$default_profile"

            file="$basefile.$profile"

            if [ -e "$file" ]; then
                Xdialog --title "Profile already exists!" --yesno "Overwrite?" 0 0
                exitstatus="$?"

                [ "$exitstatus" -eq 0 ] && break
            else
                break
            fi
        done
    fi

    GenForm > "$file"
    chmod 600 "$file"
    $UZBL_EDITOR "$file"
}

Edit () {
    profile="$( GetOption )"

    [ -z "$profile" ] && profile="$default_profile"

    file="$basefile.$profile"

    if [ -e "$file" ]; then
        $UZBL_EDITOR "$file"
    else
        New "$profile"
    fi
}

Load ()
{
    if [ -z "$file" ]; then
        profile="$( GetOption )"

        [ -z "$profile" ] && profile="$default_profile"

        file="$basefile.$profile"
    fi

    ParseFields < "$file" \
    | sed 's/@/\\@/g' \
    | socat - "unix-connect:$UZBL_SOCKET"
}

Once ()
{
    tmpfile="/tmp/${0##*/}-$$-tmpfile"
    trap 'rm -f "$tmpfile"' EXIT

    GenForm > "$tmpfile"
    chmod 600 "$tmpfile"

    $UZBL_EDITOR "$tmpfile"

    test -e "$tmpfile" &&
    ParseFields < "$tmpfile" \
    | sed 's/@/\\@/g' \
    | socat - "unix-connect:$UZBL_SOCKET"
}

case "$action" in
    new)
        New
        Load
        ;;
    edit)
        Edit
        Load
        ;;
    load)
        Load
        ;;
    once)
        Once
        ;;
    '')
        if [ -e "$file" ]; then
            Load
        else
            New
            Load
        fi
        ;;
    *)
        exit 1
        ;;
esac
