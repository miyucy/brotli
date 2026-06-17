#!/usr/bin/env bash
# macOS native leak check (no Docker). Uses the system `leaks` tool.
# Env LEAK_PROBE=1 plants a known leak to validate detection.
set -uo pipefail
cd "$(dirname "$0")/.."

echo "Building extension with debug symbols (BROTLI_DEBUG=1)..."
BROTLI_DEBUG=1 bundle exec rake compile >/dev/null

RUBY="$(rbenv which ruby 2>/dev/null || command -v ruby)"
echo "Ruby: $RUBY"
echo "Running leaks --atExit ..."

# MallocScribble overwrites freed blocks so stale stack pointers stop creating
# conservative false roots; MallocStackLogging gives symbolicated stacks.
MallocStackLogging=1 MallocScribble=1 leaks --atExit -- \
  "$RUBY" -Ilib leak/workload.rb 2>/dev/null > /tmp/brotli_leaks.txt || true

echo "--- summary ---"
grep -E "leaks for [0-9]" /tmp/brotli_leaks.txt | tail -1 || echo "(no summary line)"

echo "--- actionable (runtime brotli-frame) leak stacks ---"
# Match runtime allocation/use frames only. Exclude Init_brotli: its rb_intern /
# rb_define_* allocations live for the whole process and `leaks` always reports
# them as reachable-but-not-freed (a one-time false positive, not a leak).
if grep -inE "create_buffer|expand_buffer|append_buffer|brotli_alloc|brotli_deflate|brotli_inflate|BrotliEncoder|BrotliDecoder" /tmp/brotli_leaks.txt | grep -v "Init_brotli"; then
  echo ">> runtime brotli-frame leak detected — investigate (full report: /tmp/brotli_leaks.txt)"
  exit 1
else
  echo ">> no runtime brotli-frame leaks."
  echo "   (Init_brotli one-time setup and 'ROOT LEAK: ... rb_gc_impl_malloc'"
  echo "    entries are false positives; cross-confirm real hits with 'rake leak:valgrind'.)"
fi

# When validating the detector itself, the Fiddle probe leaks via malloc/ffi:
if [ -n "${LEAK_PROBE:-}" ]; then
  if grep -inE "fiddle|libffi|ffi_call| malloc " /tmp/brotli_leaks.txt >/dev/null; then
    echo ">> positive control: probe leak detected (detector works)."
  else
    echo ">> WARNING: LEAK_PROBE set but probe leak not found — detector may be misconfigured."
  fi
fi
