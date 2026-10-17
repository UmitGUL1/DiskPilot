#include "diskpilot/treemap.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace diskpilot {
namespace {
struct Area { std::size_t node; double value; };
long double worst(long double sum, long double minimum, long double maximum, long double side) {
    if (sum <= 0 || minimum <= 0 || side <= 0) return std::numeric_limits<double>::infinity();
    const auto s2 = sum * sum;
    const auto w2 = side * side;
    return std::max(w2 * maximum / s2, s2 / (w2 * minimum));
}
void validate(Rect bounds) {
    if (!std::isfinite(bounds.x) || !std::isfinite(bounds.y) || !std::isfinite(bounds.width) ||
        !std::isfinite(bounds.height) || bounds.width < 0 || bounds.height < 0 ||
        !std::isfinite(bounds.width * bounds.height) ||
        !std::isfinite(bounds.x + bounds.width) || !std::isfinite(bounds.y + bounds.height))
        throw std::invalid_argument("Treemap bounds must be finite and nonnegative");
}
}

std::vector<Tile> squarify(std::span<const WeightedItem> input, Rect bounds) {
    validate(bounds);
    std::vector<WeightedItem> items;
    std::unordered_set<std::size_t> ids;
    long double total = 0;
    for (const auto& item : input) {
        if (!ids.insert(item.node).second) throw std::invalid_argument("Duplicate treemap node ID");
        if (item.bytes != 0) { items.push_back(item); total += item.bytes; }
    }
    std::vector<Tile> result;
    if (items.empty() || bounds.width * bounds.height == 0) return result;
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        return a.bytes != b.bytes ? a.bytes > b.bytes : a.node < b.node;
    });
    std::vector<Area> areas;
    for (const auto& item : items)
        areas.push_back({item.node, static_cast<double>((static_cast<long double>(item.bytes) / total) * bounds.width * bounds.height)});
    result.reserve(areas.size());
    for (std::size_t start = 0; start < areas.size();) {
        if (bounds.width <= 0 || bounds.height <= 0) break;
        const double side = std::min(bounds.width, bounds.height);
        double sum = areas[start].value;
        const double maximum = sum;
        double minimum = sum;
        auto end = start + 1;
        while (end < areas.size()) {
            const double next = areas[end].value;
            if (worst(sum + next, next, maximum, side) > worst(sum, minimum, maximum, side)) break;
            sum += next; minimum = next; ++end;
        }
        if (bounds.width >= bounds.height) {
            const double width = end == areas.size() ? bounds.width : std::min(bounds.width, sum / bounds.height);
            double y = bounds.y;
            for (auto i = start; i < end; ++i) {
                const double height = i + 1 == end ? std::max(0.0, bounds.y + bounds.height - y)
                                                  : std::min(std::max(0.0, bounds.y + bounds.height - y), areas[i].value / width);
                result.push_back({areas[i].node, {bounds.x, y, width, height}}); y += height;
            }
            bounds.x += width; bounds.width = std::max(0.0, bounds.width - width);
        } else {
            const double height = end == areas.size() ? bounds.height : std::min(bounds.height, sum / bounds.width);
            double x = bounds.x;
            for (auto i = start; i < end; ++i) {
                const double width = i + 1 == end ? std::max(0.0, bounds.x + bounds.width - x)
                                                 : std::min(std::max(0.0, bounds.x + bounds.width - x), areas[i].value / height);
                result.push_back({areas[i].node, {x, bounds.y, width, height}}); x += width;
            }
            bounds.y += height; bounds.height = std::max(0.0, bounds.height - height);
        }
        start = end;
    }
    return result;
}

TreemapView directory_treemap(const ScanResult& scan, std::size_t directory, Rect bounds, std::size_t max_tiles) {
    validate(bounds);
    if (directory >= scan.nodes.size() || scan.nodes[directory].kind != NodeKind::directory)
        throw std::invalid_argument("Treemap parent must be a directory");
    std::vector<WeightedItem> items;
    for (std::size_t i = 0; i < scan.nodes.size(); ++i)
        if (scan.nodes[i].parent == directory && scan.nodes[i].logical_bytes != 0)
            items.push_back({i, scan.nodes[i].logical_bytes});
    const auto count = std::min(max_tiles, items.size());
    std::partial_sort(items.begin(), items.begin() + static_cast<std::ptrdiff_t>(count), items.end(),
        [](const auto& a, const auto& b) { return a.bytes != b.bytes ? a.bytes > b.bytes : a.node < b.node; });
    TreemapView view;
    long double all = 0, kept = 0;
    for (std::size_t i = 0; i < items.size(); ++i) {
        all += items[i].bytes;
        if (i < count) kept += items[i].bytes;
        else {
            if (items[i].bytes > std::numeric_limits<std::uintmax_t>::max() - view.omitted_bytes)
                throw std::overflow_error("Treemap omitted size overflow");
            view.omitted_bytes += items[i].bytes;
            ++view.omitted_children;
        }
    }
    // Reserve an empty region for omitted children so visible area stays proportional
    // to the full parent. A renderer may label this region "Other children".
    if (all > 0) bounds.width *= static_cast<double>(kept / all);
    items.resize(count);
    view.tiles = squarify(items, bounds);
    return view;
}
} // namespace diskpilot
