/* This is the basic linkfollowing script.
 *
 * TODO:
 * Some pages mess around a lot with the zIndex which
 *  lets some hints in the background.
 * Some positions are not calculated correctly (mostly
 *  because of uber-fancy-designed-webpages. Basic HTML and CSS
 *  works good
 * Still some links can't be followed/unexpected things
 *  happen. Blame some freaky webdesigners ;)
 */

// Globals
uzbldivid = 'uzbl_link_hints';

uzbl.follow = function() {
    // Export
    charset   = arguments[0];
    if (arguments[2] == 0 || arguments[2] == 'click') {
        newwindow = false;
        returnuri = false;
    } else if (arguments[2] == 1 || arguments[2] == 'newwindow') {
        newwindow = true;
        returnuri = false;
    } else if (arguments[2] == 'returnuri') {
        newwindow = false;
        returnuri = true;
    }

    var keypress = arguments[1];
    return arguments.callee.followLinks(keypress);
}

uzbl.follow.isFrame = function(el) {
    return (el.tagName == "FRAME" || el.tagName == "IFRAME");
}

// find the document that the given element belongs to
uzbl.follow.getDocument = function(el) {
    if (this.isFrame(el))
        return el.contentDocument;

    var doc = el;
    while (doc.parentNode !== null)
        doc = doc.parentNode;
    return doc;
}

// find all documents in the display, searching frames recursively
uzbl.follow.documents = function() {
    return this.windows().map(function(w) { return w.document; }).filter(function(d) { return d != null; });
}

// find all windows in the display, searching for frames recursively
uzbl.follow.windows = function(w) {
    w = (typeof w == 'undefined') ? window.top : w;

    var wins = [w];
    var frames = w.frames;
    for(var i = 0; i < frames.length; i++)
        wins = wins.concat(uzbl.follow.windows(frames[i]));
    return wins;
}

// search all frames for elements matching the given CSS selector
uzbl.follow.query = function(selector) {
    var res = [];
    this.documents().forEach(function (doc) {
        var set = doc.body.querySelectorAll(selector);
        // convert the NodeList to an Array
        set = Array.prototype.slice.call(set);
        res = res.concat(set);
    });
    return res;
}

// Calculate element position to draw the hint
uzbl.follow.elementPosition = function(el) {
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
}

// Calculate if an element is on the viewport.
uzbl.follow.elementInViewport = function(el) {
    offset = uzbl.follow.elementPosition(el);
    var up     = offset[0];
    var left   = offset[1];
    var width  = offset[2];
    var height = offset[3];
    return  up   < window.pageYOffset + window.innerHeight &&
            left < window.pageXOffset + window.innerWidth &&
            (up + height)  > window.pageYOffset &&
            (left + width) > window.pageXOffset;
}

// Removes all hints/leftovers that might be generated
// by this script in the given document.
uzbl.follow.removeHints = function(doc) {
    var elements = doc.getElementById(uzbldivid);
    if (elements) elements.parentNode.removeChild(elements);
}

// Clears all hints in every document
uzbl.follow.clearHints = function() {
    this.documents().forEach(uzbl.follow.removeHints);
}

// Generate a hint for an element with the given label
// Here you can play around with the style of the hints!
uzbl.follow.generateHint = function(doc, el, label, top, left) {
    var hint = doc.createElement('span');
    hint.innerText = label;
    hint.style.position = 'absolute';
    hint.style.top  = top  + 'px';
    hint.style.left = left + 'px';
    return hint;
}

