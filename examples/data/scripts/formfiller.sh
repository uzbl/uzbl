#!/bin/sh
#
# Enhanced html form (eg for logins) filler (and manager) for uzbl.
#
# uses settings files like: $keydir/<domain>
# files contain lines like: !profile=<profile_name>
#                           <fieldname>(fieldtype): <value>
# profile_name should be replaced with a name that will tell sth about that 
#       profile
# fieldtype can be checkbox, text or password, textarea - only for information 
#                               pupropse (auto-generated) - don't change that
#
# Texteares: for textareas edited text can be now splitted into more lines.
#       If there will be text, that doesn't match key line:
#                    <fieldname>(fieldtype):<value>
#       then it will be considered as a multiline for the first field above it
#       Keep in mind, that if you make more than one line for fileds like input
#       text fields, then all lines will be inserted into as one line
#
# Checkboxes/radio-buttons: to uncheck it type on of the following after the
#       colon:
#           no
#           off
#           0
#           unchecked
#           false
#       or leave it blank, even without spaces
#       otherwise it will be considered as checked
#
# user arg 1:
# edit: force editing the file (falls back to new if not found)
# new:  start with a new file.
# load: try to load from file into form
# add: try to add another profile to an existing file
# once: edit form using external editor
#
# something else (or empty): if file not available: new, otherwise load.
#

# config dmenu colors and prompt
NB="#0f0f0f"
NF="#4e7093"
SB="#003d7c"
SF="#3a9bff"

if [ "`dmenu --help 2>&1| grep lines`x" != "x" ]
then
    LINES=" -l 3 "
else
    LINES=""
fi

PROMPT="Choose profile"
MODELINE="> vim:ft=formfiller"

keydir=${XDG_DATA_HOME:-$HOME/.local/share}/uzbl/dforms

[ -d "`dirname $keydir`" ] || exit 1
[ -d "$keydir" ] || mkdir "$keydir"

editor="${VISUAL}"
if [ -z "${editor}" ]; then
    if [ -z "${EDITOR}" ]; then
        editor='xterm -e vim'
    else
        editor="xterm -e ${EDITOR}"
    fi
fi

config=$1;
shift
pid=$1;
shift
xid=$1;
shift
fifo=$1;
shift
socket=$1;
shift
url=$1;
shift
title=$1;
shift
action=$1

[ -d $keydir ] || mkdir $keydir || exit 1

domain=$(echo $url | sed 's/\(http\|https\):\/\/\([^\/]\+\)\/.*/\2/')

if [ "$action" != 'edit' -a  "$action" != 'new' -a "$action" != 'load' -a "$action" != 'add' -a "$action" != 'once' ]
then
    action="new"
    [ -e "$keydir/$domain" ] && action="load"
elif [ "$action" = 'edit' ] && [ ! -e "$keydir/$domain" ]
then
    action="new"
fi

dumpFunction="function dump() { \
    var rv=''; \
    var allFrames = new Array(window); \
    for(f=0;f<window.frames.length;f=f+1) { \
        allFrames.push(window.frames[f]); \
    } \
    for(j=0;j<allFrames.length;j=j+1) { \
        try { \
            var xp_res=allFrames[j].document.evaluate('//input', allFrames[j].document.documentElement, null, XPathResult.ANY_TYPE,null); \
            var input; \
            while(input=xp_res.iterateNext()) { \
                var type=(input.type?input.type:text); \
                if(type == 'text' || type == 'password' || type == 'search') { \
                    rv += input.name + '(' + type + '):' + input.value + '\\\\n'; \
                } \
                else if(type == 'checkbox' || type == 'radio') { \
                    rv += input.name + '{' + input.value + '}(' + type + '):' + (input.checked?'ON':'OFF') + '\\\\n'; \
                } \
            }  \
            xp_res=allFrames[j].document.evaluate('//textarea', allFrames[j].document.documentElement, null, XPathResult.ANY_TYPE,null); \
            var input; \
            while(input=xp_res.iterateNext()) { \
                rv += input.name + '(textarea):' + input.value + '\\\\n'; \
            } \
        } \
        catch(err) { } \
    } \
    return rv; \
}; "

insertFunction="function insert(fname, ftype, fvalue, fchecked) { \
    var allFrames = new Array(window); \
    for(f=0;f<window.frames.length;f=f+1) { \
        allFrames.push(window.frames[f]); \
    }  \
    for(j=0;j<allFrames.length;j=j+1) { \
        try { \
            if(ftype == 'text' || ftype == 'password' || ftype == 'search' || ftype == 'textarea') { \
                allFrames[j].document.getElementsByName(fname)[0].value = fvalue; \
            } \
            else if(ftype == 'checkbox') { \
                allFrames[j].document.getElementsByName(fname)[0].checked = fchecked;\
            } \
            else if(ftype == 'radio') { \
                var radios = allFrames[j].document.getElementsByName(fname); \
                for(r=0;r<radios.length;r+=1) { \
                    if(radios[r].value == fvalue) { \
                        radios[r].checked = fchecked; \
                    } \
                } \
            } \
        } \
        catch(err) { } \
    } \
}; "

