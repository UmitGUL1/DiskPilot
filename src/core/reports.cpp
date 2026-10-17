#include "diskpilot/reports.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace diskpilot {
namespace {
using nlohmann::json;
std::string escaped(const std::string& s) {
    std::string out;
    for(unsigned char c:s) switch(c) {
        case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;
        case '"':out+="&quot;";break;case '\'':out+="&#39;";break;
        default:if(c>=32||c=='\n'||c=='\t')out+=static_cast<char>(c);else out+='?';
    }
    return out;
}
void dump(std::ostream& out,const json& j) {
    out<<j.dump(2,' ',false,json::error_handler_t::replace)<<'\n';
    if(!out) throw std::runtime_error("Export failed");
}
fs::path utf8_path(const std::string& value) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()),value.size()));
}
}
void write_report(std::ostream& out,const ScanResult& scan,const StorageAnalysis& analysis,const std::string& format,
                  const DuplicateResult* duplicates,const std::vector<CleanupCandidate>* cleanup) {
    if(format=="csv") {write_csv(out,scan,analysis);return;}
    std::ostringstream base;write_json(base,scan,analysis);auto j=json::parse(base.str());
    j["duplicates"]=nullptr;j["cleanup_candidates"]=nullptr;
    if(duplicates) {
        auto& d=j["duplicates"];d={{"complete",duplicates->complete},{"cancelled",duplicates->cancelled},{"error_count",duplicates->error_count},{"bytes_read",duplicates->progress.bytes_read},{"groups",json::array()},{"errors",json::array()}};
        for(const auto& group:duplicates->groups) {
            json paths=json::array();for(auto i:group.nodes) paths.push_back(path_utf8(scan.nodes.at(i).path));
            d["groups"].push_back({{"sha256",group.sha256},{"verification",group.verification},{"logical_bytes_per_copy",group.bytes_per_file},{"paths",paths},
                {"logical_redundant_bytes",group.bytes_per_file*(group.nodes.size()-1)},
                {"note","Estimate only; allocation, compression and trash retention affect actual recovery."}});
        }
        for(const auto& e:duplicates->errors)d["errors"].push_back({{"path",path_utf8(e.path)},{"message",e.message}});
    }
    if(cleanup) {
        auto& c=j["cleanup_candidates"];c=json::array();
        for(const auto& item:*cleanup)c.push_back({{"path",path_utf8(scan.nodes.at(item.node).path)},{"project",path_utf8(item.project)},
            {"ecosystem",item.ecosystem},{"type",item.type},{"logical_bytes",item.logical_bytes},{"evidence",item.evidence},
            {"confidence",item.confidence==Confidence::regenerable?"regenerable":item.confidence==Confidence::review_required?"review_required":"protected"},
            {"rebuild",item.rebuild},{"warning",item.warning}});
    }
    if(format=="json") {dump(out,j);return;}
    if(format!="html")throw std::invalid_argument("Report format must be json, csv or html");
    // Untrusted values are escaped into text nodes only; no script, external
    // resource, file URL, or inline event handler is included.
    out<<"<!doctype html><html lang=\"en\"><meta charset=\"utf-8\">"
         "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
         "<meta http-equiv=\"Content-Security-Policy\" content=\"default-src 'none'; style-src 'unsafe-inline'\">"
         "<title>DiskPilot storage report</title><style>body{background:#0b1220;color:#dae4f2;font:16px system-ui;max-width:1100px;margin:40px auto;padding:24px}"
         "h1,h2{color:#65d9d1}table{width:100%;border-collapse:collapse}td,th{text-align:left;padding:12px;border-bottom:1px solid #28374c;overflow-wrap:anywhere}"
         "pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#121f32;padding:20px;border-radius:12px}.warning{color:#f5c36c}</style>"
         "<h1>DiskPilot</h1><p>Understand your storage. Reclaim it safely.</p><h2>"<<escaped(path_utf8(scan.root))<<"</h2>"
         "<p class=\"warning\">"<<(scan.complete?"Complete within scan policy":"Partial results — not a complete inventory")<<"</p>"
         "<p>"<<scan.progress.files<<" files · "<<scan.progress.directories<<" directories · "<<scan.progress.logical_bytes<<" logical bytes · "<<scan.duration.count()<<" ms</p>"
         "<p>Logical file totals differ from volume usage. Allocation excludes filesystem metadata; shared extents are not deduplicated. Moving files to trash may not free space.</p>"
         "<h2>Largest files</h2><table><tr><th>Path</th><th>Logical bytes</th></tr>";
    for(auto i:analysis.largest_files)out<<"<tr><td>"<<escaped(path_utf8(scan.nodes.at(i).path))<<"</td><td>"<<scan.nodes[i].logical_bytes<<"</td></tr>";
    out<<"</table><h2>Full report</h2><pre>"<<escaped(j.dump(2,' ',false,json::error_handler_t::replace))<<"</pre></html>\n";
    if(!out)throw std::runtime_error("HTML export failed");
}
Snapshot snapshot(const ScanResult& scan) {
    Snapshot s;s.root=scan.root;s.timestamp=scan.started_at_unix_ms;s.complete=scan.complete;
    for(const auto& n:scan.nodes) {
        const auto relative=n.path.lexically_relative(scan.root);
        s.entries.emplace(path_utf8(relative),SnapshotEntry{n.kind==NodeKind::directory?"directory":n.kind==NodeKind::file?"file":"skipped",n.logical_bytes});
    }
    return s;
}
void write_snapshot(std::ostream& out,const Snapshot& s) {
    json j{{"schema","diskpilot.snapshot"},{"version",1},{"root",path_utf8(s.root)},{"timestamp_unix_ms",s.timestamp},{"complete",s.complete},{"entries",json::array()}};
    for(const auto& [path,e]:s.entries)j["entries"].push_back({{"path",path},{"kind",e.kind},{"logical_bytes",e.logical_bytes}});
    dump(out,j);
}
Snapshot read_snapshot(std::istream& input) {
    // Bound input bytes before parsing. Snapshot contents are never executed or
    // used as cleanup paths. Keep depth shallow by disabling arbitrary nesting.
    constexpr std::size_t limit=128*1024*1024;
    std::string bytes;std::array<char,65536> buffer{};
    while(input) {input.read(buffer.data(),buffer.size());const auto n=input.gcount();if(bytes.size()+static_cast<std::size_t>(n)>limit)throw std::runtime_error("Snapshot exceeds 128 MiB");bytes.append(buffer.data(),static_cast<std::size_t>(n));}
    if(input.bad())throw std::runtime_error("Snapshot read failed");
    auto j=json::parse(bytes,[](int depth,json::parse_event_t,json&){if(depth>8)throw std::runtime_error("Snapshot nesting too deep");return true;});
    if(j.at("schema")!="diskpilot.snapshot"||j.at("version")!=1||!j.at("entries").is_array()||j.at("entries").size()>2'000'000)throw std::runtime_error("Invalid snapshot schema");
    Snapshot s;s.root=utf8_path(j.at("root").get<std::string>());s.complete=j.at("complete").get<bool>();s.timestamp=j.at("timestamp_unix_ms").get<std::int64_t>();
    if(!s.root.is_absolute()||s.root!=s.root.lexically_normal())throw std::runtime_error("Invalid snapshot root");
    for(const auto& row:j.at("entries")) {
        const auto p=row.at("path").get<std::string>(),kind=row.at("kind").get<std::string>();const auto path=utf8_path(p);
        if(p.empty()||p.find('\0')!=std::string::npos||path.is_absolute()||path.has_root_name()||path!=path.lexically_normal()||
            (kind!="file"&&kind!="directory"&&kind!="skipped"))throw std::runtime_error("Invalid snapshot entry");
        for(const auto& c:path)if(c=="..")throw std::runtime_error("Snapshot traversal rejected");
        if(!row.at("logical_bytes").is_number_unsigned())throw std::runtime_error("Invalid snapshot size");
        if(!s.entries.emplace(p,SnapshotEntry{kind,row.at("logical_bytes").get<std::uintmax_t>()}).second)throw std::runtime_error("Duplicate snapshot path");
    }
    return s;
}
std::vector<StorageChange> compare(const Snapshot& before,const Snapshot& after) {
    if(before.root!=after.root)throw std::invalid_argument("Snapshots must have the same root and platform path spelling");
    if(!before.complete||!after.complete)throw std::invalid_argument("Comparison requires complete snapshots");
    std::vector<StorageChange> changes;
    for(const auto& [p,a]:before.entries) {
        auto i=after.entries.find(p);
        if(i==after.entries.end())changes.push_back({p,a.kind,a.logical_bytes,0,false,true});
        else if(a.logical_bytes!=i->second.logical_bytes||a.kind!=i->second.kind)changes.push_back({p,i->second.kind,a.logical_bytes,i->second.logical_bytes});
    }
    for(const auto& [p,b]:after.entries)if(!before.entries.contains(p))changes.push_back({p,b.kind,0,b.logical_bytes,true,false});
    std::sort(changes.begin(),changes.end(),[](const auto& a,const auto& b){
        auto magnitude=[](const auto& c){return c.after>=c.before?c.after-c.before:c.before-c.after;};
        return magnitude(a)!=magnitude(b)?magnitude(a)>magnitude(b):a.path<b.path;
    });
    return changes;
}
void write_comparison(std::ostream& out,const Snapshot& before,const Snapshot& after,const std::vector<StorageChange>& changes) {
    json j{{"schema","diskpilot.comparison"},{"version",1},{"root",path_utf8(before.root)},{"before_unix_ms",before.timestamp},{"after_unix_ms",after.timestamp},
        {"note","Path/size comparison only. Renames appear as removal plus addition; equal size does not establish unchanged content. Directory deltas overlap descendant deltas."},{"changes",json::array()}};
    for(const auto& c:changes)j["changes"].push_back({{"path",c.path},{"kind",c.kind},{"before",c.before},{"after",c.after},{"added",c.added},{"removed",c.removed}});
    dump(out,j);
}
} // namespace diskpilot
