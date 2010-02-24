// A Link Follower for Uzbl.
// P.C. Shyamshankar <sykora@lucentbeing.com>
//
// WARNING: this script depends on the Uzbl object which is now disabled for
// WARNING  security reasons. So the script currently doesn't work but it's
// WARNING  interesting nonetheless
//
// Based extensively (like copy-paste) on the follow_numbers.js and
// linkfollow.js included with uzbl, but modified to be more customizable and
// extensible.
//
// Usage
// -----
//
// First, you'll need to make sure the script is loaded on each page. This can
// be done with:
//
// @on_event LOAD_COMMIT script /path/to/follower.js
// 
// Then you can bind it to a key:
//
// @bind f* = js follower.follow('%s', matchSpec, handler, hintStyler)
//
// where matchSpec, handler and hintStyler are parameters which control the
// operation of follower. If you don't want to customize any further, you can
// set these to follower.genericMatchSpec, follower.genericHandler and
// follower.genericHintStyler respectively.
//
// For example, 
//
// @bind f* = js follower.follow('%s', follower.genericMatchSpec, follower.genericHandler, follower.genericHintStyler)
// @bind F* = js follower.follow('%s', follower.onlyLinksMatchSpec, follower.newPageHandler, follower.newPageHintStyler)
//
// In order to make hints disappear when pressing a key (the Escape key, for
// example), you can do this:
//
// @bind <Escape> = js follower.clearHints()
//
// If your Escape is already bound to something like command mode, chain it.
//
// Alternatively, you can tell your <Escape> key to emit an event, and handle
// that instead.
//
// @bind <Escape> = event ESCAPE
// @on_event ESCAPE js follower.clearHints()
//
// Customization
// -------------
//
// If however you do want to customize, 3 Aspects of the link follower can be
// customized with minimal pain or alteration to the existing code base:
//
//  * What elements are hinted.
//  * The style of the hints displayed.
//  * How the hints are handled.
// 
// In order to customize behavior, write an alternative, and pass that in to
// follower.follow invocation. You _will_ have to modify this script, but only
// locally, it beats having to copy the entire script under a new name and
// modify.
//
// TODO:
//  * Whatever all the other TODOs in the file say.
//  * Find out how to do default arguments in Javascript.
//  * Abstract out the hints into a Hint object, make hintables a list of hint
//    objects instead of two lists.

// Helpers
String.prototype.lpad = function(padding, length) {
    var padded = this;
    while (padded.length < length) {
        padded = padding + padded;
    }

    return padded;
}

