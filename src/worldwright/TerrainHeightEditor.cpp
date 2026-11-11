#include "worldwright/TerrainHeightEditor.h"

#include "worldwright/WorldDefinition.h"
#include "worldwright/WorldEditorModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace worldwright {
namespace {

enum class Operation { Raise, Lower, Smooth };

struct VertexKey {
    int x;
    int y;

    bool operator==(const VertexKey& other) const noexcept {
        return x == other.x && y == other.y;
    }
};

struct VertexKeyHash {
    std::size_t operator()(const VertexKey& key) const noexcept {
        return (static_cast<std::size_t>(static_cast<unsigned int>(key.x)) <<
                16) ^
               static_cast<unsigned int>(key.y);
    }
};

struct Update {
    TerrainDefinition* terrain;
    int row;
    int column;
    float height;
    bool outer;
};

constexpr int kHalfCellsPerTile =
    TerrainDefinition::InnerVerticesPerTileSide * 2;

std::size_t tileIndex(int x, int y) noexcept {
    return static_cast<std::size_t>(y) * WorldDefinition::GridWidth +
           static_cast<std::size_t>(x);
}

VertexKey outerKey(int tileX, int tileY, int row, int column) noexcept {
    return {tileX * kHalfCellsPerTile + column * 2,
            tileY * kHalfCellsPerTile + row * 2};
}

VertexKey innerKey(int tileX, int tileY, int row, int column) noexcept {
    return {tileX * kHalfCellsPerTile + column * 2 + 1,
            tileY * kHalfCellsPerTile + row * 2 + 1};
}

TerrainPoint position(const VertexKey& key) noexcept {
    constexpr double halfCell = TerrainHeightEditor::CellSize / 2.0;
    return {key.x * halfCell, key.y * halfCell};
}

bool overlapsBrush(int tileX, int tileY, const TerrainBrush& brush) noexcept {
    const double minX = tileX * TerrainHeightEditor::TileSize;
    const double minY = tileY * TerrainHeightEditor::TileSize;
    const double maxX = minX + TerrainHeightEditor::TileSize;
    const double maxY = minY + TerrainHeightEditor::TileSize;
    const double nearestX = std::max(minX, std::min(brush.center.x, maxX));
    const double nearestY = std::max(minY, std::min(brush.center.y, maxY));
    return std::hypot(brush.center.x - nearestX,
                      brush.center.y - nearestY) <= brush.radius;
}

bool validBrush(const TerrainBrush& brush, Operation operation) noexcept {
    if (!std::isfinite(brush.center.x) || !std::isfinite(brush.center.y) ||
        !std::isfinite(brush.radius) || !std::isfinite(brush.strength) ||
        brush.radius <= 0.0 || brush.strength < 0.0) {
        return false;
    }
    return operation != Operation::Smooth || brush.strength <= 1.0;
}

float canonicalHeight(const WorldEditorModel& model,
                      const VertexKey& key) noexcept {
    const auto missing = [] {
        return std::numeric_limits<float>::quiet_NaN();
    };
    constexpr int worldHalfCells =
        WorldDefinition::GridWidth * kHalfCellsPerTile;
    if (key.x < 0 || key.y < 0 || key.x > worldHalfCells ||
        key.y > worldHalfCells || ((key.x ^ key.y) & 1) != 0) {
        return missing();
    }

    if ((key.x & 1) != 0) {
        // Inner vertices never lie on a tile boundary and have one owner.
        if (key.x == worldHalfCells || key.y == worldHalfCells) {
            return missing();
        }
        const int tileX = key.x / kHalfCellsPerTile;
        const int tileY = key.y / kHalfCellsPerTile;
        const auto* terrain = model.terrainAt(tileX, tileY);
        if (terrain == nullptr || !terrain->isValid()) {
            return missing();
        }
        const int column = (key.x - tileX * kHalfCellsPerTile - 1) / 2;
        const int row = (key.y - tileY * kHalfCellsPerTile - 1) / 2;
        return terrain->innerHeight(row, column);
    }

    // Outer vertices can have one, two, or four owners. Candidate iteration is
    // row-major, so the smallest enabled (tileY, tileX) is authoritative.
    const int outerX = key.x / 2;
    const int outerY = key.y / 2;
    constexpr int cellsPerTile =
        TerrainDefinition::InnerVerticesPerTileSide;
    const int baseX = outerX / cellsPerTile;
    const int baseY = outerY / cellsPerTile;
    const bool xBoundary = outerX % cellsPerTile == 0;
    const bool yBoundary = outerY % cellsPerTile == 0;
    const int minTileX = xBoundary ? baseX - 1 : baseX;
    const int minTileY = yBoundary ? baseY - 1 : baseY;
    for (int tileY = minTileY; tileY <= baseY; ++tileY) {
        for (int tileX = minTileX; tileX <= baseX; ++tileX) {
            if (!WorldDefinition::isValidTile(tileX, tileY)) {
                continue;
            }
            const auto* terrain = model.terrainAt(tileX, tileY);
            if (terrain == nullptr || !terrain->isValid()) {
                continue;
            }
            return terrain->outerHeight(outerY - tileY * cellsPerTile,
                                        outerX - tileX * cellsPerTile);
        }
    }
    return missing();
}

bool apply(WorldEditorModel& model, const TerrainBrush& brush,
           Operation operation) {
    if (!validBrush(brush, operation)) {
        return false;
    }

    const auto enabled = model.world().enabledTiles();
    std::array<bool, WorldDefinition::TileCapacity> target{};
    bool hasTarget = false;
    for (const auto tile : enabled) {
        if (!overlapsBrush(tile.x, tile.y, brush)) {
            continue;
        }
        target[tileIndex(tile.x, tile.y)] = true;
        hasTarget = true;
    }
    if (!hasTarget) {
        return true;
    }

    // Source values are cached lazily and all destination writes are deferred.
    // This is snapshot semantics without copying whole affected ADTs: every
    // smoothing result observes the unchanged terrain from the start of pass.
    std::unordered_map<VertexKey, float, VertexKeyHash> snapshot;
    const auto sourceHeightAt = [&](const VertexKey& key) {
        const auto found = snapshot.find(key);
        if (found != snapshot.end()) {
            return found->second;
        }
        const float height = canonicalHeight(model, key);
        snapshot.emplace(key, height);
        return height;
    };

    std::vector<Update> updates;
    for (const auto tile : enabled) {
        if (!target[tileIndex(tile.x, tile.y)]) {
            continue;
        }
        auto* terrain = model.mutableTerrainAt(tile.x, tile.y);
        if (terrain == nullptr || !terrain->isValid()) {
            return false;
        }
        const auto addUpdate = [&](const VertexKey& key, int row, int column,
                                   bool outer) -> bool {
            const auto point = position(key);
            const double distance =
                std::hypot(point.x - brush.center.x, point.y - brush.center.y);
            if (distance > brush.radius) {
                return true;
            }
            const double sourceHeight = sourceHeightAt(key);
            if (!std::isfinite(sourceHeight)) {
                return false;
            }
            const double weight =
                TerrainHeightEditor::influence(distance, brush.radius);
            double result = sourceHeight;
            if (operation == Operation::Raise || operation == Operation::Lower) {
                const double delta = brush.strength * weight;
                result += operation == Operation::Raise ? delta : -delta;
            } else {
                double sum = 0.0;
                int count = 0;
                for (int dy : {-1, 1}) {
                    for (int dx : {-1, 1}) {
                        const float neighbor =
                            sourceHeightAt({key.x + dx, key.y + dy});
                        if (std::isfinite(neighbor)) {
                            sum += neighbor;
                            ++count;
                        }
                    }
                }
                if (count != 0) {
                    const double average = sum / count;
                    result += brush.strength * weight *
                              (average - sourceHeight);
                }
            }
            if (!std::isfinite(result) ||
                result > std::numeric_limits<float>::max() ||
                result < -std::numeric_limits<float>::max()) {
                return false;
            }
            updates.push_back(
                {terrain, row, column, static_cast<float>(result), outer});
            return true;
        };

        for (int row = 0; row < TerrainDefinition::OuterVerticesPerTileSide;
             ++row) {
            for (int column = 0;
                 column < TerrainDefinition::OuterVerticesPerTileSide;
                 ++column) {
                if (!addUpdate(outerKey(tile.x, tile.y, row, column), row,
                               column, true)) {
                    return false;
                }
            }
        }
        for (int row = 0; row < TerrainDefinition::InnerVerticesPerTileSide;
             ++row) {
            for (int column = 0;
                 column < TerrainDefinition::InnerVerticesPerTileSide;
                 ++column) {
                if (!addUpdate(innerKey(tile.x, tile.y, row, column), row,
                               column, false)) {
                    return false;
                }
            }
        }
    }

    // Validation and calculation finish before the first mutation, so a
    // rejected operation cannot leave a partially edited world.
    for (const auto& update : updates) {
        const bool assigned = update.outer
                                  ? update.terrain->setOuterHeight(
                                        update.row, update.column, update.height)
                                  : update.terrain->setInnerHeight(
                                        update.row, update.column, update.height);
        if (!assigned) {
            return false;
        }
    }
    return true;
}

}  // namespace

