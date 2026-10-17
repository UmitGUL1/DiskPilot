#pragma once
#include "diskpilot/scanner.hpp"
#include <iosfwd>
#include <map>

namespace diskpilot {
struct AnalysisOptions {
    std::size_t top = 30;
    std::uintmax_t minimum_bytes = 0;
    std::string extension;
    std::string name_contains;
    fs::path folder; // Relative to the scan root, or absolute.
    std::optional<std::int64_t> modified_after_unix_ms;
    std::optional<std::int64_t> modified_before_unix_ms;
};
struct Category {
    std::uint64_t files = 0;
    std::uintmax_t logical_bytes = 0;
};
struct StorageAnalysis {
    std::vector<std::size_t> largest_files;
    std::vector<std::size_t> largest_folders;
    std::map<std::string, Category> extensions;
    std::optional<fs::space_info> volume;
};
[[nodiscard]] StorageAnalysis analyze(const ScanResult&, const AnalysisOptions& = {});
// JSON and CSV always include completeness and policy metadata.
void write_json(std::ostream&, const ScanResult&, const StorageAnalysis&);
void write_csv(std::ostream&, const ScanResult&, const StorageAnalysis&);
} // namespace diskpilot
