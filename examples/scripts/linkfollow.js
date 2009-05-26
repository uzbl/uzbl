// link follower for uzbl
// requires http://github.com/DuClare/uzbl/commit/6c11777067bdb8aac09bba78d54caea04f85e059
//
// first, it needs to be loaded before every time it is used.
// One way would be to use something like load_start_handler to send
// "act script link_follower.js"
//
// when script is loaded, it can be invoked with
// bind f* = js setHints("%s")
// bind f_ = js followLink("%s")
//
// based on follow_Numbers.js
//
// TODO: add classes to hinted elements


var uzblid = 'uzbl_hint';
var uzblclass = 'uzbl_hint_class'

var doc = document;

function elementPosition(el) {
    var up = el.offsetTop;
    var left = el.offsetLeft; var width = el.offsetWidth;
    var height = el.offsetHeight;

    while (el.offsetParent) {
        el = el.offsetParent;
        up += el.offsetTop;
        left += el.offsetLeft;
    }
    return [up, left, width, height];
}

function generateHint(el, label) {
    var hint = doc.createElement('div');
    hint.setAttribute('class', uzblclass);
    hint.innerText = label;
    hint.style.display = 'inline';
    hint.style.backgroundColor = '#B9FF00';
    hint.style.border = '2px solid #4A6600';
    hint.style.color = 'black';
    hint.style.fontSize = '9px';
    hint.style.fontWeight = 'bold';
    hint.style.lineHeight = '9px';
    hint.style.margin = '0px';
    hint.style.padding = '1px';
    hint.style.position = 'absolute';
    hint.style.zIndex = '10000';
    hint.style.textDecoration = 'none';
    hint.style.webkitBorderRadius = '6px';
    // Play around with this, pretty funny things to do :)
    hint.style.webkitTransform = 'scale(1) rotate(0deg) translate(-6px,-5px)';
    return hint;
}

function elementInViewport(el) {
    offset = elementPosition(el);
    var up = offset[0];
    var left = offset[1];
    var width = offset[2];
    var height = offset[3];
    return (up < window.pageYOffset + window.innerHeight && 
				left < window.pageXOffset + window.innerWidth 
				&& (up + height) > window.pageYOffset 
				&& (left + width) > window.pageXOffset);
}

function isVisible(el) {

			if (el == doc) { return true; }
			if (!el) { return false; }
			if (!el.parentNode) { return false; }
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

var hintable = "//a[@href] | //img | //input";

function Matcher(str){
	var numbers = str.replace(/[^\d]/g,"");
	var words = str.replace(/\d/g,"").split(/\s+/).map(function (n) { return new RegExp(n,"i")});
	this.test = test;
	this.toString = toString;
	this.numbers = numbers;
	function test(element) {
		// test all the regexp
		return words.every(function (regex) { return element.textContent.match(regex)});
	}
	function toString(){
		return "{"+numbers+"},{"+words+"}";
	}
}


function setHints(r){
	if(doc.body) doc.body.setAttribute("onkeyup","keyPressHandler(event)");
	var re = new Matcher(r);
	clearHints();
	var c = 1;
	var items = doc.evaluate(hintable,doc,null,XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,null);
	for (var i = 0; i < items.snapshotLength;i++){
		var item = items.snapshotItem(i);
		if(re.test(item) && isVisible(item) && elementInViewport(item)){
			var h = generateHint(item,c);
			item.appendChild(h);
			c++;
		}
	}
}

function clearHints(){
	var items = doc.evaluate("//div[@class='" + uzblclass + "']",doc,null,XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,null);
	for (var i = 0; i < items.snapshotLength;i++){
		var item = items.snapshotItem(i);
		item.parentNode.removeChild(item);
	}
}

function keyPressHandler(e) {
    var kC = window.event ? event.keyCode: e.keyCode;
    var Esc = window.event ? 27 : e.DOM_VK_ESCAPE;
    if (kC == Esc) {
        clearHints();
				doc.body.removeAttribute("onkeyup");
    }
}
function followLink(follow){
	var m = new Matcher(follow);
	var elements = doc.evaluate("//*/div[@class='"+uzblclass+"']",doc,null,XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,null);
	// filter
	var matched = [];
	for (var i = 0; i < elements.snapshotLength;i++){
		var item = elements.snapshotItem(i);
		if(m.test(item.parentNode)){
			matched.push(item.parentNode);
		}
	}
	clearHints();
	if(matched.length == 1) {
		var item = matched[0];
	} else {
		var item = matched[parseInt(m.numbers,10)-1];
	}
    if (item) {
			item.style.backgroundColor = "blue";

        var name = item.tagName;
        if (name == 'A') {
            if(item.click) {item.click()};
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
