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
// TODO: fix styling for the first element
// TODO: load the script as soon as the DOM is ready


function Hints(){
  var uzblid = 'uzbl_hint';
  var uzblclass = 'uzbl_highlight';
  var uzblclassfirst = 'uzbl_h_first';
  var doc = document;
  var visible = [];
  var hintdiv;

  this.set = hint;
  this.follow = follow;
  this.keyPressHandler = keyPressHandler;

  function elementPosition(el) {
    var up = el.offsetTop;
    var left = el.offsetLeft; var width = el.offsetWidth;
    var height = el.offsetHeight;

    while (el.offsetParent) {
      el = el.offsetParent;
      up += el.offsetTop;
      left += el.offsetLeft;
    }
    return {up: up, left: left, width: width, height: height};
  }

  function elementInViewport(p) {
    return  (p.up < window.pageYOffset + window.innerHeight && 
            p.left < window.pageXOffset + window.innerWidth && 
            (p.up + p.height) > window.pageYOffset && 
            (p.left + p.width) > window.pageXOffset);
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
      return words.every(function (regex) { return element.node.textContent.match(regex)});
    }
  }

  function HintElement(node,pos){

    this.node = node;
    this.isHinted = false;
    this.position = pos;

    this.addHint = function (labelNum) {
      // TODO: fix uzblclassfirst
      if(!this.isHinted){
        this.node.className += " " + uzblclass;
      }
      this.isHinted = true;
        
      // create hint  
      var hintNode = doc.createElement('div');
      hintNode.name = uzblid;
      hintNode.innerText = labelNum;
      hintNode.style.left = this.position.left + 'px';
      hintNode.style.top =  this.position.up + 'px';
      hintNode.style.position = "absolute";
      doc.body.firstChild.appendChild(hintNode);
        
    }
    this.removeHint = function(){
      if(this.isHinted){
        var s = (this.num)?uzblclassfirst:uzblclass;
        this.node.className = this.node.className.replace(new RegExp(" "+s,"g"),"");
        this.isHinted = false;
      }
    }
  }

  function createHintDiv(){
    var hintdiv = doc.getElementById(uzblid);
    if(hintdiv){
      hintdiv.parentNode.removeChild(hintdiv);
    }
    hintdiv = doc.createElement("div");
    hintdiv.setAttribute('id',uzblid);
    doc.body.insertBefore(hintdiv,doc.body.firstChild);
    return hintdiv;
  }

  function init(){
    // WHAT?
    doc.body.setAttribute("onkeyup","hints.keyPressHandler(event)");
    hintdiv = createHintDiv();
    visible = [];

    var items = doc.evaluate(hintable,doc,null,XPathResult.ORDERED_NODE_SNAPSHOT_TYPE,null);
    for (var i = 0;i<items.snapshotLength;i++){
      var item = items.snapshotItem(i);
      var pos = elementPosition(item);
      if(isVisible && elementInViewport(elementPosition(item))){
        visible.push(new HintElement(item,pos));
      }
    }
  }

  function clear(){

    visible.forEach(function (n) { n.removeHint(); } );
    hintdiv = doc.getElementById(uzblid);
    while(hintdiv){
      hintdiv.parentNode.removeChild(hintdiv);
      hintdiv = doc.getElementById(uzblid);
    }
  }

  function update(str) {
    var match = new Matcher(str);
    hintdiv = createHintDiv();
    var i = 1;
    visible.forEach(function (n) {
      if(match.test(n)) {
        n.addHint(i);
        i++;
      } else {
        n.removeHint();
      }});
  }

  function hint(str){
    if(str.length == 0) init();
    update(str);
  }

  function keyPressHandler(e) {
    var kC = window.event ? event.keyCode: e.keyCode;
    var Esc = window.event ? 27 : e.DOM_VK_ESCAPE;
    if (kC == Esc) {
        clear();
        doc.body.removeAttribute("onkeyup");
    }
  }

  function follow(str){
    var m = new Matcher(str);

    var items = visible.filter(function (n) { return n.isHinted });
    clear();
    var num = parseInt(m.numbers,10);
    if(num){
      var item = items[num-1].node;
    } else {
      var item = items[0].node;
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

var hints = new Hints();
//document.attachEvent("onKeyUp",hints.keyPressHandler);

// vim:set et tw=2:


