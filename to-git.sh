#!/bin/bash

# Use like:
# ( time ./ooo-svn-to-git.sh /disk/git/svn /local/test/ooo-build ) 2>&1 | tee .log

TYPE="$1"

SOURCE="$2"
TARGET="$3"

COMMITTERS="$4"
LAYOUT="$5"

FROM="$6"

WD=`pwd`

loc=$(locale -a | grep -i "en_US\.utf" | grep "8$" | head -n 1)
if [ -z "$loc" ] ; then
    echo "cannot set the utf8 locale" 1>&2
    exit 1
else
    export LANG="$loc"
fi


COMMAND=false
case "$TYPE" in
    hg)  COMMAND="./hg-fast-export" ;;
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
clonefrom  For incremental imports, specify the source repos
EOF
    exit 1;
fi

mkdir -p "$TARGET"
rm *.dump

for I in `sed -e 's/^[#:].*//' -e 's/^ignore-.*//' -e 's/=.*//' "$LAYOUT" | grep -v '^$'` ; do
    echo "$I" | sed 's/:/ /' | (
        read NAME COMMIT
        mkdir "$TARGET/$NAME"
        mkfifo $NAME.dump
        if [ -z "$COMMIT" -o -z "$FROM" ] ; then
            ( cd "$TARGET/$NAME" ; git init ; git fast-import < "$WD"/$NAME.dump ) &
        else
            ( cd "$TARGET" ; git clone -n "$FROM/$NAME" "$NAME" ; \
              cd "$NAME" ; git reset -q --hard "$COMMIT" ; git fast-import < "$WD"/$NAME.dump ; \
              git checkout -f ) &
        fi
    )
done

# execute hg-fast-export, or svn-fast-export
$COMMAND "$SOURCE" "$COMMITTERS" "$LAYOUT"
RETURN_VALUE=$?

# wait until everything's finished
while [ -n "`jobs`" ] ; do
    echo jobs:
    jobs
    sleep 1
done

# wait one more minute for everything to settle down
# FIXME rather find a real solution...
sleep 60

for I in `sed -e 's/^[#:].*//' -e 's/^ignore-.*//' -e 's/=.*//' -e 's/:.*//' "$LAYOUT" | grep -v '^$'` ; do
    ( cd "$TARGET/$I" ; pwd ; git branch | sed 's/^\*/ /' | grep 'tag-branches/' | xargs -r git branch -D )
    rm $I.dump
done

exit $RETURN_VALUE
