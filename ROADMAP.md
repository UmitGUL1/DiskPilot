# Roadmap and release gates

1. Maintain passing Windows/Linux core, GUI-controller/load and sanitizer CI (initial matrix passed).
2. Validate Windows long paths, junctions, sparse/compressed files and locked files.
3. Review SHA-256 against independent implementations and mutation scenarios.
4. Implement a handle-bound native trash adapter. Keep current refusal until safe.
5. Add source-aware manual review without weakening default source protection.
6. Expand existing GUI controller tests with visual and pointer/keyboard interaction automation.
7. Add scan exclusions, persistent recent-scan index and advanced explorer sorting.
8. Profile large-model UI operations; extend measured 10k scans to 100k, 500k and 1M-file fixtures.
9. Run clang-tidy/format checks and license/dependency distribution review.
10. Extend passing hosted-runner extracted-package checks to clean Windows/Linux machines; capture real screenshots.

No release, performance parity, recovery guarantee or completed acceptance phase
is claimed merely because corresponding source code exists.
