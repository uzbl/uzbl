#!/usr/bin/perl

# a slightly more advanced form filler
#
# uses settings file like: $keydir/<domain>

# user arg 1:
# edit: force editing of the file (fetches if file is missing)
# load: fill forms from file (fetches if file is missing)
# new:  fetch new file	

# usage example:
# bind LL = spawn /usr/share/uzbl/examples/scripts/formfiller.pl load
# bind LN = spawn /usr/share/uzbl/examples/scripts/formfiller.pl new
# bind LE = spawn /usr/share/uzbl/examples/scripts/formfiller.pl edit

use strict;
use Switch;

my $keydir = $ENV{XDG_CONFIG_HOME} . "/uzbl/forms";
my ($config,$pid,$xid,$fifo,$socket,$url,$title,$cmd) = @ARGV;
if($fifo eq "") { die "No fifo"; };

sub domain {
  my ($url) = @_;
  $url =~ s#http(s)?://([A-Za-z0-9\.-]+)(/.*)?#$2#;
  return $url;
};

my $editor = "xterm -e vim";
#my $editor = "gvim";

# ideally, there would be some way to ask uzbl for the html content instead of having to redownload it with
#	Also, you may need to fake the user-agent on some sites (like facebook)
 my $downloader = "curl -A 'Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.9.0.10) Gecko/2009042810 GranParadiso/3.0.10' ";
#my $downloader = "curl -s";

my @fields = ("type","name","value");

my %command;

$command{load} = sub {
  my ($domain) = @_;
  my $file = "$keydir/$domain";
  if( -e $file){
    open(FH,$file);
    my (@lines) = <FH>;
    close(FH);
    $|++;
    open(FIFO,">>$fifo");
    print "opened $fifo\n";
    foreach my $line (@lines) {
        if($line !~ m/^#/){
          my ($type,$name,$value) = ($line =~ /\s*(\w+)\s*\|\s*(.*?)\s*\|\s*(.*?)\s*$/);
          switch ($type) {
            case ""         {}
            case "checkbox" { printf FIFO 'act js document.getElementsByName("%s")[0].checked = %s;',	$name, $value}
            case "submit"   { printf FIFO 'act js function fs (n) {try{n.submit()} catch (e){fs(n.parentNode)}}; fs(document.getElementsByName("%s")[0]);', $name }
            else            { printf FIFO 'act js document.getElementsByName("%s")[0].value = "%s";',	$name, $value}
          }

        print FIFO "\n";
      }
    }
    $|--;
    close(FIFO);
  } else {
    $command{new}->($domain);
    $command{edit}->($domain);
  }
};
$command{edit} = sub {
  my ($domain) = @_;
  my $file = "$keydir/$domain";
  if(-e $file){
    system ($editor, $file);
  } else {
    $command{new}->($domain);
}
};
$command{new} = sub {
  my ($domain) = @_;
  my $file = "$keydir/$domain";
  open(FILE,">>$file");
  $|++;
  print FILE "#make sure that there are no extra submits, since it may trigger the wrong one\n";
  printf FILE "#%-10s | %-10s | %s\n", @fields;
  print FILE "#------------------------------\n";
  my @data = `$downloader $url`;
  foreach my $line (@data){
    if($line =~ m/<input ([^>].*?)>/i){
      $line =~ s/.*(<input ([^>].*?)>).*/\1/;
      printf FILE " %-10s | %-10s | %s\n", map { my ($r) = $line =~ /.*$_=["'](.*?)["']/;$r } @fields;
    };
  };
  close(FILE);
  $|--;
};
$command{$cmd}->(domain($url));
