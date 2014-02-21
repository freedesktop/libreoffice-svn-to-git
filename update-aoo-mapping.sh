#!/bin/bash
#
##
## This script updates the mapping of the AOO svn to git to a newer commit
##
## To use:
##
##  * git log origin/aoo/trunk
##
##    Find the commit there you want to be the new start of the history
##    You need its id (GIT_ID later) and commit message (GIT_MESSAGE)
##
##  * less conversion.AOO.log
##
##    Guess the svn commit number that matches the 1st non-empty (ie. 'done!')
##    commit _after_ the GIT_ID commit mentioned above.
##    Let's call it SVN_NUMBER
##
##    NOTE: Only those that end with "... done!" count.
##
##  * ./update-aoo-mapping.sh GIT_ID "GIT_MESSAGE" SVN_NUMBER
##
#

FILE="aoo-repositories.txt"

GIT_ID=$1
GIT_MESSAGE=`echo -n "$2" | sed 's#/#\\\\/#g'`
SVN_NUMBER=$3

SVN_PREV=$(($SVN_NUMBER-1))

# usage
[ -n "$SVN_NUMBER" -a -n "$SVN_PREV" -a -n "$GIT_ID" -a -n "$GIT_MESSAGE" ] ||{ \
    grep '^##' "$0" | sed 's/^## \?//' ; exit 1 ;
}

sed -i "s/^:revision from:.*/:revision from:$SVN_NUMBER/" "$FILE"

sed -i "s/^core:[^=]*=/core:$GIT_ID=/" "$FILE"

sed -i "s/^:commit map=core,.*/:commit map=core,$SVN_PREV:$GIT_ID/" "$FILE"

sed -i "s/\"\"\".*\"\"\"/\"\"\"$GIT_MESSAGE\"\"\"/" "$FILE"
