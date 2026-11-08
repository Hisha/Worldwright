#include "worldwright/WorldEditorModel.h"

#include <algorithm>
#include <cmath>

namespace worldwright {

WorldEditorModel::WorldEditorModel() : m_worldName("Untitled") {}

bool WorldEditorModel::validateWorldName(const std::string& name,
                                         std::string* errorMessage) {
    const auto fail = [&](const char* message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
        return false;
    };

    if (name.empty()) {
        return fail("World name cannot be empty.");
    }
    if (name.size() > 64) {
        return fail("World name must be 64 characters or fewer.");
    }
    const auto asciiLetter = [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    };
    const auto asciiDigit = [](unsigned char c) {
        return c >= '0' && c <= '9';
    };
    if (!asciiLetter(static_cast<unsigned char>(name.front()))) {
        return fail("World name must begin with an ASCII letter.");
    }
    for (const unsigned char c : name) {
        if (!asciiLetter(c) && !asciiDigit(c) && c != '_') {
            return fail("World name may contain only ASCII letters, digits, "
                        "and underscores.");
        }
    }
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    return true;
}

bool WorldEditorModel::newWorld(const std::string& name,
                                std::string* errorMessage) {
    if (!validateWorldName(name, errorMessage)) {
        return false;
    }
    m_worldName = name;
    m_world.clear();
    m_terrain.clear();
    m_selection.clear();
    return true;
}

bool WorldEditorModel::setWorldName(const std::string& name,
                                    std::string* errorMessage) {
    if (!validateWorldName(name, errorMessage)) {
        return false;
    }
    m_worldName = name;
    return true;
}

const std::string& WorldEditorModel::worldName() const noexcept {
    return m_worldName;
}

const WorldDefinition& WorldEditorModel::world() const noexcept {
    return m_world;
}

void WorldEditorModel::selectTile(int x, int y) {
    m_selection.clear();
    if (WorldDefinition::isValidTile(x, y)) {
        m_selection.push_back({x, y});
    }
}

void WorldEditorModel::selectRectangle(int firstX, int firstY, int lastX,
                                       int lastY) {
    m_selection = rectangleTiles(firstX, firstY, lastX, lastY);
}

void WorldEditorModel::clearSelection() noexcept {
    m_selection.clear();
}

const std::vector<TileCoord>& WorldEditorModel::selection() const noexcept {
    return m_selection;
}

std::optional<TileCoord> WorldEditorModel::primarySelection() const noexcept {
    if (m_selection.empty()) {
        return std::nullopt;
    }
    return m_selection.front();
}

bool WorldEditorModel::isSelected(int x, int y) const noexcept {
    return std::find(m_selection.begin(), m_selection.end(), TileCoord{x, y}) !=
           m_selection.end();
}

bool WorldEditorModel::setSelectedEnabled(bool enabled) {
    if (m_selection.empty()) {
        return false;
    }
    return setTilesEnabled(m_selection, enabled);
}

bool WorldEditorModel::setTileEnabled(int x, int y, bool enabled) {
    if (!WorldDefinition::isValidTile(x, y)) {
        return false;
    }
    if (m_world.hasTile(x, y) == enabled) {
        return false;
    }
    m_world.setTile(x, y, enabled);
    const auto index = tileIndex(x, y);
    if (enabled) {
        // try_emplace preserves an existing tile's elevation and gives newly
        // painted terrain the established default flat height of zero.
        m_terrain.try_emplace(index);
    } else {
        m_terrain.erase(index);
    }
    return true;
}

bool WorldEditorModel::setTilesEnabled(const std::vector<TileCoord>& tiles,
                                       bool enabled) {
    bool applied = false;
    for (const auto tile : tiles) {
        applied = setTileEnabled(tile.x, tile.y, enabled) || applied;
    }
    return applied;
}

bool WorldEditorModel::setSelectedElevation(float elevation) {
    if (!std::isfinite(elevation) || m_selection.empty()) {
        return false;
    }

    bool changed = false;
    for (const auto tile : m_selection) {
        if (!m_world.hasTile(tile.x, tile.y)) {
            continue;
        }
        auto found = m_terrain.find(tileIndex(tile.x, tile.y));
        if (found == m_terrain.end()) {
            found = m_terrain.try_emplace(tileIndex(tile.x, tile.y)).first;
        }
        changed = found->second.setFlatHeight(elevation) || changed;
    }
    return changed;
}

const TerrainDefinition* WorldEditorModel::terrainAt(int x, int y) const noexcept {
    if (!m_world.hasTile(x, y)) {
        return nullptr;
    }
    const auto found = m_terrain.find(tileIndex(x, y));
    return found == m_terrain.end() ? nullptr : &found->second;
}

TerrainDefinition* WorldEditorModel::mutableTerrainAt(int x, int y) noexcept {
    if (!m_world.hasTile(x, y)) {
        return nullptr;
    }
    const auto found = m_terrain.find(tileIndex(x, y));
    return found == m_terrain.end() ? nullptr : &found->second;
}

std::optional<float> WorldEditorModel::elevationAt(int x, int y) const noexcept {
    const auto* terrain = terrainAt(x, y);
    if (terrain == nullptr) {
        return std::nullopt;
    }
    return terrain->outerHeight(0, 0);
}

std::optional<TileCoord> WorldEditorModel::tileAt(double contentX,
                                                  double contentY,
                                                  double tileSize) noexcept {
    if (!std::isfinite(contentX) || !std::isfinite(contentY) ||
        !std::isfinite(tileSize) || tileSize <= 0.0 || contentX < 0.0 ||
        contentY < 0.0) {
        return std::nullopt;
    }
    const int x = static_cast<int>(std::floor(contentX / tileSize));
    const int y = static_cast<int>(std::floor(contentY / tileSize));
    if (!WorldDefinition::isValidTile(x, y)) {
        return std::nullopt;
    }
    return TileCoord{x, y};
}

std::vector<TileCoord> WorldEditorModel::rectangleTiles(int firstX, int firstY,
                                                         int lastX,
                                                         int lastY) {
    const int minX = std::max(0, std::min(firstX, lastX));
    const int minY = std::max(0, std::min(firstY, lastY));
    const int maxX = std::min(WorldDefinition::GridWidth - 1,
                              std::max(firstX, lastX));
    const int maxY = std::min(WorldDefinition::GridHeight - 1,
                              std::max(firstY, lastY));
    std::vector<TileCoord> result;
    if (minX > maxX || minY > maxY) {
        return result;
    }
    result.reserve(static_cast<std::size_t>(maxX - minX + 1) *
                   static_cast<std::size_t>(maxY - minY + 1));
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            result.push_back({x, y});
        }
    }
    return result;
}

std::size_t WorldEditorModel::tileIndex(int x, int y) noexcept {
    return static_cast<std::size_t>(y) * WorldDefinition::GridWidth +
           static_cast<std::size_t>(x);
}

}  // namespace worldwright
