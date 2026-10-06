#!/usr/bin/env bash
# ABOUTME: End-to-end tests of the read-only tools (fanread, fankeys) against the real SMC, no root needed.
# ABOUTME: Skips cleanly where the SMC exposes no fans (CI runners, fanless Macs); the write path is test_hw.sh.
set -uo pipefail
BIN=${BIN:-bin}
fails=0
pass() { echo "ok   $1"; }
fail() { echo "FAIL $1" >&2; fails=$((fails + 1)); }

if ! out=$("$BIN/fanread" 2>/dev/null) || ! [[ $(head -1 <<<"$out") =~ ^fans:\ ([1-9])$ ]]; then
  echo "skip e2e tests (no fans exposed by the SMC here)"
  exit 0
fi

n=${BASH_REMATCH[1]}
lines=$(grep -cE '^fan [0-9]+: actual=[0-9]+ rpm target=[0-9]+ min=[0-9]+ max=[0-9]+ mode=(auto|manual|unknown) ' <<<"$out")
if [[ $lines -eq $n ]]; then pass "fanread: one well-formed line per fan ($n)"; else fail "fanread: expected $n fan lines, got $lines"; fi
keys=$("$BIN/fankeys")
if grep -q '^FNum type=ui8 ' <<<"$keys"; then pass "fankeys: lists FNum"; else fail "fankeys: FNum missing"; fi
if grep -q '^F0Ac ' <<<"$keys"; then pass "fankeys: lists F0Ac"; else fail "fankeys: F0Ac missing"; fi

if [[ $fails -eq 0 ]]; then echo "e2e tests: ok"; else echo "$fails e2e test(s) failed" >&2; exit 1; fi
