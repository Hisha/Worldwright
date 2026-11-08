#ifndef WORLDWRIGHT_WORLD_DEFINITION_H
#define WORLDWRIGHT_WORLD_DEFINITION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace worldwright {

// A terrain tile coordinate in the same convention used by WdtReader and by
// ADT file names: x is the grid column and y is the grid row, both in [0, 63].
struct TileCoord {
    int x = 0;
    int y = 0;
};

inline bool operator==(const TileCoord& lhs, const TileCoord& rhs) noexcept {
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

inline bool operator!=(const TileCoord& lhs, const TileCoord& rhs) noexcept {
    return !(lhs == rhs);
}

// Inclusive bounding rectangle of enabled tiles.
struct TileBounds {
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
};

inline bool operator==(const TileBounds& lhs, const TileBounds& rhs) noexcept {
    return lhs.minX == rhs.minX && lhs.minY == rhs.minY &&
           lhs.maxX == rhs.maxX && lhs.maxY == rhs.maxY;
}

inline bool operator!=(const TileBounds& lhs, const TileBounds& rhs) noexcept {
    return !(lhs == rhs);
}

// An editable world definition independent of any file format or GUI toolkit.
//
// The model owns a fixed 64 x 64 terrain grid. An empty world (zero enabled
// tiles) is a valid state and is the default state. The class is a value type:
// copies are independent, and there are no owning raw pointers.
class WorldDefinition {
public:
    static constexpr int GridSize = 64;
    static constexpr int GridWidth = GridSize;
    static constexpr int GridHeight = GridSize;
    static constexpr std::size_t TileCapacity =
        static_cast<std::size_t>(GridWidth) * static_cast<std::size_t>(GridHeight);

    // True when (x, y) lies inside the 64 x 64 grid.
    static bool isValidTile(int x, int y) noexcept;

    // Enables or disables one tile. Returns false for an invalid coordinate and
    // leaves the world unchanged. Repeating the same request is harmless and
    // returns true for a valid coordinate.
    bool setTile(int x, int y, bool enabled);

    // True only for an in-range enabled tile. Negative and out-of-range
    // coordinates return false.
    bool hasTile(int x, int y) const noexcept;

    // Number of enabled terrain tiles, never the grid capacity unless all
    // 4096 tiles are enabled.
    std::size_t tileCount() const noexcept;

    bool empty() const noexcept;

    // Removes every enabled tile. The world remains valid.
    void clear() noexcept;

    // Enabled tiles in row-major order (y outer, x inner).
    std::vector<TileCoord> enabledTiles() const;

    // Inclusive bounds of enabled tiles. Returns std::nullopt for an empty
    // world, which is unambiguous and avoids sentinel coordinates.
    std::optional<TileBounds> bounds() const noexcept;

private:
    std::array<std::uint8_t, TileCapacity> m_tiles{};
    std::size_t m_tileCount = 0;
};

}  // namespace worldwright

#endif  // WORLDWRIGHT_WORLD_DEFINITION_H
