#!/bin/sh

AINAME="Baczek's KP AI"
AISHORTNAME=BaczekKPAI
AIDLL=output/default/SkirmishAI.dll
TEMPDIR=output/temp
SKIRMISHAIDIR=AI/Skirmish
DEPSDIR=dependencies
ZIP="7za a -tzip"

if [ ! -f "$AIDLL" ]; then
	echo "$AIDLL doesn't exist, aborting"
	exit 1
fi

if [ -d "$TEMPDIR" ]; then
	echo "$TEMPDIR exists, aborting"
	exit 1
fi

mkdir -p "$TEMPDIR"

# get version
AIVERSION=$(cat data/AIInfo.lua | grep -- "-- AI version"  \
		| awk '{ print $3 }' | sed -e "s/[\\\"',]//g" )

echo $AINAME $AIVERSION

# create directory structure

DISTDIR="$TEMPDIR/$SKIRMISHAIDIR/$AISHORTNAME/$AIVERSION"

mkdir -p "$DISTDIR/py"


# package files
cp -p "$AIDLL" "$DISTDIR"
cp -p data/*.lua "$DISTDIR"
cp -p data/py/*.py "$DISTDIR/py"

# docs
cp -p README_BaczekKPAI.txt "$TEMPDIR"

# dll dependencies
cp -p "$DEPSDIR"/* "$TEMPDIR"

DIR=`pwd`
ARCHIVENAME="$DIR/$AISHORTNAME-$AIVERSION.zip"

(cd "$TEMPDIR" && $ZIP "$ARCHIVENAME" *)

rm -rf "$TEMPDIR"
