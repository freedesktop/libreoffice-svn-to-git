#!/bin/bash
#
# To create a mapping between Hg and Git tags
# Just a quick hack ;-)
#
# Use like:
# for I in * ; do ( cd $I ; echo -n ":tag map=$I," ; /local/projects/svn-to-git/tags-hg-to-git.sh ) ; done > /local/projects/svn-to-git/tags-hg-to-git.sh
#

( cd /local/home/ooogit/mercurial/DEV300 ; hg tags ) > /tmp/tags

for T in `git tag` 
do
    N=`echo $T | sed 's#^ooo/##'`
    ( grep -m 1 "$N" /tmp/tags &&
      git show $T | grep '^commit' | head -n 1 | sed 's/commit //' ) | tr ':\n' '  '
    echo
done | while read TAG COMMIT HG GIT
do
    [ -n "$COMMIT" ] && echo -n "$COMMIT:$GIT "
done
echo
