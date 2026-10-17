#include "fixture.hpp"
#include "diskpilot/reports.hpp"
#include <nlohmann/json.hpp>
#include <sstream>

namespace {
using ReportTest=DiskFixture;
TEST_F(ReportTest, HtmlEscapesUntrustedPathsAndFindings) {
    write("a.txt","abc");auto s=scan();s.root=diskpilot::fs::path("<script>alert(1)</script>");
    std::vector<diskpilot::CleanupCandidate> c(1);c[0].node=0;c[0].evidence="<img src=x onerror=evil()>";
    std::ostringstream out;diskpilot::write_report(out,s,diskpilot::analyze(s),"html",nullptr,&c);
    EXPECT_EQ(out.str().find("<script>"),std::string::npos);EXPECT_EQ(out.str().find("<img "),std::string::npos);
    EXPECT_NE(out.str().find("&lt;img"),std::string::npos);EXPECT_NE(out.str().find("default-src 'none'"),std::string::npos);
}
TEST_F(ReportTest, JsonContainsCompletenessAndFindings) {
    write("__pycache__/x.pyc","abc");auto s=scan();auto c=diskpilot::detect_cleanup(s);auto d=diskpilot::DuplicateDetector{}.find(s);
    std::ostringstream out;diskpilot::write_report(out,s,diskpilot::analyze(s),"json",&d,&c);const auto j=nlohmann::json::parse(out.str());
    EXPECT_TRUE(j["complete"].get<bool>());EXPECT_EQ(j["cleanup_candidates"].size(),1U);EXPECT_TRUE(j["duplicates"]["complete"].get<bool>());
}
TEST_F(ReportTest, SnapshotRoundTripAndComparison) {
    write("a","abc");auto previous=diskpilot::snapshot(scan());std::ostringstream out;diskpilot::write_snapshot(out,previous);std::istringstream input(out.str());auto decoded=diskpilot::read_snapshot(input);
    EXPECT_EQ(decoded.root,previous.root);EXPECT_EQ(decoded.entries.size(),previous.entries.size());
    write("a","abcdef");write("b","new");auto next=diskpilot::snapshot(scan());const auto changes=diskpilot::compare(decoded,next);
    EXPECT_EQ(changes.size(),3U); // root aggregate, grown a, new b
    bool added=false;for(const auto& c:changes)if(c.path=="b"){added=c.added;EXPECT_EQ(c.after,3U);}EXPECT_TRUE(added);
}
TEST_F(ReportTest, PartialOrUnrelatedSnapshotsCannotBeCompared) {
    const auto a=diskpilot::snapshot(scan());auto b=a;b.complete=false;EXPECT_THROW(diskpilot::compare(a,b),std::invalid_argument);
    b=a;b.root/="elsewhere";EXPECT_THROW(diskpilot::compare(a,b),std::invalid_argument);
}
TEST_F(ReportTest, InvalidSnapshotsAndTraversalAreRejected) {
    auto s=diskpilot::snapshot(scan());std::ostringstream out;diskpilot::write_snapshot(out,s);auto j=nlohmann::json::parse(out.str());
    j["entries"][0]["path"]="../outside";std::istringstream bad(j.dump());EXPECT_THROW(diskpilot::read_snapshot(bad),std::runtime_error);
    j["entries"][0]["path"]=".";j["entries"][0]["logical_bytes"]=-1;std::istringstream negative(j.dump());EXPECT_THROW(diskpilot::read_snapshot(negative),std::runtime_error);
    j["entries"][0]["logical_bytes"]=0;j["entries"].push_back(j["entries"][0]);std::istringstream duplicate(j.dump());EXPECT_THROW(diskpilot::read_snapshot(duplicate),std::runtime_error);
}
TEST_F(ReportTest, SnapshotContainsNoFileContents) {
    write("secret","THIS_IS_PRIVATE_CONTENT");std::ostringstream out;diskpilot::write_snapshot(out,diskpilot::snapshot(scan()));
    EXPECT_EQ(out.str().find("THIS_IS_PRIVATE_CONTENT"),std::string::npos);
}
}
