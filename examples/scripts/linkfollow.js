// link follower for uzbl
// requires http://github.com/DuClare/uzbl/commit/6c11777067bdb8aac09bba78d54caea04f85e059
//
// first, it needs to be loaded before every time it is used.
// One way would be to use something like load_start_handler to send
// "act script /usr/share/examples/scripts/linkfollow.js"
// (currently, it is recommended to use load_finish_handler since the JS code seems to get
// flushed. Using a load_start_handler with a 1s delay works but not always)
//
// when script is loaded, it can be invoked with
// bind f* = js hints.set("%s")
// bind f_ = js hints.follow("%s")
//
// At the moment, it may be useful to have way of forcing uzbl to load the script
// bind :lf = script /usr/share/examples/scripts/linkfollow.js
//
// To enable hint highlighting, add:
// set stylesheet_uri = /usr/share/uzbl/examples/data/style.css
//
// based on follow_Numbers.js
//
// TODO: set CSS styles (done, but not working properly)
// TODO: load the script as soon as the DOM is ready



function Hints(){
	var uzblid = 'uzbl_hint';
	var uzblclass = 'uzbl_highlight';
	var uzblclassfirst = 'uzbl_h_first';
	var doc = document;
	this.set = setHints;
	this.follow = followHint;
	this.keyPressHandler = keyPressHandler;

	function hasClass(ele,cls) {
			return ele.className.split(/ /).some(function (n) { return n == cls });
	}
	 
	function addClass(ele,cls) {
			if (!hasClass(ele,cls)) ele.className += " "+cls;
	}
	 
	function removeClass(ele,cls) {
		ele.className = ele.className.split(/ /).filter(function (n) { n != cls}).join(" ");
	}

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

	function generateHint(pos, label) {
			var hint = doc.createElement('div');
			hint.setAttribute('name', uzblid);
			hint.innerText = label;
      //the css is set with ./examples/data/style.css
			hint.style.left = pos[1] + 'px';
			hint.style.top = pos[0] + 'px';
			//var img = el.getElementsByTagName('img');
			//if (img.length > 0) {
			//    hint.style.left = pos[1] + img[0].width / 2 + 'px';
			//}
			return hint;
	}

	function elementInViewport(offset) {
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
		if(doc.body) doc.body.onkeyup = this.keyPressHandler;
		var re = new Matcher(r);
		clearHints();
		var hintdiv = doc.createElement('div');
		hintdiv.setAttribute('id', uzblid);
		var c = 1;
		var items = doc.evaluate(hintable,doc,null,XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,null);
		for (var i = 0; i < items.snapshotLength;i++){
			var item = items.snapshotItem(i);
			var pos = elementPosition(item);
			if(re.test(item) && isVisible(item) && elementInViewport(pos)){
				var h = generateHint(pos,c);
				h.href = function () {return item};
				hintdiv.appendChild(h);
				if(c==1){
					addClass(item,uzblclassfirst);
		//			addClass(item,uzblclass);
				} else {
					addClass(item,uzblclass);
				}
				c++;
			}
		}
		if (document.body) {
				document.body.insertBefore(hintdiv,document.body.firstChild);
		}
	}

	function clearHints(){
		var hintdiv = doc.getElementById(uzblid);
		if(hintdiv){
			hintdiv.parentNode.removeChild(hintdiv);
		}
		var first = doc.getElementsByClassName(uzblclassfirst)[0];
		if(first){
			removeClass(first,uzblclassfirst);
		}
		// TODO: not all class attributes get removed
		var items = doc.getElementsByClassName(uzblclass);
		for (var i = 0; i<items.length; i++){
			removeClass(items[i],uzblclass);
		};
		
	}

	function keyPressHandler(e) {
			var kC = window.event ? event.keyCode: e.keyCode;
			var Esc = window.event ? 27 : e.DOM_VK_ESCAPE;
			if (kC == Esc) {
					clearHints();
					doc.body.removeAttribute("onkeyup");
			}
	}
	function followHint(follow){
		var m = new Matcher(follow);
		var elements = doc.getElementsByClassName(uzblclass);
		// filter
		var matched = [];
		matched.push(doc.getElementsByClassName(uzblclassfirst)[0]);
		for (var i = 0; i < elements.length;i++){
			if(m.test(elements[i])){
				matched.push(elements[i]);
			}
		}
		clearHints();
		var n = parseInt(m.numbers,10);
		if(n){
			var item = matched[n-1];
		} else {
			var item = matched[0];
		}
		if (item) {
			item.style.borderStyle = "dotted";
			item.style.borderWidth = "thin";

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
}
var hints;
//document.addEventListener("DOMContentLoaded",function () { hints = new Hints()},false);

var hints = new Hints();

// vim:set et tw=2:


