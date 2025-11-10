#!/bin/bash

OPT="$1"

# set up source
SRC=./_TEST/src
TGT=./_TEST/tgt
rm -rf "$SRC"/*
mkdir -p "$SRC"/dir{1,2}/subdir{1,2}


while read -r dir; do
    fname="$(basename "$dir" | sed 's/dir/file/')"'.txt'
    # echo "Creating file: $dir/$fname"
    touch "$dir/$fname"
done < <(find $SRC -type d)

make

METAOUT="$(pwd)/_METAout.bin"
rm  $METAOUT 2>/dev/null


./rsync -a $OPT --meta-log="$METAOUT" --meta-str="MTOP" --meta-fmt='iTN' "$SRC"/ "$TGT"

hexdump -C $METAOUT
