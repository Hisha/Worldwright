#ifndef WORLDWRIGHT_TERRAIN_MESH_H
#define WORLDWRIGHT_TERRAIN_MESH_H

#include "worldwright/WorldDefinition.h"

#include <cstdint>
#include <cstddef>
#include <optional>
#include <vector>

namespace worldwright {

class WorldEditorModel;

struct TerrainMeshVertex {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float nx = 0.0f;
    float ny = 1.0f;
    float nz = 0.0f;
};

struct TerrainMeshBounds {
    float minimumX = 0.0f;
    float minimumY = 0.0f;
    float minimumZ = 0.0f;
    float maximumX = 0.0f;
    float maximumY = 0.0f;
    float maximumZ = 0.0f;
    bool valid = false;
};

struct TerrainMeshTileRange {
    TileCoord tile;
    std::size_t firstVertex = 0;
    std::size_t vertexCount = 0;
    std::size_t firstIndex = 0;
    std::size_t indexCount = 0;
    TerrainMeshBounds bounds;
};

// CPU-side render data built directly from WorldEditorModel. Coordinates use
// editor X as render X, elevation as render Y, and editor Y as render Z.
struct TerrainMesh {
    std::vector<TerrainMeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<TerrainMeshVertex> boundaryVertices;
    std::vector<TileCoord> tiles;
    // Ranges refer directly to vertices/indices above. Picking uses them for
    // tile-level rejection and cursor height queries without another mesh.
    std::vector<TerrainMeshTileRange> tileRanges;
    TerrainMeshBounds bounds;

    bool empty() const noexcept { return indices.empty(); }
};

class TerrainMeshBuilder {
public:
    // With a focus tile, include it and enabled neighbors within tileRadius.
    // Without one, choose the first enabled tile as the focus. This bounds the
    // W006 working set while naturally rendering a complete 3x3 test world.
    static TerrainMesh build(const WorldEditorModel& model,
                             std::optional<TileCoord> focus = std::nullopt,
                             int tileRadius = 1);
};

}  // namespace worldwright

#endif
