#!/usr/bin/env bash
set -euo pipefail

BIN="./shellforge"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

out=$(printf 'print hello\nquit 0\n' | "$BIN")
[[ "$out" == *"hello"* ]]

out=$(printf 'list / | first -n 1\nquit 0\n' | "$BIN")
[[ -n "$out" ]]

printf 'print ShellForge > %s/out.txt\nshow %s/out.txt\nquit 0\n' "$TMP" "$TMP" | "$BIN" > "$TMP/result.txt"
grep -q 'ShellForge' "$TMP/result.txt"

out=$(printf 'export DEMO=works\nprint $DEMO\nquit 0\n' | "$BIN")
[[ "$out" == *"works"* ]]

out=$(printf 'false\nprint $?\nquit 0\n' | "$BIN")
[[ "$out" == *"1"* ]]

echo "All ShellForge tests passed."
