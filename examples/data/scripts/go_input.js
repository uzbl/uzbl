var elements = document.querySelectorAll("textarea, input" + [
	":not([type='button'])",
	":not([type='checkbox'])",
	":not([type='hidden'])",
	":not([type='image'])",
	":not([type='radio'])",
	":not([type='reset'])",
	":not([type='submit'])"].join(""));
function gi() {
	if (elements) {
		var el, i = 0;
		while((el = elements[i++])) {
			var style=getComputedStyle(el, null);
			if (style.display !== 'none' && style.visibility === 'visible') {
				if (el.type === "file") {
					el.click();
				}
				else {
					el.focus();
				}
				return "XXXFORM_ACTIVEXXX";
			}
		}
	}
}

gi();
