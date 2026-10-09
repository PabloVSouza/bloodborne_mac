#!/usr/bin/env bash
# tools/grab.sh [--screen] [NAME]: dumps the game's next presented frame (the game must run with
# BB_PRESENT_DUMP_TRIGGER=$PWD/out/grab.trigger BB_DUMP_DIR=$PWD/out/grab, as mac_bench.sh sets)
# and converts it to out/grab/NAME.png. --screen: the window's image instead, with the settings
# menu and FPS counter (BB_SCREEN_DUMP_TRIGGER=$PWD/out/screen.trigger). Only the game's own
# frame: never a screen capture.
cd -- "$(dirname -- "$0")/.."
kind=present trigger=out/grab.trigger
[[ ${1:-} == --screen ]] && { kind=screen trigger=out/screen.trigger; shift; }
mkdir -p out/grab
before=$(ls out/grab/*_${kind}_*.raw 2>/dev/null | wc -l)
touch "$trigger"
for i in $(seq 1 100); do
    sleep 0.2
    [[ $(ls out/grab/*_${kind}_*.raw 2>/dev/null | wc -l) -gt $before ]] && break
done
latest=$(ls -t out/grab/*_${kind}_*.raw 2>/dev/null | head -1)
[[ -n $latest ]] || { echo "no frame dumped"; exit 1; }
sleep 0.5
png=$(python3 tools/raw2png.py "$latest" | tail -1)
[[ -n ${1:-} ]] && mv "$png" "out/grab/$1.png" && png=out/grab/$1.png
rm -f "$latest"
echo "$png"
