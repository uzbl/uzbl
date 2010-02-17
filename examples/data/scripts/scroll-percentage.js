// VIM ruler style scroll message
(function() {
	var run = Uzbl.run;
	var update_message = function() {
		var innerHeight = window.innerHeight;
		var scrollY = window.scrollY;
		var height = document.height;
		var message;

		if (UzblZoom.type === "full") {
			var zoom_level = UzblZoom.level;
			innerHeight = Math.ceil(innerHeight * zoom_level);
			scrollY = Math.ceil(scrollY * zoom_level);
			height -= 1;
		}

		if (! height) {
			message = "";
		}
		else if (height <= innerHeight) {
			message = run("print @scroll_all_indicator") || "All";
		}
		else if (scrollY === 0) {
			message = run("print @scroll_top_indicator") || "Top";
		}
		else if (scrollY + innerHeight >= height) {
			message = run("print @scroll_bottom_indicator") || "Bot";
		}
		else {
			var percentage = Math.round(scrollY / (height - innerHeight) * 100);
			message = percentage + "%";
		}
		run("set scroll_message=" + message);
	};

	self.UzblZoom = {
		get level() {
			return Number(run("print @zoom_level")) || 1;
		},
		set level(level) {
			if (typeof level === "number" && level > 0) {
				run("set zoom_level = " + level);
				update_message();
			}
		},
		get type() {
			return run("print @zoom_type") || "text";
		},
		set type(type) {
			if ((type === "text" || type === "full") && this.type != type) {
				run("toggle_zoom_type");
				run("set zoom_type = " + type);
				update_message();
			}
		},
		toggle_type: function() {
			this.type = (this.type === "text" ? "full" : "text");
		}
	};

	window.addEventListener("DOMContentLoaded", update_message, false);
	window.addEventListener("load", update_message, false);
	window.addEventListener("resize", update_message, false);
	window.addEventListener("scroll", update_message, false);
	update_message();
})();

// vim: set noet ff=unix
