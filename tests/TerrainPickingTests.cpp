#include "worldwright/TerrainHeightEditor.h"
#include "worldwright/TerrainMesh.h"
#include "worldwright/TerrainPicking.h"
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

bool near(double a, double b, double epsilon = 0.002) {
    return std::abs(a - b) <= epsilon;
}

worldwright::TerrainRay vertical(double x, double z, double top = 1000.0) {
    return {{x, top, z}, {0.0, -1.0, 0.0}};
}
}  // namespace

int main() {
    using namespace worldwright;
    constexpr int tileX = 5;
    constexpr int tileY = 7;
    constexpr double cell = TerrainHeightEditor::CellSize;

    WorldEditorModel empty;
    CHECK(!TerrainPicking::intersect(TerrainMeshBuilder::build(empty),
                                     vertical(0.0, 0.0)).has_value());

    WorldEditorModel model;
    CHECK(model.setTileEnabled(tileX, tileY, true));
    model.selectTile(tileX, tileY);
    auto flat = TerrainMeshBuilder::build(model);
    CHECK(flat.tileRanges.size() == 1);
    const double centerX = (tileX + 0.5) * TerrainHeightEditor::TileSize;
    const double centerZ = (tileY + 0.5) * TerrainHeightEditor::TileSize;
    const auto flatHit = TerrainPicking::intersect(flat,
                                                   vertical(centerX, centerZ));
    CHECK(flatHit.has_value());
    CHECK(near(flatHit->terrain.x, centerX));
    CHECK(near(flatHit->terrain.y, centerZ));
    CHECK(near(flatHit->elevation, 0.0));
    CHECK((flatHit->tile == TileCoord{tileX, tileY}));
    CHECK(!TerrainPicking::intersect(
              flat, vertical((tileX - 1) * TerrainHeightEditor::TileSize,
                             centerZ)).has_value());

    auto* terrain = model.mutableTerrainAt(tileX, tileY);
    CHECK(terrain != nullptr);
    CHECK(terrain->setOuterHeight(20, 30, 25.0f));
    CHECK(terrain->setOuterHeight(40, 50, -18.0f));
    auto shaped = TerrainMeshBuilder::build(model);
    auto point = TerrainHeightEditor::outerVertexPosition(tileX, tileY, 20, 30);
    const auto raised = TerrainPicking::intersect(shaped,
                                                  vertical(point.x, point.y));
    CHECK(raised.has_value());
    CHECK(near(raised->elevation, 25.0));
    point = TerrainHeightEditor::outerVertexPosition(tileX, tileY, 40, 50);
    const auto depression = TerrainPicking::intersect(
        shaped, vertical(point.x, point.y));
    CHECK(depression.has_value());
    CHECK(near(depression->elevation, -18.0));

    // A complete sloped tile exercises triangle interpolation and cursor
    // placement away from vertices.
    for (int row = 0; row < TerrainDefinition::OuterVerticesPerTileSide; ++row)
        for (int column = 0;
             column < TerrainDefinition::OuterVerticesPerTileSide; ++column)
            CHECK(terrain->setOuterHeight(row, column,
                                          static_cast<float>(column * cell * 0.2)));
    for (int row = 0; row < TerrainDefinition::InnerVerticesPerTileSide; ++row)
        for (int column = 0;
             column < TerrainDefinition::InnerVerticesPerTileSide; ++column)
            CHECK(terrain->setInnerHeight(
                row, column, static_cast<float>((column + 0.5) * cell * 0.2)));
    const auto slope = TerrainMeshBuilder::build(model);
    const TerrainPoint slopePoint{tileX * TerrainHeightEditor::TileSize +
                                      37.25 * cell,
                                  tileY * TerrainHeightEditor::TileSize +
                                      63.6 * cell};
    const auto slopeHeight = TerrainPicking::surfaceElevation(slope, slopePoint);
    CHECK(slopeHeight.has_value());
    CHECK(near(*slopeHeight, 37.25 * cell * 0.2));
    const auto slopeHit = TerrainPicking::intersect(
        slope, vertical(slopePoint.x, slopePoint.y));
    CHECK(slopeHit.has_value());
    CHECK(near(slopeHit->elevation, *slopeHeight));

    TerrainCamera camera;
    camera.fit(slope.bounds, 16.0 / 9.0);
    const auto centerRay = TerrainPicking::screenRay(camera, 0.0, 0.0,
                                                     16.0 / 9.0);
    const auto centerHit = TerrainPicking::intersect(slope, centerRay);
    CHECK(centerHit.has_value());
    CHECK(near(centerHit->terrain.x, camera.target().x, 0.02));
    CHECK(near(centerHit->terrain.y, camera.target().z, 0.02));

    // Exact enabled ADT boundary intersections are accepted. Removing the
    // neighbor removes its geometry and leaves the remaining edge pickable.
    CHECK(model.setTileEnabled(tileX + 1, tileY, true));
    auto boundaryMesh = TerrainMeshBuilder::build(model);
    const double boundaryX = (tileX + 1) * TerrainHeightEditor::TileSize;
    CHECK(TerrainPicking::intersect(boundaryMesh,
                                    vertical(boundaryX, centerZ)).has_value());
    CHECK(model.setTileEnabled(tileX + 1, tileY, false));
    boundaryMesh = TerrainMeshBuilder::build(model);
    CHECK(boundaryMesh.tileRanges.size() == 1);
    CHECK(TerrainPicking::intersect(boundaryMesh,
                                    vertical(boundaryX, centerZ)).has_value());
    CHECK(TerrainHeightEditor::editableAt(model, {boundaryX, centerZ}));
    CHECK(!TerrainHeightEditor::editableAt(
        model, {boundaryX + cell * 0.5, centerZ}));

    // Two stacked triangles prove that the nearest valid intersection wins.
    TerrainMesh stacked;
    stacked.vertices = {{0, 10, 0}, {10, 10, 0}, {0, 10, 10},
                        {0, 0, 0}, {10, 0, 0}, {0, 0, 10}};
    stacked.indices = {0, 2, 1, 3, 5, 4};
    TerrainMeshTileRange range;
    range.tile = {0, 0};
    range.vertexCount = stacked.vertices.size();
    range.indexCount = stacked.indices.size();
    range.bounds = {0, 0, 0, 10, 10, 10, true};
    stacked.tileRanges.push_back(range);
    const auto nearest = TerrainPicking::intersect(stacked,
                                                   vertical(2.0, 2.0, 20.0));
    CHECK(nearest.has_value());
    CHECK(near(nearest->elevation, 10.0));
    CHECK(near(nearest->distance, 10.0));

    std::cout << "Terrain picking failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
