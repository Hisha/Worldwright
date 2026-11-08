#include "worldwright/WorldDefinition.h"

namespace worldwright {
namespace {

std::size_t tileIndex(int x, int y) noexcept {
    return static_cast<std::size_t>(y) * WorldDefinition::GridWidth +
           static_cast<std::size_t>(x);
}

}  // namespace

bool WorldDefinition::isValidTile(int x, int y) noexcept {
    return x >= 0 && y >= 0 && x < GridWidth && y < GridHeight;
}

bool WorldDefinition::setTile(int x, int y, bool enabled) {
    if (!isValidTile(x, y)) {
        return false;
    }

    auto& value = m_tiles[tileIndex(x, y)];
    if (enabled) {
        if (value == 0) {
            value = 1;
            ++m_tileCount;
        }
    } else if (value != 0) {
        value = 0;
        --m_tileCount;
    }

    return true;
}

bool WorldDefinition::hasTile(int x, int y) const noexcept {
    if (!isValidTile(x, y)) {
        return false;
    }
    return m_tiles[tileIndex(x, y)] != 0;
}

std::size_t WorldDefinition::tileCount() const noexcept {
    return m_tileCount;
}

bool WorldDefinition::empty() const noexcept {
    return m_tileCount == 0;
}

void WorldDefinition::clear() noexcept {
    m_tiles.fill(0);
    m_tileCount = 0;
}

std::vector<TileCoord> WorldDefinition::enabledTiles() const {
    std::vector<TileCoord> tiles;
    tiles.reserve(m_tileCount);

    for (int y = 0; y < GridHeight; ++y) {
        for (int x = 0; x < GridWidth; ++x) {
            if (m_tiles[tileIndex(x, y)] != 0) {
                tiles.push_back(TileCoord{x, y});
            }
        }
    }

    return tiles;
}

std::optional<TileBounds> WorldDefinition::bounds() const noexcept {
    if (m_tileCount == 0) {
        return std::nullopt;
    }

    int minX = GridWidth;
    int minY = GridHeight;
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < GridHeight; ++y) {
        for (int x = 0; x < GridWidth; ++x) {
            if (m_tiles[tileIndex(x, y)] == 0) {
                continue;
            }
            if (x < minX) {
                minX = x;
            }
            if (y < minY) {
                minY = y;
            }
            if (x > maxX) {
                maxX = x;
            }
            if (y > maxY) {
                maxY = y;
            }
        }
    }

    return TileBounds{minX, minY, maxX, maxY};
}

}  // namespace worldwright
