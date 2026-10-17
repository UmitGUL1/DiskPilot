# Roadmap and release gates

1. Run Windows/Linux core, GUI-load and sanitizer CI; fix all failures.
2. Validate Windows long paths, junctions, sparse/compressed files and locked files.
3. Review SHA-256 against independent implementations and mutation scenarios.
4. Implement a handle-bound native trash adapter. Keep current refusal until safe.
5. Add source-aware manual review without weakening default source protection.
6. Add GUI automation for scan/cancel, navigation, filters and preview refusal.
7. Add scan exclusions, persistent recent-scan index and advanced explorer sorting.
8. Profile large-model UI operations; measure 10k, 100k, 500k and 1M-file fixtures.
9. Run clang-tidy/format checks and license/dependency distribution review.
10. Verify extracted packages on clean Windows/Linux machines and capture real screenshots.

No release, performance parity, recovery guarantee or completed acceptance phase
is claimed merely because corresponding source code exists.
