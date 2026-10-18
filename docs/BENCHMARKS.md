# Measured development benchmark

Source commit: `08bc2098f96dcce1669e977982e876a914713dcc`.
Run: https://github.com/UmitGUL1/DiskPilot/actions/runs/39074093163

The fixture contained 10,000 files, each containing 128 bytes, distributed over
10 directories. Totals and scan completeness were checked before accepting a
measurement. Fixture creation is excluded. These are GitHub-hosted runner results,
not a claim about real-world user folders, cold-cache performance or other tools.

| Environment | Engine scan | Whole process | Files / whole-process second |
| --- | ---: | ---: | ---: |
| Windows Server 2022, AMD64 Family 25 Model 1 | 399 ms | 0.4120 s | 24,270 |
| Linux 6.17 Azure x86_64, glibc 2.39 | 42 ms | 0.0483 s | 206,905 |

Whole-process time includes process startup, scan, analysis and JSON export.
Engine time is the scanner's reported duration. Cache state was not controlled;
these are single measurements without repeated-run statistics. Peak memory and
physical storage device characteristics were not measured. Runner processor
metadata for Linux only reported `x86_64`.

Raw JSON files are retained as `benchmark-10000-windows-2022` and
`benchmark-10000-ubuntu-24.04` artifacts on the linked run, subject to retention.
The workflow generates fresh artifacts on later relevant pushes; values may vary.

100k/500k/1M-file tests, memory profiling, duplicate throughput, treemap timing,
cancellation latency and representative user folders remain separate work.
