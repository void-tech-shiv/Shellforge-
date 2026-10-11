#!/usr/bin/env bash
set -euo pipefail

BIN="./shellforge"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

out="$($BIN <<'CMDS'
print hello
path
print $?
quit
CMDS
)"

grep -q '^hello$' <<<"$out"
grep -q '^0$' <<<"$out"

(
  cd "$TMP"
  "$OLDPWD/$BIN" <<'CMDS'
make demo
new note.txt
print shellforge > note.txt
show note.txt
list
quit
CMDS
) > "$TMP/out.txt"

test -d "$TMP/demo"
test -f "$TMP/note.txt"
grep -q 'shellforge' "$TMP/out.txt"
grep -q 'note.txt' "$TMP/out.txt"

pipe_out="$($BIN <<'CMDS'
print alpha | tr a-z A-Z
quit
CMDS
)"
grep -q '^ALPHA$' <<<"$pipe_out"

$BIN <<CMDS >/dev/null
print error-test > "$TMP/combined.txt" 2>&1
quit
CMDS
grep -q 'error-test' "$TMP/combined.txt"

job_out="$($BIN <<'CMDS'
sleep 0.2 &
tasks
sleep 0.3
tasks
quit
CMDS
)"
grep -q 'Running' <<<"$job_out"

echo "All ShellForge smoke tests passed."
