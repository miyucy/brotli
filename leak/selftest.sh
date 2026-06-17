#!/usr/bin/env bash
# Positive-control self-test: certify that each detector actually catches a
# known planted leak (the Fiddle probe). A detector that cannot see this leak
# cannot be trusted when it later reports "clean". Run this BEFORE trusting any
# real result.
#
#   leak/selftest.sh [image]      (default image: brotli-leak)
set -uo pipefail
IMAGE="${1:-brotli-leak}"
PASS=0
FAIL=0
ok()  { echo "PASS: $1 — $2"; PASS=$((PASS + 1)); }
bad() { echo "FAIL: $1 — $2"; FAIL=$((FAIL + 1)); }
hr()  { echo "------------------------------------------------------------"; }

# Probe only (skip the slow brotli loop): 2000 * 4096 = ~8 MB known leak.
PROBE="-e LEAK_PROBE=1 -e LEAK_WORK=0 -e LEAK_ITER=2000"

hr; echo "[1/6] valgrind memcheck — must report a definite leak"
out=$(docker run --rm $PROBE "$IMAGE" bash -lc \
  "valgrind --leak-check=full --show-leak-kinds=definite ruby -Ilib leak/workload.rb 2>&1" 2>/dev/null || true)
line=$(echo "$out" | grep -E "definitely lost:" | tail -1)
if echo "$line" | grep -qE "definitely lost: [1-9][0-9,]* bytes"; then
  ok "valgrind" "$line"
else
  bad "valgrind" "${line:-no 'definitely lost' line found}"
fi

hr; echo "[2/6] valgrind massif — heap peak must rise by the probe size"
peak() {
  docker run --rm $1 "$IMAGE" bash -lc \
    "valgrind --tool=massif --massif-out-file=/tmp/m.out ruby -Ilib leak/workload.rb >/dev/null 2>&1; \
     grep -oE 'mem_heap_B=[0-9]+' /tmp/m.out | cut -d= -f2 | sort -n | tail -1" 2>/dev/null
}
p1=$(peak "$PROBE")
p0=$(peak "-e LEAK_WORK=0 -e LEAK_ITER=2000")
diff=$(( ${p1:-0} - ${p0:-0} ))
if [ "$diff" -ge 6000000 ]; then
  ok "massif" "peak rose by ${diff}B with probe (≈8MB expected)"
else
  bad "massif" "peak delta ${diff}B (probe=${p1:-?} noprobe=${p0:-?})"
fi

hr; echo "[3/6] AddressSanitizer corruption — must catch a planted heap overflow"
# ASan is a corruption detector here (LSan leak-scan deadlocks on stock Ruby).
# Validate the toolchain deterministically with a fully-instrumented standalone
# C probe (no Ruby/Fiddle interception nuances). The `rake leak:asan` method
# then applies the same ASan build to scan the live extension.
out=$(docker run --rm "$IMAGE" bash -c \
  'gcc -fsanitize=address -g -O0 -o /tmp/ap leak/asan_probe.c && ASAN_OPTIONS=halt_on_error=1 /tmp/ap 2>&1' 2>/dev/null || true)
if echo "$out" | grep -qE "heap-buffer-overflow"; then
  ok "asan" "$(echo "$out" | grep -m1 'heap-buffer-overflow' | sed 's/^=*//;s/ *$//')"
else
  bad "asan" "ASan did not flag the planted overflow"
fi

hr; echo "[4/6] memory-limit canary — probe must OOM-kill the container (exit 137)"
docker run --rm --memory=64m --memory-swap=64m \
  -e LEAK_PROBE=1 -e LEAK_WORK=0 -e LEAK_ITER=50000 \
  "$IMAGE" ruby -Ilib leak/workload.rb >/dev/null 2>&1
code=$?
if [ "$code" -eq 137 ]; then
  ok "memlimit" "OOM-killed (exit 137) on a ~200MB probe under 64m"
else
  bad "memlimit" "exit=$code (expected 137)"
fi

# The injectors are leak TRIGGERS, not detectors. We validate that each can
# actually force an allocation to fail (exit 42 from alloc_probe). Driving the
# failure into one specific brotli allocation in a live, retrying Ruby VM is
# probabilistic and is exercised by the `rake leak:fiu` / `leak:faults` real
# runs, not asserted here.
hr; echo "[5/6] libfiu injector — must force an allocation to fail"
code=$(docker run --rm "$IMAGE" bash -c \
  'gcc -O0 -o /tmp/ap leak/alloc_probe.c && \
   fiu-run -x -c "enable_random name=libc/mm/malloc,probability=0.05" /tmp/ap; echo "RC=$?"' 2>/dev/null \
   | grep -oE "RC=[0-9]+" | cut -d= -f2)
if [ "${code:-0}" -eq 42 ]; then
  ok "fiu" "forced malloc failure (alloc_probe exit 42)"
else
  bad "fiu" "no failure injected (alloc_probe exit ${code:-?})"
fi

hr; echo "[6/6] custom shim injector — must force an allocation to fail"
code=$(docker run --rm "$IMAGE" bash -c \
  'gcc -shared -fPIC -o /tmp/fm.so leak/failmalloc.c -ldl && \
   gcc -O0 -o /tmp/ap leak/alloc_probe.c && \
   FAIL_SIZE=17185 LD_PRELOAD=/tmp/fm.so /tmp/ap; echo "RC=$?"' 2>/dev/null \
   | grep -oE "RC=[0-9]+" | cut -d= -f2)
if [ "${code:-0}" -eq 42 ]; then
  ok "faults" "forced malloc(17185) failure (alloc_probe exit 42)"
else
  bad "faults" "no failure injected (alloc_probe exit ${code:-?})"
fi

hr
echo "RESULT: ${PASS} passed, ${FAIL} failed"
[ "$FAIL" -eq 0 ]
