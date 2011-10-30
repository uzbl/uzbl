uzbl.formfiller = {

    // this is pointlessly duplicated in uzbl.follow
    textInputTypes: [
      'text', 'password', 'search', 'email', 'url', 'number', 'range', 'color',
      'date', 'month', 'week', 'time', 'datetime', 'datetime-local'
    ]

    ,

    // this is pointlessly duplicated in uzbl.follow
    inputTypeIsText: function(type) {
        return uzbl.formfiller.textInputTypes.indexOf(type) >= 0;
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
                var inputs = allFrames[j].document.getElementsByTagName("input");

                for( var k = 0; k < inputs.length; ++k ) {
                    var input = inputs[k];
                    if ( ! input.name ) {
                        continue
                    }
                    if ( uzbl.formfiller.inputTypeIsText(input.type) ) {
                        rv += '%' + escape(input.name) + '(' + input.type + '):' + input.value + '\n';
                    } else if ( input.type == 'checkbox' || input.type == 'radio' ) {
                        rv += '%' + escape(input.name) + '(' + input.type + '){' + escape(input.value) + '}:' + (input.checked?'1':'0') + '\n';
                    }
                }

                var textareas = allFrames[j].document.getElementsByTagName("textarea");
                for( var k = 0; k < textareas.length; ++k ) {
                    var textarea = textareas[k];
                    if ( ! textarea.name ) {
                        continue
                    }
                    rv += '%' + escape(textarea.name) + '(textarea):\n' + textarea.value.replace(/(^|\n)\\/g,"$1\\\\").replace(/(^|\n)%/g,"$1\\%") + '\n%\n';
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
