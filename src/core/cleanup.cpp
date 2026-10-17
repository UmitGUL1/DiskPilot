#include "diskpilot/cleanup.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <unordered_set>

namespace diskpilot {
namespace {
std::string lower(std::string s) {for(auto& c:s)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return s;}
bool source(const fs::path& p) {
    static const std::set<std::string> extensions{".c",".cpp",".cxx",".cc",".h",".hpp",".py",".js",".ts",".tsx",".jsx",".rs",".java",".kt",".qml",".cs",".go",".swift",".rb",".php"};
    return extensions.contains(lower(path_utf8(p.extension())));
}
bool protected_path(const fs::path& p) {
    // Apply component checks so nested .git and Windows critical directories are
    // protected regardless of casing. No substring or lexical-prefix matching.
    static const std::set<std::string> names{".git",".svn",".hg","windows","program files","program files (x86)","programdata","system volume information","$recycle.bin",".trash",".local",".ssh",".gnupg"};
    for(const auto& c:p) if(names.contains(lower(path_utf8(c)))) return true;
#ifndef _WIN32
    for(const auto* root:{"/bin","/boot","/dev","/etc","/lib","/lib64","/proc","/root","/run","/sbin","/sys","/usr","/var"})
        if(p==root||path_below(p,root)) return true;
#endif
    return p==p.root_path();
}
std::vector<PlanEntry> inventory(const fs::path& root, std::stop_token stop = {}) {
    if(!link_free_path(root)||protected_path(root)) throw std::runtime_error("Protected or linked path");
    ScanOptions options;options.max_entries=100000;
    auto s=FileSystemScanner{}.scan(root,options,stop);
    if(!s.complete||s.progress.skipped_links||s.skipped_mounts) throw std::runtime_error("Tree cannot be completely revalidated");
    std::vector<PlanEntry> out;out.reserve(s.nodes.size());
    for(const auto& n:s.nodes) {
        if(stop.stop_requested())throw std::runtime_error("Cleanup review cancelled");
        if(n.kind!=NodeKind::directory&&n.kind!=NodeKind::file) throw std::runtime_error("Special filesystem object in candidate");
        if(protected_path(n.path)||source(n.path)) throw std::runtime_error("Source code or protected metadata in candidate");
        if(n.kind==NodeKind::file) {
            std::error_code ec;const auto count=fs::hard_link_count(n.path,ec);
            if(ec||count!=1) throw std::runtime_error("Cannot safely plan a hard-linked file");
        }
        out.push_back({n.path.lexically_relative(s.root),file_stamp(n.path)});
    }
    std::sort(out.begin(),out.end(),[](const auto& a,const auto& b){return a.relative.native()<b.relative.native();});
    return out;
}
}
std::vector<CleanupCandidate> detect_cleanup(const ScanResult& scan) {
    std::vector<CleanupCandidate> out;
    // Context is derived solely from this scan, never by executing manifests.
    std::unordered_set<fs::path> files;
    std::vector<std::pair<bool,bool>> bytecode_children(scan.nodes.size());
    for(const auto& n:scan.nodes) if(n.kind==NodeKind::file) files.insert(n.path);
    for(const auto& n:scan.nodes) if(n.parent<scan.nodes.size()) {
        const bool bytecode=n.kind==NodeKind::file&&n.path.extension()==".pyc";
        bytecode_children[n.parent].first|=bytecode;
        bytecode_children[n.parent].second|=!bytecode;
    }
    auto has=[&](const fs::path& p,const char* name){return files.contains(p/name);};
    for(std::size_t i=1;i<scan.nodes.size();++i) {
        const auto& n=scan.nodes[i];if(n.kind!=NodeKind::directory) continue;
        const auto name=path_utf8(n.path.filename());
        const auto parent=n.path.parent_path();
        CleanupCandidate c;c.node=i;c.project=parent;c.logical_bytes=n.logical_bytes;
        if(name=="__pycache__") {
            const auto [bytecode,unexpected]=bytecode_children[i];
            if(!bytecode) continue;
            c.ecosystem="Python";c.type="Python bytecode";c.evidence="Contains .pyc files";
            c.rebuild="Python regenerates bytecode when modules are imported.";
            if(!unexpected) c.confidence=Confidence::regenerable;
        } else if((name==".pytest_cache"||name==".mypy_cache"||name==".ruff_cache")&&(has(parent,"pyproject.toml")||has(parent,"setup.py")||has(parent,"requirements.txt"))) {
            c.ecosystem="Python";c.type="Tool cache";c.evidence="Python project manifest beside recognized cache directory";
            c.rebuild="Tool recreates cache on the next run; analysis may be slower.";
        } else if(name=="node_modules"&&has(parent,"package.json")&&(has(parent,"package-lock.json")||has(parent,"pnpm-lock.yaml")||has(parent,"yarn.lock"))) {
            c.ecosystem="JavaScript";c.type="Installed dependencies";c.evidence="Project package.json and dependency lock file";
            c.rebuild="Reinstall dependencies using the project's package manager. Network access may be required; local package edits would be lost.";
        } else if((name=="dist"||name=="build"||name==".turbo")&&has(parent,"package.json")) {
            c.ecosystem="JavaScript";c.type="Possible generated output";c.evidence="package.json in parent; directory name is only a hint";
            c.rebuild="Review package scripts manually. Output may contain unique user data.";
        } else if(name=="cache"&&parent.filename()==".next"&&has(parent.parent_path(),"package.json")) {
            c.project=parent.parent_path();c.ecosystem="JavaScript";c.type="Next.js cache";c.evidence=".next/cache within a package.json project";
            c.rebuild="Next build regenerates cache; may require dependencies and network.";
        } else if(has(n.path,"CMakeCache.txt")&&has(parent,"CMakeLists.txt")) {
            c.ecosystem="C/C++";c.type="CMake build tree";c.evidence="CMakeCache.txt inside directory; CMakeLists.txt in parent";
            c.rebuild="Configure and compile again. Review custom outputs, generated sources and local cache settings.";
        } else if(name=="target"&&has(parent,"Cargo.toml")&&has(n.path,".rustc_info.json")) {
            c.ecosystem="Rust";c.type="Cargo target";c.evidence="Cargo.toml and target/.rustc_info.json";
            c.rebuild="Cargo recompiles; toolchains and dependencies must remain available.";
        } else if(name=="target"&&has(parent,"pom.xml")) {
            c.ecosystem="Java";c.type="Maven target";c.evidence="pom.xml in parent; generated output requires review";
            c.rebuild="Maven rebuild; dependencies may require network. Review generated sources.";
        } else if(name=="build"&&(has(parent,"build.gradle")||has(parent,"build.gradle.kts"))) {
            c.ecosystem="Gradle / Android";c.type="Gradle build outputs";c.evidence="Gradle project manifest in parent";
            c.rebuild="Gradle rebuild; Android SDK and downloaded dependencies may be needed.";
        } else continue;
        if(!scan.complete||protected_path(n.path)) {c.confidence=Confidence::protected_path;c.warning="Protected location or incomplete scan; execution prohibited.";}
        out.push_back(std::move(c));
    }
    return out;
}
CleanupPlan make_cleanup_plan(const ScanResult& scan,const std::vector<CleanupCandidate>& candidates,std::span<const std::size_t> selected,std::stop_token stop) {
    if(!scan.complete||scan.cancelled||scan.root.empty()) throw std::runtime_error("A complete scan is required");
    CleanupPlan p;p.root=scan.root;
    std::set<std::size_t> seen;
    for(auto index:selected) {
        if(stop.stop_requested())throw std::runtime_error("Cleanup review cancelled");
        if(index>=candidates.size()||!seen.insert(index).second) throw std::runtime_error("Invalid or repeated selection");
        const auto& c=candidates[index];
        if(c.node>=scan.nodes.size()||c.confidence==Confidence::protected_path) throw std::runtime_error("Protected candidate");
        const auto& n=scan.nodes[c.node];
        if(!path_below(n.path,scan.root)||protected_path(n.path)) throw std::runtime_error("Candidate escapes scan root or is protected");
        for(const auto& item:p.items) if(item.path==n.path||path_below(n.path,item.path)||path_below(item.path,n.path)) throw std::runtime_error("Overlapping cleanup selections");
        const auto now=file_stamp(n.path);
        if(now.kind!=NodeKind::directory||now.identity!=n.identity||!n.modified_at_unix_ms||now.modified!=*n.modified_at_unix_ms) throw std::runtime_error("Candidate changed since scan");
        auto manifest=inventory(n.path,stop);
        // Compare every scanned descendant, not only the parent directory mtime.
        std::vector<PlanEntry> expected;
        for(const auto& child:scan.nodes) if(child.path==n.path||path_below(child.path,n.path)) {
            if(!child.modified_at_unix_ms) throw std::runtime_error("Missing metadata");
            expected.push_back({child.path.lexically_relative(n.path),{child.kind,child.identity,child.kind==NodeKind::file?child.logical_bytes:0,*child.modified_at_unix_ms}});
        }
        for(auto& entry:manifest) if(entry.stamp.kind==NodeKind::directory) entry.stamp.size=0;
        std::sort(expected.begin(),expected.end(),[](const auto& a,const auto& b){return a.relative.native()<b.relative.native();});
        if(manifest!=expected) throw std::runtime_error("Candidate contents changed since scan");
        if(c.logical_bytes>std::numeric_limits<std::uintmax_t>::max()-p.logical_bytes) throw std::overflow_error("Plan size overflow");
        p.logical_bytes+=c.logical_bytes;p.items.push_back({c,n.path,std::move(manifest)});
    }
    return p;
}
void NativeTrashService::move(const fs::path&) {
    // Path-based shell deletion can race replacement after revalidation. Until a
    // native handle-bound trash adapter is validated, refuse the operation.
    throw std::runtime_error("Safe native trash adapter is not yet validated. No files were moved or permanently deleted.");
}
std::vector<CleanupOutcome> execute_cleanup(const CleanupPlan& plan,bool approved,TrashService& trash,std::stop_token stop) {
    if(!approved) throw std::runtime_error("Explicit approval of this plan is required");
    std::vector<CleanupOutcome> out;
    for(const auto& item:plan.items) {
        if(stop.stop_requested()) break;
        try {
            if(!path_below(item.path,plan.root)||protected_path(item.path)) throw std::runtime_error("Invalid cleanup boundary");
            auto current=inventory(item.path,stop);
            for(auto& e:current) if(e.stamp.kind==NodeKind::directory)e.stamp.size=0;
            if(current!=item.manifest) throw std::runtime_error("Contents changed since preview; create a new plan");
            trash.move(item.path);
            std::error_code ec;auto status=fs::symlink_status(item.path,ec);
            if((ec&&ec!=std::errc::no_such_file_or_directory)||fs::exists(status)) throw std::runtime_error("Trash operation could not be verified");
            out.push_back({item.path,true,"Moved to trash. Free disk space may not increase until trash is emptied."});
        } catch(const std::exception& e) {out.push_back({item.path,false,e.what()});}
    }
    return out;
}
} // namespace diskpilot
