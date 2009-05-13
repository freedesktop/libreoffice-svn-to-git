#!/bin/bash

# Use like:
# ( time ./ooo-svn-to-git.sh /disk/git/svn /local/test/ooo-build ) 2>&1 | tee .log

SOURCE="$1"
TARGET="$2"

COMMITTERS="$3"
LAYOUT="$4"

WD=`pwd`

if [ ! -d "$SOURCE" -o -e "$TARGET" -o -z "$TARGET" -o -z "$COMMITTERS" -o -z "$LAYOUT" ] ; then
    cat 1>&2 <<EOF
Usage: svn-to-git.sh source target

source     Directory with the SVN repo
target     Directory where the resulting git repos are created (must not exist)
committers A file describing the committers
layout     A file describing the layout of the repositories
EOF
    exit 1;
fi

mkdir -p "$TARGET"
rm *.dump

for I in `sed -e 's/^[#:].*//' -e 's/^ignore-.*//' -e 's/=.*//' "$LAYOUT" | grep -v '^$'` ; do
    mkdir "$TARGET/$I"
    mkfifo $I.dump
    ( cd "$TARGET/$I" ; git init ; git fast-import < "$WD"/$I.dump ) &
done

./svn-fast-export "$SOURCE" "$COMMITTERS" "$LAYOUT"

# wait until everything's finished
while [ -n "`jobs`" ] ; do
    echo jobs:
    jobs
    sleep 1
done

for I in `sed -e 's/^[#:].*//' -e 's/^ignore-.*//' -e 's/=.*//' "$LAYOUT" | grep -v '^$'` ; do
    ( cd "$TARGET/$I" ; echo `pwd` ; git branch | sed 's/^\*/ /' | grep 'tag-branches/' | xargs git branch -D )
    rm $I.dump
done