function Follower() { 

    // Globals
    var uzblID = 'uzbl-follow'; // ID to apply to each hint. 
    var uzblContainerID = 'uzbl-follow-container'; // ID to apply to the div containing hints. 

    // Translation table, used to display something other than numbers as hint
    // labels. Typically set to the ten keys of the home row.
    //
    // Must have exactly 10 elements.
    //
    // I haven't parameterized this, to make it customizable. Should I? Do
    // people really use more than one set of keys at a time?
    var translation = ["a", "r", "s", "t", "d", "h", "n", "e", "i", "o"];

    // MatchSpecs
    // These are XPath expressions which indicate which elements will be hinted.
    // Use multiple expressions for different situations, like hinting only form
    // elements, or only links, etc.
    //
    // TODO: Check that these XPath expressions are correct, and optimize/make
    // them more elegant. Preferably by someone who actually knows XPath, unlike
    // me.

    // Vimperator default (copy-pasted, I never used vimperator).
    this.genericMatchSpec = " //*[@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link' or @href] | //input[not(@type='hidden')] | //a | //area | //iframe | //textarea | //button | //select";

    // Matches only links, suitable for opening in a new instance (I think).
    this.onlyLinksMatchSpec = " //*[@href] | //a | //area";

    // Follow Handlers
    // These decide how an element should be 'followed'. The handler is passed
    // the element in question.

    // Generic Handler, opens links in the same instance, emits the FORM_ACTIVE
    // event if a form element was chosen. Also clears the keycmd.
    this.genericHandler = function(node) {
        if (node) {
            if (window.itemClicker != undefined) {
                window.itemClicker(node);
            } else {
                var tag = node.tagName.toLowerCase();
                if (tag == 'a') {
                    node.click();
                    window.location = node.href;
                } else if (tag == 'input') {
                    var inputType = node.getAttribute('type');
                    if (inputType == undefined)
                        inputType = 'text';

                    inputType = inputType.toLowerCase();

                    if (inputType == 'text' || inputType == 'file' || inputType == 'password') {
                        node.focus();
                        node.select();
                    } else {
                        node.click();
                    }
                    Uzbl.run("event FORM_ACTIVE");
                } else if (tag == 'textarea'|| tag == 'select') {
                    node.focus();
                    node.select();
                    Uzbl.run("event FORM_ACTIVE");
                } else {
                    node.click();
                    if ((node.href != undefined) && node.href)
                        window.location = node.href;
                }
            }
        }
        Uzbl.run("event SET_KEYCMD");
    }
    
    // Handler to open links in a new page. The rest is the same as before.
    this.newPageHandler = function(node) {
        if (node) {
            if (window.itemClicker != undefined) {
                window.itemClicker(node);
            } else {
                var tag = node.tagName.toLowerCase();
                if (tag == 'a') {
                    node.click();
                    Uzbl.run("@new_window " + node.href);
                } else if (tag == 'input') {
                    var inputType = node.getAttribute('type');
                    if (inputType == undefined)
                        inputType = 'text';

                    inputType = inputType.toLowerCase();

                    if (inputType == 'text' || inputType == 'file' || inputType == 'password') {
                        node.focus();
                        node.select();
                    } else {
                        node.click();
                    }
                    Uzbl.run("event FORM_ACTIVE");
                } else if (tag == 'textarea'|| tag == 'select') {
                    node.focus();
                    node.select();
                    Uzbl.run("event FORM_ACTIVE");
                } else {
                    node.click();
                    if ((node.href != undefined) && node.href)
                        window.location = node.href;
                }
            }
        }
        Uzbl.run("event SET_KEYCMD");
    };

    // Hint styling.
    // Pretty much any attribute of the hint object can be modified here, but it
    // was meant to change the styling. Useful to differentiate between hints
    // with different handlers.
    //
    // Hint stylers are applied at the end of hint creation, so that they
    // override the defaults.

    this.genericHintStyler = function(hint) {
        hint.style.backgroundColor = '#AAAAAA';
        hint.style.border = '2px solid #4A6600';
        hint.style.color = 'black';
        hint.style.fontSize = '10px';
        hint.style.fontWeight = 'bold';
        hint.style.lineHeight = '12px';
        return hint;
    };

    this.newPageHintStyler = function(hint) {
        hint.style.backgroundColor = '#FFCC00';
        hint.style.border = '2px solid #4A6600';
        hint.style.color = 'black';
        hint.style.fontSize = '10px';
        hint.style.fontWeight = 'bold';
        hint.style.lineHeight = '12px';
        return hint;
    };

    // Beyond lies a jungle of pasta and verbosity.

    // Translate a numeric label using the translation table.
    function translate(digitLabel, translationTable) {
        translatedLabel = '';
        for (var i = 0; i < digitLabel.length; i++) {
            translatedLabel += translationTable[digitLabel.charAt(i)];
        }

        return translatedLabel;
    }

    function computeElementPosition(element) {
        var up = element.offsetTop;
        var left = element.offsetLeft;
        var width = element.offsetWidth;
        var height = element.offsetHeight;

        while (element.offsetParent) {
            element = element.offsetParent;
            up += element.offsetTop;
            left += element.offsetLeft;
        }

        return {up: up, left: left, width: width, height: height};
    }

    // Pretty much copy-pasted from every other link following script.
    function isInViewport(element) {
        offset = computeElementPosition(element);

        var up = offset.up;
        var left = offset.left;
        var width = offset.width;
        var height = offset.height;

        return up < window.pageYOffset + window.innerHeight &&
               left < window.pageXOffset + window.innerWidth &&
               (up + height) > window.pageYOffset &&
               (left + width) > window.pageXOffset;
    }

    function isVisible(element) {
        if (element == document) {
            return true;
        }

        if (!element){
            return false;
        }

        if (element.style) {
            if (element.style.display == 'none' || element.style.visibiilty == 'hidden') {
                return false;
            }
        }

        return isVisible(element.parentNode);
    }

    function generateHintContainer() {
        var container = document.getElementById(uzblContainerID);
        if (container) {
            container.parentNode.removeChild(container);
        }

        container = document.createElement('div');
        container.id = uzblContainerID;

        if (document.body) {
            document.body.appendChild(container);
        }
        return container;
    }

    // Generate everything that is to be hinted, as per the given matchSpec.
    // hintables[0] refers to the items, hintables[1] to their labels.
    function generateHintables(matchSpec) {
        var hintables = [[], []];

        var itemsFromXPath = document.evaluate(matchSpec, document, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);

        for (var i = 0; i < itemsFromXPath.snapshotLength; ++i) {
            var element = itemsFromXPath.snapshotItem(i);
            if (element && isVisible(element) && isInViewport(element)) {
                hintables[0].push(element);
            }
        }

        // Assign labels to each hintable. Can't be combined with the previous
        // step, because we didn't know how many there were at that time.
        var hintLength = hintables.length;
        for (var i = 0; i < hintables[0].length; ++i) {
            var code = translate(i.toString(), translation);
            hintables[1].push(code.lpad(translation[0], hintLength));
        }

        return hintables;
    }

    // Filter the hintables based on input from the user. Makes the screen less
    // cluttered after the user has typed some prefix of hint labels.
    function filterHintables(hintables, target) {
        var filtered = [[], []];

        var targetPattern = new RegExp("^" + target);

        for (var i = 0; i < hintables[0].length; i++) {
            if (hintables[1][i].match(targetPattern)) {
                filtered[0].push(hintables[0][i]);
                filtered[1].push(hintables[1][i].substring(target.length));
            }
        }

        return filtered;
    }

    // TODO make this use the container variable from main, instead of searching
    // for it?
    function clearHints() {
        var container = document.getElementById(uzblContainerID);
        if (container) {
            container.parentNode.removeChild(container);
        }
    }

    // So that we can offer this as a separate function.
    this.clearHints = clearHints;

    function makeHint(node, code, styler) {
        var position = computeElementPosition(node);
        var hint = document.createElement('div');

        hint.name = uzblID;
        hint.innerText = code;
        hint.style.display = 'inline';

        hint.style.margin = '0px';
        hint.style.padding = '1px';
        hint.style.position = 'absolute';
        hint.style.zIndex = '10000';

        hint.style.left = position.left + 'px';
        hint.style.top = position.up + 'px';

        var img = node.getElementsByTagName('img');
        if (img.length > 0) {
            hint.style.left = position.left + img[0].width / 2 + 'px';
        }

        hint.style.textDecoration = 'none';
        hint.style.webkitBorderRadius = '6px';
        hint.style.webkitTransform = 'scale(1) rotate(0deg) translate(-6px, -5px)';

        hint = styler(hint); // So that custom hint stylers can override the above.
        return hint;
    }


    function drawHints(container, hintables, styler) {
        for (var i = 0; i < hintables[0].length; i++) {
            hint = makeHint(hintables[0][i], hintables[1][i], styler);
            container.appendChild(hint);
        }

        if (document.body) {
            document.body.appendChild(container);
        }
    }

    // The main hinting function. I don't know how to do default values to
    // functions, so all arguments must be specified. Use generics if you must.
    this.follow = function(target, matchSpec, handler, hintStyler) {
        var container = generateHintContainer(); // Get a container to hold all hints. 
        var allHintables = generateHintables(matchSpec); // Get all items that can be hinted.
        hintables = filterHintables(allHintables, target); // Filter them based on current input. 
        
        clearHints(); // Clear existing hints, if any.

        if (hintables[0].length == 0) {
            // Nothing was hinted, user pressed an unknown key, maybe?
            // Do nothing.
        } else if (hintables[0].length == 1) {
            handler(hintables[0][0]); // Only one hint remains, handle it. 
        } else {
            drawHints(container, hintables, hintStyler); // Draw whatever hints remain. 
        }

        return;
    };
} 

// Make on-click links clickable.
try {
    HTMLElement.prototype.click = function() {
        if (typeof this.onclick == 'function') {
            this.onclick({
                type: 'click'
            });
        }
    };
} catch(e) {}

follower = new Follower();