// Here we choose what to do with an element if we
// want to "follow" it. On form elements we "select"
// or pass the focus, on links we try to perform a click,
// but at least set the href of the link. (needs some improvements)
uzbl.follow.clickElem = function(item) {
    if(!item) return;

    if (item instanceof HTMLInputElement) {
        var type = item.type;
        if (type == 'text' || type == 'file' || type == 'password') {
            item.focus();
            item.select();
            return "XXXEMIT_FORM_ACTIVEXXX";
        }
        // otherwise fall through to a simulated mouseclick.
    } else if (item instanceof HTMLTextAreaElement || item instanceof HTMLSelectElement) {
        item.focus();
        if(typeof item.select != 'undefined')
            item.select();
        return "XXXEMIT_FORM_ACTIVEXXX";
    }

    // simulate a mouseclick to activate the element
    var mouseEvent = document.createEvent("MouseEvent");
    mouseEvent.initMouseEvent("click", true, true, window, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
    item.dispatchEvent(mouseEvent);
    return "XXXRESET_MODEXXX";
}

// Draw all hints for all elements passed.
uzbl.follow.reDrawHints = function(elems, chars) {
    var elements  = elems.map(function(pair) { return pair[0] });
    var labels    = elems.map(function(pair) { return pair[1].substring(chars) });
    // we have to calculate element positions before we modify the DOM
    // otherwise the elementPosition call slows way down.
    var positions = elements.map(uzbl.follow.elementPosition);

    this.documents().forEach(function(doc) {
        uzbl.follow.removeHints(doc);
        if (!doc.body) return;
        doc.hintdiv = doc.createElement('div');
        doc.hintdiv.id = uzbldivid;
        if(newwindow) doc.hintdiv.className = "new-window";
        doc.body.appendChild(doc.hintdiv);
    });

    elements.forEach(function(el, i) {
        var label = labels[i];
        var pos   = positions[i];
        try {
            var doc   = uzbl.follow.getDocument(el);
            var h = uzbl.follow.generateHint(doc, el, label, pos[0], pos[1]);
            doc.hintdiv.appendChild(h);
        } catch (err) {
            // Unable to attach label -> shrug it off and continue
        }
    });
}

// pass: number of keys
// returns: key length
uzbl.follow.labelLength = function(n) {
    var oldn = n;
    var keylen = 0;
    if(n < 2) return 1;
    n -= 1; // Our highest key will be n-1
    while(n) {
        keylen += 1;
        n = Math.floor(n / charset.length);
    }
    return keylen;
}

// pass: number
// returns: label
uzbl.follow.intToLabel = function(n) {
    var label = '';
    do {
        label = charset.charAt(n % charset.length) + label;
        n = Math.floor(n / charset.length);
    } while(n);
    return label;
}

// pass: label
// returns: number
uzbl.follow.labelToInt = function(label) {
    var n = 0;
    for(var i = 0; i < label.length; ++i) {
        n *= charset.length;
        n += charset.indexOf(label[i]);
    }
    return n;
}

// Put it all together
uzbl.follow.followLinks = function(follow) {
    var s = follow.split('');
    var linknr = this.labelToInt(follow);

    var followable  = 'a, area, textarea, select, input:not([type=hidden]), button, *[onclick]';
    var uri         = 'a, area, frame, iframe';
    //var focusable   = 'a, area, textarea, select, input:not([type=hidden]), button, frame, iframe, applet, object';
    //var desc        = '*[title], img[alt], applet[alt], area[alt], input[alt]';
    //var image       = 'img, input[type=image]';

    if(newwindow || returnuri)
        var res = this.query(uri);
    else
        var res = this.query(followable);

    var elems = res.filter(uzbl.follow.elementInViewport);
    var len = this.labelLength(elems.length);

    if (s.length == len && linknr < elems.length && linknr >= 0) {
        // an element has been selected!
        var el = elems[linknr];

        // clear all of our hints
        this.clearHints();

        if (returnuri) {
            var uri = el.src || el.href;
            return "XXXRETURNED_URIXXX" + uri
        }

        if (newwindow) {
            // we're opening a new window using the URL attached to this element
            var uri = el.src || el.href;
            if(uri.match(/javascript:/)) return;
            window.open(uri);
            return "XXXRESET_MODEXXX"
        }

        // we're just going to click the element
        return this.clickElem(el);
    }

    var leftover = [];
    for (var j = 0; j < elems.length; j++) {
        var b = true;
        var label = this.intToLabel(j);
        var n = label.length;
        for (n; n < len; n++)
            label = charset.charAt(0) + label;
        for (var k = 0; k < s.length; k++)
            b = b && label.charAt(k) == s[k];
        if (b)
            leftover.push([elems[j], label]);
    }

    this.reDrawHints(leftover, s.length);
}
