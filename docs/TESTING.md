# Tests and verification

The implementation at `956762735cae496c9698d112b581e21a79f33886` passed all seven
GitHub build/test/package jobs. See [../IMPLEMENTATION_STATUS.md](../IMPLEMENTATION_STATUS.md)
for exact counts, run links, artifacts and scope limitations.

## Commands

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

To include desktop checks, provide Qt 6.5+ (CI uses 6.8.3) and configure with
`-DDISKPILOT_BUILD_GUI=ON`. Tests additionally require Qt Test. GUI tests use an
offscreen software backend; `gui_load` checks application construction and
`gui_controller` exercises real controller operations on synthetic files.

## Coverage exercised

- Empty/nested/Unicode/deep trees, invalid roots, progress/cancellation, entry
  limits, hard links, symlink loops/ancestors, disappearing directories and POSIX
  permission denial. Privilege-dependent fixtures explicitly skip if unavailable.
- Analysis sorting/filtering, extension totals, JSON round trips, CSV escaping and
  spreadsheet formula neutralization, export stream failures and treemap geometry.
- Published SHA-256 vectors, multi-block/million-byte input, streamed duplicates,
  same partial content with different middles, empty files, hard-link exclusion,
  changed/removed/link-replaced files and cancellation.
- Cleanup false positives/context, exact approved metadata plans, source/Git
  protection, repeated selections, incomplete scans, changed descendants,
  symlink replacement, adapter failure and native adapter refusal.
- Snapshot round trips/growth/addition, incompatible roots/incomplete scans,
  traversal/negative sizes/duplicate paths, absence of contents and HTML escaping.
- CLI help/version/usage, scan/filter exports, duplicates, dry-run cleanup,
  exclusive snapshot creation, comparison and HTML output.
- GUI controller scans, folder navigation, filtering, treemap queries, duplicate
  selection constraints, cleanup preview, snapshots/reports, invalid roots and
  cancellation settling without a stuck worker.

Linux CI runs AddressSanitizer and UndefinedBehaviorSanitizer in a separate Debug
build. Desktop jobs compile and test both Windows and Linux. Package jobs repeat
tests, install using an absolute prefix, create CPack ZIPs and checksums, extract
them, then launch the contained CLI and GUI. Windows removes SDK search paths for
the extracted GUI check. This is not a full clean-machine or visual-interaction test.

## Performance and remaining checks

`tools/benchmark.py` creates controlled 10k/100k/500k/1M-file fixtures and verifies
scan totals before reporting timing. The 10k workflow was run on both platforms;
actual measurements are in [BENCHMARKS.md](BENCHMARKS.md). Peak memory is explicitly
unmeasured. Larger benchmarks, native edge-case fixtures, static analysis, visual
QA and clean-machine deployment remain distinct release gates.
