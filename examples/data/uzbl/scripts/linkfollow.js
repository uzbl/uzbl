// link follower for uzbl
// requires http://github.com/DuClare/uzbl/commit/6c11777067bdb8aac09bba78d54caea04f85e059
//
// first, it needs to be loaded before every time it is used.
// One way would be to use the load_commit_handler:
// set load_commit_handler = sh 'echo "script /usr/share/uzbl/examples/scripts/linkfollow.js" > "$4"'
//
// when script is loaded, it can be invoked with
// bind f* = js hints.set("%s",   hints.open)
// bind f_ = js hints.follow("%s",hints.open)
//
// At the moment, it may be useful to have way of forcing uzbl to load the script
// bind :lf = script /usr/share/uzbl/examples/scripts/linkfollow.js
//
// The default style for the hints are pretty ugly, so it is recommended to add the following
// to config file
// set stylesheet_uri = /usr/share/uzbl/examples/data/style.css
//
// based on follow_Numbers.js
//
// TODO: fix styling for the first element
// TODO: emulate mouseover events when visiting some elements
// TODO: rewrite the element->action handling


function Hints(){

  // Settings
  ////////////////////////////////////////////////////////////////////////////

  // if set to true, you must explicitly call hints.follow(), otherwise it will
  // follow the link if there is only one matching result
  var requireReturn = true;

  // Case sensitivity flag
  var matchCase = "i";

  // For case sensitive matching, uncomment:
  // var matchCase = "";


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

  // the vimperator defaults minus the xhtml elements, since it gave DOM errors
  var hintable = " //*[@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link' or @href] | //input[not(@type='hidden')] | //a | //area | //iframe | //textarea | //button | //select";

  function Matcher(str){
    var numbers = str.replace(/[^\d]/g,"");
    var words = str.replace(/\d/g,"").split(/\s+/).map(function (n) { return new RegExp(n,matchCase)});
    this.test = test;
    this.toString = toString;
    this.numbers = numbers;
    function matchAgainst(element){
      if(element.node.nodeName == "INPUT"){
        return element.node.value;
      } else {
        return element.node.textContent;
      }
    }
    function test(element) {
      // test all the regexp
      var item = matchAgainst(element);
      return words.every(function (regex) { return item.match(regex)});
    }
  }

  function HintElement(node,pos){

    this.node = node;
    this.isHinted = false;
    this.position = pos;
    this.num = 0;

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

  function update(str,openFun) {
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
    if(!requireReturn){
      if(i==2){ //only been incremented once
        follow(str,openFun);
      }
    }
  }

  function hint(str,openFun){
    if(str.length == 0) init();
    update(str,openFun);
  }

  function keyPressHandler(e) {
    var kC = window.event ? event.keyCode: e.keyCode;
    var Esc = window.event ? 27 : e.DOM_VK_ESCAPE;
    if (kC == Esc) {
        clear();
        doc.body.removeAttribute("onkeyup");
    }
  }

  this.openNewWindow = function(item){
    // TODO: this doesn't work yet
    item.className += " uzbl_follow";
    window.open(item.href,"uzblnew","");
  }
  this.open = function(item){
    simulateMouseOver(item);
    item.className += " uzbl_follow";
    window.location = item.href;
  }

  function simulateMouseOver(item){
    var evt = doc.createEvent("MouseEvents");
    evt.initMouseEvent("MouseOver",true,true,
        doc.defaultView,1,0,0,0,0,
        false,false,false,false,0,null);
    return item.dispatchEvent(evt);
  }


  function follow(str,openFunction){
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
      var name = item.tagName;
      if (name == 'A') {
        if(item.click) {item.click()};
          openFunction(item);
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
        openFunction(item);
      }
    }
  }
}

var hints = new Hints();

// vim:set et sw=2:
