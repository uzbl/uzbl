/* This script finds all editable input elemnts and generate file for
 * formfiller script. It must be invoked from formfiller.sh */

(function () {
  /* evaluate XPath query */
  var xp_res=document.evaluate("//input", document.documentElement, null, XPathResult.ANY_TYPE,null);
  var rv="";
  var input;

  while(input=xp_res.iterateNext()) {
    var type=(input.type?input.type:text);
    switch (type) {
      case "text":
      case "password":
      case "file":
        rv += input.name + "(" + type + "):" + input.value + "\n";
	break;
      case "checkbox":
      case "radio":
        rv += input.name + "[" + input.value + "]" + "(" + type + "):" + (input.checked?"ON":"") + "\n";
	break;
      /* Not supported:
       *   case "button":
       *   case "image":
       *   case "reset":
       *   case "submit":
       *   case "hidden":
       */
    }
  }
  return rv;
})()
