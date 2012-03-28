#!/usr/bin/perl


use Env;
use File::Find;

### BEGIN SHAMELESS SWIPE FROM TEXT::GLOB
$strict_leading_dot    = 0;
$strict_wildcard_slash = 0;

use constant debug => 1;

sub glob_to_regex {
    my $glob = shift;
    my $regex = glob_to_regex_string($glob);
    return qr/^$regex$/;
}

sub glob_to_regex_string {
    my $glob = shift;
    my ($regex, $in_curlies, $escaping);
    local $_;
    my $first_byte = 1;
    for ($glob =~ m/(.)/gs) {
        if ($first_byte) {
            if ($strict_leading_dot) {
                $regex .= '(?=[^\.])' unless $_ eq '.';
            }
            $first_byte = 0;
        }
        if ($_ eq '/') {
            $first_byte = 1;
        }
        if ($_ eq '.' || $_ eq '(' || $_ eq ')' || $_ eq '|' ||
            $_ eq '+' || $_ eq '^' || $_ eq '$' || $_ eq '@' || $_ eq '%' ) {
            $regex .= "\\$_";
        }
        elsif ($_ eq '*') {
            $regex .= $escaping ? "\\*" :
              $strict_wildcard_slash ? "[^/]*" : ".*";
        }
        elsif ($_ eq '?') {
            $regex .= $escaping ? "\\?" :
              $strict_wildcard_slash ? "[^/]" : ".";
        }
        elsif ($_ eq '{') {
            $regex .= $escaping ? "\\{" : "(";
            ++$in_curlies unless $escaping;
        }
        elsif ($_ eq '}' && $in_curlies) {
            $regex .= $escaping ? "}" : ")";
            --$in_curlies unless $escaping;
        }
        elsif ($_ eq ',' && $in_curlies) {
            $regex .= $escaping ? "," : "|";
        }
        elsif ($_ eq "\\") {
            if ($escaping) {
                $regex .= "\\\\";
                $escaping = 0;
            }
            else {
                $escaping = 1;
            }
            next;
        }
        else {
            $regex .= $_;
            $escaping = 0;
        }
        $escaping = 0;
    }
    print "# $glob $regex\n" if debug;

    return $regex;
}

sub match_glob {
    print "# ", join(', ', map { "'$_'" } @_), "\n" if debug;
    my $glob = shift;
    my $regex = glob_to_regex $glob;
    local $_;
    grep { $_ =~ $regex } @_;
}
### END SHAMELESS SWIPE


sub run_all_scripts {
    foreach (@_) {
        find(sub {$f = $_; run_script($File::Find::name) if is_script($f)}, $_);
    }
}

sub is_script {
    open(FILE, $_[0]);
    while (<FILE>) {
        if (/\s*\/\/\s*==UserScript==\s*$/m) {
            return 1;
        }
    }
    return 0;
}

sub run_script() {
    $script = shift;
    $old = $/;
    undef $/;
    open(FH, ($script));
    $FILE = <FH>;
    if ($FILE =~ /^\s*\/\/\s*==UserScript==\s*(.*)^\s*\/\/\s*==\/UserScript==\s*$/ms) {
        $skip = 1;
        @meta = split('\n', "$1");
        print "$script\n";
        TEST: { foreach (@meta) {
            if (/^\s*\/\/\s*\@(?:exclude)\s*(.+?)\s*$/) {
                $skip = 0;
                last TEST;
            }
            elsif ($skip && /^\s*\/\/\s*\@(?:match|include)\s*(.+?)\s*$/) {
                if (match_glob($1, $ENV{UZBL_URI})) {
                    $skip = 0;
                }
            }
        }}
        if ($skip == 0) {
            `notify-send "$script" "$ENV{UZBL_URI}"`;
            open(FIFO, ">> $ENV{UZBL_FIFO}");
            print FIFO "script '$script'\n";
            close(FIFO);
        }
    }
    $/ = $old;
    # my $meta x=~$(sed -ne '/^\s*\/\/\s*==UserScript==\s*$/,/^\s*\/\/\s*==\/UserScript==\s*$/p' "$SCRIPT")
}

my @dirs=("/usr/local/share/", "/usr/share/", $ENV{HOME}. "/.local/share");
for $i (0 .. @dirs-1) {
    $dirs[$i] .= "/uzbl/userscripts/";
}

run_all_scripts(@dirs);
# "`dirname $0`/../userscripts"
