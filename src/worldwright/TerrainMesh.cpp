#include "worldwright/TerrainMesh.h"

#include "worldwright/TerrainDefinition.h"
#include "worldwright/WorldEditorModel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace worldwright {
namespace {

struct GlobalVertexKey {
    int halfX;
    int halfZ;

    bool operator==(const GlobalVertexKey& other) const noexcept {
        return halfX == other.halfX && halfZ == other.halfZ;
    }
};

struct GlobalVertexKeyHash {
    std::size_t operator()(const GlobalVertexKey& key) const noexcept {
        return (static_cast<std::size_t>(static_cast<unsigned int>(key.halfX))
                << 16) ^
               static_cast<unsigned int>(key.halfZ);
    }
};

struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

Vector3 operator-(const TerrainMeshVertex& left,
                  const TerrainMeshVertex& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vector3 cross(const Vector3& left, const Vector3& right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

void extendBounds(TerrainMeshBounds& bounds,
                  const TerrainMeshVertex& vertex) noexcept {
    if (!bounds.valid) {
        bounds = {vertex.x, vertex.y, vertex.z, vertex.x, vertex.y, vertex.z,
                  true};
        return;
    }
    bounds.minimumX = std::min(bounds.minimumX, vertex.x);
    bounds.minimumY = std::min(bounds.minimumY, vertex.y);
    bounds.minimumZ = std::min(bounds.minimumZ, vertex.z);
    bounds.maximumX = std::max(bounds.maximumX, vertex.x);
    bounds.maximumY = std::max(bounds.maximumY, vertex.y);
    bounds.maximumZ = std::max(bounds.maximumZ, vertex.z);
}

}  // namespace

TerrainMesh TerrainMeshBuilder::build(const WorldEditorModel& model,
                                       std::optional<TileCoord> focus,
                                       int tileRadius) {
    TerrainMesh mesh;
    const auto enabled = model.world().enabledTiles();
    if (enabled.empty()) {
        return mesh;
    }
    if (!focus.has_value() ||
        !model.world().hasTile(focus->x, focus->y)) {
        focus = enabled.front();
    }
    tileRadius = std::max(0, tileRadius);
    for (const auto tile : enabled) {
        if (std::abs(tile.x - focus->x) <= tileRadius &&
            std::abs(tile.y - focus->y) <= tileRadius) {
            mesh.tiles.push_back(tile);
        }
    }

    constexpr int outerSide = TerrainDefinition::OuterVerticesPerTileSide;
    constexpr int innerSide = TerrainDefinition::InnerVerticesPerTileSide;
    constexpr std::size_t verticesPerTile =
        TerrainDefinition::UniqueVertexCount;
    constexpr std::size_t indicesPerTile =
        static_cast<std::size_t>(innerSide) * innerSide * 12;
    mesh.vertices.reserve(mesh.tiles.size() * verticesPerTile);
    mesh.indices.reserve(mesh.tiles.size() * indicesPerTile);
    mesh.boundaryVertices.reserve(mesh.tiles.size() * innerSide * 8);
    std::vector<GlobalVertexKey> keys;
    keys.reserve(mesh.tiles.size() * verticesPerTile);

    constexpr float cell = TerrainDefinition::TileSize / innerSide;
    const auto appendVertex = [&](int halfX, int halfZ, float height) {
        TerrainMeshVertex vertex{halfX * cell * 0.5f, height,
                                 halfZ * cell * 0.5f, 0.0f, 1.0f, 0.0f};
        mesh.vertices.push_back(vertex);
        keys.push_back({halfX, halfZ});
        extendBounds(mesh.bounds, vertex);
        return static_cast<std::uint32_t>(mesh.vertices.size() - 1);
    };

    for (const auto tile : mesh.tiles) {
        const auto* terrain = model.terrainAt(tile.x, tile.y);
        if (terrain == nullptr || !terrain->isValid()) {
            continue;
        }
        TerrainMeshTileRange range;
        range.tile = tile;
        range.firstVertex = mesh.vertices.size();
        range.firstIndex = mesh.indices.size();
        const std::uint32_t base =
            static_cast<std::uint32_t>(mesh.vertices.size());
        const int tileHalfX = tile.x * innerSide * 2;
        const int tileHalfZ = tile.y * innerSide * 2;
        for (int row = 0; row < outerSide; ++row) {
            for (int column = 0; column < outerSide; ++column) {
                appendVertex(tileHalfX + column * 2,
                             tileHalfZ + row * 2,
                             terrain->outerHeight(row, column));
            }
        }
        for (int row = 0; row < innerSide; ++row) {
            for (int column = 0; column < innerSide; ++column) {
                appendVertex(tileHalfX + column * 2 + 1,
                             tileHalfZ + row * 2 + 1,
                             terrain->innerHeight(row, column));
            }
        }
        const auto outer = [base](int row, int column) {
            return base + static_cast<std::uint32_t>(row * outerSide + column);
        };
        const auto inner = [base](int row, int column) {
            return base + static_cast<std::uint32_t>(
                              TerrainDefinition::UniqueOuterVertexCount +
                              row * innerSide + column);
        };
        for (int row = 0; row < innerSide; ++row) {
            for (int column = 0; column < innerSide; ++column) {
                const std::uint32_t center = inner(row, column);
                // Same four-triangle topology as MCVT/MCNR serialization.
                const std::uint32_t triangles[12] = {
                    center, outer(row, column), outer(row + 1, column),
                    center, outer(row, column + 1), outer(row, column),
                    center, outer(row + 1, column + 1),
                    outer(row, column + 1), center,
                    outer(row + 1, column), outer(row + 1, column + 1)};
                mesh.indices.insert(mesh.indices.end(), std::begin(triangles),
                                    std::end(triangles));
            }
        }

        const auto boundaryPoint = [&](int row, int column) {
            return TerrainMeshVertex{
                (tile.x * innerSide + column) * cell,
                terrain->outerHeight(row, column) + 0.08f,
                (tile.y * innerSide + row) * cell, 0.0f, 1.0f, 0.0f};
        };
        for (int i = 0; i < innerSide; ++i) {
            mesh.boundaryVertices.push_back(boundaryPoint(0, i));
            mesh.boundaryVertices.push_back(boundaryPoint(0, i + 1));
            mesh.boundaryVertices.push_back(boundaryPoint(innerSide, i));
            mesh.boundaryVertices.push_back(boundaryPoint(innerSide, i + 1));
            mesh.boundaryVertices.push_back(boundaryPoint(i, 0));
            mesh.boundaryVertices.push_back(boundaryPoint(i + 1, 0));
            mesh.boundaryVertices.push_back(boundaryPoint(i, innerSide));
            mesh.boundaryVertices.push_back(boundaryPoint(i + 1, innerSide));
        }
        range.vertexCount = mesh.vertices.size() - range.firstVertex;
        range.indexCount = mesh.indices.size() - range.firstIndex;
        for (std::size_t i = range.firstVertex;
             i < range.firstVertex + range.vertexCount; ++i) {
            extendBounds(range.bounds, mesh.vertices[i]);
        }
        mesh.tileRanges.push_back(range);
    }

    std::unordered_map<GlobalVertexKey, Vector3, GlobalVertexKeyHash> normals;
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3) {
        const auto ia = mesh.indices[i];
        const auto ib = mesh.indices[i + 1];
        const auto ic = mesh.indices[i + 2];
        const Vector3 face = cross(mesh.vertices[ib] - mesh.vertices[ia],
                                   mesh.vertices[ic] - mesh.vertices[ia]);
        for (const auto index : {ia, ib, ic}) {
            auto& normal = normals[keys[index]];
            normal.x += face.x;
            normal.y += face.y;
            normal.z += face.z;
        }
    }
    for (std::size_t i = 0; i < mesh.vertices.size(); ++i) {
        const auto normal = normals[keys[i]];
        const double length = std::sqrt(normal.x * normal.x +
                                        normal.y * normal.y +
                                        normal.z * normal.z);
        if (std::isfinite(length) && length > 0.0) {
            mesh.vertices[i].nx = static_cast<float>(normal.x / length);
            mesh.vertices[i].ny = static_cast<float>(normal.y / length);
            mesh.vertices[i].nz = static_cast<float>(normal.z / length);
        }
    }
    return mesh;
}

}  // namespace worldwright
