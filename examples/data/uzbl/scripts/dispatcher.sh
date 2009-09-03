#!/bin/sh
#
# Sample uzbl eventhandler
#
# demonstrating one possible way to access and process
# uzbl's event messages
#
# Usage: uzbl | eventhandler
#

VERBOSE=1


ALTPRESSED=0
KEYCMD= # buffer for building up commands

log () {
	[ -n "$VERBOSE" ] && echo "$1"
}

fifo () {
	log "$1 > ${FIFO_PATH:-unset}"
	[ -n "$FIFO_PATH" ] && echo "$1" > "$FIFO_PATH"
}

log "Init eventhandler"

clear_modifiers() {
    ALT_PRESSED=0
    CONTROL_PRESSED=0
    #etc.
    fifo 'set status_message = '
}

while read EVENT; do
	if [ "$(echo $EVENT | cut -d ' ' -f 1)" != 'EVENT' ]; then
		continue;
	fi
	EVENT="`echo $EVENT | sed 's/^EVENT //'`"
    # get eventname
    ENAME=`echo "$EVENT" | sed -ne 's/\([A-Z]*\) .*/\1/p'`

    if [ x"$ENAME" = 'xFIFO_SET' ]; then
        FIFO_PATH=`echo $EVENT | cut -d ' ' -f 3`

    elif [ x"$ENAME" = x'KEY_PRESS' ]; then
        KEY=$(echo "$EVENT" | sed -ne 's/KEY_PRESS \[.*\] \(.*$\)/\1/p')

        # Clear mofifiers on Escape
        #
        [ "$KEY" = Escape ] && clear_modifiers

        # Modifier: Alt_L
        #
        if [ x"$KEY" = x'Alt_L' ];then
            clear_modifiers
            ALT_PRESSED=1

            # place an indicator showing the active modifier
            # on uzbl's statusbar
            #
            fifo 'set status_message = @status_message <span foreground="red" weight="bold">Alt</span>'
        fi

        if [ "$ALT_PRESSED" = 1 ]; then
            
            # Keys
            #
            if [ x"$KEY" = x'a' ]; then
                ALT_PRESSED=0
                fifo 'set inject_html = <html><body> <h1>You pressed Alt+a </h1> </body></html>'
                fifo 'set status_message = '
                
                # delete keycmd
                # here not needed. loading a new page
                # resets it by default
                #
                #echo 'set keycmd = ' > "$F_PATH"
            fi
            if [ x"$KEY" = x'b' ]; then
                ALT_PRESSED=0
                fifo 'set inject_html = <html><body> <h1>You pressed Alt+b </h1> </body></html>'
                fifo 'set status_message = '
            fi

        fi

        # Modifier: Control_L and Control_R.
        #
        if [ x"$KEY" = x'Control_L' -o x"$KEY" = x'Control_R' ];then
            clear_modifiers
            CONTROL_PRESSED=1
            fifo 'set status_message = @status_message <span foreground="red" weight="bold">Control</span>'
            #etc.
        fi
    fi
done
