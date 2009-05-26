#!/bin/sh

AINAME="Baczek's KP AI"
AISHORTNAME=BaczekKPAI
TEMPDIR=output/temp
SKIRMISHAIDIR=AI/Skirmish
DEPSDIRS="deps/common" 
PYLIB="deps/python/library.zip"
ZIP="7za a -tzip"

case $1 in
	gcc)
		AIDLL=output/default/SkirmishAI.dll
		DEPSDIRS="$DEPSDIRS deps/gcc"
		KIND=gcc
		;;
	msvc)
		AIDLL=SkirmishAI.dll
		DEPSDIRS="$DEPSDIRS deps/msvc"
		KIND=msvc
		;;
	*)
		echo "unknown compiler \"$1\" or missing option, aborting"
		exit 1
		;;
esac

if [ ! -f "$AIDLL" ]; then
	echo "$AIDLL doesn't exist, aborting"
	exit 1
fi

if [ -d "$TEMPDIR" ]; then
	echo "$TEMPDIR exists, aborting"
	exit 1
fi

mkdir -p "$TEMPDIR" || exit 1

# get version
AIVERSION=$(cat data/AIInfo.lua | grep -- "-- AI version"  \
		| awk '{ print $3 }' | sed -e "s/[\\\"',]//g" )
GITVERSION=$(git rev-parse HEAD | sed 's/\(.......\).*/\1/')

echo $AINAME $AIVERSION $GITVERSION

# create directory structure

DISTDIR="$TEMPDIR/$SKIRMISHAIDIR/$AISHORTNAME/$AIVERSION"

mkdir -p "$DISTDIR/py" || exit 1


# package files
cp -p "$AIDLL" "$DISTDIR"
cp -p data/*.lua "$DISTDIR"
cp -p data/py/*.py "$DISTDIR/py"
cp -p "$PYLIB" "$DISTDIR/py"

# docs
cp -p README_BaczekKPAI.txt "$TEMPDIR"

# dll dependencies
for dep in $DEPSDIRS; do
	cp -p "$dep"/* "$TEMPDIR"
done

DIR=`pwd`
ARCHIVENAME="$DIR/$AISHORTNAME-$AIVERSION-$KIND-$GITVERSION.zip"

(cd "$TEMPDIR" && $ZIP "$ARCHIVENAME" *)

rm -rf "$TEMPDIR"
