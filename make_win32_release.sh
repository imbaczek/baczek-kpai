#!/bin/sh

AINAME="Baczek's KP AI"
AISHORTNAME=BaczekKPAI
TEMPDIR=output/temp
SKIRMISHAIDIR=AI/Skirmish
DISTDIR="dist"
DEPSDIRS="deps/common" 
PYLIBDIR="deps/python"
ZIP="7za a -tzip"
ZIP7="7za a"

case $1 in
	gcc)
		AIDLL=output/default/SkirmishAI.dll
		AIDBG=output/default/SkirmishAI.dbg
		DEPSDIRS="$DEPSDIRS deps/gcc"
		KIND=gcc
		;;
	msvc)
		AIDLL=SkirmishAI.dll
		AIDBG=SkirmishAI.pdb
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


# zip debug info
if [ -f "$AIDBG" ]; then
	DEBUGARCHIVE="debug/$KIND/$AIVERSION-$GITVERSION.7z"
	mkdir -p "debug/$KIND" || exit 1
	echo "zipping debug info: $DEBUGARCHIVE"
	$ZIP7 "$DEBUGARCHIVE" "$AIDBG" >/dev/null
fi

# create directory structure
ZIPDIR="$TEMPDIR/$SKIRMISHAIDIR/$AISHORTNAME/$AIVERSION"
ARCHIVENAME="$AISHORTNAME-$AIVERSION-$KIND-$GITVERSION.zip"

echo "creating release zip: $ARCHIVENAME"

mkdir -p "$ZIPDIR/py" || exit 1


# package files
cp -p "$AIDLL" "$ZIPDIR"
cp -p data/*.lua "$ZIPDIR"
cp -p data/py/*.py "$ZIPDIR/py"

# package python stdlib
cp -p "$PYLIBDIR"/* "$ZIPDIR/py"

# docs
cp -p README_BaczekKPAI.txt "$TEMPDIR"

# dll dependencies
for dep in $DEPSDIRS; do
	cp -p "$dep"/* "$TEMPDIR"
done

DIR=`pwd`
ABSARCHIVENAME="$DIR/$ARCHIVENAME"

(cd "$TEMPDIR" && $ZIP "$ABSARCHIVENAME" *) >/dev/null

rm -rf "$TEMPDIR"

mkdir -p "$DISTDIR" || exit 1
mv "$ABSARCHIVENAME" "$DISTDIR"

echo "archive moved to $DISTDIR"
