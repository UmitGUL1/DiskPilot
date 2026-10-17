#pragma once
#include "diskpilot/file_access.hpp"

namespace diskpilot {
enum class Confidence { regenerable, review_required, protected_path };
struct CleanupCandidate {
    std::size_t node = 0;
    fs::path project;
    std::string ecosystem;
    std::string type;
    std::string evidence;
    std::string rebuild;
    Confidence confidence = Confidence::review_required;
    std::uintmax_t logical_bytes = 0;
    std::string warning = "Inspect all contents. Regeneration may require network access. In-use status is unknown.";
};
struct PlanEntry { fs::path relative; FileStamp stamp; bool operator==(const PlanEntry&) const = default; };
struct CleanupItem { CleanupCandidate candidate; fs::path path; std::vector<PlanEntry> manifest; };
struct CleanupPlan {
    fs::path root;
    std::vector<CleanupItem> items;
    std::uintmax_t logical_bytes = 0;
};
struct CleanupOutcome { fs::path path; bool moved_to_trash = false; std::string message; };
[[nodiscard]] std::vector<CleanupCandidate> detect_cleanup(const ScanResult&);
// Only explicitly selected, nonoverlapping candidates enter the plan. Incomplete
// scans, source-bearing trees, links, hardlinks and protected paths are rejected.
[[nodiscard]] CleanupPlan make_cleanup_plan(const ScanResult&, const std::vector<CleanupCandidate>&,
                                           std::span<const std::size_t> selected, std::stop_token = {});
class TrashService {
  public:
    virtual ~TrashService() = default;
    virtual void move(const fs::path&) = 0;
};
class NativeTrashService final : public TrashService {
  public:
    void move(const fs::path&) override;
};
// Requires approval of the precise in-memory plan. No plan is deserialized for
// execution and no permanent deletion implementation exists.
[[nodiscard]] std::vector<CleanupOutcome> execute_cleanup(const CleanupPlan&, bool approved,
                                                         TrashService&, std::stop_token = {});
} // namespace diskpilot
