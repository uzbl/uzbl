#!/bin/sh
#
# Sample uzbl key handler
#
# demonstrating one possible way to access and process
# uzbl's event messages
#
# Usage: uzbl | keyhandler
#

ALTPRESSED=0

clear_modifiers() {
    ALT_PRESSED=0
    CONTROL_PRESSED=0
    #etc.
    echo 'set status_message = ' > "$FIFO_PATH"
}

while read EVENT; do
    # get eventname
    ENAME=`echo "$EVENT" | sed -ne 's/\([A-Z]*\) .*/\1/p'`

    if [ x"$ENAME" = x'KEYPRESS' ]; then
        KEY=$(echo "$EVENT" | sed -ne 's/KEYPRESS \[.*\] \(.*$\)/\1/p')
        FIFO_PATH='/tmp/uzbl_fifo_'$(echo "$EVENT" | sed -ne 's/KEYPRESS \[\(.*\)\] .*$/\1/p')

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
            echo 'set status_message = @status_message <span foreground="red" weight="bold">Alt</span>' > "$FIFO_PATH"
        fi

        if [ "$ALT_PRESSED" = 1 ]; then
            
            # Keys
            #
            if [ x"$KEY" = x'a' ]; then
                ALT_PRESSED=0
                echo 'set inject_html = <html><body> <h1>You pressed Alt+a </h1> </body></html>' > "$FIFO_PATH"
                echo 'set status_message = ' > "$FIFO_PATH"
                
                # delete keycmd
                # here not needed. loading a new page
                # resets it by default
                #
                #echo 'set keycmd = ' > "$F_PATH"
            fi
            if [ x"$KEY" = x'b' ]; then
                ALT_PRESSED=0
                echo 'set inject_html = <html><body> <h1>You pressed Alt+b </h1> </body></html>' > "$FIFO_PATH"
                echo 'set status_message = ' > "$FIFO_PATH"
            fi

        fi

        # Modifier: Control_L and Control_R.
        #
        if [ x"$KEY" = x'Control_L' -o x"$KEY" = x'Control_R' ];then
            clear_modifiers
            CONTROL_PRESSED=1
            echo 'set status_message = @status_message <span foreground="red" weight="bold">Control</span>' > "$FIFO_PATH" 
            #etc.
        fi
    fi
done
