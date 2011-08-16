uzbl.formfiller = {

    inputTypeIsText: function(type) {
        var types = [ 'text', 'password', 'search', 'email', 'url',
                      'number', 'range', 'color', 'date', 'month',
                      'week', 'time', 'datetime', 'datetime-local' ];

        for(var i = 0; i < types.length; ++i)
          if(types[i] == type) return true;

        return false;
    }

    ,

    dump: function() {
        var rv = '';
        var allFrames = new Array(window);

        for ( var f = 0; f < window.frames.length; ++f ) {
            allFrames.push(window.frames[f]);
        }

        for ( var j = 0; j < allFrames.length; ++j ) {
            try {
                var xp_res = allFrames[j].document.evaluate(
                    '//input', allFrames[j].document.documentElement, null, XPathResult.ANY_TYPE,null
                );
                var input;
                while ( input = xp_res.iterateNext() ) {
                    if ( inputTypeIsText(input.type) ) {
                        rv += '%' + escape(input.name) + '(' + input.type + '):' + input.value + '\n';
                    } else if ( input.type == 'checkbox' || input.type == 'radio' ) {
                        rv += '%' + escape(input.name) + '(' + input.type + '){' + escape(input.value) + '}:' + (input.checked?'1':'0') + '\n';
                    }
                }
                xp_res = allFrames[j].document.evaluate(
                    '//textarea', allFrames[j].document.documentElement, null, XPathResult.ANY_TYPE,null
                );
                var input;
                while ( input = xp_res.iterateNext() ) {
                    rv += '%' + escape(input.name) + '(textarea):\n' + input.value.replace(/\n\\/g,"\n\\\\").replace(/\n%/g,"\n\\%") + '%\n';
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
        for ( var f = 0; f < window.frames.length; ++f ) {
            allFrames.push(window.frames[f]);
        }
        for ( var j = 0; j < allFrames.length; ++j ) {
            try {
                if ( uzbl.formfiller.inputTypeIsText(ftype) || ftype == 'textarea' ) {
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
