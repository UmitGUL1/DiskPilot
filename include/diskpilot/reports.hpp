#pragma once
#include "diskpilot/analysis.hpp"
#include "diskpilot/cleanup.hpp"
#include "diskpilot/duplicates.hpp"
#include <istream>

namespace diskpilot {
void write_report(std::ostream&, const ScanResult&, const StorageAnalysis&, const std::string& format,
                  const DuplicateResult* = nullptr, const std::vector<CleanupCandidate>* = nullptr);
struct SnapshotEntry { std::string kind; std::uintmax_t logical_bytes = 0; };
struct Snapshot {
    fs::path root;
    std::int64_t timestamp = 0;
    bool complete = false;
    std::map<std::string,SnapshotEntry> entries; // Relative generic UTF-8 paths.
};
struct StorageChange { std::string path; std::string kind; std::uintmax_t before = 0, after = 0; bool added = false, removed = false; };
[[nodiscard]] Snapshot snapshot(const ScanResult&);
void write_snapshot(std::ostream&, const Snapshot&);
[[nodiscard]] Snapshot read_snapshot(std::istream&);
[[nodiscard]] std::vector<StorageChange> compare(const Snapshot&, const Snapshot&);
void write_comparison(std::ostream&, const Snapshot&, const Snapshot&, const std::vector<StorageChange>&);
} // namespace diskpilot
