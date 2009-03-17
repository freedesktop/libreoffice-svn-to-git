#!/bin/bash

# Use like:
# ( time ./ooo-svn-to-git.sh /disk/git/svn /local/test/ooo-build ) 2>&1 | tee .log

SOURCE="$1"
TARGET="$2"

COMMITTERS="ooo-build-committers.txt"
LAYOUT="ooo-build-repositories.txt"

WD=`pwd`

if [ ! -d "$SOURCE" -o -e "$TARGET" -o -z "$TARGET" ] ; then
    cat 1>&2 <<EOF
Usage: ooo-build-svn-to-git.sh source target

source Directory with the SVN repo
target Directory where the resulting git repos are created (must not exist)
EOF
    exit 1;
fi

mkdir -p "$TARGET"
rm *.dump

for I in `sed -e 's/^[#:].*//' -e 's/^did-not-fit-anywhere.*//' -e 's/=.*//' "$LAYOUT" | grep -v '^$'` ; do
    mkdir "$TARGET/$I"
    mkfifo $I.dump
    ( cd "$TARGET/$I" ; git init ; git fast-import < "$WD"/$I.dump ) &
done

./svn-fast-export "$SOURCE" "$COMMITTERS" "$LAYOUT"

for I in `sed -e 's/^[#:].*//' -e 's/^did-not-fit-anywhere.*//' -e 's/=.*//' "$LAYOUT" | grep -v '^$'` ; do
    rm $I.dump
done