if [ "$action" = 'load' ]
then
    [ -e $keydir/$domain ] || exit 2
    if [ `cat $keydir/$domain|grep "!profile"|wc -l` -gt 1 ]
    then
        menu=`cat $keydir/$domain| \
              sed -n 's/^!profile=\([^[:blank:]]\+\)/\1/p'`
        option=`echo -e -n "$menu"| dmenu ${LINES} -nb "${NB}" -nf "${NF}" -sb "${SB}" -sf "${SF}" -p "${PROMPT}"`
    fi

    # Remove comments
    sed '/^>/d' -i $tmpfile

    sed 's/^\([^{]\+\){\([^}]*\)}(\(radio\|checkbox\)):\(off\|no\|false\|unchecked\|0\|$\)/\1{\2}(\3):0/I;s/^\([^{]\+\){\([^}]*\)}(\(radio\|checkbox\)):[^0]\+/\1{\2}(\3):1/I' -i $keydir/$domain
    fields=`cat $keydir/$domain | \
        sed -n "/^!profile=${option}/,/^!profile=/p" | \
        sed '/^!profile=/d' | \
        sed 's/^\([^(]\+(\)\(radio\|checkbox\|text\|search\|textarea\|password\)):/%{>\1\2):<}%/' | \
        sed 's/^\(.\+\)$/<{br}>\1/' | \
        tr -d '\n' | \
        sed 's/<{br}>%{>\([^(]\+(\)\(radio\|checkbox\|text\|search\|textarea\|password\)):<}%/\\n\1\2):/g'`
    printf '%s\n' "${fields}" | \
        sed -n -e "s/\([^(]\+\)(\(password\|text\|search\|textarea\)\+):[ ]*\(.\+\)/js $insertFunction; insert('\1', '\2', '\3', 0);/p" | \
        sed -e 's/@/\\@/g;s/<{br}>/\\\\n/g' > $fifo
    printf '%s\n' "${fields}" | \
        sed -n -e "s/\([^{]\+\){\([^}]*\)}(\(radio\|checkbox\)):[ ]*\(.\+\)/js $insertFunction; insert('\1', '\3', '\2', \4);/p" | \
        sed -e 's/@/\\@/g' > $fifo
elif [ "$action" = "once" ]
then
    tmpfile=`mktemp`
    printf 'js %s dump(); \n' "$dumpFunction" | \
        socat - unix-connect:$socket | \
        sed -n '/^[^(]\+([^)]\+):/p' > $tmpfile
    echo "$MODELINE" >> $tmpfile
    ${editor} $tmpfile

    [ -e $tmpfile ] || exit 2

    # Remove comments
    sed '/^>/d' -i $tmpfile

    sed 's/^\([^{]\+\){\([^}]*\)}(\(radio\|checkbox\)):\(off\|no\|false\|unchecked\|0\|$\)/\1{\2}(\3):0/I;s/^\([^{]\+\){\([^}]*\)}(\(radio\|checkbox\)):[^0]\+/\1{\2}(\3):1/I' -i $tmpfile
    fields=`cat $tmpfile | \
        sed 's/^\([^(]\+(\)\(radio\|checkbox\|text\|search\|textarea\|password\)):/%{>\1\2):<}%/' | \
        sed 's/^\(.\+\)$/<{br}>\1/' | \
        tr -d '\n' | \
        sed 's/<{br}>%{>\([^(]\+(\)\(radio\|checkbox\|text\|search\|textarea\|password\)):<}%/\\n\1\2):/g'`
    printf '%s\n' "${fields}" | \
        sed -n -e "s/\([^(]\+\)(\(password\|text\|search\|textarea\)\+):[ ]*\(.\+\)/js $insertFunction; insert('\1', '\2', '\3', 0);/p" | \
        sed -e 's/@/\\@/g;s/<{br}>/\\\\n/g' > $fifo
    printf '%s\n' "${fields}" | \
        sed -n -e "s/\([^{]\+\){\([^}]*\)}(\(radio\|checkbox\)):[ ]*\(.\+\)/js $insertFunction; insert('\1', '\3', '\2', \4);/p" | \
        sed -e 's/@/\\@/g' > $fifo
    rm -f $tmpfile
else
    if [ "$action" = 'new' -o "$action" = 'add' ]
    then
	[ "$action" = 'new' ] && echo "$MODELINE" > $keydir/$domain
        echo "!profile=NAME_THIS_PROFILE$RANDOM" >> $keydir/$domain
        #
        # 2. and 3. line (tr -d and sed) are because, on gmail login for example,
        # <input > tag is splited into lines
        # ex:
        # <input name="Email"
        #        type="text"
        #        value="">
        # So, tr removes all new lines, and sed inserts new line after each >
        # Next sed selects only <input> tags and only with type = "text" or = "password"
        # If type is first and name is second, then another sed will change their order
        # so the last sed will make output
        #       text_from_the_name_attr(text or password):
        #
        #       login(text):
        #       passwd(password):
        #
        printf 'js %s dump(); \n' "$dumpFunction" | \
            socat - unix-connect:$socket | \
            sed -n '/^[^(]\+([^)]\+):/p' >> $keydir/$domain
    fi
    [ -e "$keydir/$domain" ] || exit 3 #this should never happen, but you never know.
    $editor "$keydir/$domain" #TODO: if user aborts save in editor, the file is already overwritten
fi

# vim:fileencoding=utf-8:sw=4
