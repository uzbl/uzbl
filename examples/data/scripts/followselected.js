(function() {
        var selection = window.getSelection().getRangeAt(0);
        var container = document.createElement('div');
        var elements;
        var idx;
        if('' + selection){
            // Check for links contained within the selection
            container.appendChild(selection.cloneContents());
            elements = container.getElementsByTagName('a');
            for(idx in elements){
                if(elements[idx].href){
                    document.location.href = elements[idx].href;
                    return;
                }
            }
            // Check for links which contain the selection
            container = selection.startContainer;
            while(container != document){
                if(container.href){
                    document.location.href = container.href;
                    return;
                }
                container = container.parentNode;
            }
        }
})();
