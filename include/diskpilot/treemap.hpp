#pragma once
#include "diskpilot/scanner.hpp"
#include <span>

namespace diskpilot {
struct Rect { double x = 0, y = 0, width = 0, height = 0; };
struct WeightedItem { std::size_t node = 0; std::uintmax_t bytes = 0; };
struct Tile { std::size_t node = 0; Rect bounds; };
// Squarified layout. Positive weights only, sorted by size then node ID.
// Invalid bounds/duplicate node IDs throw invalid_argument; zero weights are omitted.
[[nodiscard]] std::vector<Tile> squarify(std::span<const WeightedItem>, Rect);
// A bounded top-K view; aggregated small children retain no fabricated node ID.
struct TreemapView {
    std::vector<Tile> tiles;
    std::uintmax_t omitted_bytes = 0;
    std::size_t omitted_children = 0;
};
[[nodiscard]] TreemapView directory_treemap(const ScanResult&, std::size_t directory,
                                           Rect bounds, std::size_t max_tiles = 2000);
} // namespace diskpilot
