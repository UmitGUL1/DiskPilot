# Architecture

The Qt-independent core exposes synchronous cancellable services. The CLI calls
them directly. The Qt controller owns one `std::jthread`, captures immutable input,
and queues result/progress updates onto the UI thread. Worker completion is joined
on shutdown. Normal scans never read file contents.

## Core boundaries

- `scanner`: native Windows/POSIX metadata, iterative flat FileNode model with
  parent indices, unique identity allocation, bounded errors and completeness.
- `analysis`: sorting/filtering, extensions, logical/allocation/volume separation,
  escaped JSON and spreadsheet-safe scan CSV.
- `treemap`: deterministic squarified geometry and capped directory child views;
  rendering is separate and uses a single QML Canvas.
- `file_access`: no-follow native handles, identity/size/mtime validation, checked
  bounded reads and exclusive report-file creation.
- `duplicates`: streamed partial/full SHA-256 and byte verification, hard-link
  identity exclusion, cancellation and explicit per-file errors.
- `cleanup`: context-based rules, explanations, user-selected metadata manifests,
  approval/revalidation and injectable trash service. Native trash fails closed.
- `reports`: JSON/CSV/HTML, metadata-only snapshots and path/size comparisons.

## Safety boundary

File paths are not capabilities. Native checked reads improve verification but
path-based enumeration still has concurrent ancestor-replacement limits. The
application does not promise security against adversarial filesystem mutation.
Metadata is not a content hash, and a scan is not an atomic filesystem snapshot.

Cleanup plans reject protected paths, sources, hard links, links, incomplete scans,
changed descendants and overlapping selections. They cannot be deserialized from
reports or snapshots for execution. Approval is separate from planning. Native
trash remains unavailable until handle-bound, recoverable operations can pass race
and failure testing. There is no permanent-deletion implementation.

## Desktop boundary

QML consumes real engine results and displays no invented metrics. Scan and
duplicate work execute off the UI thread. Filesystem models and treemap tiles are
capped for rendering; full findings can be exported. Untrusted text is rendered
as plain text; report HTML independently escapes it. Preferences use local QSettings.

Some UI model queries remain synchronous and need profiling at large scales.
Core, controller, app-load, sanitizer and package tests now pass on GitHub.
See IMPLEMENTATION_STATUS.md for exact evidence and remaining release requirements;
these checks do not establish native-trash support or visual acceptance.
