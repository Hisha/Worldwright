#include "worldwright/AdtWriter.h"
#include "worldwright/TerrainHeightEditor.h"
#include "worldwright/WorldEditorModel.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const char* expression, int line) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
    }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

bool near(float lhs, float rhs, float epsilon = 0.0001f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

worldwright::TerrainBrush brushAt(worldwright::TerrainPoint center,
                                  double radius, double strength) {
    return {{center.x, center.y}, radius, strength};
}

worldwright::WorldEditorModel oneTile(int x = 10, int y = 10) {
    worldwright::WorldEditorModel model;
    CHECK(model.setTileEnabled(x, y, true));
    return model;
}

void testGeometryRaiseLowerAndFalloff() {
    using Editor = worldwright::TerrainHeightEditor;
    auto model = oneTile();
    const auto outer = Editor::outerVertexPosition(10, 10, 50, 60);
    const auto inner = Editor::innerVertexPosition(10, 10, 50, 60);
    CHECK(near(static_cast<float>(inner.x - outer.x),
               static_cast<float>(Editor::CellSize / 2.0)));
    CHECK(near(static_cast<float>(inner.y - outer.y),
               static_cast<float>(Editor::CellSize / 2.0)));

    CHECK(Editor::raise(model, brushAt(outer, 1.0, 2.5)));
    CHECK(near(model.terrainAt(10, 10)->outerHeight(50, 60), 2.5f));
    CHECK(model.terrainAt(10, 10)->outerHeight(50, 61) == 0.0f);
    CHECK(model.terrainAt(10, 10)->innerHeight(50, 60) == 0.0f);
    CHECK(Editor::lower(model, brushAt(outer, 1.0, 0.75)));
    CHECK(near(model.terrainAt(10, 10)->outerHeight(50, 60), 1.75f));

    const auto adjacent = Editor::outerVertexPosition(10, 10, 50, 61);
    worldwright::TerrainPoint midpoint{(outer.x + adjacent.x) / 2.0,
                                       outer.y};
    CHECK(Editor::raise(model,
                        brushAt(midpoint, Editor::CellSize, 4.0)));
    CHECK(near(model.terrainAt(10, 10)->outerHeight(50, 60), 3.75f));
    CHECK(near(model.terrainAt(10, 10)->outerHeight(50, 61), 2.0f));
    CHECK(near(static_cast<float>(Editor::influence(Editor::CellSize / 2.0,
                                                    Editor::CellSize)),
               0.5f));
}

void testSmoothingSnapshotAndDeterminism() {
    using Editor = worldwright::TerrainHeightEditor;
    auto original = oneTile();
    auto* terrain = original.mutableTerrainAt(10, 10);
    CHECK(terrain->setOuterHeight(64, 64, 100.0f));
    const auto center = Editor::outerVertexPosition(10, 10, 64, 64);
    auto first = original;
    auto second = original;
    CHECK(Editor::smooth(first, brushAt(center, 1.0, 1.0)));
    CHECK(Editor::smooth(second, brushAt(center, 1.0, 1.0)));
    CHECK(first.terrainAt(10, 10)->outerHeight(64, 64) == 0.0f);
    for (int row = 0; row < 129; ++row) {
        for (int column = 0; column < 129; ++column) {
            CHECK(first.terrainAt(10, 10)->outerHeight(row, column) ==
                  second.terrainAt(10, 10)->outerHeight(row, column));
        }
    }

    auto innerModel = oneTile();
    auto* innerTerrain = innerModel.mutableTerrainAt(10, 10);
    CHECK(innerTerrain->setInnerHeight(63, 63, 80.0f));
    const auto innerCenter = Editor::innerVertexPosition(10, 10, 63, 63);
    CHECK(Editor::smooth(innerModel,
                         brushAt(innerCenter, 1.0, 0.5)));
    CHECK(near(innerModel.terrainAt(10, 10)->innerHeight(63, 63), 40.0f));
}

void checkHorizontalSeam(const worldwright::WorldEditorModel& model, int westX,
                         int y) {
    const auto* west = model.terrainAt(westX, y);
    const auto* east = model.terrainAt(westX + 1, y);
    for (int row = 0; row < 129; ++row) {
        CHECK(west->outerHeight(row, 128) == east->outerHeight(row, 0));
    }
}

void checkVerticalSeam(const worldwright::WorldEditorModel& model, int x,
                       int northY) {
    const auto* north = model.terrainAt(x, northY);
    const auto* south = model.terrainAt(x, northY + 1);
    for (int column = 0; column < 129; ++column) {
        CHECK(north->outerHeight(128, column) ==
              south->outerHeight(0, column));
    }
}

void testCrossTileSeamsAndMissingTiles() {
    using Editor = worldwright::TerrainHeightEditor;
    worldwright::WorldEditorModel model;
    for (int y = 20; y <= 21; ++y) {
        for (int x = 20; x <= 21; ++x) {
            CHECK(model.setTileEnabled(x, y, true));
        }
    }
    const worldwright::TerrainPoint corner{21 * Editor::TileSize,
                                            21 * Editor::TileSize};
    CHECK(Editor::raise(model, brushAt(corner, 12.0, 3.25)));
    checkHorizontalSeam(model, 20, 20);
    checkHorizontalSeam(model, 20, 21);
    checkVerticalSeam(model, 20, 20);
    checkVerticalSeam(model, 21, 20);
    const float shared = model.terrainAt(20, 20)->outerHeight(128, 128);
    CHECK(shared == model.terrainAt(21, 20)->outerHeight(128, 0));
    CHECK(shared == model.terrainAt(20, 21)->outerHeight(0, 128));
    CHECK(shared == model.terrainAt(21, 21)->outerHeight(0, 0));

    for (int pass = 0; pass < 20; ++pass) {
        CHECK(Editor::lower(model, brushAt(corner, 8.0, 0.1)));
        CHECK(Editor::smooth(model, brushAt(corner, 8.0, 0.2)));
    }
    CHECK(model.terrainAt(20, 20)->outerHeight(128, 128) ==
          model.terrainAt(21, 21)->outerHeight(0, 0));

    auto twoTiles = oneTile(30, 30);
    CHECK(twoTiles.setTileEnabled(31, 30, true));
    const worldwright::TerrainPoint edge{31 * Editor::TileSize,
                                         30 * Editor::TileSize +
                                             64 * Editor::CellSize};
    CHECK(Editor::raise(twoTiles, brushAt(edge, 10.0, 5.0)));
    CHECK(twoTiles.terrainAt(30, 30)->outerHeight(64, 128) == 5.0f);
    CHECK(twoTiles.terrainAt(31, 30)->outerHeight(64, 0) == 5.0f);
    checkHorizontalSeam(twoTiles, 30, 30);

    auto missing = oneTile(40, 40);
    const worldwright::TerrainPoint missingEdge{41 * Editor::TileSize,
                                                 40 * Editor::TileSize};
    CHECK(Editor::raise(missing, brushAt(missingEdge, 10.0, 2.0)));
    CHECK(!missing.world().hasTile(41, 40));
    CHECK(missing.terrainAt(41, 40) == nullptr);
}

void testPreexistingMismatchPolicy() {
    using Editor = worldwright::TerrainHeightEditor;
    auto model = oneTile(12, 12);
    CHECK(model.setTileEnabled(13, 12, true));
    auto* west = model.mutableTerrainAt(12, 12);
    auto* east = model.mutableTerrainAt(13, 12);
    CHECK(west->setOuterHeight(20, 128, 7.0f));
    CHECK(east->setOuterHeight(20, 0, 99.0f));
    const auto seam = Editor::outerVertexPosition(12, 12, 20, 128);
    CHECK(Editor::raise(model, brushAt(seam, 1.0, 2.0)));
    CHECK(west->outerHeight(20, 128) == 9.0f);
    CHECK(east->outerHeight(20, 0) == 9.0f);
}

std::uint32_t u32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           static_cast<std::uint32_t>(data[offset + 1]) << 8 |
           static_cast<std::uint32_t>(data[offset + 2]) << 16 |
           static_cast<std::uint32_t>(data[offset + 3]) << 24;
}

