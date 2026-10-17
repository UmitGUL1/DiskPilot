#include "diskpilot/file_access.hpp"
#include <algorithm>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace diskpilot {
std::string path_utf8(const fs::path& path) {
    const auto s = path.u8string();
    return {reinterpret_cast<const char*>(s.data()), s.size()};
}
namespace {
#ifdef _WIN32
fs::path extended(const fs::path& p) {
    const auto s = fs::absolute(p).native();
    if (s.starts_with(L"\\\\?\\")) return s;
    return s.starts_with(L"\\\\") ? fs::path(L"\\\\?\\UNC\\" + s.substr(2)) : fs::path(L"\\\\?\\" + s);
}
FileStamp from_handle(HANDLE h) {
    BY_HANDLE_FILE_INFORMATION i{};
    if (!GetFileInformationByHandle(h, &i)) throw std::runtime_error("Cannot read file identity");
    FileStamp s;
    s.kind = (i.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? NodeKind::skipped_link
             : (i.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? NodeKind::directory : NodeKind::file;
    s.identity = std::to_string(i.dwVolumeSerialNumber) + ":" + std::to_string(i.nFileIndexHigh) + ":" + std::to_string(i.nFileIndexLow);
    s.size = (static_cast<std::uintmax_t>(i.nFileSizeHigh) << 32U) | i.nFileSizeLow;
    s.modified = static_cast<std::int64_t>(((static_cast<std::uint64_t>(i.ftLastWriteTime.dwHighDateTime) << 32U) | i.ftLastWriteTime.dwLowDateTime) / 10000U) - 11644473600000LL;
    return s;
}
#else
FileStamp from_stat(const struct stat& i) {
    return {S_ISLNK(i.st_mode) ? NodeKind::skipped_link : S_ISREG(i.st_mode) ? NodeKind::file : S_ISDIR(i.st_mode) ? NodeKind::directory : NodeKind::other,
            std::to_string(i.st_dev) + ":" + std::to_string(i.st_ino),
            S_ISREG(i.st_mode) ? static_cast<std::uintmax_t>(i.st_size) : 0,
            static_cast<std::int64_t>(i.st_mtim.tv_sec) * 1000 + i.st_mtim.tv_nsec / 1000000};
}
#endif
}
FileStamp file_stamp(const fs::path& p) {
#ifdef _WIN32
    const auto h = CreateFileW(extended(p).c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (h == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open metadata: " + path_utf8(p));
    try { auto s = from_handle(h); CloseHandle(h); return s; }
    catch (...) { CloseHandle(h); throw; }
#else
    struct stat i{};
    if (lstat(p.c_str(), &i)) throw std::runtime_error("Cannot stat: " + path_utf8(p));
    return from_stat(i);
#endif
}
bool link_free_path(const fs::path& p) {
    if (!p.is_absolute() || p != p.lexically_normal()) return false;
    try {
        fs::path part;
        for (const auto& c : p) {
            if (c == "..") return false;
            part /= c;
            if (!part.has_root_directory()) continue;
            const auto s = file_stamp(part);
            if (s.kind == NodeKind::skipped_link || s.kind == NodeKind::other) return false;
        }
        return true;
    } catch (...) { return false; }
}
bool path_below(const fs::path& child, const fs::path& parent) {
    const auto relative = child.lexically_normal().lexically_relative(parent.lexically_normal());
    return !relative.empty() && relative != "." && !relative.is_absolute() && *relative.begin() != "..";
}
void write_new_file(const fs::path& input, const std::string& bytes) {
    const auto path=fs::absolute(input).lexically_normal();
    if(!link_free_path(path.parent_path()))throw std::runtime_error("Export parent is missing or linked");
#ifdef _WIN32
    const auto h=CreateFileW(extended(path).c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Export requires a new, writable filename");
    std::size_t offset=0;bool good=true;
    while(offset<bytes.size()) {DWORD written=0;const auto n=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,1024*1024));if(!WriteFile(h,bytes.data()+offset,n,&written,nullptr)||!written){good=false;break;}offset+=written;}
    good=FlushFileBuffers(h)&&good;CloseHandle(h);
#else
    const int fd=open(path.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd<0)throw std::runtime_error("Export requires a new, writable filename");
    std::size_t offset=0;bool good=true;
    while(offset<bytes.size()){const auto n=::write(fd,bytes.data()+offset,bytes.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0){good=false;break;}offset+=static_cast<std::size_t>(n);}
    good=(fsync(fd)==0)&&good;close(fd);
#endif
    if(!good)throw std::runtime_error("Export failed; partial output may remain at the selected filename");
}
struct CheckedFile::Impl {
    fs::path path;
    FileStamp initial;
#ifdef _WIN32
    HANDLE h = INVALID_HANDLE_VALUE;
    ~Impl() { if (h != INVALID_HANDLE_VALUE) CloseHandle(h); }
    FileStamp stamp() const { return from_handle(h); }
#else
    int fd = -1;
    ~Impl() { if (fd >= 0) close(fd); }
    FileStamp stamp() const {
        struct stat s{};
        if (fstat(fd, &s)) throw std::runtime_error("Cannot revalidate open file");
        return from_stat(s);
    }
#endif
};
CheckedFile::CheckedFile(const FileNode& n) : impl_(std::make_unique<Impl>()) {
    if (n.kind != NodeKind::file || n.identity.empty() || !n.modified_at_unix_ms || !link_free_path(n.path))
        throw std::runtime_error("Unsafe or untracked file");
    impl_->path = n.path;
#ifdef _WIN32
    impl_->h = CreateFileW(extended(n.path).c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (impl_->h == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open file for verification");
#else
    impl_->fd = open(n.path.c_str(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (impl_->fd < 0) throw std::runtime_error("Cannot open file for verification");
#endif
    impl_->initial = impl_->stamp();
    if (impl_->initial.kind != NodeKind::file || impl_->initial.identity != n.identity ||
        impl_->initial.size != n.logical_bytes || impl_->initial.modified != *n.modified_at_unix_ms)
        throw std::runtime_error("File changed since scan");
}
CheckedFile::~CheckedFile() = default;
std::size_t CheckedFile::read(std::span<std::byte> bytes) {
#ifdef _WIN32
    DWORD count = 0;
    if (bytes.size() > MAXDWORD || !ReadFile(impl_->h, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr))
        throw std::runtime_error("Read failed");
    return count;
#else
    ssize_t count;
    do { count = ::read(impl_->fd, bytes.data(), bytes.size()); } while (count < 0 && errno == EINTR);
    if (count < 0) throw std::runtime_error("Read failed");
    return static_cast<std::size_t>(count);
#endif
}
void CheckedFile::seek(std::uintmax_t offset) {
    if (offset > static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max())) throw std::runtime_error("Offset out of range");
#ifdef _WIN32
    LARGE_INTEGER position{}; position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(impl_->h, position, nullptr, FILE_BEGIN)) throw std::runtime_error("Seek failed");
#else
    if (lseek(impl_->fd, static_cast<off_t>(offset), SEEK_SET) < 0) throw std::runtime_error("Seek failed");
#endif
}
void CheckedFile::verify() const {
    if (impl_->stamp() != impl_->initial || file_stamp(impl_->path) != impl_->initial || !link_free_path(impl_->path))
        throw std::runtime_error("File changed during verification");
}
} // namespace diskpilot
