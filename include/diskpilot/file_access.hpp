#pragma once
#include "diskpilot/scanner.hpp"
#include <memory>
#include <span>

namespace diskpilot {
struct FileStamp {
    NodeKind kind = NodeKind::other;
    std::string identity;
    std::uintmax_t size = 0;
    std::int64_t modified = 0;
    bool operator==(const FileStamp&) const = default;
};
// Metadata never follows the final link/reparse point. Throws on metadata errors.
[[nodiscard]] FileStamp file_stamp(const fs::path&);
[[nodiscard]] bool link_free_path(const fs::path&);
[[nodiscard]] bool path_below(const fs::path& child, const fs::path& parent);
[[nodiscard]] std::string path_utf8(const fs::path&);
// Exclusive creation: never truncates an existing file, including symlinks.
void write_new_file(const fs::path&, const std::string& bytes);
// Uses a native handle and rejects non-regular files, reparse points, and identity
// changes. Windows blocks writers while open; POSIX changes are checked on handle.
class CheckedFile {
  public:
    explicit CheckedFile(const FileNode&);
    ~CheckedFile();
    CheckedFile(const CheckedFile&) = delete;
    CheckedFile& operator=(const CheckedFile&) = delete;
    std::size_t read(std::span<std::byte>);
    void seek(std::uintmax_t offset);
    void verify() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace diskpilot
