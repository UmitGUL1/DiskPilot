# DiskPilot

**Understand your storage. Reclaim it safely.**

[![Build and test](https://github.com/UmitGUL1/DiskPilot/actions/workflows/build.yml/badge.svg)](https://github.com/UmitGUL1/DiskPilot/actions/workflows/build.yml)

DiskPilot is a local C++20 storage analyzer with a Qt-independent CLI and an
optional Qt Quick desktop application. It scans real metadata, verifies duplicate
contents, and explains developer cleanup candidates using project context.

Windows/Linux core and Qt desktop builds, controller tests, Linux sanitizers and
extracted-package checks pass in GitHub Actions. This remains a development build;
native trash execution is unavailable. See [IMPLEMENTATION_STATUS.md](IMPLEMENTATION_STATUS.md)
for exact results and remaining release gates.

## Features in source

- Iterative metadata scanning, bounded retained entries, progress and cancellation.
- Logical versus unique allocation accounting and honest partial-scan warnings.
- Largest files, extension distribution, filters and squarified treemap geometry.
- Duplicate detection: size, partial SHA-256, full SHA-256, byte comparison.
- Context-aware JavaScript, CMake, Python, Rust, Maven and Gradle cleanup review.
- Selected cleanup plans with descendant metadata revalidation.
- Local metadata snapshots, path/size comparison, JSON/CSV/HTML reports.
- Qt Quick dashboard, explorer, treemap, duplicate review, cleanup preview and settings.

**Native trash execution is disabled.** A handle-bound native trash adapter must
pass race and recovery validation first. No permanent-deletion fallback exists.
Plans also reject source-bearing trees, links, hard links and protected metadata.
Regenerable is a classification, never a guarantee of safe removal.

## Build and test

Intended baseline: CMake 3.24+, C++20, Visual Studio 2022 with Windows SDK on
Windows or GCC 12+ on Linux. Dependencies are pinned by commit; initial
configuration needs Git and internet access. Runtime is offline.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
```

For the desktop application provide Qt 6.5+ Quick/Quick Controls development
packages, then add `-DDISKPILOT_BUILD_GUI=ON`. CI targets Qt 6.8.3. Set
`CMAKE_PREFIX_PATH` to Qt's installation when necessary. Install using
`cmake --install build --config Release --prefix ABSOLUTE_STAGE_DIRECTORY`.
Qt's deployment helper requires an absolute installation prefix.

Windows executable paths normally include `Release/`. Qt is not required by the
core or CLI. `DISKPILOT_BUILD_TESTS=OFF` skips tests without validating anything.
Offline configuration can use CMake FetchContent source overrides.

## CLI

```sh
diskpilot-cli --help
diskpilot-cli scan "D:/Projects"
diskpilot-cli scan "D:/Projects" --format json > report.json
diskpilot-cli largest "D:/Projects" --top 30 --min-size 1048576
diskpilot-cli largest "D:/Projects" --extension .cpp --name parser
diskpilot-cli duplicates "D:/Photos" --format json > duplicates.json
diskpilot-cli dev-cleanup "D:/Projects" --dry-run --format html > cleanup.html
diskpilot-cli snapshot "D:/Projects" --output before.json
diskpilot-cli compare before.json after.json > comparison.json
```

Filters affect largest-file rows, leaving scan totals and categories unfiltered.
Snapshot exports exclusively create new files. Shell redirection follows your
shell's normal overwrite rules. CSV exports the scan table; JSON/HTML include
detailed findings. Progress goes to stderr. Exit codes: 0 complete within policy,
1 partial/error, 2 usage, 130 cancelled. Blocking OS calls can delay cancellation.

## Accounting and safety

Ordinary scanning never reads file contents or executes scripts. Verification
streams native checked handles with bounded buffers. Windows blocks writers while
each verification handle is open. POSIX uses no-follow handles and metadata checks.
These checks cannot establish immutability against an adversary restoring metadata.
Duplicate groups describe verification-time equality; rescan before acting.

Links, junctions, reparse points and cross-device directories are skipped. Root
link ancestors and `..` components are rejected. Linux same-device bind mounts
are not fully distinguished. Scanning samples a live filesystem; path enumeration
can race ancestor replacement and is not a security sandbox.

Logical bytes count paths; allocation counts each observed identity once and
excludes filesystem metadata, shared/reflink extents and reserved space. Neither
equals volume used space. Trash may retain allocated bytes. Nested directory
totals overlap. Snapshot comparisons use paths/sizes: renames appear as removal
and addition, and equal size does not prove equal content. Reports contain paths
but no file contents. Keep exported reports private when paths are sensitive.

## Development packages

The package workflow creates development ZIPs and checksums without publishing
a release. [Download artifacts from the verified package run](https://github.com/UmitGUL1/DiskPilot/actions/runs/39074229536).
Both platform ZIPs passed extracted CLI/GUI loading checks on hosted runners;
the Windows check removes SDK paths to exercise bundled Qt. Linux requires
compatible host libraries. Clean-machine portability is not yet established.

See [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md),
[ROADMAP.md](ROADMAP.md) and [docs/TESTING.md](docs/TESTING.md).
Measured 10,000-file scan results are in [docs/BENCHMARKS.md](docs/BENCHMARKS.md).
Screenshots will be added only after an actual application capture.
