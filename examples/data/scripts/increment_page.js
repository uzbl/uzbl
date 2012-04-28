(function () { var found = location.href.match(/(.*?)(\d+)([^\d]*)$/); if (found) { location = found[1] + (Number(found[2]) + 1) + found[3]; }})()
