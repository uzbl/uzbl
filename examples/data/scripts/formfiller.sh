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
. "$UZBL_UTIL_DIR/uzbl-util.sh"

mkdir -p "$UZBL_FORMS_DIR" || exit

domain="${UZBL_URI#*://}"
domain="${domain%%/*}"

test "$domain" || exit

basefile="$UZBL_FORMS_DIR/$domain"
default_profile="default"

action="$1"
shift

generate_form () {
    uzbl_control "js uzbl.formfiller.dump();\n" \
    | awk '
        /^formfillerstart$/ {
            while (getline) {
                if (/^%!end/) {
                    exit
                }
                print
            }
        }
    '
}

get_option () {
    DMENU_SCHEME="formfiller"
    DMENU_PROMPT="profile"
    DMENU_LINES=4

    . "$UZBL_UTIL_DIR/dmenu.sh"

    count=""

    for x in "$basefile"*; do
        [ "$x" = "$basefile*" ] && continue

        count="1$count"
    done

    case "$count" in
        11*)
            DMENU_MORE_ARGS=
            if [ -n "$DMENU_HAS_PLACEMENT" ]; then
                . "$UZBL_UTIL_DIR/uzbl-window.sh"

                DMENU_MORE_ARGS="$DMENU_PLACE_X $(( $UZBL_WIN_POS_X + 1 )) $DMENU_PLACE_Y $(( $UZBL_WIN_POS_Y + $UZBL_WIN_HEIGHT - 184 )) $DMENU_PLACE_WIDTH $UZBL_WIN_WIDTH"
            fi

            ls "$basefile"* | sed -e 's!^'"$basefile"'\.!!' | $DMENU $DMENU_MORE_ARGS
            ;;
        1)
            echo "$basefile"*
            ;;
        *)
            ;;
    esac
}

parse_profile () {
    awk '/^%/ {

        sub(/%/, "")
        gsub(/\\/, "\\\\\\\\")
        gsub(/@/, "\\@")
        gsub(/"/, "\\\"")

        split($0, parts, /\(|\)|\{|\}/)

        field = $0
        sub(/[^:]*:/, "", field)

        if (parts[2] ~ /^(checkbox|radio)$/) {
            printf("js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",%s);\n",
                parts[1], parts[2], parts[3], field)
        } else if (parts[2] == "textarea") {
            field = ""
            while (getline) {
                if (/^%/) {
                    break
                }
                sub(/^\\/, "")
                # JavaScript escape
                gsub(/\\/, "\\\\\\\\")
                gsub(/"/, "\\\"")
                # To support the possibility of the last line of the textarea
                # not being terminated by a newline, we add the newline here.
                # The "if (field)" is so that this does not happen in the first
                # iteration.
                if (field) {
                    field = field "\\n"
                }
                field = field $0
            }
            # Uzbl escape
            gsub(/\\/, "\\\\\\\\", field)
            gsub(/@/, "\\@", field)
            printf("js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",0);\n",
                parts[1], parts[2], field)
        } else {
            printf("js uzbl.formfiller.insert(\"%s\",\"%s\",\"%s\",0);\n",
                parts[1], parts[2], field)
        }

    }'
}

new_profile () {
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

    generate_form > "$file"
    chmod 600 "$file"
    $UZBL_EDITOR "$file"
}

edit_profile () {
    profile="$( get_option )"

    [ -z "$profile" ] && profile="$default_profile"

    file="$basefile.$profile"

    if [ -e "$file" ]; then
        $UZBL_EDITOR "$file"
    else
        new_profile "$profile"
    fi
}

load_profile () {
    if [ -z "$file" ]; then
        profile="$( get_option )"

        [ -z "$profile" ] && profile="$default_profile"

        file="$basefile.$profile"
    fi

    parse_profile < "$file" \
    | uzbl_send
}

one_time_profile ()
{
    tmpfile="$XDG_SOCKET_DIR/uzbl/formfiller-${0##*/}-$$"
    trap 'rm -f "$tmpfile"' EXIT

    generate_form > "$tmpfile"
    chmod 600 "$tmpfile"

    $UZBL_EDITOR "$tmpfile"

    test -e "$tmpfile" &&
    parse_profile < "$tmpfile" \
    | uzbl_send
}

case "$action" in
    new)
        new_profile
        load_profile
        ;;
    edit)
        edit_profile
        load_profile
        ;;
    load)
        load_profile
        ;;
    once)
        one_time_profile
        ;;
    '')
        if [ -e "$file" ]; then
            load_profile
        else
            new_profile
            load_profile
        fi
        ;;
    *)
        exit 1
        ;;
esac
