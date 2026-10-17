#include "diskpilot/scanner.hpp"

#include <algorithm>
#include <system_error>
#include <unordered_set>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <sys/stat.h>
#endif

namespace diskpilot {
namespace {
struct Metadata {
    NodeKind kind = NodeKind::other;
    std::uintmax_t logical = 0;
    std::optional<std::uintmax_t> allocated;
    std::string identity;
    std::uint64_t device = 0;
    std::optional<std::int64_t> modified_at_unix_ms;
};

#ifdef _WIN32
class Handle {
  public:
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() { if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HANDLE get() const { return value_; }
  private:
    HANDLE value_;
};

fs::path native_path(const fs::path& path) {
    auto value = path.native();
    if (value.starts_with(L"\\\\?\\")) return path;
    if (value.starts_with(L"\\\\")) return fs::path(L"\\\\?\\UNC\\" + value.substr(2));
    return fs::path(L"\\\\?\\" + value);
}

std::optional<Metadata> metadata(const fs::path& path, std::error_code& error) {
    const auto native = native_path(path);
    const DWORD attributes = GetFileAttributesW(native.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        error = {static_cast<int>(GetLastError()), std::system_category()};
        return std::nullopt;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        Metadata result;
        result.kind = NodeKind::skipped_link;
        return result;
    }
    Handle handle(CreateFileW(native.c_str(), FILE_READ_ATTRIBUTES,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr, OPEN_EXISTING,
                             FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    if (handle.get() == INVALID_HANDLE_VALUE) {
        error = {static_cast<int>(GetLastError()), std::system_category()};
        return std::nullopt;
    }
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(handle.get(), &info)) {
        error = {static_cast<int>(GetLastError()), std::system_category()};
        return std::nullopt;
    }
    Metadata result;
    if ((info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        result.kind = NodeKind::skipped_link;
        return result;
    }
    result.kind = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
                      ? NodeKind::directory : NodeKind::file;
    result.device = info.dwVolumeSerialNumber;
    const auto filetime = (static_cast<std::uint64_t>(info.ftLastWriteTime.dwHighDateTime) << 32U) |
                          info.ftLastWriteTime.dwLowDateTime;
    result.modified_at_unix_ms = static_cast<std::int64_t>(filetime / 10000U) - 11644473600000LL;
    result.identity = std::to_string(result.device) + ":" +
                      std::to_string(info.nFileIndexHigh) + ":" + std::to_string(info.nFileIndexLow);
    result.logical = (static_cast<std::uintmax_t>(info.nFileSizeHigh) << 32U) | info.nFileSizeLow;
    FILE_STANDARD_INFO standard{};
    if (GetFileInformationByHandleEx(handle.get(), FileStandardInfo, &standard, sizeof(standard)) &&
        standard.AllocationSize.QuadPart >= 0) {
        result.allocated = static_cast<std::uintmax_t>(standard.AllocationSize.QuadPart);
    }
    if ((info.dwFileAttributes & (FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_SPARSE_FILE)) != 0) {
        FILE_COMPRESSION_INFO compressed{};
        if (GetFileInformationByHandleEx(handle.get(), FileCompressionInfo, &compressed, sizeof(compressed)) &&
            compressed.CompressedFileSize.QuadPart >= 0)
            result.allocated = static_cast<std::uintmax_t>(compressed.CompressedFileSize.QuadPart);
        else result.allocated.reset();
    }
    return result;
}
#else
fs::path native_path(const fs::path& path) { return path; }

std::optional<Metadata> metadata(const fs::path& path, std::error_code& error) {
    struct stat info {};
    if (::lstat(path.c_str(), &info) != 0) {
        error = {errno, std::generic_category()};
        return std::nullopt;
    }
    Metadata result;
    result.kind = S_ISLNK(info.st_mode) ? NodeKind::skipped_link
                  : S_ISDIR(info.st_mode) ? NodeKind::directory
                  : S_ISREG(info.st_mode) ? NodeKind::file : NodeKind::other;
    result.device = static_cast<std::uint64_t>(info.st_dev);
    result.modified_at_unix_ms = static_cast<std::int64_t>(info.st_mtim.tv_sec) * 1000 + info.st_mtim.tv_nsec / 1000000;
    result.identity = std::to_string(result.device) + ":" + std::to_string(info.st_ino);
    if (result.kind == NodeKind::file) {
        result.logical = static_cast<std::uintmax_t>(info.st_size);
        if (info.st_blocks >= 0) result.allocated = static_cast<std::uintmax_t>(info.st_blocks) * 512U;
    }
    return result;
}
#endif

void record_error(ScanResult& result, const fs::path& path, const char* operation,
                  const std::error_code& error) {
    ++result.error_count;
    const bool denied = error == std::errc::permission_denied || error == std::errc::operation_not_permitted;
    if (denied) ++result.access_denied_count;
    if (result.errors.size() < 1000) result.errors.push_back({path, operation, error.message(), denied});
}

bool add_bytes(std::uintmax_t& target, std::uintmax_t value) {
    if (value > std::numeric_limits<std::uintmax_t>::max() - target) return false;
    target += value;
    return true;
}
} // namespace

ScanResult FileSystemScanner::scan(const fs::path& input, const ScanOptions& options,
                                    std::stop_token stop, ProgressCallback callback) const {
    const auto started = std::chrono::steady_clock::now();
    auto last_progress = started;
    ScanResult result;
    result.started_at_unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auto finish = [&]() {
        if (callback) callback(result.progress);
        result.cancelled = stop.stop_requested();
        result.complete = result.error_count == 0 && !result.cancelled && !result.limit_reached;
        result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
    };
    std::error_code error;
    if (input.empty()) {
        record_error(result, input, "root", std::make_error_code(std::errc::invalid_argument));
        finish();
        return result;
    }
    for (const auto& part : input) {
        if (part == "..") {
            record_error(result, input, "parent traversal components are not supported",
                         std::make_error_code(std::errc::invalid_argument));
            finish(); return result;
        }
    }
    result.root = fs::absolute(input, error).lexically_normal();
    if (error) {
        record_error(result, input, "absolute path", error);
        finish();
        return result;
    }
    // Reject link/reparse components in the requested root, including ancestors.
    fs::path component;
    std::optional<Metadata> root_metadata;
    for (const auto& part : result.root) {
        component /= part;
        if (!component.has_root_directory()) continue;
        error.clear();
        root_metadata = metadata(component, error);
        if (!root_metadata || root_metadata->kind != NodeKind::directory) {
            record_error(result, component, "root must be a directory without link ancestors",
                         error ? error : std::make_error_code(std::errc::operation_not_supported));
            finish();
            return result;
        }
    }
    if (!root_metadata || options.max_entries == 0) {
        record_error(result, result.root, "invalid root or entry limit",
                     std::make_error_code(std::errc::invalid_argument));
        finish();
        return result;
    }
    result.nodes.push_back({result.root, no_parent, NodeKind::directory});
    result.nodes.front().identity = root_metadata->identity;
    result.nodes.front().modified_at_unix_ms = root_metadata->modified_at_unix_ms;
    result.progress.entries = 1;
    result.progress.directories = 1;
    std::unordered_set<std::string> file_identities;
    std::unordered_set<std::string> directories{root_metadata->identity};

    // Flat model acts as an iterative work list; no recursion or per-file tasks.
    for (std::size_t cursor = 0; cursor < result.nodes.size() && !stop.stop_requested(); ++cursor) {
        if (result.nodes[cursor].kind != NodeKind::directory) continue;
        const auto directory = result.nodes[cursor].path;
        error.clear();
        const auto current = metadata(directory, error);
        if (!current || current->kind != NodeKind::directory || current->identity != result.nodes[cursor].identity) {
            record_error(result, directory, "directory changed before enumeration",
                         error ? error : std::make_error_code(std::errc::state_not_recoverable));
            continue;
        }
        fs::directory_iterator it(native_path(directory), fs::directory_options::none, error);
        const fs::directory_iterator end;
        if (error) { record_error(result, directory, "open directory", error); continue; }
        while (it != end && !stop.stop_requested()) {
            if (result.nodes.size() >= options.max_entries) { result.limit_reached = true; break; }
            const auto path = directory / it->path().filename();
            error.clear();
            const auto info = metadata(path, error);
            if (!info) {
                record_error(result, path, "metadata", error);
            } else {
                FileNode node{path, cursor, info->kind};
                node.identity = info->identity;
                node.modified_at_unix_ms = info->modified_at_unix_ms;
                ++result.progress.entries;
                if (info->kind == NodeKind::skipped_link) {
                    ++result.progress.skipped_links;
                } else if (info->kind == NodeKind::directory) {
                    ++result.progress.directories;
                    if (!options.cross_filesystems && info->device != root_metadata->device) {
                        ++result.skipped_mounts;
                        node.kind = NodeKind::other;
                    } else if (!directories.insert(info->identity).second) {
                        node.kind = NodeKind::other;
                        node.repeated_identity = true;
                    }
                } else if (info->kind == NodeKind::file) {
                    node.logical_bytes = info->logical;
                    node.file_count = 1;
                    ++result.progress.files;
                    if (!add_bytes(result.progress.logical_bytes, info->logical)) {
                        record_error(result, path, "logical size overflow",
                                     std::make_error_code(std::errc::value_too_large));
                    }
                    node.repeated_identity = !file_identities.insert(info->identity).second;
                    node.allocated_complete = info->allocated.has_value();
                    if (!node.repeated_identity && info->allocated) node.unique_allocated_bytes = *info->allocated;
                }
                result.nodes.push_back(std::move(node));
            }
            const auto now = std::chrono::steady_clock::now();
            if (callback && now - last_progress >= options.progress_interval) {
                callback(result.progress);
                last_progress = now;
            }
            error.clear();
            it.increment(error);
            if (error) { record_error(result, directory, "iterate directory", error); break; }
        }
        if (result.limit_reached) break;
    }
    for (std::size_t i = result.nodes.size(); i-- > 1;) {
        const auto& child = result.nodes[i];
        auto& parent = result.nodes[child.parent];
        const bool logical_ok = add_bytes(parent.logical_bytes, child.logical_bytes);
        const bool allocated_ok = add_bytes(parent.unique_allocated_bytes, child.unique_allocated_bytes);
        parent.file_count += child.file_count;
        parent.allocated_complete = parent.allocated_complete && child.allocated_complete && allocated_ok;
        if (!logical_ok || !allocated_ok) {
            record_error(result, parent.path, "aggregate size overflow",
                         std::make_error_code(std::errc::value_too_large));
        }
    }
    finish();
    return result;
}
} // namespace diskpilot
