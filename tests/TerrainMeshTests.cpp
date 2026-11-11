#include "worldwright/TerrainCamera.h"
#include "worldwright/TerrainMesh.h"
#include "worldwright/WorldEditorModel.h"

#include <cmath>
#include <iostream>

namespace {
int failures = 0;
#define CHECK(expression)                                                        \
    do {                                                                         \
        if (!(expression)) {                                                     \
            ++failures;                                                          \
            std::cerr << "FAIL line " << __LINE__ << ": " #expression << '\n'; \
        }                                                                        \
    } while (false)

bool near(double a, double b, double epsilon = 0.0001) {
    return std::abs(a - b) <= epsilon;
}

bool sameVertex(const worldwright::TerrainMeshVertex& a,
                const worldwright::TerrainMeshVertex& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.nx == b.nx &&
           a.ny == b.ny && a.nz == b.nz;
}
}  // namespace

int main() {
    using namespace worldwright;
    constexpr std::size_t verticesPerTile = TerrainDefinition::UniqueVertexCount;
    constexpr std::size_t trianglesPerTile =
        static_cast<std::size_t>(TerrainDefinition::InnerVerticesPerTileSide) *
        TerrainDefinition::InnerVerticesPerTileSide * 4;

    WorldEditorModel empty;
    const auto emptyMesh = TerrainMeshBuilder::build(empty);
    CHECK(emptyMesh.empty());
    CHECK(emptyMesh.vertices.empty());
    CHECK(!emptyMesh.bounds.valid);

    WorldEditorModel flat;
    CHECK(flat.setTileEnabled(10, 20, true));
    flat.selectTile(10, 20);
    const auto flatMesh = TerrainMeshBuilder::build(flat);
    CHECK(flatMesh.tiles.size() == 1);
    CHECK(flatMesh.vertices.size() == verticesPerTile);
    CHECK(flatMesh.indices.size() == trianglesPerTile * 3);
    CHECK(flatMesh.boundaryVertices.size() ==
          static_cast<std::size_t>(TerrainDefinition::InnerVerticesPerTileSide) *
              8);
    for (const auto& vertex : flatMesh.vertices) {
        CHECK(vertex.y == 0.0f);
        CHECK(near(vertex.nx, 0.0));
        CHECK(near(vertex.ny, 1.0));
        CHECK(near(vertex.nz, 0.0));
    }

    // The first cell has its four serialized outer corners and its inner
    // center. It must produce four upward-facing triangles.
    const std::uint32_t expectedFirstCell[] = {
        static_cast<std::uint32_t>(TerrainDefinition::UniqueOuterVertexCount),
        0, TerrainDefinition::OuterVerticesPerTileSide,
        static_cast<std::uint32_t>(TerrainDefinition::UniqueOuterVertexCount),
        1, 0,
        static_cast<std::uint32_t>(TerrainDefinition::UniqueOuterVertexCount),
        TerrainDefinition::OuterVerticesPerTileSide + 1, 1,
        static_cast<std::uint32_t>(TerrainDefinition::UniqueOuterVertexCount),
        TerrainDefinition::OuterVerticesPerTileSide,
        TerrainDefinition::OuterVerticesPerTileSide + 1};
    for (std::size_t i = 0; i < std::size(expectedFirstCell); ++i) {
        CHECK(flatMesh.indices[i] == expectedFirstCell[i]);
    }
    for (std::size_t i = 0; i < 12; i += 3) {
        const auto& a = flatMesh.vertices[flatMesh.indices[i]];
        const auto& b = flatMesh.vertices[flatMesh.indices[i + 1]];
        const auto& c = flatMesh.vertices[flatMesh.indices[i + 2]];
        const double signedY = (b.z - a.z) * (c.x - a.x) -
                               (b.x - a.x) * (c.z - a.z);
        CHECK(signedY > 0.0);
    }

    // Verify both unique grids map to their actual positions and heights.
    auto* terrain = flat.mutableTerrainAt(10, 20);
    CHECK(terrain != nullptr);
    CHECK(terrain->setOuterHeight(3, 4, 25.0f));
    CHECK(terrain->setInnerHeight(3, 4, -12.0f));
    const auto shaped = TerrainMeshBuilder::build(flat);
    const std::size_t outerIndex = 3 *
        TerrainDefinition::OuterVerticesPerTileSide + 4;
    const std::size_t innerIndex = TerrainDefinition::UniqueOuterVertexCount +
        3 * TerrainDefinition::InnerVerticesPerTileSide + 4;
    CHECK(shaped.vertices[outerIndex].y == 25.0f);
    CHECK(shaped.vertices[innerIndex].y == -12.0f);
    CHECK(near(shaped.vertices[outerIndex].x,
               (10 * TerrainDefinition::InnerVerticesPerTileSide + 4) *
                   TerrainDefinition::TileSize /
                   TerrainDefinition::InnerVerticesPerTileSide, 0.01));
    CHECK(shaped.bounds.minimumY == -12.0f);
    CHECK(shaped.bounds.maximumY == 25.0f);

    // Shared edge positions, elevations, and normals are identical between
    // adjacent ADTs. A missing neighbor simply removes that ADT's geometry.
    WorldEditorModel adjacent;
    CHECK(adjacent.setTileEnabled(30, 30, true));
    CHECK(adjacent.setTileEnabled(31, 30, true));
    adjacent.selectTile(30, 30);
    auto* west = adjacent.mutableTerrainAt(30, 30);
    auto* east = adjacent.mutableTerrainAt(31, 30);
    CHECK(west->setOuterHeight(64, 128, 18.0f));
    CHECK(east->setOuterHeight(64, 0, 18.0f));
    const auto joined = TerrainMeshBuilder::build(adjacent);
    CHECK(joined.tiles.size() == 2);
    const std::size_t westEdge = 64 *
        TerrainDefinition::OuterVerticesPerTileSide + 128;
    const std::size_t eastEdge = verticesPerTile + 64 *
        TerrainDefinition::OuterVerticesPerTileSide;
    CHECK(sameVertex(joined.vertices[westEdge], joined.vertices[eastEdge]));
    CHECK(joined.vertices[westEdge].y == 18.0f);
    CHECK(adjacent.setTileEnabled(31, 30, false));
    const auto missingNeighbor = TerrainMeshBuilder::build(adjacent);
    CHECK(missingNeighbor.tiles.size() == 1);
    CHECK(missingNeighbor.vertices.size() == verticesPerTile);

    // Generation order and floating-point results are deterministic.
    const auto first = TerrainMeshBuilder::build(flat);
    const auto second = TerrainMeshBuilder::build(flat);
    CHECK(first.indices == second.indices);
    CHECK(first.vertices.size() == second.vertices.size());
    for (std::size_t i = 0; i < first.vertices.size(); ++i) {
        CHECK(sameVertex(first.vertices[i], second.vertices[i]));
    }

    TerrainCamera camera;
    camera.fit(shaped.bounds, 16.0 / 9.0);
    CHECK(camera.distance() > 0.0);
    CHECK(camera.nearPlane() > 0.0);
    CHECK(camera.farPlane() > camera.nearPlane());
    const auto beforeOrbit = camera.position();
    camera.orbit(20.0, -200.0);
    CHECK(camera.pitch() == -89.0);
    const auto afterOrbit = camera.position();
    CHECK(!near(beforeOrbit.x, afterOrbit.x) ||
          !near(beforeOrbit.y, afterOrbit.y));
    const double beforeZoom = camera.distance();
    camera.zoom(1.0);
    CHECK(camera.distance() < beforeZoom);
    const auto target = camera.target();
    camera.pan(10.0, 5.0);
    const auto panned = camera.target();
    CHECK(!near(target.x, panned.x) || !near(target.y, panned.y) ||
          !near(target.z, panned.z));
    camera.reset();
    CHECK(camera.yaw() == -45.0);
    CHECK(camera.pitch() == -35.0);

    std::cout << "Terrain mesh/camera failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
