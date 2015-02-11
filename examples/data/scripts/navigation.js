uzbl.navigation = (function () {
'use strict';

var trySelectors = function (selectors) {
    var elem;
    var i;
    for (i = 0; i < selectors.length; ++i) {
        elem = document.querySelector(selectors[i]);
        if (elem !== null) {
            return elem;
        }
    }
    return undefined;
};

var nextAnchor = function () {
    return trySelectors(
      [ 'a[rel="next"]'
      , 'link[rel="next"]'
      , 'a[title="Next"]'
      , 'a[title="next"]'
      ]
    );
};

var prevAnchor = function () {
    return trySelectors(
      [ 'a[rel="prev"]'
      , 'link[rel="prev"]'
      , 'a[title="Previous"]'
      , 'a[title="previous"]'
      ]
    );
};

var findLink = function (elem) {
    if (typeof elem !== 'undefined') {
        return elem.href;
    }

    return "";
};

return {
    next: function () {
        return findLink(nextAnchor());
    },
    prev: function () {
        return findLink(prevAnchor());
    }
};
}());
