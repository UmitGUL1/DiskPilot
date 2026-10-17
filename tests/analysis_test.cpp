#include "diskpilot/analysis.hpp"
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <sstream>

namespace {
diskpilot::ScanResult fixture() {
    diskpilot::ScanResult scan;
    scan.root = "/sample";
    scan.complete = false;
    scan.progress.logical_bytes = 30;
    scan.progress.files = 3;
    scan.error_count = 1;
    scan.nodes = {{"/sample", diskpilot::no_parent, diskpilot::NodeKind::directory, 30},
                  {"/sample/b.TXT", 0, diskpilot::NodeKind::file, 10},
                  {"/sample/a.txt", 0, diskpilot::NodeKind::file, 10},
                  {"/sample/quote\"line\n.bin", 0, diskpilot::NodeKind::file, 10}};
    return scan;
}
TEST(Analysis, TiesUseDeterministicPathOrder) {
    const auto scan = fixture();
    diskpilot::AnalysisOptions options; options.top = 2;
    const auto result = diskpilot::analyze(scan, options);
    ASSERT_EQ(result.largest_files.size(), 2U);
    EXPECT_EQ(result.largest_files[0], 2U);
    EXPECT_EQ(result.largest_files[1], 1U);
    EXPECT_EQ(result.extensions.at(".txt").files, 2U);
    EXPECT_EQ(result.extensions.at(".txt").logical_bytes, 20U);
}
TEST(Analysis, FiltersDoNotChangeCategoryTotals) {
    const auto scan = fixture();
    diskpilot::AnalysisOptions options; options.extension = ".TXT"; options.name_contains = "b.";
    const auto result = diskpilot::analyze(scan, options);
    ASSERT_EQ(result.largest_files.size(), 1U); EXPECT_EQ(result.largest_files[0], 1U);
    EXPECT_EQ(result.extensions.at(".txt").files, 2U);
    options.minimum_bytes = 11;
    EXPECT_TRUE(diskpilot::analyze(scan, options).largest_files.empty());
    options.top = 0;
    EXPECT_TRUE(diskpilot::analyze(scan, options).largest_folders.empty());
}
TEST(Reports, JsonRoundTripsUntrustedNamesAndCompleteness) {
    const auto scan = fixture(); const auto analysis = diskpilot::analyze(scan);
    std::ostringstream output; diskpilot::write_json(output, scan, analysis);
    const auto json = nlohmann::json::parse(output.str());
    EXPECT_FALSE(json.at("complete").get<bool>());
    EXPECT_EQ(json.at("error_count"), 1);
    EXPECT_EQ(json.at("logical_bytes"), 30);
    EXPECT_EQ(json.at("largest_files")[2]["path"], "/sample/quote\"line\n.bin");
}
TEST(Analysis, FolderAndTimeFiltersUseMetadataAndPathComponents) {
    auto scan = fixture();
    scan.nodes[1].path = "/sample/sub/file";
    scan.nodes[1].modified_at_unix_ms = 500;
    scan.nodes[2].path = "/sample/submarine/file";
    scan.nodes[2].modified_at_unix_ms = 500;
    diskpilot::AnalysisOptions options; options.folder = "sub";
    options.modified_after_unix_ms = 400; options.modified_before_unix_ms = 600;
    const auto result = diskpilot::analyze(scan, options);
    ASSERT_EQ(result.largest_files.size(), 1U); EXPECT_EQ(result.largest_files[0], 1U);
    options.modified_after_unix_ms = 501;
    EXPECT_TRUE(diskpilot::analyze(scan, options).largest_files.empty());
}
TEST(Reports, CsvEscapesQuotesAndNewlines) {
    const auto scan = fixture(); const auto analysis = diskpilot::analyze(scan);
    std::ostringstream output; diskpilot::write_csv(output, scan, analysis);
    EXPECT_NE(output.str().find("\"/sample/quote\"\"line\n.bin\""), std::string::npos);
    EXPECT_NE(output.str().find(",false,false,1\r\n"), std::string::npos);
}
TEST(Reports, CsvNeutralizesSpreadsheetFormulaPaths) {
    auto scan = fixture(); scan.nodes[1].path = "=HYPERLINK(bad)";
    std::ostringstream output; diskpilot::write_csv(output, scan, diskpilot::analyze(scan));
    EXPECT_NE(output.str().find("\"'=HYPERLINK(bad)\""), std::string::npos);
}
TEST(Reports, FailedStreamIsReported) {
    const auto scan = fixture(); std::ostringstream output;
    output.setstate(std::ios::badbit);
    EXPECT_THROW(diskpilot::write_json(output, scan, diskpilot::analyze(scan)), std::runtime_error);
    EXPECT_THROW(diskpilot::write_csv(output, scan, diskpilot::analyze(scan)), std::runtime_error);
}
}
