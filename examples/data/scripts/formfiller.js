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

// These are duplicated in uzbl.follow.
// Find all windows in the display, searching for frames recursively.
var windows = function (w) {
    var win = (w === undefined) ? window.top : w;

    var wins = [win];
    var frames = slice.apply(win.frames);

    frames.forEach(function (frame) {
        wins = wins.concat(windows(frame));
    });

    return wins;
};

// Find all documents in the display, searching frames recursively.
var documents = function () {
    return windows().map(function (w) {
        return w.document;
    }).filter(function (d) {
        return d !== undefined;
    });
};

return {
    dump: function () {
        var rv = '';

        documents().forEach(function (doc) {
            try {
                var elems = doc.getElementsByTagName('input');
                var inputs = slice.apply(elems);

                inputs.filter(hasName).forEach(function (input) {
                    if (inputTypeIsText(input.type)) {
                        rv += '%' + escape(input.name) + '(' + input.type + '):' + input.value + '\n';
                    } else if ((input.type === 'checkbox') || (input.type === 'radio')) {
                        rv += '%' + escape(input.name) + '(' + input.type + '){' + escape(input.value) + '}:' + (input.checked ? '1' : '0') + '\n';
                    }
                });

                elems = doc.getElementsByTagName('textarea');
                var textareas = slice.apply(elems);

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

        documents().forEach(function (doc) {
            try {
                if (inputTypeIsText(ftype) || (ftype === 'textarea')) {
                    doc.getElementsByName(fname)[0].value = fvalue;
                } else if (ftype === 'checkbox') {
                    doc.getElementsByName(fname)[0].checked = fchecked;
                } else if (ftype === 'radio') {
                    fvalue = unescape(fvalue);
                    var elems = doc.getElementsByName(fname);
                    var radios = slice.apply(elems);
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
