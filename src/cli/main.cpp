#include "diskpilot/scanner.hpp"
#include "diskpilot/analysis.hpp"
#include "diskpilot/version.hpp"
#include "diskpilot/reports.hpp"
#include <fstream>
#include <sstream>
#include <csignal>
#include <charconv>
#include <iostream>
#include <exception>
#include <string>
#include <vector>

namespace {
volatile std::sig_atomic_t interrupted = 0;
void handle_interrupt(int) { interrupted = 1; }
std::string utf8(const diskpilot::fs::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
int run(const std::vector<diskpilot::fs::path>& args) {
    if (args.empty() || (args.size() == 1 && args[0] == "--help")) {
        std::cout << "DiskPilot - Understand your storage. Reclaim it safely.\n"
                     "Usage: diskpilot-cli [--help | --version]\n"
                     "       diskpilot-cli scan DIRECTORY [OPTIONS]\n"
                     "       diskpilot-cli largest DIRECTORY [OPTIONS]\n"
                     "       diskpilot-cli duplicates DIRECTORY [--format text|json|html]\n"
                     "       diskpilot-cli dev-cleanup DIRECTORY [--dry-run] [--format text|json|html]\n"
                     "       diskpilot-cli snapshot DIRECTORY --output NEW_FILE.json\n"
                     "       diskpilot-cli compare BEFORE.json AFTER.json\n"
                     "Options: --format text|json|csv|html --top N --min-size BYTES\n"
                     "         --extension .EXT --name TEXT --folder PATH\n"
                     "         --modified-after-ms UNIX_MS --modified-before-ms UNIX_MS\n\n"
                     "Read-only scan. Links/reparse points and mount crossings are skipped.\n"
                     "Root link ancestors are rejected. Ctrl+C requests cancellation.\n"
                     "Logical bytes count paths; unique allocation counts file identities once.\n"
                     "Exit codes: 0 complete, 1 partial/error, 2 usage, 130 cancelled.\n";
        return 0;
    }
    if (args.size() == 1 && args[0] == "--version") {
        std::cout << diskpilot::version() << '\n';
        return 0;
    }
    if (args[0] == "compare") {
        if(args.size()!=3) {std::cerr<<"compare requires two snapshot files.\n";return 2;}
        std::ifstream a(args[1],std::ios::binary),b(args[2],std::ios::binary);
        if(!a||!b)throw std::runtime_error("Cannot open snapshots");
        auto before=diskpilot::read_snapshot(a),after=diskpilot::read_snapshot(b);
        diskpilot::write_comparison(std::cout,before,after,diskpilot::compare(before,after));return 0;
    }
    if (args.size() < 2 || (args[0] != "scan" && args[0] != "largest" && args[0] != "duplicates" && args[0] != "dev-cleanup" && args[0] != "snapshot")) {
        std::cerr << "Unknown command or invalid arguments. Run diskpilot-cli --help.\n";
        return 2;
    }
    std::string format = "text";
    diskpilot::fs::path output;
    diskpilot::AnalysisOptions analysis_options;
    for (std::size_t i = 2; i < args.size(); i += 2) {
        if(args[i]=="--dry-run"&&args[0]=="dev-cleanup") {--i;continue;}
        if (i + 1 == args.size()) { std::cerr << "Option requires a value.\n"; return 2; }
        const auto flag = utf8(args[i]);
        const auto value = utf8(args[i + 1]);
        if (flag == "--format") format = value;
        else if(flag=="--output"&&args[0]=="snapshot") output=args[i+1];
        else if (flag == "--extension") analysis_options.extension = value;
        else if (flag == "--name") analysis_options.name_contains = value;
        else if (flag == "--folder") analysis_options.folder = args[i + 1];
        else if (flag == "--top" || flag == "--min-size" || flag == "--modified-after-ms" || flag == "--modified-before-ms") {
            std::uintmax_t number = 0;
            const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
            if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
                std::cerr << "Expected an unsigned integer for " << flag << '\n'; return 2;
            }
            if (flag == "--top") {
                if (number > 100'000) { std::cerr << "--top must be <= 100000\n"; return 2; }
                analysis_options.top = static_cast<std::size_t>(number);
            } else if (flag == "--min-size") analysis_options.minimum_bytes = number;
            else {
                if (number > static_cast<std::uintmax_t>(std::numeric_limits<std::int64_t>::max())) {
                    std::cerr << "Timestamp is out of range.\n"; return 2;
                }
                if (flag == "--modified-after-ms") analysis_options.modified_after_unix_ms = static_cast<std::int64_t>(number);
                else analysis_options.modified_before_unix_ms = static_cast<std::int64_t>(number);
            }
        } else { std::cerr << "Unknown option: " << flag << '\n'; return 2; }
    }
    if (format != "text" && format != "json" && format != "csv" && format != "html") {
        std::cerr << "Format must be text, json, csv or html.\n"; return 2;
    }
    if(format=="csv"&&(args[0]=="duplicates"||args[0]=="dev-cleanup")) {std::cerr<<"Use JSON or HTML for findings. CSV is a scan table.\n";return 2;}
    if(args[0]=="snapshot"&&output.empty()){std::cerr<<"snapshot requires --output NEW_FILE.json\n";return 2;}
    if (analysis_options.modified_after_unix_ms && analysis_options.modified_before_unix_ms &&
        *analysis_options.modified_after_unix_ms > *analysis_options.modified_before_unix_ms) {
        std::cerr << "Modification date range is reversed.\n"; return 2;
    }
    std::signal(SIGINT, handle_interrupt);
    std::stop_source cancellation;
    const diskpilot::FileSystemScanner scanner;
    const auto result = scanner.scan(args[1], {}, cancellation.get_token(),
        [&](const diskpilot::ScanProgress& progress) {
            if (interrupted) cancellation.request_stop();
            std::cerr << "\rScanned " << progress.files << " files, "
                      << progress.directories << " directories       " << std::flush;
        });
    std::cerr << '\n';
    const auto analysis = diskpilot::analyze(result, analysis_options);
    const int exit_code = result.cancelled ? 130 : result.complete ? 0 : 1;
    if(args[0]=="snapshot") {
        std::ostringstream data;diskpilot::write_snapshot(data,diskpilot::snapshot(result));diskpilot::write_new_file(output,data.str());
        std::cout<<"Snapshot saved: "<<utf8(output)<<'\n';return exit_code;
    }
    diskpilot::DuplicateResult duplicates;
    std::vector<diskpilot::CleanupCandidate> cleanup;
    const bool find_duplicates=args[0]=="duplicates",find_cleanup=args[0]=="dev-cleanup";
    if(find_duplicates)duplicates=diskpilot::DuplicateDetector{}.find(result,cancellation.get_token(),[&](const auto&){if(interrupted)cancellation.request_stop();});
    if(find_cleanup)cleanup=diskpilot::detect_cleanup(result);
    const int findings_code=find_duplicates?(duplicates.cancelled?130:duplicates.complete?0:1):exit_code;
    if(format=="json"||format=="html") {
        diskpilot::write_report(std::cout,result,analysis,format,find_duplicates?&duplicates:nullptr,find_cleanup?&cleanup:nullptr);return findings_code;
    }
    if (format == "csv") { diskpilot::write_csv(std::cout, result, analysis); return exit_code; }
    if(find_duplicates) {
        std::cout<<"Duplicate verification: "<<(duplicates.complete?"complete":"partial or cancelled")<<'\n';
        for(const auto& g:duplicates.groups) {std::cout<<"Byte-verified group: "<<g.bytes_per_file<<" bytes per copy; SHA-256 "<<g.sha256<<'\n';for(auto i:g.nodes)std::cout<<"  "<<utf8(result.nodes[i].path)<<'\n';}
        for(const auto& e:duplicates.errors)std::cerr<<utf8(e.path)<<": "<<e.message<<'\n';
        return findings_code;
    }
    if(find_cleanup) {
        std::cout<<"Read-only cleanup review. Nothing is selected or deleted.\n";
        for(const auto& c:cleanup)std::cout<<utf8(result.nodes[c.node].path)<<"\n  "<<c.ecosystem<<": "<<c.type<<" ("<<c.logical_bytes<<" logical bytes)\n  Evidence: "<<c.evidence<<"\n  Rebuild: "<<c.rebuild<<"\n  "<<c.warning<<'\n';
        return exit_code;
    }
    std::cout << "Target: " << utf8(result.root) << '\n'
              << "Status: " << (result.cancelled ? "cancelled" : result.complete ? "complete within policy" : "partial") << '\n'
              << "Files: " << result.progress.files << '\n'
              << "Directories (including root): " << result.progress.directories << '\n'
              << "Logical bytes: " << result.progress.logical_bytes << '\n';
    if (!result.nodes.empty()) {
        std::cout << "Unique file allocation bytes"
                  << (result.nodes[0].allocated_complete ? ": " : " (partial metadata): ")
                  << result.nodes[0].unique_allocated_bytes << '\n';
    }
    std::cout << "Skipped links/reparse points: " << result.progress.skipped_links << '\n'
              << "Skipped mounts: " << result.skipped_mounts << '\n'
              << "Errors: " << result.error_count << '\n'
              << "Access denied: " << result.access_denied_count << '\n'
              << "Entry limit reached: " << (result.limit_reached ? "yes" : "no") << '\n'
              << "Duration ms: " << result.duration.count() << '\n';
    for (const auto& error : result.errors)
        std::cerr << utf8(error.path) << ": " << error.operation << ": " << error.message << '\n';
    if (analysis.volume) {
        std::cout << "Volume capacity bytes: " << analysis.volume->capacity << '\n'
                  << "Volume free bytes: " << analysis.volume->free << '\n'
                  << "Volume available bytes: " << analysis.volume->available << '\n';
    }
    std::cout << "Largest files (logical bytes):\n";
    for (auto index : analysis.largest_files)
        std::cout << result.nodes[index].logical_bytes << '\t' << utf8(result.nodes[index].path) << '\n';
    std::cout << "Largest folders (logical bytes):\n";
    for (auto index : analysis.largest_folders)
        std::cout << result.nodes[index].logical_bytes << '\t' << utf8(result.nodes[index].path) << '\n';
    return exit_code;
}
template <typename Char> int entry(int argc, Char* argv[]) {
    try {
        std::vector<diskpilot::fs::path> args;
        for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
        return run(args);
    } catch (const std::exception& error) {
        std::cerr << "DiskPilot failed: " << error.what() << '\n';
        return 1;
    }
}
}
#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) { return entry(argc, argv); }
#else
int main(int argc, char* argv[]) { return entry(argc, argv); }
#endif
