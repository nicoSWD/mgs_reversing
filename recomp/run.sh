#!/bin/sh
# usage: ./run.sh SECONDS [args...]  -- build, run against assets/image/disc.cue, print the log tail
cd "$(dirname "$0")"
secs=${1:-20}; [ $# -gt 0 ] && shift
cmake --build build -j8 2>&1 | grep -E 'error|Error' | head -20
rm -f shots/*.png
./psxrecomp-mgs --cue assets/image/disc.cue --turbo --screenshot 600,1000,1500,2000,2500,3000,3500,4000,5000,6000,8000,10000 shots/s "$@" > /tmp/psx_run.log 2>&1 &
pid=$!; sleep "$secs"; kill $pid 2>/dev/null; wait $pid 2>/dev/null
tail -${TAILN:-25} /tmp/psx_run.log; ls shots
