for (var i=0; i < document.links.length; i++) {
    var uzblid = 'uzbl_link_hint_';
    var li = document.links[i];
    var pre = document.getElementById(uzblid+i);

    if (pre) {
        li.removeChild(pre);
    } else {
        var hint = document.createElement('div');
        hint.setAttribute('id',uzblid+i);
        hint.innerHTML = i;
        hint.style.display='inline';
        hint.style.lineHeight='90%';
        hint.style.backgroundColor='red';
        hint.style.color='white';
        hint.style.fontSize='small-xx';
        hint.style.fontWeight='light';
        hint.style.margin='0px';
        hint.style.padding='2px';
        hint.style.position='absolute';
        hint.style.textDecoration='none';
        hint.style.left=li.style.left;
        hint.style.top=li.style.top;
        li.insertAdjacentElement('afterBegin',hint);
    }
}
