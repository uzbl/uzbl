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
    charset = arguments[0]
    keypress = arguments[1]
    newwindow = arguments[2]

    // Some shortcuts and globals
    uzblid = 'uzbl_link_hint';
    uzbldivid = uzblid + '_div_container';
    doc = document;
    win = window;
    links = doc.links;
    forms = doc.forms;

    // Make onclick-links "clickable"
    try {
        HTMLElement.prototype.click = function() {
            if (typeof this.onclick == 'function') {
                this.onclick({ type: 'click' });
            }
        }
    } catch(e) {}

    arguments.callee.followLinks(keypress);

}

// Calculate element position to draw the hint
// Pretty accurate but fails in some very fancy cases
uzbl.follow.elementPosition = function(el) {
    var up = el.offsetTop;
    var left = el.offsetLeft;
    var width = el.offsetWidth;
    var height = el.offsetHeight;
    while (el.offsetParent) {
        el = el.offsetParent;
        up += el.offsetTop;
        left += el.offsetLeft;
    }
    return [up, left, width, height];
}

// Calculate if an element is visible
uzbl.follow.isVisible = function(el) {
    if (el == doc) return true;
    if (!el) return false;
    if (!el.parentNode) return false;

    if (el.style) {
        if (el.style.display == 'none') return false;
        if (el.style.visibility == 'hidden') return false;
    }
    return this.isVisible(el.parentNode);
}

// Calculate if an element is on the viewport.
uzbl.follow.elementInViewport = function(el) {
    offset = this.elementPosition(el);
    var up = offset[0];
    var left = offset[1];
    var width = offset[2];
    var height = offset[3];
    return up < window.pageYOffset + window.innerHeight && left < window.pageXOffset + window.innerWidth && (up + height) > window.pageYOffset && (left + width) > window.pageXOffset;
}

// Removes all hints/leftovers that might be generated
// by this script.
uzbl.follow.removeAllHints = function() {
    var elements = doc.getElementById(uzbldivid);
    if (elements) elements.parentNode.removeChild(elements);
}

// Generate a hint for an element with the given label
// Here you can play around with the style of the hints!
uzbl.follow.generateHint = function(el, label) {
    var pos = this.elementPosition(el);
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
    hint.style.left = pos[1] + 'px';
    hint.style.top = pos[0] + 'px';
    hint.style.textDecoration = 'none';
    hint.style.webkitTransform = 'translate(-5px,-5px)';
    return hint;
}

// Here we choose what to do with an element if we
// want to "follow" it. On form elements we "select"
// or pass the focus, on links we try to perform a click,
// but at least set the href of the link. (needs some improvements)
uzbl.follow.clickElem = function(item) {
    this.removeAllHints();
    if (item) {
        if (newwindow && item.tagName == 'A') window.open(item.href);
        else {
            var name = item.tagName;
            if (name == 'A') {
                item.click();
                window.location = item.href;
            } else if (name == 'INPUT') {
                var type = item.getAttribute('type').toUpperCase();
                if (type == 'TEXT' || type == 'FILE' || type == 'PASSWORD') {
                    item.focus();
                    item.select();
                } else {
                    item.click();
                }
            } else if (name == 'TEXTAREA' || name == 'SELECT') {
                item.focus();
                item.select();
            } else {
                item.click();
                window.location = item.href;
            }
        }
    }
}

// Returns a list of all links (in this version
// just the elements itself, but in other versions, we
// add the label here.
uzbl.follow.addLinks = function() {
    res = [[], []];
    for (var l = 0; l < links.length; l++) {
        var li = links[l];
        if (this.isVisible(li) && this.elementInViewport(li)) res[0].push(li);
    }
    return res;
}

// Same as above, just for the form elements
uzbl.follow.addFormElems = function() {
    res = [[], []];
    for (var f = 0; f < forms.length; f++) {
        for (var e = 0; e < forms[f].elements.length; e++) {
            var el = forms[f].elements[e];
            if (el && ['INPUT', 'TEXTAREA', 'SELECT'].indexOf(el.tagName) + 1 && this.isVisible(el) && this.elementInViewport(el)) res[0].push(el);
        }
    }
    return res;
}

// Draw all hints for all elements passed. "len" is for
// the number of chars we should use to avoid collisions
uzbl.follow.reDrawHints = function(elems, chars) {
    this.removeAllHints();
    var hintdiv = doc.createElement('div');
    hintdiv.setAttribute('id', uzbldivid);
    for (var i = 0; i < elems[0].length; i++) {
        if (elems[0][i]) {
            var label = elems[1][i].substring(chars);
            var h = this.generateHint(elems[0][i], label);
            hintdiv.appendChild(h);
        }
    }
    if (document.body) document.body.appendChild(hintdiv);
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
    var i;
    for(i = 0; i < label.length; ++i) {
        n *= charset.length;
        n += charset.indexOf(label[i]);
    }
    return n;
}

// Put it all together
uzbl.follow.followLinks = function(follow) {
    //if(follow.charAt(0) == 'l') {
    //    follow = follow.substr(1);
    //    charset = 'thsnlrcgfdbmwvz-/';
    //}
    var s = follow.split('');
    var linknr = this.labelToInt(follow);
    if (document.body) document.body.setAttribute('onkeyup', 'keyPressHandler(event)');
    var linkelems = this.addLinks();
    var formelems = this.addFormElems();
    var elems = [linkelems[0].concat(formelems[0]), linkelems[1].concat(formelems[1])];
    var len = this.labelLength(elems[0].length);
    var oldDiv = doc.getElementById(uzbldivid);
    var leftover = [[], []];
    if (s.length == len && linknr < elems[0].length && linknr >= 0) this.clickElem(elems[0][linknr]);
    else {
        for (var j = 0; j < elems[0].length; j++) {
            var b = true;
            var label = this.intToLabel(j);
            var n = label.length;
            for (n; n < len; n++) label = charset.charAt(0) + label;
            for (var k = 0; k < s.length; k++) b = b && label.charAt(k) == s[k];
            if (b) {
                leftover[0].push(elems[0][j]);
                leftover[1].push(label);
            }
        }
        this.reDrawHints(leftover, s.length);
    }
}
