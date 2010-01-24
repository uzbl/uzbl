/*
 * Edit forms in external editor
 *
 * (c) 2009, Robert Manea
 * utf8 functions are (c) by Webtoolkit.info (http://www.webtoolkit.info/)
 *
 *
 * Installation:
 *      - Copy this script to $HOME/.local/share/uzbl/scripts
 *      - Add the following to $HOME/.config/uzbl/config:
 *          @bind E = script @scripts_dir/extedit.js
 *      - Set your preferred editor
 *          set editor = gvim
 *      - non-GUI editors
 *          set editor = xterm -e vim
 *
 * Usage:
 *      Select (click) an editable form, go to command mode and hit E
 *
*/


function utf8_decode ( str_data ) {
    var tmp_arr = [], i = 0, ac = 0, c1 = 0, c2 = 0, c3 = 0;

    str_data += '';

    while ( i < str_data.length ) {
        c1 = str_data.charCodeAt(i);
        if (c1 < 128) {
            tmp_arr[ac++] = String.fromCharCode(c1);
            i++;
        } else if ((c1 > 191) && (c1 < 224)) {
            c2 = str_data.charCodeAt(i+1);
            tmp_arr[ac++] = String.fromCharCode(((c1 & 31) << 6) | (c2 & 63));
            i += 2;
        } else {
            c2 = str_data.charCodeAt(i+1);
            c3 = str_data.charCodeAt(i+2);
            tmp_arr[ac++] = String.fromCharCode(((c1 & 15) << 12) | ((c2 & 63) << 6) | (c3 & 63));
            i += 3;
        }
    }

    return tmp_arr.join('');
}


function utf8_encode ( argString ) {
    var string = (argString+''); // .replace(/\r\n/g, "\n").replace(/\r/g, "\n");

    var utftext = "";
    var start, end;
    var stringl = 0;

    start = end = 0;
    stringl = string.length;
    for (var n = 0; n < stringl; n++) {
        var c1 = string.charCodeAt(n);
        var enc = null;

        if (c1 < 128) {
            end++;
        } else if (c1 > 127 && c1 < 2048) {
            enc = String.fromCharCode((c1 >> 6) | 192) + String.fromCharCode((c1 & 63) | 128);
        } else {
            enc = String.fromCharCode((c1 >> 12) | 224) + String.fromCharCode(((c1 >> 6) & 63) | 128) + String.fromCharCode((c1 & 63) | 128);
        }
        if (enc !== null) {
            if (end > start) {
                utftext += string.substring(start, end);
            }
            utftext += enc;
            start = end = n+1;
        }
    }

    if (end > start) {
        utftext += string.substring(start, string.length);
    }

    return utftext;
}


(function() {
 var actelem  = document.activeElement;

 if(actelem.type == 'text' || actelem.type == 'textarea') {
    var editor   = Uzbl.run("print @external_editor") || "gvim";
    var filename = Uzbl.run("print @(mktemp /tmp/uzbl_edit.XXXXXX)@");

    if(actelem.value)
        Uzbl.run("sh 'echo " + window.btoa(utf8_encode(actelem.value)) + " | base64 -d > " + filename + "'");

    Uzbl.run("sync_sh '" + editor + " " + filename + "'");
    actelem.value = utf8_decode(window.atob(Uzbl.run("print @(base64 -w 0 " + filename + ")@")));

    Uzbl.run("sh 'rm -f " + filename + "'");
 }

 })();