bool TerrainHeightEditor::raise(WorldEditorModel& model,
                                const TerrainBrush& brush) {
    return apply(model, brush, Operation::Raise);
}

bool TerrainHeightEditor::lower(WorldEditorModel& model,
                                const TerrainBrush& brush) {
    return apply(model, brush, Operation::Lower);
}

bool TerrainHeightEditor::smooth(WorldEditorModel& model,
                                 const TerrainBrush& brush) {
    return apply(model, brush, Operation::Smooth);
}

double TerrainHeightEditor::influence(double distance,
                                      double radius) noexcept {
    if (!std::isfinite(distance) || !std::isfinite(radius) || distance < 0.0 ||
        radius <= 0.0 || distance >= radius) {
        return 0.0;
    }
    return 1.0 - distance / radius;
}

bool TerrainHeightEditor::editableAt(const WorldEditorModel& model,
                                     TerrainPoint point) noexcept {
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || point.x < 0.0 ||
        point.y < 0.0) return false;
    const double scaledX = point.x / TileSize;
    const double scaledY = point.y / TileSize;
    if (scaledX > WorldDefinition::GridWidth + 1e-7 ||
        scaledY > WorldDefinition::GridHeight + 1e-7) return false;
    const int baseX = static_cast<int>(std::floor(scaledX));
    const int baseY = static_cast<int>(std::floor(scaledY));
    constexpr double epsilon = 1e-7;
    const bool xBoundary = std::abs(scaledX - std::round(scaledX)) <= epsilon;
    const bool yBoundary = std::abs(scaledY - std::round(scaledY)) <= epsilon;
    const int minimumX = xBoundary ? baseX - 1 : baseX;
    const int minimumY = yBoundary ? baseY - 1 : baseY;
    for (int tileY = minimumY; tileY <= baseY; ++tileY) {
        for (int tileX = minimumX; tileX <= baseX; ++tileX) {
            if (WorldDefinition::isValidTile(tileX, tileY) &&
                model.world().hasTile(tileX, tileY)) return true;
        }
    }
    return false;
}

TerrainPoint TerrainHeightEditor::outerVertexPosition(int tileX, int tileY,
                                                       int row,
                                                       int column) noexcept {
    return position(outerKey(tileX, tileY, row, column));
}

TerrainPoint TerrainHeightEditor::innerVertexPosition(int tileX, int tileY,
                                                       int row,
                                                       int column) noexcept {
    return position(innerKey(tileX, tileY, row, column));
}

}  // namespace worldwright
