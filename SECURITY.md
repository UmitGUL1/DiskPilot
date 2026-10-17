# Security policy

This development build is not a production-hardened cleanup tool. Native trash
execution fails closed and permanent deletion is absent. Do not enable either
without a reviewed native design and concurrent replacement/recovery tests.

Scans use live path enumeration, not a filesystem snapshot. Read verification
uses native handles and identity/size/mtime checks; adversarial metadata restoration
is outside the current correctness guarantee. Scanning untrusted, concurrently
mutated filesystem trees needs further handle-relative traversal hardening.

Reports and snapshots can reveal local paths. No contents, telemetry or reports
are uploaded automatically. HTML escapes untrusted values; CSV neutralizes
spreadsheet formulas. Snapshot input is bounded and never accepted as cleanup input.

Report vulnerabilities privately through GitHub's security advisory facility when
enabled for the eventual repository. If unavailable, request a private channel
without publicly posting exploit details or personal files. No response SLA or
support coverage is promised for this unreleased version.
