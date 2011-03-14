uzbl.formfiller = {

    dump: function() {
        var rv = '';
        var allFrames = new Array(window);
        for ( f=0; f<window.frames.length; ++f ) {
            allFrames.push(window.frames[f]);
        }
        for ( j=0; j<allFrames.length; ++j ) {
            try {
                var xp_res = allFrames[j].document.evaluate(
                    '//input', allFrames[j].document.documentElement, null, XPathResult.ANY_TYPE,null
                );
                var input;
                while ( input = xp_res.iterateNext() ) {
                    var type = (input.type?input.type:text);
                    if ( type == 'text' || type == 'password' || type == 'search' ) {
                        rv += '%' + escape(input.name) + '(' + type + '):' + input.value + '\n';
                    }
                    else if ( type == 'checkbox' || type == 'radio' ) {
                        rv += '%' + escape(input.name) + '(' + type + '){' + escape(input.value) + '}:' + (input.checked?'1':'0') + '\n';
                    }
                }
                xp_res = allFrames[j].document.evaluate(
                    '//textarea', allFrames[j].document.documentElement, null, XPathResult.ANY_TYPE,null
                );
                var input;
                while ( input = xp_res.iterateNext() ) {
                    rv += '%' + escape(input.name) + '(textarea):\n' + input.value.replace(/\n%/g,"\n\\%") + '\n%\n';
                }
            }
            catch (err) { }
        }
        return 'formfillerstart\n' + rv + '%!end';
    }

    ,

    insert: function(fname, ftype, fvalue, fchecked) {
        fname = unescape(fname);
        var allFrames = new Array(window);
        for ( f=0; f<window.frames.length; ++f ) {
            allFrames.push(window.frames[f]);
        }
        for ( j=0; j<allFrames.length; ++j ) {
            try {
                if ( ftype == 'text' || ftype == 'password' || ftype == 'search' || ftype == 'textarea' ) {
                    allFrames[j].document.getElementsByName(fname)[0].value = fvalue;
                }
                else if ( ftype == 'checkbox' ) {
                    allFrames[j].document.getElementsByName(fname)[0].checked = fchecked;
                }
                else if ( ftype == 'radio' ) {
                    fvalue = unescape(fvalue);
                    var radios = allFrames[j].document.getElementsByName(fname);
                    for ( r=0; r<radios.length; ++r ) {
                        if ( radios[r].value == fvalue ) {
                            radios[r].checked = fchecked;
                        }
                    }
                }
            }
            catch (err) { }
        }
    }

}
