(function () {
    var body = document.body;
    var doc = document.documentElement;
    var percentage = '--';
    if (doc.scrollHeight != doc.clientHeight) {
        var current_position = body.scrollTop + doc.scrollTop;
        var total_height = doc.scrollHeight - doc.clientHeight;
        percentage = current_position / total_height;
        percentage = Math.round(10000 * percentage) / 100;
    }
    return (percentage + '%');
})()
