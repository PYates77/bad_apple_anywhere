#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 5 ]; then
    echo "Usage:"
    echo "  $0 <width> <height> <fps> <symbol_name> <output.h>"
    exit 1
fi

VIDEO="badapple.mp4"
WIDTH="$1"
HEIGHT="$2"
FPS="$3"
SYMBOL="$4"
OUTFILE="badapple.h"

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

echo "Generating monochrome frames..."

ffmpeg -loglevel error \
    -i "$VIDEO" \
    -vf "fps=${FPS},scale=${WIDTH}:${HEIGHT}:flags=neighbor,format=monow" \
    "$TMPDIR/frame_%06d.pbm"

files=($(ls "$TMPDIR"/*.pbm | sort))

if [ ${#files[@]} -eq 0 ]; then
    echo "Error: no frames generated"
    exit 1
fi

ROW_BYTES=$(( (WIDTH + 7) / 8 ))
FRAME_SIZE=$(( ROW_BYTES * HEIGHT ))
FRAME_COUNT=${#files[@]}

echo "Frames: $FRAME_COUNT"
echo "Frame size: $FRAME_SIZE bytes"

{
echo "#pragma once"
echo "#include <stdint.h>"
echo ""
echo "#define BADAPPLE_WIDTH  $WIDTH"
echo "#define BADAPPLE_HEIGHT $HEIGHT"
echo "#define BADAPPLE_ROW_BYTES $ROW_BYTES"
echo "#define BADAPPLE_FRAME_SIZE $FRAME_SIZE"
echo "#define BADAPPLE_FRAME_COUNT $FRAME_COUNT"
echo ""
echo "static const uint8_t badapple_data[] = {"

for f in "${files[@]}"; do
    echo "  /* $(basename "$f") */"
    tail -n +3 "$f" | xxd -i | sed '1d;$d;s/^/  /'
done

echo "};"
} > "$OUTFILE"

echo "Generated $OUTFILE"
