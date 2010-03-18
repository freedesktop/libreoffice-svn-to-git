#!/bin/bash

# Use like:
# ( time ./ooo-svn-to-git.sh /disk/git/svn /local/test/ooo-build ) 2>&1 | tee .log

TYPE="$1"

SOURCE="$2"
TARGET="$3"

COMMITTERS="$4"
LAYOUT="$5"

WD=`pwd`

COMMAND=false
case "$TYPE" in
    hg)  COMMAND='./hg-fast-export' ;;
    svn) COMMAND='./svn-fast-export' ;;
esac

if [ ! -d "$SOURCE" -o -e "$TARGET" -o -z "$TARGET" -o -z "$COMMITTERS" -o -z "$LAYOUT" -o "$COMMAND" = "false" ] ; then
    cat 1>&2 <<EOF
Usage: to-git.sh type source target

Converts a SVN or a Mercurial repository to git.

type       Type of the repo to convert - "svn" or "hg"
source     Directory with the repo
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

# execute hg-fast-export, or svn-fast-export
"$COMMAND" "$SOURCE" "$COMMITTERS" "$LAYOUT"
RETURN_VALUE=$?

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

exit $RETURN_VALUE
