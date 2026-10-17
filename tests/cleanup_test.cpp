#include "fixture.hpp"
#include "diskpilot/cleanup.hpp"
#include <algorithm>

namespace {
using CleanupTest=DiskFixture;
// Fake test adapter only; never recursively deletes the fixture's candidate.
class RecordingTrash:public diskpilot::TrashService {
  public:
    unsigned calls=0;bool fail=false;
    void move(const diskpilot::fs::path& p)override{
        ++calls;if(fail)throw std::runtime_error("Locked test candidate");
        diskpilot::fs::rename(p,p.parent_path()/(p.filename().string()+"-test-trash"));
    }
};
std::vector<std::size_t> first{0};
TEST_F(CleanupTest, BareDirectoryNamesAreNotEnough) {
    write("build/important","data");write("target/save","data");write("dist/save","data");write("node_modules/custom","data");
    EXPECT_TRUE(diskpilot::detect_cleanup(scan()).empty());
}
TEST_F(CleanupTest, ContextExplainsRebuildAndOfflineRisk) {
    write("project/package.json","{}");write("project/package-lock.json","{}");write("project/node_modules/package/data","data");
    auto candidates=diskpilot::detect_cleanup(scan());ASSERT_EQ(candidates.size(),1U);
    EXPECT_EQ(candidates[0].ecosystem,"JavaScript");EXPECT_EQ(candidates[0].confidence,diskpilot::Confidence::review_required);
    EXPECT_NE(candidates[0].rebuild.find("Network"),std::string::npos);
}
TEST_F(CleanupTest, BytecodePlanRequiresApprovalAndExactRevalidation) {
    write("project/__pycache__/module.pyc","bytecode");auto s=scan();auto candidates=diskpilot::detect_cleanup(s);
    ASSERT_EQ(candidates.size(),1U);EXPECT_EQ(candidates[0].confidence,diskpilot::Confidence::regenerable);
    auto plan=diskpilot::make_cleanup_plan(s,candidates,first);ASSERT_EQ(plan.items.size(),1U);
    RecordingTrash trash;EXPECT_THROW(diskpilot::execute_cleanup(plan,false,trash),std::runtime_error);EXPECT_EQ(trash.calls,0U);
    const auto outcomes=diskpilot::execute_cleanup(plan,true,trash);
    ASSERT_EQ(outcomes.size(),1U);EXPECT_TRUE(outcomes[0].moved_to_trash);EXPECT_EQ(trash.calls,1U);
    EXPECT_TRUE(diskpilot::fs::exists(root/"project/__pycache__-test-trash/module.pyc"));
}
TEST_F(CleanupTest, SourceCodeInsideGeneratedDirectoryIsProtected) {
    write("project/package.json","{}");write("project/dist/source.ts","source");auto s=scan();auto c=diskpilot::detect_cleanup(s);
    ASSERT_EQ(c.size(),1U);EXPECT_THROW(diskpilot::make_cleanup_plan(s,c,first),std::runtime_error);
}
TEST_F(CleanupTest, ChangedFileAfterPreviewRefusesTrash) {
    write("__pycache__/x.pyc","one");auto s=scan();auto c=diskpilot::detect_cleanup(s);auto plan=diskpilot::make_cleanup_plan(s,c,first);
    write("__pycache__/x.pyc","longer contents");RecordingTrash trash;
    auto results=diskpilot::execute_cleanup(plan,true,trash);ASSERT_EQ(results.size(),1U);EXPECT_FALSE(results[0].moved_to_trash);EXPECT_EQ(trash.calls,0U);
}
TEST_F(CleanupTest, ChangedContentsSinceScanRefusePlanEvenIfDirectoryTimeIsRestored) {
    write("__pycache__/x.pyc","one");auto s=scan();auto c=diskpilot::detect_cleanup(s);write("__pycache__/x.pyc","longer");
    EXPECT_THROW(diskpilot::make_cleanup_plan(s,c,first),std::runtime_error);
}
TEST_F(CleanupTest, PartialScanAndRepeatedSelectionAreRejected) {
    write("__pycache__/x.pyc","one");auto s=scan();auto c=diskpilot::detect_cleanup(s);
    const std::vector<std::size_t> repeated{0,0};EXPECT_THROW(diskpilot::make_cleanup_plan(s,c,repeated),std::runtime_error);
    s.complete=false;EXPECT_THROW(diskpilot::make_cleanup_plan(s,c,first),std::runtime_error);
}
TEST_F(CleanupTest, GitMetadataAndNestedLinksAreProtected) {
    write("__pycache__/x.pyc","one");write("__pycache__/.git/config","private");auto s=scan();auto c=diskpilot::detect_cleanup(s);
    EXPECT_THROW(diskpilot::make_cleanup_plan(s,c,first),std::runtime_error);
}
TEST_F(CleanupTest, ReplacementSymlinkPreventsExecution) {
    write("__pycache__/x.pyc","one");auto s=scan();auto c=diskpilot::detect_cleanup(s);auto plan=diskpilot::make_cleanup_plan(s,c,first);
    diskpilot::fs::rename(root/"__pycache__",root/"original");std::error_code ec;diskpilot::fs::create_directory_symlink(root/"original",root/"__pycache__",ec);if(ec)GTEST_SKIP()<<ec.message();
    RecordingTrash trash;auto results=diskpilot::execute_cleanup(plan,true,trash);ASSERT_EQ(results.size(),1U);EXPECT_FALSE(results[0].moved_to_trash);EXPECT_EQ(trash.calls,0U);
}
TEST_F(CleanupTest, PartialFailuresAreRecordedAndNativeAdapterFailsClosed) {
    write("__pycache__/x.pyc","one");auto s=scan();auto c=diskpilot::detect_cleanup(s);auto plan=diskpilot::make_cleanup_plan(s,c,first);
    RecordingTrash trash;trash.fail=true;auto outcomes=diskpilot::execute_cleanup(plan,true,trash);ASSERT_EQ(outcomes.size(),1U);EXPECT_FALSE(outcomes[0].moved_to_trash);
    diskpilot::NativeTrashService native;outcomes=diskpilot::execute_cleanup(plan,true,native);ASSERT_EQ(outcomes.size(),1U);EXPECT_FALSE(outcomes[0].moved_to_trash);EXPECT_TRUE(diskpilot::fs::exists(root/"__pycache__/x.pyc"));
}
}
