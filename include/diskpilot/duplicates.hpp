#pragma once
#include "diskpilot/scanner.hpp"
#include <array>
#include <span>

namespace diskpilot {
// Incremental FIPS 180-4 SHA-256; the final equality guarantee is byte comparison.
class Sha256 {
  public:
    void update(std::span<const std::byte>);
    [[nodiscard]] std::string finish();
  private:
    void block();
    std::array<std::uint32_t, 8> state_{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    std::array<std::byte, 64> buffer_{};
    std::size_t used_ = 0;
    std::uint64_t bytes_ = 0;
    bool finished_ = false;
};
struct DuplicateGroup {
    std::string sha256;
    std::uintmax_t bytes_per_file = 0;
    std::vector<std::size_t> nodes;
    // All members have distinct identities and have been byte-compared.
    std::string verification = "byte_verified";
};
struct DuplicateProgress {
    std::uint64_t files_hashed = 0;
    std::uintmax_t bytes_read = 0; // Includes partial, full and comparison reads.
};
struct DuplicateResult {
    std::vector<DuplicateGroup> groups;
    std::vector<ScanError> errors;
    std::uint64_t error_count = 0;
    DuplicateProgress progress;
    bool complete = false;
    bool cancelled = false;
};
using DuplicateCallback = std::function<void(const DuplicateProgress&)>;
class DuplicateDetector {
  public:
    [[nodiscard]] DuplicateResult find(const ScanResult&, std::stop_token = {}, DuplicateCallback = {}) const;
};
} // namespace diskpilot
