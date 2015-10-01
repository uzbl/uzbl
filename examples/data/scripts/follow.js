/*jslint browser: true, vars: true, maxerr: 50, indent: 4 */
/*global uzbl, HTMLInputElement, HTMLTextAreaElement, HTMLSelectElement */
// This is the basic linkfollowing script.
//
// TODO:
// Some pages mess around a lot with the zIndex which
//  lets some hints in the background.
// Some positions are not calculated correctly (mostly
//  because of uber-fancy-designed-webpages. Basic HTML and CSS
//  works good
// Still some links can't be followed/unexpected things
//  happen. Blame some freaky webdesigners ;)

uzbl.follow = (function () {
'use strict';

// Constants
var uzblDivId = 'uzbl_link_hints';
var uzblMatchClass = 'uzbl-follow-text-match';
var uzblNewWindowClass = 'new-window';
// This is duplicated in uzbl.formfiller.
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

// Variables
var gMode;
var gCharset;

// Helpers
var slice = Array.prototype.slice;
var getSelection = window.getSelection;

// Functions
// Return true if the given element is a frame.
var isFrame = function (el) {
    return (el.tagName === 'FRAME' || el.tagName === 'IFRAME');
};

// Find the document that the given element belongs to.
var getDocument = function (el) {
    if (isFrame(el)) {
        return el.contentDocument;
    }

    var doc = el;
    while (doc.parentNode !== null) {
        doc = doc.parentNode;
    }
    return doc;
};

// Find all windows in the display, searching for frames recursively.
var windows = function (w) {
    var win = (typeof w === 'undefined') ? window.top : w;

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
        return ((typeof d !== 'undefined') && (d.domain === document.domain));
    });
};

// Search all frames for elements matching the given CSS selector.
var query = function (selector) {
    var elems = [];

    documents().forEach(function (doc) {
        var set = doc.body.querySelectorAll(selector);

        set = slice.apply(set);
        elems = elems.concat(set);
    });

    return elems;
};

// Calculate element position to draw the hint.
var elementPosition = function (el) {
    // el.getBoundingClientRect is another way to do this, but when a link is
    // line-wrapped we want our hint at the left end of the link, not its
    // bounding rectangle
    var up     = el.offsetTop;
    var left   = el.offsetLeft;
    var width  = el.offsetWidth;
    var height = el.offsetHeight;

    while (el.offsetParent) {
        el = el.offsetParent;
        up += el.offsetTop;
        left += el.offsetLeft;
    }

    return [up, left, width, height];
};

// Calculate if an element is on the viewport.
var elementInViewport = function (el) {
    var offset = elementPosition(el);
    var up     = offset[0];
    var left   = offset[1];
    var width  = offset[2];
    var height = offset[3];
    return up   < window.pageYOffset + window.innerHeight &&
           left < window.pageXOffset + window.innerWidth &&
           (up + height)  > window.pageYOffset &&
           (left + width) > window.pageXOffset;
};

// Removes all hints/leftovers that might be generated
// by this script in the given document.
var removeHints = function (doc) {
    var elements = doc.getElementById(uzblDivId);
    if (elements) {
        elements.parentNode.removeChild(elements);
    }

    // This returns a live NodeList, which is super-annoying when we to try
    // to remove the class.
    var matches = doc.getElementsByClassName(uzblMatchClass);
    matches = slice.apply(matches);

    matches.forEach(function (match) {
        match.classList.remove(uzblMatchClass);
    });
};

// Generate a hint for an element with the given label.
// Here you can play around with the style of the hints!
var generateHint = function (doc, el, label, top, left) {
    var hint = doc.createElement('span');
    hint.innerText = label;
    hint.style.position = 'absolute';
    hint.style.top  = top  + 'px';
    hint.style.left = left + 'px';
    return hint;
};

// This is duplicated in uzbl.formfiller.
var inputTypeIsText = function (type) {
    return (textInputTypes.indexOf(type) >= 0);
};

// pass: number of keys
// returns: key length
var labelLength = function (n) {
    var keylen = 0;
    if (n < 2) {
        return 1;
    }
    n -= 1; // Our highest key will be n-1
    while (n) {
        keylen += 1;
        n = Math.floor(n / gCharset.length);
    }
    return keylen;
};

// Converts an integer 'n' to a string of length 'len' composed of
// characters selected from gCharset.
var intToLabel = function (n, len) {
    var label = '';
    var setLength = gCharset.length;
    var x;
    do {
        label = gCharset.charAt(n % setLength) + label;
        n = Math.floor(n / setLength);
    } while (n);

    for (x = label.length; x < len; x += 1) {
        label = gCharset.charAt(0) + label;
    }

    return label;
};

// pass: label
// returns: number
var labelToInt = function (label) {
    var n = 0;
    var i;
    for (i = 0; i < label.length; i += 1) {
        n *= gCharset.length;
        n += gCharset.indexOf(label[i]);
    }
    return n;
};

var findMatchingHintId = function (elems, str) {
    var linknr = labelToInt(str);

    var len = labelLength(elems.length);

    if ((str.length === len) && (linknr < elems.length) && (linknr >= 0)) {
        // An element has been selected!
        var el = elems[linknr];
        return [el];
    }

    return elems.filter(function (el, i) {
        // Return elements whose labels begin with the given str.
        var label = intToLabel(i, len);
        return label.slice(0, str.length) === str;
    });
};

