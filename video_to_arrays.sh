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
  -vf "fps=${FPS},scale=${WIDTH}:${HEIGHT},format=monow" \
  "${OUTDIR}/%06d.pbm"

echo "Frames written to $OUTDIR"

#!/usr/bin/env bash

OUTFILE="badapple_frames.h"

echo "#include <stdint.h>" > "$OUTFILE"
echo "" >> "$OUTFILE"

for f in "${OUTDIR}"; do
    name=$(basename "$f" .pbm)

    # remove header (first 2 lines)
    data=$(tail -n +3 "$f" | xxd -i)

    echo "const uint8_t ${name}[] = {" >> "$OUTFILE"
    echo "$data" | sed '1d;$d' >> "$OUTFILE"
    echo "};" >> "$OUTFILE"
    echo "" >> "$OUTFILE"
done
