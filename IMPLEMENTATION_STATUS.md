# DiskPilot verification status

Updated: 2026-10-18, Europe/Istanbul.

Verified implementation commit: `956762735cae496c9698d112b581e21a79f33886`.
Later documentation-only commits do not alter these tested binaries or sources.

## Completed GitHub verification

Build/test run: https://github.com/UmitGUL1/DiskPilot/actions/runs/39074229540
Package run: https://github.com/UmitGUL1/DiskPilot/actions/runs/39074229536

| Check | Result | Executed CTest cases |
| --- | --- | ---: |
| Linux core and CLI | PASS | 58 / 58 |
| Windows core and CLI | PASS | 57 / 57 |
| Linux Qt desktop, controller and app loading | PASS | 60 / 60 |
| Windows Qt desktop, controller and app loading | PASS | 59 / 59 |
| Linux AddressSanitizer + UndefinedBehaviorSanitizer | PASS | 58 / 58 |
| Linux package, checksum and extracted CLI/GUI loading | PASS | 60 / 60 before packaging |
| Windows package, checksum and extracted CLI/GUI loading | PASS | 59 / 59 before packaging |

All seven jobs completed successfully. Windows has one fewer core case because
the POSIX mode-000 permission test is not compiled there; it was not a failed or
silently skipped Windows check. Do not sum matrix counts as unique tests.

The core is independent of Qt. Desktop jobs used Qt 6.8.3. Controller integration
tests exercise real fixture scans, navigation, filtering, treemap geometry,
duplicate review with one copy preserved, cleanup preview, snapshot save/compare,
report export, invalid roots and best-effort cancellation.

## Distribution artifacts

The package run contains `DiskPilot-development-windows-2022` and
`DiskPilot-development-ubuntu-24.04`. Each artifact includes a platform ZIP and
`SHA256SUMS.txt`. CPack generated `DiskPilot-0.1.0-win64.zip` and
`DiskPilot-0.1.0-Linux.zip`; both were extracted and the contained CLI and GUI
were launched successfully. Windows verification removed Qt SDK paths and plugin
environment overrides before GUI startup, checking the bundled Qt deployment.

These are development artifacts, not a published GitHub Release or installer.
Hosted-runner extraction checks do not replace clean-machine compatibility tests
or interactive visual acceptance. Linux still depends on compatible host libraries.

## Defects fixed during verification

- Made queued GUI callback wrappers mutable so captured result objects can be moved.
- Used an absolute installation prefix required by Qt's deployment helper.
- Restored Unix executable permissions after archive extraction for the package test.
- Tested the extracted ZIP contents rather than only the installation staging tree.

## Measured performance

Controlled 10,000-file benchmarks ran successfully on Windows and Linux. Results
and their exact source/run are recorded in [docs/BENCHMARKS.md](docs/BENCHMARKS.md).
These are single hosted-runner measurements; cache state was not controlled.
No peak-memory, 100k/500k/1M, duplicate-throughput or cancellation-latency claim is made.

## Safety and remaining release gates

Native OS trash execution remains intentionally unavailable. The executor's
approval/revalidation and failure handling are tested through a fixture adapter;
the native adapter refuses operations. No permanent-deletion fallback exists.
Passing tests does not enable unimplemented deletion functionality.

Live scans are not atomic snapshots. Adversarial concurrent path replacement and
same-metadata content rewrites are outside the current guarantees. Source-bearing,
linked, hard-linked, protected, changed and overlapping cleanup selections are
refused by default.

Remaining work includes native handle-bound trash integration, additional native
long-path/junction/sparse/mount fixtures, GUI visual/manual testing, large-model
profiling, scan exclusions, recent-scan indexing, advanced explorer sorting,
static analysis and clean-machine distribution/license review. The broad master
specification is not represented as fully complete merely because current CI passes.

Local compilation initially failed in the portable toolchain's temporary/cache
operations. Those historical environment failures were superseded by the actual
Windows/Linux GitHub builds above. The source and fixes have been pushed to `main`.
