# Local memory-leak detection harness

Local-only (no CI) leak detection for the `brotli` native extension, using
several independent methods so a leak can be cross-confirmed. macOS arm64 can't
run Valgrind natively, so most methods run in a `linux/arm64` Docker container
(native under Docker Desktop on Apple Silicon — no qemu); macOS `leaks` runs on
the host.

## Quick start

```sh
bundle exec rake leak:image       # build the toolchain image (once)
bundle exec rake leak:selftest    # PROVE every detector catches a planted leak
bundle exec rake leak:valgrind    # then trust the real runs
```

**Always run `leak:selftest` first.** A detector reporting "0 leaks" means
nothing until you've shown it can detect a real one. `selftest` plants a known
leak (`LEAK_PROBE`) and asserts each detector reports it.

## Methods

| Task | Tool | Detects |
|------|------|---------|
| `rake leak:valgrind` | Valgrind memcheck via `ruby_memcheck` | definite leaks on success paths (source of truth) |
| `rake leak:massif`   | Valgrind massif + `ms_print` | heap growth across iterations (plateau vs staircase) |
| `rake leak:asan`     | AddressSanitizer (`detect_leaks=0`) | heap overflow / use-after-free in the C code |
| `rake leak:fiu`      | libfiu `fiu-run` | forces malloc/realloc failure → exercises OOM cleanup |
| `rake leak:faults`   | custom `LD_PRELOAD` shim | deterministically fails inflate's output-buffer growth (default `FAIL_MIN_SIZE=8388608`); RSS growth = leak |
| `rake leak:memlimit` | `docker run --memory=64m` | unbounded *success-path* growth → exit 137 |
| `rake leak:macos`    | macOS `leaks` (native) | quick host smoke test (filter to brotli frames) |
| `rake leak:selftest` | all of the above | certifies each detector via a planted leak |
| `rake leak:all`      | — | runs the Docker methods in sequence |

## How each detector is validated

`leak:selftest` plants a known defect for each method and asserts it is caught,
so a later "clean" verdict on real code *means* something. Positive-control
fixtures:

- **Leak detectors** (valgrind, massif, memlimit, macOS `leaks`): `LEAK_PROBE=1`
  makes `workload.rb` leak raw C memory via Fiddle (`malloc`, never freed) —
  invisible to Ruby's GC, so every malloc-level tool must report it.
- **ASan** (corruption): `leak/asan_probe.c` — a fully-instrumented standalone
  heap-buffer-overflow ASan must catch.
- **Injectors** (libfiu, shim): `leak/alloc_probe.c` — a tiny target that exits
  42 when an allocation is forced to fail, proving the injector fires. (Driving
  the failure into one specific brotli allocation in the live, retrying Ruby VM
  is probabilistic — that is the `leak:fiu` / `leak:faults` real run, not the
  self-test.)

Latest self-test: **6/6 detector checks pass**. On real code, `leak:faults`
caught the output-buffer OOM leak below (`RSS_DELTA` +215 MB over 50
iterations) before it was fixed.

## Targets

- **Fixed** (should be clean): `rb_str_new` OOM leak in `inflate`/`deflate`.
- **Fixed** (should be clean): output-buffer OOM leak. `inflate` used to grow
  its output buffer with `ruby_xrealloc` inside the no-GVL worker. When that
  failed, Ruby re-acquired the GVL and raised `NoMemoryError` from inside the
  worker, and the longjmp skipped the caller's cleanup, leaking the decoder
  state and the buffer. The buffer now uses `malloc`/`realloc`/`free`, and the
  GVL-holding caller raises `NoMemoryError` after cleanup. Check it:

  ```sh
  bundle exec rake leak:faults LEAK_ITER=50   # fails every allocation >= 8 MB,
                                              # i.e. the BOMB payload's buffer
  ```

  `RSS_DELTA` should match a run without `FAIL_MIN_SIZE` (about +20 MB at 50
  iterations, the workload's own heap growth). With the leak it grew by about
  4 MB per iteration (+215 MB at 50 iterations).

## Build flags

`extconf.rb` gains two env-gated, otherwise-inert hooks (one append reaches
brotli.c, buffer.c and the vendored sources):

- `BROTLI_DEBUG=1` → `-O0 -g3 -fno-omit-frame-pointer` (readable frames).
- `BROTLI_ASAN=1`  → adds `-fsanitize=address` (compile + link).

A plain `rake compile` / `rake test` is unaffected.

## Notes / caveats

- **Injectors and leak detectors can't share a run** — libfiu/the shim and
  Valgrind/ASan all intercept `malloc`. Injection and leak-detection are
  separate runs; the injection signal is a crash or RSS growth, not a Valgrind
  report.
- **ASan runs in corruption mode (`detect_leaks=0`)**, not leak mode:
  LeakSanitizer's stop-the-world scan deadlocks against Ruby's threads on a
  stock (non-ASan) Ruby. So ASan here catches overflow/UAF; leaks are covered by
  `leak:valgrind` (the source of truth), `massif`, `macos`, and `memlimit`. Its
  self-test plants a heap overflow (`LEAK_PROBE=corrupt`), not a leak.
- **macOS `leaks`** marks Ruby GC-managed memory as `ROOT LEAK … rb_gc_impl_malloc`
  — conservative-GC false positives. Only brotli-frame stacks are actionable;
  cross-confirm in Valgrind.
- **`mallocfail` is intentionally not used** — its per-allocation `backtrace()`
  is documented to fail on threaded programs, and Ruby is multi-threaded.
