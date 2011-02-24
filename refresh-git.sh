#!/bin/bash

BIN_DIR="$(dirname $0)"
GIT_BASE=
HG_REPO=
COMMITTERS="ooo-committers.txt"
LAYOUT=
BRANCH="master"

while [ "${1:-}" != "" ] ; do
    case $1 in
	-g|--git-base) shift
	    GIT_BASE="$1"
	    ;;
	--hg) shift
	    HG_REPO="$1"
	    ;;
	-l|--layou) shift
	    LAYOUT="$1"
	    ;;
	-b|--branch) shift
	    BRANCH="$1"
	    ;;
	-c|--committers) shift
	    COMMITTERS="$1"
	    ;;
	-h|--help)
	    echo "$0 --git-base <dir where the git repos are> --hg <hg_source_repo> -l|--layout <layout_file> [-c|--committers <committers_file> ]"
	    exit 0
	    ;;
	-*)
	    echo "Invalid options $1" 1>&2
	    exit 1
	    ;;
	*)
	    echo "extra argument $1" 1>&2
	    exit 1
	    ;;
    esac
    shift
done

if [ -z "$GIT_BASE" ] ; then
    echo "Missing mandatory --git-base option" 1>&2
    exit 1
else
    if [ ! -d "$GIT_BASE" ] ; then
	echo "Git repos base directory $GIT_BASE does not exist" 1>&2
	exit 1
    fi
fi

if [ -z "$HG_REPO" ] ; then
    echo "Missing mandatory --hg option" 1>&2
    exit 1
else
    if [ ! -d "$HG_REPO" ] ; then
	echo "Hg repos $HG_REPO does not exist" 1>&2
	exit 1
    fi
fi

if [ -z "$LAYOUT" ] ; then
    echo "Missing mandatory --layout option" 1>&2
    exit 1
else
    if [ ! -f "$LAYOUT" ] ; then
	echo "Layout file $LAYOUT does not exist" 1>&2
	exit 1
    fi
fi

WD=`pwd`

for I in `sed -e 's/^[#:].*//' -e 's/^ignore-.*//' -e 's/=.*//' "$LAYOUT" | grep -v '^$'` ; do
    echo "$I" | sed 's/:/ /' | (
        read NAME COMMIT
	rm $NAME.dump
        mkfifo $NAME.dump
        (
	    cd "$GIT_BASE/$NAME" ;
	    found=$(git branch | grep " $BRANCH")
	    if [ -z "$found" ] ; then
		git checkout -b "${BRANCH}" "$COMMIT"
	    else
		git checkout ${BRANCH}
	    fi
	    git reset -q --hard "$COMMIT";
	    git fast-import < "$WD"/$NAME.dump
	    git checkout -f
	) &
    )
done

# execute hg-fast-export, or svn-fast-export
${BIN_DIR}/hg-fast-export "$HG_REPO" "$COMMITTERS" "$LAYOUT"
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
    ( cd "$GIT_BASE/$I" ; echo `pwd` ; git branch | sed 's/^\*/ /' | grep 'tag-branches/' | xargs git branch -D )
    rm $I.dump
done

exit $RETURN_VALUE

