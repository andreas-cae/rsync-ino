#!/bin/bash

# Parse arguments using getopts
# Arguments:
#   -r : remote source
#   -f : use filter
#   -o <option> : rsync option to test
REMOTE=N
FILTER=""
OPT=""
while getopts "ro:f:F" opt; do
    case ${opt} in
        r )
            REMOTE=Y
            ;;
        o )
            OPT="${OPTARG}"
            ;;
        f )
            FILTER=${OPTARG}
            ;;
        F )
            FILTER="--filter '- **/subdir2'"
            ;;
        \? )
            echo "Invalid option: -$OPTARG" 1>&2
            ;;
        : )
            echo "Invalid option: -$OPTARG requires an argument" 1>&2
            ;;
    esac
done
shift $((OPTIND -1))


# set up source
SRC=./_TEST/src
TGT=./_TEST/tgt
rm -rf "$SRC"/*
rm -rf "$TGT"/*
mkdir -p "$SRC"/dir{1,2}/subdir{1,2}

while read -r dir; do
    fname="$(basename "$dir" | sed 's/dir/file/')"'.txt'
    # echo "Creating file: $dir/$fname"
    touch "$dir/$fname"
done < <(find $SRC -type d)

# Set up links
ln "$SRC/src.txt" "$SRC/src_hlink.lnk"
ln -s "src.txt" "$SRC/src_slink.snk"

make

METAOUT="$(pwd)/_METAout.bin"
rm "$METAOUT" 2>/dev/null

if [ "$REMOTE" = "Y" ]; then
    SRC="cs3-0092:$(pwd)/_TEST/src"
    echo "using remote source: $SRC"
fi

if [[ -n "$FILTER" ]]; then
    echo "using filter: $FILTER"
fi

./rsync -a $OPT $FILTER --meta-log="$METAOUT" --meta-str="MTOP" --meta-fmt='iTN' "$SRC"/ "$TGT"

hexdump -C "$METAOUT"

python3 metareader.py "$METAOUT"

find "$TGT" -exec ls -dli {} +
