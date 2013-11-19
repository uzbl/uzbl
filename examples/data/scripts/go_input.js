/*jslint browser: true, vars: true, maxerr: 50, indent: 4 */
/*global getComputedStyle */
uzbl.go_input = function () {
'use strict';

var maskType = function (type) {
    return ':not([type="' + type + '"])';
};

var invalidElements = [
        'button',
        'checkbox',
        'hidden',
        'image',
        'radio',
        'reset',
        'submit'
    ].map(maskType);

var elems = document.querySelectorAll('textarea, input' + invalidElements.join(''));

var isVisible = function (el) {
    var style = getComputedStyle(el, null);

    return ((style.display !== 'none') && (style.visibility === 'visible'));
};

if (elems) {
    elems = elems.filter(isVisible);

    if (elems.length) {
        var el = elems[0];

        if (el.type === 'file') {
            el.click();
        } else {
            el.focus();
        }

        return 'XXXFORM_ACTIVEXXX';
    }
}
};
