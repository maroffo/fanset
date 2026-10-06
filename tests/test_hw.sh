#!/usr/bin/env bash
# ABOUTME: Hardware end-to-end test of the write path: fans to 60%, check manual mode and spin-up, then restore auto.
# ABOUTME: Needs sudo and a Mac with fans, so it never runs in CI. Automatic mode is restored on any exit.
set -euo pipefail
BIN=${BIN:-bin}
PCT=60

restore() {
  sudo "$BIN/fanset" auto >/dev/null 2>&1 ||
    echo "WARNING: could not restore automatic mode, run: sudo $BIN/fanset auto (or reboot)" >&2
}
trap restore EXIT

sudo -v
echo "== setting fans to $PCT%"
sudo "$BIN/fanset" "$PCT" # exits non-zero if any fan refused the write or went back to auto
echo "== waiting 10s for the fans to spin up"
sleep 10
out=$("$BIN/fanread")
echo "$out"
# Every fan must be manual, target at PCT% of max (or min), actual within 15% of target.
awk -v pct="$PCT" '
  /^fan [0-9]+:/ {
    n++
    for (i = 1; i <= NF; i++) if (split($i, kv, "=") == 2) val[kv[1]] = kv[2]
    want = val["max"] * pct / 100
    if (want < val["min"]) want = val["min"]
    if (val["mode"] != "manual") { print "FAIL " $1 $2 " not manual"; bad++ }
    if (val["target"] < want - 2 || val["target"] > want + 2) { print "FAIL " $1 $2 " target " val["target"] ", expected ~" want; bad++ }
    if (val["actual"] < val["target"] * 0.85) { print "FAIL " $1 $2 " actual " val["actual"] " still far from target " val["target"]; bad++ }
  }
  END { if (n == 0) { print "FAIL no fans found"; exit 1 } exit (bad > 0) }
' <<<"$out"

echo "== restoring automatic mode"
trap - EXIT
sudo "$BIN/fanset" auto
echo "hardware test: ok"
