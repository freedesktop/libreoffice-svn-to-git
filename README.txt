svn-to-git tool
===============

This tool is based on svn-fast-export.c [1] which is nice, but lacks lots of
features we needed for the OpenOffice.org (from the OOo SVN) and ooo-build
(from the Gnome SVN) conversions to git.

Please note that this tool is created for our needs, so it does not have
a great build system, the code might be hacky etc., but it works really
well ;-) - at least for us [2].

Features:

- perfect import of trunk (master), branches and tags
  - even when you svn cp things around like
    svn cp branches/blah/bleh trunk/foo/bar
  - when there are commits to the tags

- change login names to real names and email addresses

- converts ChangeLog-like commit messages to git-like (configurable)

- allows you to split one SVN tree to multiple git trees
  - based on regexps

- allows you to ignore broken tags/commits

- and is really fast

So far I succesfully used it on the ooo-build tree [3], and use it regularly
for nightly imports from the OOo SVN [4].

Maybe you'll need to tweak it for your needs a bit, but hopefully it will
generally work for you well :-)

Compilation
===========

sudo zypper install subversion-devel
make

How to import your SVN tree to git
==================================

- You need a local copy of the SVN tree
  - see http://svn.collab.net/repos/svn/trunk/notes/svnsync.txt

- Then you need to create a file with the list of committers (to map the login
  names to real people and mail addresses), and a repository layout file

- As the last thing, you have to run svn-to-git.sh :-)
  - it will tell you what parameters does it need

Some example configurations:

- ooo-build
  - ooo-build-committers.txt - the list of committers
  - ooo-build-repositories.txt - the layout of the repositories + config
  - ooo-build-svn-to-git.sh - script that does it

- ooo
  - ooo-committers.txt - the list of committers
  - ooo-repositories.txt - the layout of the repositories + config
  - ooo-svn-to-git.sh - script that does it

HTH,
Jan Holesovsky <kendy@suse.cz>

[1] http://repo.or.cz/w/fast-export.git
[2] http://www.go-oo.org
[3] http://cgit.freedesktop.org/ooo-build/ooo-build
[4] the rest under http://cgit.freedesktop.org/ooo-build
