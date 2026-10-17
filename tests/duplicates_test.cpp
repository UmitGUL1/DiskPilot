#include "fixture.hpp"
#include "diskpilot/duplicates.hpp"
#include "diskpilot/file_access.hpp"
#include <sstream>

namespace {
using DuplicateTest=DiskFixture;
std::string hash(const std::string& value) {
    diskpilot::Sha256 h;h.update(std::as_bytes(std::span(value.data(),value.size())));return h.finish();
}
TEST(Sha256Test, PublishedVectorsAndIncrementalBoundaries) {
    EXPECT_EQ(hash(""),"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(hash("abc"),"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    const std::string text(1'000'000,'a');
    EXPECT_EQ(hash(text),"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    diskpilot::Sha256 incremental;
    for(char c:text)incremental.update(std::as_bytes(std::span(&c,1)));
    EXPECT_EQ(incremental.finish(),hash(text));
    EXPECT_THROW(incremental.finish(),std::logic_error);
}
TEST_F(DuplicateTest, ConfirmsOnlyDistinctIdenticalFiles) {
    write("a","same");write("b","same");write("c","else");write("empty1","");write("empty2","");
    const auto result=diskpilot::DuplicateDetector{}.find(scan());
    ASSERT_TRUE(result.complete);ASSERT_EQ(result.groups.size(),2U);
    EXPECT_EQ(result.groups[0].bytes_per_file,0U);EXPECT_EQ(result.groups[0].nodes.size(),2U);
    EXPECT_EQ(result.groups[1].bytes_per_file,4U);EXPECT_EQ(result.groups[1].nodes.size(),2U);
    EXPECT_EQ(result.groups[1].verification,"byte_verified");
}
TEST_F(DuplicateTest, SamePartialContentDifferentMiddleIsNotDuplicate) {
    std::string a(20000,'a'),b=a;b[10000]='b';
    write("a",a);write("b",b);
    const auto result=diskpilot::DuplicateDetector{}.find(scan());
    EXPECT_TRUE(result.complete);EXPECT_TRUE(result.groups.empty());EXPECT_EQ(result.progress.files_hashed,2U);
}
TEST_F(DuplicateTest, StreamsBeyondBufferBoundaries) {
    const std::string value(200003,'z');write("a",value);write("b",value);
    const auto result=diskpilot::DuplicateDetector{}.find(scan());
    ASSERT_TRUE(result.complete);ASSERT_EQ(result.groups.size(),1U);
    EXPECT_EQ(result.groups[0].sha256,hash(value));EXPECT_GE(result.progress.bytes_read,value.size()*4);
}
TEST_F(DuplicateTest, HardLinksAreNotPhysicalDuplicateCopies) {
    write("a","same");std::error_code ec;diskpilot::fs::create_hard_link(root/"a",root/"link",ec);
    if(ec)GTEST_SKIP()<<ec.message();
    const auto result=diskpilot::DuplicateDetector{}.find(scan());
    EXPECT_TRUE(result.complete);EXPECT_TRUE(result.groups.empty());
}
TEST_F(DuplicateTest, ChangedSinceScanIsExcludedAndReported) {
    write("a","same");write("b","same");auto s=scan();write("a","different length");
    const auto result=diskpilot::DuplicateDetector{}.find(s);
    EXPECT_FALSE(result.complete);EXPECT_GT(result.error_count,0U);EXPECT_TRUE(result.groups.empty());
}
TEST_F(DuplicateTest, RemovedCandidateIsNotConfirmed) {
    write("a","same");write("b","same");const auto s=scan();diskpilot::fs::remove(root/"a");
    const auto result=diskpilot::DuplicateDetector{}.find(s);
    EXPECT_FALSE(result.complete);EXPECT_GT(result.error_count,0U);EXPECT_TRUE(result.groups.empty());
}
TEST_F(DuplicateTest, LinkReplacementIsRefused) {
    write("a","same");write("b","same");auto s=scan();diskpilot::fs::remove(root/"b");
    std::error_code ec;diskpilot::fs::create_symlink(root/"a",root/"b",ec);if(ec)GTEST_SKIP()<<ec.message();
    const auto result=diskpilot::DuplicateDetector{}.find(s);
    EXPECT_FALSE(result.complete);EXPECT_TRUE(result.groups.empty());
}
TEST_F(DuplicateTest, CancellationClearsGroupsAndReportsIncomplete) {
    write("a",std::string(200000,'a'));write("b",std::string(200000,'a'));
    std::stop_source source;const auto result=diskpilot::DuplicateDetector{}.find(scan(),source.get_token(),[&](const auto&){source.request_stop();});
    EXPECT_TRUE(result.cancelled);EXPECT_FALSE(result.complete);EXPECT_TRUE(result.groups.empty());
}
TEST_F(DuplicateTest, IncompleteScanCannotYieldCompleteDuplicateResult) {
    auto s=scan();s.complete=false;
    EXPECT_FALSE(diskpilot::DuplicateDetector{}.find(s).complete);
}
TEST_F(DuplicateTest, ExclusiveExportDoesNotOverwriteFiles) {
    write("report","original");
    EXPECT_THROW(diskpilot::write_new_file(root/"report","replacement"),std::runtime_error);
    std::ifstream in(root/"report");std::string text;in>>text;EXPECT_EQ(text,"original");
    EXPECT_NO_THROW(diskpilot::write_new_file(root/"new-report","new"));
}
}
