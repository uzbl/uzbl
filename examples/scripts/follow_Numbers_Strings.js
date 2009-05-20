var uzblid = 'uzbl_link_hint';
var uzbldivid = uzblid + '_div_container';
var doc = document;
var win = window;
var links = document.links;
var forms = document.forms;
try {
    HTMLElement.prototype.click = function() {
        if (typeof this.onclick == 'function') {
            this.onclick({
                type: 'click'
            });
        }
    };
} catch(e) {}
function keyPressHandler(e) {
    var kC = window.event ? event.keyCode: e.keyCode;
    var Esc = window.event ? 27 : e.DOM_VK_ESCAPE;
    if (kC == Esc) {
        removeAllHints();
    }
}
function elementPosition(el) {
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
function isVisible(el) {
    if (el == doc) {
        return true;
    }
    if (!el) {
        return false;
    }
    if (!el.parentNode) {
        return false;
    }
    if (el.style) {
        if (el.style.display == 'none') {
            return false;
        }
        if (el.style.visibility == 'hidden') {
            return false;
        }
    }
    return isVisible(el.parentNode);
}
function elementInViewport(el) {
    offset = elementPosition(el);
    var up = offset[0];
    var left = offset[1];
    var width = offset[2];
    var height = offset[3];
    return up < window.pageYOffset + window.innerHeight && left < window.pageXOffset + window.innerWidth && (up + height) > window.pageYOffset && (left + width) > window.pageXOffset;
}
function removeAllHints() {
    var elements = doc.getElementById(uzbldivid);
    if (elements) {
        elements.parentNode.removeChild(elements);
    }
}
function generateHint(el, label) {
    var pos = elementPosition(el);
    var hint = doc.createElement('div');
    hint.setAttribute('name', uzblid);
    hint.innerText = label;
    hint.style.display = 'inline';
    hint.style.backgroundColor = '#B9FF00';
    hint.style.border = '2px solid #4A6600';
    hint.style.color = 'black';
    hint.style.zIndex = '1000';
    hint.style.fontSize = '9px';
    hint.style.fontWeight = 'bold';
    hint.style.lineHeight = '9px';
    hint.style.margin = '0px';
    hint.style.padding = '1px';
    hint.style.position = 'absolute';
    hint.style.left = pos[1] + 'px';
    hint.style.top = pos[0] + 'px';
    var img = el.getElementsByTagName('img');
    if (img.length > 0) {
        hint.style.left = pos[1] + img[0].width / 2 + 'px';
    }
    hint.style.textDecoration = 'none';
    hint.style.webkitBorderRadius = '6px';
    hint.style.webkitTransform = 'scale(1) rotate(0deg) translate(-6px,-5px)';
    return hint;
}
function clickElem(item) {
    removeAllHints();
    if (item) {
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
function addLinks() {
    res = [[], []];
    for (var l = 0; l < links.length; l++) {
        var li = links[l];
        if (isVisible(li) && elementInViewport(li)) {
            res[0].push(li);
            res[1].push(li.innerText.toLowerCase());
        }
    }
    return res;
}
function addFormElems() {
    res = [[], []];
    for (var f = 0; f < forms.length; f++) {
        for (var e = 0; e < forms[f].elements.length; e++) {
            var el = forms[f].elements[e];
            if (el && ['INPUT', 'TEXTAREA', 'SELECT'].indexOf(el.tagName) + 1 && isVisible(el) && elementInViewport(el)) {
                res[0].push(el);
                if (el.getAttribute('value')) {
                    res[1].push(el.getAttribute('value').toLowerCase());
                } else {
                    res[1].push(el.getAttribute('name').toLowerCase());
                }
            }
        }
    }
    return res;
}
function reDrawHints(elems, len) {
    var hintdiv = doc.createElement('div');
    hintdiv.setAttribute('id', uzbldivid);
    hintdiv.style.opacity = '0.0';
    for (var i = 0; i < elems[0].length; i++) {
        var label = i + '';
        var n = label.length;
        for (n; n < len; n++) {
            label = '0' + label;
        }
        if (elems[0][i]) {
            var h = generateHint(elems[0][i], label);
            hintdiv.appendChild(h);
        }
    }
    if (document.body) {
        document.body.appendChild(hintdiv);
        hintdiv.style.opacity = '0.7'
    }
}
function followLinks(follow) {
    var s = follow.split('');
    var linknr = parseInt(follow, 10);
    if (document.body) document.body.setAttribute('onkeyup', 'keyPressHandler(event)');
    var linkelems = addLinks();
    var formelems = addFormElems();
    var elems = [linkelems[0].concat(formelems[0]), linkelems[1].concat(formelems[1])];
    var len = (elems[0].length + '').length;
    var oldDiv = doc.getElementById(uzbldivid);
    var leftover = [[], []];
    if (linknr + 1 && s.length == len && linknr < elems[0].length && linknr >= 0) {
        clickElem(elems[0][linknr]);
    } else {
        for (var j = 0; j < elems[0].length; j++) {
            var b = true;
            for (var k = 0; k < s.length; k++) {
                b = b && elems[1][j].charAt(k) == s[k];
            }
            if (!b) {
                elems[0][j] = null;
                elems[1][j] = null;
            } else {
                leftover[0].push(elems[0][j]);
                leftover[1].push(elems[1][j]);
            }
        }
        if (leftover[0].length == 1) {
            clickElem(leftover[0][0]);
        } else if (!oldDiv) {
            if (linknr + 1 || s.length == 0) {
                reDrawHints(elems, len);
            } else {
                reDrawHints(leftover, len);
            }
        }
    }
}
followLinks('%s');
