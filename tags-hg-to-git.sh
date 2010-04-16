#!/bin/bash
#
# To create a mapping between Hg and Git tags
# Just a quick hack ;-)
#
# Use like:
# cd ooo-build/src/clone
# for I in * ; do [ -d $I ] && ( cd $I ; echo -n ":commit map=$I," ; /local/projects/svn-to-git/tags-hg-to-git.sh ) ; done > repositories.txt
#

( cd /local/projects/mercurial/OOO320 ; hg tags ) > /tmp/tags

for T in `git tag` 
do
    N=`echo $T | sed 's#^ooo/##'`
    ( grep -m 1 "$N" /tmp/tags &&
      git show $T | grep -m 1 '^commit ' | sed 's/commit //' ) | tr ':\n' '  '
    echo
done | while read TAG COMMIT HG GIT
do
    [ -n "$COMMIT" ] && echo -n "$COMMIT:$GIT "
done
echo
