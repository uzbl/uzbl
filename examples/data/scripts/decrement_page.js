(function () { var found = location.href.match(/(.*?)(\d+)([^\d]*)$/); if (found && found[2] != "0") { location = found[1] + (Number(found[2]) - 1) + found[3]; }})()
