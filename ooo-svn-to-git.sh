#!/bin/bash

# Use like:
# ( time ./ooo-svn-to-git.sh /disk/git/svn /local/test/ooo-build ) 2>&1 | tee .log

SOURCE="$1"
TARGET="$2"

COMMITTERS="committers.txt"
LAYOUT="reposlayout.txt"

WD=`pwd`

if [ ! -d "$SOURCE" -o -e "$TARGET" -o -z "$TARGET" ] ; then
    cat 1>&2 <<EOF
Usage: ooo-svn-to-git.sh source target

source Directory with the SVN repo
target Directory where the resulting git repos are created (must not exist)
EOF
    exit 1;
fi

mkdir -p "$TARGET"
rm *.dump

for I in `sed 's/=.*//' "$LAYOUT"` ; do
    if [ "$I" != "did-not-fit-anywhere" ] ; then
        mkdir "$TARGET/$I"

        mkfifo $I.dump

        ( cd "$TARGET/$I" ; git init ; git fast-import < "$WD"/$I.dump ) &
    fi
done

./svn-fast-export "$SOURCE" "$COMMITTERS" "$LAYOUT"
