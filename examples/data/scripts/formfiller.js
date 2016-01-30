/*jslint browser: true, vars: true, maxerr: 50, indent: 4 */
/*global uzbl, escape, unescape */

uzbl.formfiller = (function () {
'use strict';

// Constants
// This is duplicated in uzbl.follow.
var textInputTypes = [
    'color',
    'date',
    'datetime',
    'datetime-local',
    'email',
    'month',
    'number',
    'password',
    'range',
    'search',
    'text',
    'time',
    'url',
    'week'
];

// Helpers
var slice = Array.prototype.slice;

// Functions
// This is duplicated in uzbl.follow.
var inputTypeIsText = function (type) {
    return (textInputTypes.indexOf(type) >= 0);
};

var hasName = function (el) {
    return el.name;
};

return {
    dump: function () {
        var rv = '';
        var frames = slice.apply(window.frames);
        frames.push(window);

        frames.forEach(function (frame) {
            try {
                var inputs = slice.apply(frame.document.getElementsByTagName('input'));
                inputs.filter(hasName).forEach(function (input) {
                    if (inputTypeIsText(input.type)) {
                        rv += '%' + escape(input.name) + '(' + input.type + '):' + input.value + '\n';
                    } else if ((input.type === 'checkbox') || (input.type === 'radio')) {
                        rv += '%' + escape(input.name) + '(' + input.type + '){' + escape(input.value) + '}:' + (input.checked ? '1' : '0') + '\n';
                    }
                });

                var textareas = slice.apply(frame.document.getElementsByTagName('textarea'));
                textareas.filter(hasName).forEach(function (textarea) {
                    var escaped = textarea.value.replace(/(^|\n)\\/g, '$1\\\\').replace(/(^|\n)%/g, '$1\\%');
                    rv += '%' + escape(textarea.name) + '(textarea):\n' + escaped + '\n%\n';
                });
            } catch (err) {
                console.log('Error occurred when generating formfiller text: ' + err);
            }
        });

        return 'formfillerstart\n' + rv + '%!end';
    },

    insert: function (fname, ftype, fvalue, fchecked) {
        fname = unescape(fname);
        var frames = slice.apply(window.frames);
        frames.push(window);

        frames.forEach(function (frame) {
            try {
                if (inputTypeIsText(ftype) || (ftype === 'textarea')) {
                    frame.document.getElementsByName(fname)[0].value = fvalue;
                } else if (ftype === 'checkbox') {
                    frame.document.getElementsByName(fname)[0].checked = fchecked;
                } else if (ftype === 'radio') {
                    fvalue = unescape(fvalue);
                    var radios = frames.document.getElementsByName(fname);
                    radios.forEach(function (radio) {
                        if (radio.value === fvalue) {
                            radio.checked = fchecked;
                        }
                    });
                }
            } catch (err) {
                console.log('Error occurred when applying formfiller text: ' + err);
            }
        });
    }
};
}());
