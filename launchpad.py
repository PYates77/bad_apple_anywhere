#!/usr/bin/env python

## Display Bad Apple On Launchpad Mini (which is a midi controller, but it has LEDs)
#  generate the bitmaps:
#   ./video_to_bmp.sh 8 8 60 launchpad

from lpminimk3 import Mode, find_launchpads
from lpminimk3.graphics import Text
import random
from pathlib import Path
from PIL import Image
import time

lp = find_launchpads()[0]  # Get the first available launchpad
lp.open()  # Open device for reading and writing on MIDI interface (by default)
lp.mode = Mode.PROG  # Switch to the programmer mode

directory_path = Path('./launchpad')

TARGET_FPS = 60
INTERVAL = 1.0 / TARGET_FPS
next_time = time.perf_counter()

for file in sorted(directory_path.iterdir()):
    img = Image.open(file)
    height, width = img.size
    pixels = img.load()
    for x in range(width):
        for y in range(height):
            # Get the current pixel value (R, G, B)
            r, g, b = pixels[x, y]
            lp.panel.led(x,y+1).color = (r,g,b)

    next_time += INTERVAL
    sleep_time = next_time - time.perf_counter()
    if sleep_time > 0:
        time.sleep(sleep_time)
    else:
        next_time = time.perf_counter()
