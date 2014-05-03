### Users

Just use Uzbl, hang around in our IRC channel, try out different things and report bugs.
If you're feeling more adventurous, you can use one of the development branches and give bug
reports and suggestions straight to the developer in charge of that, so the
same problems don't occur when they get merged into the master branch.
Play around with the configs and scripts and see if you can improve things.
The wiki can be a good source of inspiration.
You may also find mistakes in our documentation.

### Developers

If you don't feel like just sending bug reports, you can dive into the
code and start hacking.
Even if you're not good at C, thanks to uzbl's 'ecosystem' of scripts there
is a lot that can be done.
However, it's usually a good idea to tell us first
what you want to do, to avoid unneeded or duplicate work.

Read on for more info...

### Clone/patch/merge workflow

1. clone the code with git.
   Either you host your git repo yourself, or use something userfriendly
   like [Github](http://www.github.com)
   If you want to use github, you basically just need to register and click
   the fork button on [uzbl/uzbl](http://github.com/uzbl/uzbl)
   If you're new to Git/github, have no fear:

   * [Github guides (highly recommended)](http://github.com/guides/home)
   * [Guides: Fork a project and submit your modifications](http://github.com/guides/fork-a-project-and-submit-your-modifications)

2. Do your work, test it and push to your repo. It may be interesting to ask
   others for input on your work.  Develop against master for small changes/fixes,
   experimental or a new topic branch for experimental/intrusive work.

3. If you think your code should be in the main uzbl code and meets all
   requirements (see below), then file a Pull Request on github.

### Patch/branch requirements before merging:

* Patches must be about one thing.  If you want to work on multiple things,
  create new branches. I allow exceptions for trivial typo fixes and such, but
  that's it. This also implies that you also need to update your tree reguraly.
  Don't fall behind too much. (i.e., merge from upstream)
* Any change in functionality that you want merged in must also be documented.
  There is a readme and some files in the repository which should correspond to
  the code base at all times. Update them, not only for end users but also for
  your fellow hackers.
* Your code should not introduce any compile warnings or errors. And also, no
  regressions but that's harder to check.
* Please try to keep the code clean - we don't like extraneous whitespace.
  Follow the coding style in the file. The sample pre-commit hook can check for
  trailing whitespace - so go ahead and::

    $ cp .git/hooks/pre-commit.sample .git/hooks/pre-commit

That said, you can always ask us to check on your stuff or ask for advice.

### Bug Reporting

Bug reports are also welcome, especially the ones that come with a patch ;-) .
Before making a new ticket, check whether the bug is reported already. If you
want to report a bug and you don't know where the problem in the code is,
please supply the output of `uzbl-core --bug-info`.

### Valgrind profiling

  $ add this to Makefile header: CFLAGS=-g
  $ recompile
  $ valgrind --tool=callgrind ./uzbl-core ....
  $ kcachegrind callgrind.out.foo

### Memory leak checking

  valgrind --tool=memcheck --leak-check=full ./uzbl-core

### Writing unit tests

If you can, write a unit test for a bugfix or new functionality. Add relevant
unit tests to existing .c files in tests/. Others should be made in new source
files with corresponding changes to the tests/Makefile. Run all tests with
`make test`

### Debugging / backtraces

* compile with -ggdb (enabled by default on experimental tree)
* run: `gdb ./uzbl-core`
* `(gdb) run -c /path/to/config`
* `bt` if it segfaults to see a backtrace
* you'll find more info on the interwebs
