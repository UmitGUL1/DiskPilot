# Contributing

Build the Qt-independent core and run CTest first. Enable the Qt desktop target
when modifying GUI code. Include reproducible fixtures for confirmed defects,
especially link changes, hard links, partial results and export escaping.

Use C++20 RAII and composition, format with the repository clang-format profile,
and keep Qt out of the core. Do not execute discovered projects, hide scan errors,
introduce telemetry or enable path-based permanent deletion.

PRs should explain the problem, resulting behavior, actual tests performed and
remaining limits. Never describe unexecuted tests as passing. Redact private paths
from logs. Follow CODE_OF_CONDUCT.md and SECURITY.md.