float f32(const std::vector<std::uint8_t>& data, std::size_t offset) {
    const auto bits = u32(data, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void testValidationCompatibilityAndSerialization() {
    using Editor = worldwright::TerrainHeightEditor;
    auto model = oneTile(5, 6);
    const auto center = Editor::outerVertexPosition(5, 6, 1, 1);
    const auto before = model.terrainAt(5, 6)->outerHeight(1, 1);
    CHECK(!Editor::raise(model, brushAt(center, 0.0, 1.0)));
    CHECK(!Editor::raise(model, brushAt(center, -1.0, 1.0)));
    CHECK(!Editor::raise(model, brushAt(center, 1.0, -1.0)));
    CHECK(!Editor::smooth(model, brushAt(center, 1.0, 1.01)));
    CHECK(!Editor::raise(
        model, brushAt({std::numeric_limits<double>::quiet_NaN(), center.y},
                       1.0, 1.0)));
    CHECK(!Editor::raise(model,
                         brushAt(center, 1.0,
                                 std::numeric_limits<double>::infinity())));
    CHECK(model.terrainAt(5, 6)->outerHeight(1, 1) == before);

    auto overflow = oneTile(6, 6);
    auto* overflowTerrain = overflow.mutableTerrainAt(6, 6);
    CHECK(overflowTerrain->setFlatHeight(
        std::numeric_limits<float>::max()));
    const auto overflowCenter = Editor::outerVertexPosition(6, 6, 1, 1);
    CHECK(!Editor::raise(
        overflow,
        brushAt(overflowCenter, 1.0, std::numeric_limits<double>::max())));
    CHECK(overflow.terrainAt(6, 6)->outerHeight(1, 1) ==
          std::numeric_limits<float>::max());

    worldwright::TerrainDefinition terrain;
    CHECK(terrain.setFlatHeight(42.5f));
    CHECK(!terrain.setOuterHeight(
        0, 0, std::numeric_limits<float>::quiet_NaN()));
    CHECK(!terrain.setInnerHeight(
        0, 0, std::numeric_limits<float>::infinity()));
    CHECK(terrain.outerHeight(0, 0) == 42.5f);
    CHECK(terrain.innerHeight(0, 0) == 42.5f);

    CHECK(Editor::raise(model, brushAt(center, 1.0, 12.5)));
    const auto* edited = model.terrainAt(5, 6);
    CHECK(edited->outerHeight(1, 1) == 12.5f);
    worldwright::AdtWriter writer;
    std::vector<std::uint8_t> bytes;
    CHECK(writer.serialize(*edited, 5, 6, bytes));
    const auto firstMcnk = u32(bytes, 92);
    const auto header = firstMcnk + 8;
    const auto mcvtPayload = firstMcnk + u32(bytes, header + 20) + 8;
    const float base = f32(bytes, header + 112);
    CHECK(near(base + f32(bytes, mcvtPayload + 18 * 4), 12.5f));

    auto lifecycle = oneTile(8, 8);
    const auto lifecyclePoint = Editor::outerVertexPosition(8, 8, 10, 10);
    CHECK(Editor::raise(lifecycle, brushAt(lifecyclePoint, 1.0, 3.0)));
    CHECK(lifecycle.setTileEnabled(8, 8, false));
    CHECK(lifecycle.setTileEnabled(8, 8, true));
    CHECK(lifecycle.terrainAt(8, 8)->outerHeight(10, 10) == 0.0f);
}

}  // namespace

int main() {
    testGeometryRaiseLowerAndFalloff();
    testSmoothingSnapshotAndDeterminism();
    testCrossTileSeamsAndMissingTiles();
    testPreexistingMismatchPolicy();
    testValidationCompatibilityAndSerialization();
    std::cout << "Terrain editor checks: " << checks
              << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
