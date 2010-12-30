// this is a userscript inspired by "Pipermail Navigation Links" by Michael
// Stone <http://userscripts.org/scripts/show/3174>.

// it adds previous month/next month navigation links in pipermail mailing
// list archives.

// we wrap the whole thing in a function (that gets called immediately) so
// that this script doesn't interfere with any javascript in the page.
(function() {

// figure out what page we're looking at right now
var urlparts = document.location.toString().split("/");
var currView = urlparts[urlparts.length-1].split("#")[0];
var currDate = urlparts[urlparts.length-2].split("-");

// figure out the URLs to the next month and previous month
var months = [ 'January', 'February', 'March', 'April', 'May', 'June', 'July',
               'August', 'September', 'October', 'November', 'December' ];

var thisMonth = currDate[1];
var prevMonth;
var nextMonth;

var thisYear  = currDate[0];
var prevYear = thisYear;
var nextYear = thisYear;

if(thisMonth == 'January') {
  prevMonth = "December";
  nextMonth = "February";
  prevYear = parseInt(thisYear) - 1;
} else if(thisMonth == 'December') {
  prevMonth = "November";
  nextMonth = "January";
  nextYear = parseInt(thisYear) + 1;
} else {
  var monthNum = months.indexOf(thisMonth);
  prevMonth = months[monthNum - 1];
  nextMonth = months[monthNum + 1];
}

var prevHref = "../" + prevYear + "-" + prevMonth + "/" + currView;
var nextHref = "../" + nextYear + "-" + nextMonth + "/" + currView;

// find the navigation header and footer
var selector = "a[href='date.html#start']";

// if we're on a "date" page then the date link isn't displayed
if(currView == "date.html")
  selector = "a[href='author.html#start']";

var navLinks = document.querySelectorAll(selector);

// append the prev/next links to the navigation header and footer
for(var i = 0; i < navLinks.length; i++) {
  var victim = navLinks[i].parentNode;

  var prevEl = document.createElement("a");
  prevEl.textContent = "[ prev month ]";
  prevEl.href = prevHref;

  var nextEl = document.createElement("a");
  nextEl.textContent = "[ next month ]";
  nextEl.href = nextHref;

  victim.appendChild(prevEl);
  victim.appendChild(document.createTextNode(" "));
  victim.appendChild(nextEl);
}

})();