var getInterestingElements = function () {
    var followable  = 'a, area, textarea, select, input:not([type=hidden]), button, *[onclick]';
    var uri         = 'a, area, frame, iframe';
    //var focusable   = 'a, area, textarea, select, input:not([type=hidden]), button, frame, iframe, applet, object';
    //var desc        = '*[title], img[alt], applet[alt], area[alt], input[alt]';
    //var image       = 'img, input[type=image]';

    var elems;

    if (gMode === 'newwindow' || gMode === 'returnuri') {
        elems = query(uri);
    } else {
        elems = query(followable);
    }

    return elems.filter(elementInViewport);
};

// Draw all hints for all elements passed.
var reDrawHints = function (elems, len) {
    // We have to calculate element positions before we modify the DOM
    // otherwise the elementPosition call slows way down.
    var positions = elems.map(elementPosition);

    documents().forEach(function (doc) {
        removeHints(doc);
        if (!doc.body) {
            return;
        }

        try {
            doc.uzbl_hintdiv = doc.createElement('div');
            doc.uzbl_hintdiv.id = uzblDivId;
            if (gMode === 'newwindow') {
                doc.uzbl_hintdiv.className = uzblNewWindowClass;
            }
            doc.body.appendChild(doc.uzbl_hintdiv);
        } catch (err) {
            // Unable to attach label -> log it and continue.
            console.log('Error occurred when creating hint div: ' + err);
        }
    });

    elems.forEach(function (el, i) {
        var label = intToLabel(i, len);
        var pos   = positions[i];
        var uri;
        var doc;
        var h;

        if (gMode === 'newwindow') {
            uri = el.src || el.href;

            if (uri.match(/^javascript:/)) {
                return;
            }
        }

        try {
            doc = getDocument(el);
            h = generateHint(doc, el, label, pos[0], pos[1]);
            doc.uzbl_hintdiv.appendChild(h);
        } catch (err) {
            // Unable to attach label -> log it and continue.
            console.log('Error occurred when drawing hints: ' + err);
        }
    });
};

var clearHints = function () {
    documents().forEach(removeHints);
};

// Here we choose what to do with an element that's been selected.
// On text form elements we focus and select the content. On other
// elements we simulate a mouse click.
var clickElem = function (el) {
    if (!el) {
        return;
    }

    if ((el instanceof HTMLInputElement) && (inputTypeIsText(el.type))) {
        el.focus();
        el.select();
        return 'XXXFORM_ACTIVEXXX';
    } else if ((el instanceof HTMLTextAreaElement) || (el instanceof HTMLSelectElement)) {
        el.focus();
        if (typeof el.select !== 'undefined') {
            el.select();
        }
        return 'XXXFORM_ACTIVEXXX';
    }

    // Simulate a mouseclick to activate the element.
    var mouseEvent = document.createEvent('MouseEvent');
    mouseEvent.initMouseEvent('click', true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
    el.dispatchEvent(mouseEvent);
    return 'XXXRESET_MODEXXX';
};

var followElement = function (el) {
    var uri;

    // clear all of our hints
    clearHints();

    switch (gMode) {
    case 'returnuri':
    case 'newwindow':
        uri = el.src || el.href;
        return 'XXXRETURNED_URIXXX' + uri;
    case 'click':
    default:
        return clickElem(el);
    }
};

var setMode = function (action) {
    switch (action) {
    case 'newwindow':
        gMode = 'newwindow';
        break;
    case 'returnuri':
        gMode = 'returnuri';
        break;
    case 'click':
    default:
        gMode = 'click';
        break;
    }
};

var setCharset = function (charset) {
    gCharset = charset;
};

// Return the follow object.
return {
    followLinks: function (charset, str, action) {
        setMode(action);
        setCharset(charset);

        var elems   = getInterestingElements();
        var matches = findMatchingHintId(elems, str);

        if (matches.length === 1) {
            return followElement(matches[0]);
        } else {
            var len = labelLength(elems.length) - str.length;
            reDrawHints(matches, len);
        }
    },

    followTextContent: function (str, action) {
        setMode(action);

        str = str.toUpperCase();

        var matching = [];

        var elems = getInterestingElements();
        elems.forEach(function (el) {
            // Do a case-insensitive match on element content.
            if (el.textContent.toUpperCase().match(str)) {
                el.classList.add(uzblMatchClass);
                matching.push(el);
            } else {
                el.classList.remove(uzblMatchClass);
            }
        });

        if (matching.length === 1) {
            return followElement(matching[0]);
        }
    },

    followSelection: function (action) {
        setMode(action);

        var selection = getSelection();
        if (!selection) {
            return;
        }

        var node = selection.anchorNode;
        if (!node) {
            return;
        }

        var el = node.parentElement;
        if (!el) {
            return;
        }

        return followElement(el);
    },

    followMenu: function (action) {
        setMode(action);

        var elems = getInterestingElements();
        var output = '';

        elems.forEach(function (el) {
            var link = el.src || el.href;
            var title = el.textContent;
            var entry = '';

            if (typeof link === 'undefined' ||
                link === '' ||
                link.match(/^javascript:/)) {
                return;
            }

            title = title.trim();

            entry += link;
            entry += '\t';
            entry += title;
            entry += '\n';

            output += entry;
        });

        return output;
    },

    clearHints: clearHints
};
}());
