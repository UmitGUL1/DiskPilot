# Verification plan

No tests in this checkpoint have been executed. The local toolchain cannot run.
Portable toolchain attempts failed during temporary/cache operations; they do not
establish successful compilation. Qt remains unavailable locally.
The provided GitHub Actions workflow is a future verification mechanism, not a
successful CI result.

## Existing checks

GoogleTest source covers empty/nested/Unicode/deep trees, invalid roots, cancellation,
entry limits, hard links, symlink loops, link ancestors, disappearing directories,
and Linux permission denial. Symlink fixtures explicitly skip when privileges are
unavailable; mode-000 permission fixtures skip when a privileged runner can bypass them.

Analysis/report tests cover deterministic sorting, filters, category totals, JSON
round trips, CSV quote/newline handling, spreadsheet formula neutralization and
export stream failures. Treemap tests cover area ratios, non-overlap, bounds,
deterministic ordering, zero values, invalid geometry and capped directory views.

CTest also defines help/version/invalid-command checks and CLI fixture scans with
JSON parsing, filtered results, CSV output, missing roots and invalid numeric options.

## Commands

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

Run both Windows and Linux. Inspect skipped tests as well as failures. Add native
Windows junction, long-path, sparse/compressed allocation fixtures and Linux mount
fixtures before release acceptance. Add sanitizers on a supported Linux compiler,
actual stress fixtures and cancellation latency measurements in Phase 9.

Archive/source inventory checks are packaging checks only; they do not establish
C++ syntax, linkability, runtime behavior or passing tests.

## Added coverage

- Published SHA-256 vectors (including multi-block and million-byte input),
  streamed duplicates, same partial content/different middle, empty files,
  hard-link exclusion, changed/removed/link-replaced files and cancellation.
- Cleanup false positives, ecosystem evidence, exact approved plans, source and
  Git protection, repeat selection, incomplete scans, changed descendants,
  symlink replacement, adapter failure and native fail-closed behavior.
- Snapshot round trips, growth/addition, incomplete/different roots, traversal,
  negative sizes, duplicate paths, absence of contents and HTML escaping.
- CLI staged duplicates, read-only cleanup, snapshot overwrite refusal,
  comparison and HTML reports.
- Optional GUI load test using `DiskPilot --smoke-test` with an offscreen software
  backend; this does not replace user-interaction or visual acceptance testing.

The CI workflow defines Windows/Linux core and Qt desktop jobs plus Linux
ASan/UBSan. The manual package workflow adds install and GUI-load checks but does
not prove clean-machine portability. Nothing has been pushed or run in GitHub yet.

## Benchmarks

`tools/benchmark.py` creates controlled 10k/100k/500k/1M-file fixtures and verifies
scan totals before reporting actual time. Peak memory is explicitly unmeasured.
No benchmark result is present yet. Do not publish fixture generation time as
scanner throughput, or infer measurements from this script's existence.
