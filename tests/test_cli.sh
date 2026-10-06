#!/usr/bin/env bash
# ABOUTME: CLI tests: argument and permission checks for fanset (usage errors, refusal without root).
# ABOUTME: Need no hardware and no root; the real-SMC checks live in test_e2e.sh and test_hw.sh.
set -uo pipefail
BIN=${BIN:-bin}
fails=0
pass() { echo "ok   $1"; }
fail() { echo "FAIL $1" >&2; fails=$((fails + 1)); }

# expect <name> <expected rc> <expected stderr substring> -- <command...>
expect() {
  local name=$1 want_rc=$2 want_err=$3 err rc
  shift 4
  err=$("$@" 2>&1 >/dev/null)
  rc=$?
  if [[ $rc -eq $want_rc && $err == *"$want_err"* ]]; then pass "$name"; else fail "$name (rc=$rc, stderr=$err)"; fi
}

expect "fanset: no args" 2 "usage:" -- "$BIN/fanset"
expect "fanset: two args" 2 "usage:" -- "$BIN/fanset" 80 90
expect "fanset: zero" 2 "usage:" -- "$BIN/fanset" 0
expect "fanset: above 100" 2 "usage:" -- "$BIN/fanset" 150
expect "fanset: not a number" 2 "usage:" -- "$BIN/fanset" abc
expect "fanset: trailing junk" 2 "usage:" -- "$BIN/fanset" 80abc
if [[ $(id -u) -ne 0 ]]; then
  expect "fanset: refuses without root" 1 "needs root" -- "$BIN/fanset" 80
  expect "fanset auto: refuses without root" 1 "needs root" -- "$BIN/fanset" auto
else
  echo "skip root checks (running as root)"
fi

if [[ $fails -eq 0 ]]; then echo "cli tests: ok"; else echo "$fails cli test(s) failed" >&2; exit 1; fi
