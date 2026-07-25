#!/usr/bin/env bash
set -e

if [ "$#" -ne 4 ]; then
    echo "Usage: $0 <width> <height> <fps> <output_dir>"
    exit 1
fi

INPUT="badapple.mp4"
WIDTH="$1"
HEIGHT="$2"
FPS="$3"
OUTDIR="$4"

mkdir -p "$OUTDIR"

ffmpeg -i "$INPUT" \
    -vf "fps=${FPS},scale=${WIDTH}:${HEIGHT}" \
    "$OUTDIR/%06d.bmp"

echo "Frames written to $OUTDIR"
