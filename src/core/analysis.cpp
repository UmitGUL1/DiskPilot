#include "diskpilot/analysis.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <ostream>
#include <stdexcept>

namespace diskpilot {
namespace {
std::string utf8(const fs::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
std::string lower(std::string value) {
    for (auto& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}
std::string csv_cell(std::string value) {
    // Neutralize spreadsheet formulas, including leading control/whitespace.
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first != std::string::npos && std::string("=+-@").find(value[first]) != std::string::npos)
        value.insert(value.begin(), '\'');
    std::string result = "\"";
    for (char ch : value) { if (ch == '"') result += '"'; result += ch; }
    return result + '"';
}
}

StorageAnalysis analyze(const ScanResult& scan, const AnalysisOptions& options) {
    StorageAnalysis result;
    std::error_code error;
    if (!scan.root.empty()) {
        auto space = fs::space(scan.root, error);
        if (!error) result.volume = space;
    }
    for (std::size_t i = 0; i < scan.nodes.size(); ++i) {
        const auto& node = scan.nodes[i];
        if (node.kind == NodeKind::directory && i != 0) result.largest_folders.push_back(i);
        if (node.kind != NodeKind::file) continue;
        const auto extension = lower(utf8(node.path.extension()));
        auto& category = result.extensions[extension];
        ++category.files;
        category.logical_bytes += node.logical_bytes;
        if (node.logical_bytes < options.minimum_bytes) continue;
        if (!options.extension.empty() && extension != lower(options.extension)) continue;
        if (!options.name_contains.empty() && utf8(node.path.filename()).find(options.name_contains) == std::string::npos) continue;
        if (!options.folder.empty()) {
            const auto folder = (options.folder.is_absolute() ? options.folder : scan.root / options.folder).lexically_normal();
            const auto relative = node.path.lexically_relative(folder);
            if (relative.empty() || *relative.begin() == "..") continue;
        }
        if (options.modified_after_unix_ms && (!node.modified_at_unix_ms || *node.modified_at_unix_ms < *options.modified_after_unix_ms)) continue;
        if (options.modified_before_unix_ms && (!node.modified_at_unix_ms || *node.modified_at_unix_ms > *options.modified_before_unix_ms)) continue;
        result.largest_files.push_back(i);
    }
    auto trim = [&](auto& indices) {
        auto less = [&](std::size_t a, std::size_t b) {
            if (scan.nodes[a].logical_bytes != scan.nodes[b].logical_bytes)
                return scan.nodes[a].logical_bytes > scan.nodes[b].logical_bytes;
            return scan.nodes[a].path.native() < scan.nodes[b].path.native();
        };
        const auto count = std::min(options.top, indices.size());
        std::partial_sort(indices.begin(), indices.begin() + static_cast<std::ptrdiff_t>(count), indices.end(), less);
        indices.resize(count);
    };
    trim(result.largest_files); trim(result.largest_folders);
    return result;
}

void write_json(std::ostream& output, const ScanResult& scan, const StorageAnalysis& analysis) {
    using nlohmann::json;
    json report{{"schema_version", 1}, {"target", utf8(scan.root)},
                {"complete", scan.complete}, {"cancelled", scan.cancelled},
                {"limit_reached", scan.limit_reached}, {"files", scan.progress.files},
                {"directories_including_root", scan.progress.directories},
                {"logical_bytes", scan.progress.logical_bytes}, {"duration_ms", scan.duration.count()},
                {"started_at_unix_ms", scan.started_at_unix_ms},
                {"error_count", scan.error_count}, {"access_denied_count", scan.access_denied_count},
                {"skipped_links", scan.progress.skipped_links}, {"skipped_mounts", scan.skipped_mounts},
                {"policy", "read-only; no links or reparse points; no mount crossings by default"},
                {"allocation_scope", "unique observed file identities; excludes directory/filesystem metadata"}};
    report["unique_allocated_bytes"] = scan.nodes.empty() || !scan.nodes[0].allocated_complete
                                          ? json(nullptr) : json(scan.nodes[0].unique_allocated_bytes);
    report["volume"] = nullptr;
    if (analysis.volume) {
        report["volume"] = {{"capacity", analysis.volume->capacity}, {"free", analysis.volume->free},
                             {"available", analysis.volume->available},
                             {"used", analysis.volume->capacity - analysis.volume->free}};
    }
    auto rows = [&](const auto& indices) {
        json array = json::array();
        for (auto index : indices) {
            const auto& node = scan.nodes.at(index);
            array.push_back({{"path", utf8(node.path)}, {"logical_bytes", node.logical_bytes},
                             {"unique_allocated_bytes", node.allocated_complete ? json(node.unique_allocated_bytes) : json(nullptr)},
                             {"file_count", node.file_count}, {"repeated_identity", node.repeated_identity}});
            array.back()["modified_at_unix_ms"] = node.modified_at_unix_ms ? json(*node.modified_at_unix_ms) : json(nullptr);
        }
        return array;
    };
    report["largest_files"] = rows(analysis.largest_files);
    report["largest_folders"] = rows(analysis.largest_folders);
    report["extensions"] = json::array();
    for (const auto& [extension, category] : analysis.extensions)
        report["extensions"].push_back({{"extension", extension}, {"files", category.files}, {"logical_bytes", category.logical_bytes}});
    report["errors"] = json::array();
    for (const auto& error : scan.errors)
        report["errors"].push_back({{"path", utf8(error.path)}, {"operation", error.operation}, {"message", error.message}});
    // Replace invalid UTF-8 from arbitrary POSIX filenames rather than failing export.
    output << report.dump(2, ' ', false, json::error_handler_t::replace) << '\n';
    if (!output) throw std::runtime_error("JSON export failed");
}

void write_csv(std::ostream& output, const ScanResult& scan, const StorageAnalysis& analysis) {
    output << "kind,path,logical_bytes,unique_allocated_bytes,file_count,scan_complete,scan_cancelled,error_count\r\n";
    auto row = [&](const char* kind, const FileNode& node) {
        output << kind << ',' << csv_cell(utf8(node.path)) << ',' << node.logical_bytes << ',';
        if (node.allocated_complete) output << node.unique_allocated_bytes;
        output << ',' << node.file_count << ',' << (scan.complete ? "true" : "false") << ','
               << (scan.cancelled ? "true" : "false") << ',' << scan.error_count << "\r\n";
    };
    if (!scan.nodes.empty()) row("summary", scan.nodes[0]);
    else output << "summary," << csv_cell(utf8(scan.root)) << ",0,,0,false,"
                << (scan.cancelled ? "true" : "false") << ',' << scan.error_count << "\r\n";
    for (auto index : analysis.largest_files) row("file", scan.nodes.at(index));
    for (auto index : analysis.largest_folders) row("directory", scan.nodes.at(index));
    if (!output) throw std::runtime_error("CSV export failed");
}
} // namespace diskpilot
