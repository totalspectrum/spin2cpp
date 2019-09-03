#!/bin/bash
#
# dummy signing script; does not actually sign anything,
# just copies the input to output
# this is used for default builds; when we actually want
# to sign an executable we need to pass SIGN=script.sh to
# the Makefile to override this script
#
if [ "$#" != "1" ]; then
    echo "Usage: $0 basename"
    echo "   e.g. $0 foo will sign foo.exe to produce foo.signed.exe"
    exit 2
fi

name="$1"
rm -f $name.signed.exe
cp $name.exe $name.signed.exe
