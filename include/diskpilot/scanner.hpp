#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace diskpilot {
namespace fs = std::filesystem;
inline constexpr std::size_t no_parent = std::numeric_limits<std::size_t>::max();

enum class NodeKind { directory, file, skipped_link, other };
struct FileNode {
    fs::path path;
    std::size_t parent = no_parent;
    NodeKind kind = NodeKind::other;
    std::uintmax_t logical_bytes = 0;
    // Physical bytes attributed only to the first observed hard-link identity.
    std::uintmax_t unique_allocated_bytes = 0;
    bool allocated_complete = true;
    std::uint64_t file_count = 0;
    bool repeated_identity = false;
    std::string identity;
    std::optional<std::int64_t> modified_at_unix_ms;
};
struct ScanError {
    fs::path path;
    std::string operation;
    std::string message;
    bool access_denied = false;
};
struct ScanProgress {
    std::uint64_t entries = 0;
    std::uint64_t files = 0;
    std::uint64_t directories = 0; // Includes the selected root.
    std::uint64_t skipped_links = 0;
    std::uintmax_t logical_bytes = 0;
};
struct ScanOptions {
    // Bounds retained entries; partial results are reported when exceeded.
    std::size_t max_entries = 2'000'000;
    bool cross_filesystems = false;
    std::chrono::milliseconds progress_interval{100};
};
struct ScanResult {
    fs::path root;
    std::vector<FileNode> nodes;
    std::vector<ScanError> errors; // Bounded samples, not necessarily all errors.
    ScanProgress progress;
    std::uint64_t error_count = 0;
    std::uint64_t access_denied_count = 0;
    std::uint64_t skipped_mounts = 0;
    std::chrono::milliseconds duration{0};
    std::int64_t started_at_unix_ms = 0;
    bool complete = false; // Complete only within the documented link/mount policy.
    bool cancelled = false;
    bool limit_reached = false;
};
using ProgressCallback = std::function<void(const ScanProgress&)>;

class IFileSystemScanner {
  public:
    virtual ~IFileSystemScanner() = default;
    [[nodiscard]] virtual ScanResult scan(const fs::path&, const ScanOptions& = {},
                                          std::stop_token = {}, ProgressCallback = {}) const = 0;
};

// Platform metadata adapter selected at compile time; no Qt dependency.
class FileSystemScanner final : public IFileSystemScanner {
  public:
    [[nodiscard]] ScanResult scan(const fs::path&, const ScanOptions& = {},
                                  std::stop_token = {}, ProgressCallback = {}) const override;
};
} // namespace diskpilot
