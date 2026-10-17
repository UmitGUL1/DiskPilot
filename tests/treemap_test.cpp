#include "diskpilot/treemap.hpp"
#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
#include <stdexcept>

TEST(Treemap, ConservesAreaAndWeightRatios) {
    const std::vector<diskpilot::WeightedItem> items{{0, 60}, {1, 30}, {2, 10}};
    const auto tiles = diskpilot::squarify(items, {10, 20, 100, 80});
    ASSERT_EQ(tiles.size(), 3U);
    double area = 0;
    for (const auto& tile : tiles) {
        const auto& r = tile.bounds;
        EXPECT_GE(r.width, 0); EXPECT_GE(r.height, 0);
        EXPECT_GE(r.x, 10); EXPECT_GE(r.y, 20);
        EXPECT_LE(r.x + r.width, 110.000001); EXPECT_LE(r.y + r.height, 100.000001);
        EXPECT_NEAR(r.width * r.height, 8000.0 * static_cast<double>(items[tile.node].bytes) / 100, 0.00001);
        area += r.width * r.height;
    }
    EXPECT_NEAR(area, 8000, 0.00001);
}
TEST(Treemap, TilesDoNotOverlap) {
    std::vector<diskpilot::WeightedItem> items;
    for (std::size_t i = 0; i < 100; ++i) items.push_back({i, i + 1});
    const auto tiles = diskpilot::squarify(items, {0, 0, 700, 400});
    for (std::size_t i = 0; i < tiles.size(); ++i)
        for (auto j = i + 1; j < tiles.size(); ++j) {
            const auto& a = tiles[i].bounds; const auto& b = tiles[j].bounds;
            const auto width = std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x);
            const auto height = std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y);
            EXPECT_TRUE(width <= 0.000001 || height <= 0.000001);
        }
}
TEST(Treemap, StableOrderingAndZeroHandling) {
    const std::vector<diskpilot::WeightedItem> a{{2, 1}, {1, 1}, {3, 0}};
    const std::vector<diskpilot::WeightedItem> b{{1, 1}, {3, 0}, {2, 1}};
    const auto x = diskpilot::squarify(a, {0, 0, 100, 100});
    const auto y = diskpilot::squarify(b, {0, 0, 100, 100});
    ASSERT_EQ(x.size(), 2U); ASSERT_EQ(y.size(), 2U);
    for (std::size_t i = 0; i < x.size(); ++i) {
        EXPECT_EQ(x[i].node, y[i].node); EXPECT_DOUBLE_EQ(x[i].bounds.x, y[i].bounds.x);
        EXPECT_DOUBLE_EQ(x[i].bounds.y, y[i].bounds.y);
    }
    EXPECT_TRUE(diskpilot::squarify(a, {0, 0, 0, 100}).empty());
}
TEST(Treemap, RejectsInvalidGeometryAndDuplicateIds) {
    const std::vector<diskpilot::WeightedItem> duplicate{{1, 2}, {1, 3}};
    EXPECT_THROW(diskpilot::squarify(duplicate, {0, 0, 1, 1}), std::invalid_argument);
    EXPECT_THROW(diskpilot::squarify({}, {0, 0, -1, 1}), std::invalid_argument);
    EXPECT_THROW(diskpilot::squarify({}, {0, 0, INFINITY, 1}), std::invalid_argument);
}
TEST(Treemap, BoundedViewPreservesAreaOfOmittedChildren) {
    diskpilot::ScanResult scan;
    scan.nodes = {{"root", diskpilot::no_parent, diskpilot::NodeKind::directory, 100},
                  {"root/a", 0, diskpilot::NodeKind::file, 60},
                  {"root/b", 0, diskpilot::NodeKind::file, 40}};
    const auto view = diskpilot::directory_treemap(scan, 0, {0, 0, 100, 100}, 1);
    ASSERT_EQ(view.tiles.size(), 1U);
    EXPECT_EQ(view.omitted_bytes, 40U); EXPECT_EQ(view.omitted_children, 1U);
    EXPECT_NEAR(view.tiles[0].bounds.width * view.tiles[0].bounds.height, 6000, 0.0001);
    EXPECT_THROW(diskpilot::directory_treemap(scan, 1, {0, 0, 100, 100}), std::invalid_argument);
}
