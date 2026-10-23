#!/bin/sh
# Block 3 crash test: kill -9 baobab in the middle of a stream of puts, restart
# it, and check that every put it acknowledged ("Executed.") survived.
#
# Runs in a temporary directory, so your own data-000001.log is untouched.
set -eu

ROOT=$(cd "$(dirname "$0")" && pwd)
N=${N:-50000}
DELAY=${DELAY:-0.5}

make -s -C "$ROOT" baobab
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cd "$WORK"

seq 0 $((N - 1)) | awk '{ print "put key" $1 " val" $1 }' > puts.txt
echo ".exit" >> puts.txt

"$ROOT/baobab" < puts.txt > out.txt &
pid=$!
sleep "$DELAY"
kill -9 "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true

# Puts run in order, so the first $acked keys are the acknowledged ones.
acked=$(grep -c 'Executed\.' out.txt || true)
if [ "$acked" -ge "$N" ]; then
    echo "baobab finished all $N puts before the kill; rerun with a larger N or smaller DELAY"
    exit 1
fi

seq 0 $((acked - 1)) | awk '{ print "get key" $1 }' > gets.txt
echo ".exit" >> gets.txt
"$ROOT/baobab" < gets.txt > recovered.txt

loaded=$(sed -n 's/^Loaded \([0-9]*\) keys.*/\1/p' recovered.txt)
missing=$(grep -c 'Key not found' recovered.txt || true)
torn=$(grep -c 'Discarding torn tail' recovered.txt || true)

echo "killed after $acked acknowledged puts (of $N)"
echo "restart loaded $loaded keys; torn tail discarded: $([ "$torn" -gt 0 ] && echo yes || echo no)"
if [ "$missing" -ne 0 ] || [ "$loaded" -lt "$acked" ]; then
    echo "FAIL: $missing acknowledged keys missing after restart"
    exit 1
fi
echo "PASS: all $acked acknowledged puts survived kill -9"
