/* This is the basic linkfollowing script.
 * Its pretty stable, and you configure which keys to use for hinting
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

uzbl.follow = function() {
    // Export
    charset   = arguments[0];
    newwindow = arguments[2];

    // Some shortcuts and globals
    uzblid = 'uzbl_link_hint';
    uzbldivid = uzblid + '_div_container';

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
    var rect = el.getBoundingClientRect();

    var left, up;

    if (uzbl.follow.isFrame(el)) {
        left = document.defaultView.scrollX;
        up   = document.defaultView.scrollY;
    } else {
        left = Math.max((rect.left + document.defaultView.scrollX), document.defaultView.scrollX);
        up   = Math.max((rect.top  + document.defaultView.scrollY), document.defaultView.scrollY);
    }

    return [up, left, rect.width, rect.height];
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
// by this script.
uzbl.follow.removeAllHints = function(doc) {
    var elements = doc.getElementById(uzbldivid);
    if (elements) elements.parentNode.removeChild(elements);
}

// Generate a hint for an element with the given label
// Here you can play around with the style of the hints!
uzbl.follow.generateHint = function(doc, el, label, top, left) {
    var hint = doc.createElement('div');
    hint.setAttribute('name', uzblid);
    hint.innerText = label;
    hint.style.display = 'inline';
    if (newwindow) hint.style.backgroundColor = '#ffff00';
    else hint.style.backgroundColor = '#aaff00';
    hint.style.border = '2px solid #556600';
    hint.style.color = 'black';
    hint.style.fontFamily = 'Verdana';
    hint.style.fontSize = '9px';
    hint.style.fontWeight = 'bold';
    hint.style.fontVariant = 'normal';
    hint.style.lineHeight = '9px';
    hint.style.margin = '0px';
    hint.style.width = 'auto'; // fix broken rendering on w3schools.com
    hint.style.padding = '1px';
    hint.style.position = 'absolute';
    hint.style.zIndex = '1000';
    hint.style.textDecoration = 'none';
    hint.style.webkitTransform = 'translate(-5px,-5px)';
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
    var name = item.tagName;

    if (name == 'INPUT') {
        var type = item.getAttribute('type').toUpperCase();
        if (type == 'TEXT' || type == 'FILE' || type == 'PASSWORD') {
            item.focus();
            item.select();
            return "XXXEMIT_FORM_ACTIVEXXX";
        }
        // otherwise fall through to a simulated mouseclick.
    } else if (name == 'TEXTAREA' || name == 'SELECT') {
        item.focus();
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
        uzbl.follow.removeAllHints(doc);
        if (!doc.body) return;
        doc.hintdiv = doc.createElement('div');
        doc.hintdiv.setAttribute('id', uzbldivid);
        doc.body.appendChild(doc.hintdiv);
    });

    elements.forEach(function(el, i) {
        var label = labels[i];
        var pos   = positions[i];
        var doc   = uzbl.follow.getDocument(el);
        var h = uzbl.follow.generateHint(doc, el, label, pos[0], pos[1]);
        doc.hintdiv.appendChild(h);
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

    var followable  = 'a, area, textarea, select, input:not([type=hidden]), button';
    var uri         = 'a, area, frame, iframe';
    //var focusable   = 'a, area, textarea, select, input:not([type=hidden]), button, frame, iframe, applet, object';
    //var desc        = '*[title], img[alt], applet[alt], area[alt], input[alt]';
    //var image       = 'img, input[type=image]';

    if(newwindow)
        var res = this.query(uri);
    else
        var res = this.query(followable);

    var elems = res.filter(uzbl.follow.elementInViewport);
    var len = this.labelLength(elems.length);

    if (s.length == len && linknr < elems.length && linknr >= 0) {
        // an element has been selected!
        var el = elems[linknr];

        // clear all of our hints
        this.documents().forEach(uzbl.follow.removeAllHints);

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
