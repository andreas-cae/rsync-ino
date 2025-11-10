#!/bin/bash

OPT="$1"

# set up source
SRC=TEST/src
TGT=TEST/tgt
rm -rf "$SRC"/*
mkdir -p "$SRC"/dir{1,2}/subdir{1,2}


while read -r dir; do
    fname="$(basename "$dir" | sed 's/dir/file/')"'.txt'
    echo "Creating file: $dir/$fname"
    touch "$dir/$fname"
done < <(find $SRC -type d)


METAOUT="$(pwd)/METAout.bin"
rm  $METAOUT

echo ./rsync -a $OPT --meta-log="$METAOUT" --meta-top="MTOP" --meta-fmt='iTN' "$SRC"/ "$TGT"
