#include "diskpilot/scanner.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <fstream>

namespace {
namespace fs = std::filesystem;
class ScannerTest : public ::testing::Test {
  protected:
    fs::path root;
    diskpilot::FileSystemScanner scanner;
    void SetUp() override {
        static std::atomic<unsigned long> sequence{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = fs::temp_directory_path() / ("diskpilot-test-" + std::to_string(stamp) + "-" + std::to_string(sequence++));
        ASSERT_TRUE(fs::create_directory(root));
    }
    void TearDown() override {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    void write(const fs::path& relative, const std::string& data) {
        fs::create_directories((root / relative).parent_path());
        std::ofstream stream(root / relative, std::ios::binary);
        stream << data;
        stream.close();
        ASSERT_TRUE(stream.good());
    }
};
TEST_F(ScannerTest, EmptyDirectoryIsComplete) {
    const auto scan = scanner.scan(root);
    EXPECT_TRUE(scan.complete);
    ASSERT_EQ(scan.nodes.size(), 1U);
    EXPECT_EQ(scan.progress.directories, 1U);
    EXPECT_EQ(scan.nodes[0].logical_bytes, 0U);
}
TEST_F(ScannerTest, AggregatesNestedAndEmptyFiles) {
    write("a.txt", "abc"); write("nested/b.bin", "12345"); write("nested/empty", "");
    const auto scan = scanner.scan(root);
    ASSERT_TRUE(scan.complete);
    EXPECT_EQ(scan.progress.files, 3U);
    EXPECT_EQ(scan.progress.logical_bytes, 8U);
    EXPECT_EQ(scan.nodes[0].logical_bytes, 8U);
    EXPECT_EQ(scan.nodes[0].file_count, 3U);
    EXPECT_EQ(scan.progress.directories, 2U);
}
TEST_F(ScannerTest, SupportsUnicodeNames) {
    write(fs::path(std::u8string(u8"ünicode-测试")) / fs::path(std::u8string(u8"ş.txt")), "data");
    const auto scan = scanner.scan(root);
    EXPECT_TRUE(scan.complete);
    EXPECT_EQ(scan.progress.logical_bytes, 4U);
}
TEST_F(ScannerTest, MissingRootIsPartial) {
    const auto scan = scanner.scan(root / "does-not-exist");
    EXPECT_FALSE(scan.complete); EXPECT_GT(scan.error_count, 0U); EXPECT_TRUE(scan.nodes.empty());
}
TEST_F(ScannerTest, FileAndEmptyRootsAreRejected) {
    write("file", "x");
    EXPECT_FALSE(scanner.scan(root / "file").complete);
    EXPECT_FALSE(scanner.scan({}).complete);
    EXPECT_FALSE(scanner.scan(root / "nested/..").complete);
}
TEST_F(ScannerTest, PreCancelledScanIsNotComplete) {
    std::stop_source stop; stop.request_stop();
    const auto scan = scanner.scan(root, {}, stop.get_token());
    EXPECT_TRUE(scan.cancelled); EXPECT_FALSE(scan.complete); EXPECT_EQ(scan.progress.files, 0U);
}
TEST_F(ScannerTest, ProgressCanCancelTraversal) {
    for (int i = 0; i < 10; ++i) write(std::to_string(i), "x");
    diskpilot::ScanOptions options; options.progress_interval = std::chrono::milliseconds(0);
    std::stop_source stop;
    const auto scan = scanner.scan(root, options, stop.get_token(), [&](const auto&) { stop.request_stop(); });
    EXPECT_TRUE(scan.cancelled); EXPECT_FALSE(scan.complete); EXPECT_LT(scan.progress.files, 10U);
}
TEST_F(ScannerTest, EntryLimitReportsPartialResults) {
    write("a", "x"); write("b", "yy");
    diskpilot::ScanOptions options; options.max_entries = 2;
    const auto scan = scanner.scan(root, options);
    EXPECT_FALSE(scan.complete); EXPECT_TRUE(scan.limit_reached); EXPECT_EQ(scan.nodes.size(), 2U);
}
TEST_F(ScannerTest, HardLinksCountLogicalPathsButPhysicalIdentityOnce) {
    write("original", "same bytes");
    std::error_code error; fs::create_hard_link(root / "original", root / "copy", error);
    if (error) GTEST_SKIP() << error.message();
    const auto scan = scanner.scan(root);
    ASSERT_TRUE(scan.complete);
    EXPECT_EQ(scan.progress.files, 2U); EXPECT_EQ(scan.progress.logical_bytes, 20U);
    unsigned repeats = 0; std::uintmax_t allocation = 0;
    for (const auto& node : scan.nodes) {
        if (node.kind != diskpilot::NodeKind::file) continue;
        repeats += node.repeated_identity ? 1U : 0U;
        allocation += node.unique_allocated_bytes;
    }
    EXPECT_EQ(repeats, 1U); EXPECT_EQ(scan.nodes[0].unique_allocated_bytes, allocation);
}
TEST_F(ScannerTest, DirectoryLinksAreSkippedIncludingLoops) {
    write("folder/data", "abc");
    std::error_code error; fs::create_directory_symlink(root, root / "folder/loop", error);
    if (error) GTEST_SKIP() << error.message();
    const auto scan = scanner.scan(root);
    EXPECT_TRUE(scan.complete); EXPECT_EQ(scan.progress.files, 1U);
    EXPECT_EQ(scan.progress.skipped_links, 1U); EXPECT_EQ(scan.progress.logical_bytes, 3U);
}
TEST_F(ScannerTest, RootWithLinkAncestorIsRejected) {
    fs::create_directories(root / "real/child");
    std::error_code error; fs::create_directory_symlink(root / "real", root / "alias", error);
    if (error) GTEST_SKIP() << error.message();
    EXPECT_FALSE(scanner.scan(root / "alias/child").complete);
}
TEST_F(ScannerTest, DeepTreeUsesIterativeTraversal) {
    fs::path path; for (int i = 0; i < 40; ++i) path /= "d";
    write(path / "leaf", "z");
    const auto scan = scanner.scan(root);
    EXPECT_TRUE(scan.complete); EXPECT_EQ(scan.progress.directories, 41U);
    EXPECT_EQ(scan.nodes[0].logical_bytes, 1U);
}
TEST_F(ScannerTest, DisappearingDirectoryIsReported) {
    write("folder/data", "abc");
    diskpilot::ScanOptions options; options.progress_interval = std::chrono::milliseconds(0);
    bool removed = false;
    const auto scan = scanner.scan(root, options, {}, [&](const auto&) {
        if (!removed) { fs::remove_all(root / "folder"); removed = true; }
    });
    EXPECT_FALSE(scan.complete); EXPECT_GT(scan.error_count, 0U);
}
#ifndef _WIN32
TEST_F(ScannerTest, PermissionDenialIsReportedOrSkippedForPrivilegedRunner) {
    fs::create_directory(root / "private");
    fs::permissions(root / "private", fs::perms::none);
    const auto scan = scanner.scan(root);
    fs::permissions(root / "private", fs::perms::owner_all);
    if (scan.complete) GTEST_SKIP() << "Runner can traverse mode-000 directories";
    EXPECT_GT(scan.access_denied_count, 0U);
}
#endif
}
