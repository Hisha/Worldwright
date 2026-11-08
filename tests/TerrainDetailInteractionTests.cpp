#include "worldwright/EditorInteraction.h"
#include "worldwright/TerrainDetailInteraction.h"
#include "worldwright/TerrainHeightEditor.h"
#include "worldwright/WorldEditorModel.h"
#include "worldwright/WorldExporter.h"
#include "worldwright/AdtWriter.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

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

bool near(double a, double b, double epsilon = 1e-7) {
    return std::abs(a - b) <= epsilon;
}

void testCoordinateMappingRadiusZoomAndFit() {
    using worldwright::TerrainDetailInteraction;
    const worldwright::TerrainDetailView view{2.5, -100.0, 75.0};
    const worldwright::TerrainPoint world{123.25, 456.5};
    const auto screen = TerrainDetailInteraction::worldToScreen(view, world);
    const auto restored = TerrainDetailInteraction::screenToWorld(view, screen);
    CHECK(near(restored.x, world.x));
    CHECK(near(restored.y, world.y));
    CHECK(near(TerrainDetailInteraction::brushRadiusPixels(view, 12.0), 30.0));

    const worldwright::TerrainPoint cursor{320.0, 240.0};
    const auto before = TerrainDetailInteraction::screenToWorld(view, cursor);
    const auto zoomed = TerrainDetailInteraction::zoomAt(view, cursor, 1.7);
    const auto after = TerrainDetailInteraction::screenToWorld(zoomed, cursor);
    CHECK(near(before.x, after.x));
    CHECK(near(before.y, after.y));

    const auto fitted = TerrainDetailInteraction::fitActive({30, 31}, 900, 600);
    const auto activeCenter = TerrainDetailInteraction::worldToScreen(
        fitted, {(30.5) * worldwright::TerrainHeightEditor::TileSize,
                 (31.5) * worldwright::TerrainHeightEditor::TileSize});
    CHECK(activeCenter.x > 0.0 && activeCenter.x < 900.0);
    CHECK(activeCenter.y > 0.0 && activeCenter.y < 600.0);
}

void testBrushValidation() {
    using Interaction = worldwright::TerrainDetailInteraction;
    CHECK(Interaction::validBrushParameters(10.0, 2.0, false));
    CHECK(Interaction::validBrushParameters(10.0, 0.5, true));
    CHECK(!Interaction::validBrushParameters(0.0, 1.0, false));
    CHECK(!Interaction::validBrushParameters(-1.0, 1.0, false));
    CHECK(!Interaction::validBrushParameters(1.0, 0.0, false));
    CHECK(!Interaction::validBrushParameters(1.0, 1.01, true));
    CHECK(!Interaction::validBrushParameters(
        std::numeric_limits<double>::infinity(), 1.0, false));
}

void testDeterministicContinuousStroke() {
    worldwright::TerrainStrokeSampler sparse;
    auto sparsePoints = sparse.begin({0.0, 0.0}, 10.0);
    const auto sparseTail = sparse.append({100.0, 0.0});
    sparsePoints.insert(sparsePoints.end(), sparseTail.begin(), sparseTail.end());

    worldwright::TerrainStrokeSampler frequent;
    auto frequentPoints = frequent.begin({0.0, 0.0}, 10.0);
    for (int x = 1; x <= 100; ++x) {
        const auto part = frequent.append({static_cast<double>(x), 0.0});
        frequentPoints.insert(frequentPoints.end(), part.begin(), part.end());
    }
    CHECK(sparsePoints.size() == 11);
    CHECK(frequentPoints.size() == sparsePoints.size());
    for (std::size_t i = 0; i < sparsePoints.size(); ++i) {
        CHECK(near(sparsePoints[i].x, frequentPoints[i].x));
        CHECK(near(sparsePoints[i].y, frequentPoints[i].y));
    }

    worldwright::TerrainStrokeSampler segmented;
    auto points = segmented.begin({0.0, 0.0}, 7.5);
    for (const double x : {3.0, 11.0, 29.0, 51.0}) {
        const auto part = segmented.append({x, 0.0});
        points.insert(points.end(), part.begin(), part.end());
    }
    CHECK(points.size() == 7);
    CHECK(near(points.back().x, 45.0));
}

void testEditingStateAndExistingLayoutOperations() {
    using Editor = worldwright::TerrainHeightEditor;
    worldwright::WorldEditorModel model;
    CHECK(model.setTileEnabled(20, 20, true));
    CHECK(model.setTileEnabled(21, 20, true));
    model.selectTile(20, 20);
    const auto center = Editor::outerVertexPosition(20, 20, 64, 128);
    CHECK(Editor::raise(model, {center, 12.0, 4.0}));
    const float west = model.terrainAt(20, 20)->outerHeight(64, 128);
    const float east = model.terrainAt(21, 20)->outerHeight(64, 0);
    CHECK(west == 4.0f);
    CHECK(east == west);

    // Workspace changes are views over the same model; changing selection
    // (the active-ADT source) cannot reconstruct terrain.
    model.selectTile(21, 20);
    CHECK(model.terrainAt(20, 20)->outerHeight(64, 128) == west);
    model.selectTile(20, 20);
    CHECK(model.terrainAt(21, 20)->outerHeight(64, 0) == east);

    const auto layoutStroke = worldwright::EditorInteraction::gridLine(22, 20,
                                                                        24, 20);
    CHECK(worldwright::EditorInteraction::applyTerrainStroke(
        model, worldwright::EditMode::PaintTerrain, layoutStroke));
    CHECK(model.world().hasTile(22, 20));
    CHECK(model.world().hasTile(24, 20));
    CHECK(model.terrainAt(20, 20)->outerHeight(64, 128) == west);
}

void testExportUsesSculptedGeometry() {
    namespace fs = std::filesystem;
    using Editor = worldwright::TerrainHeightEditor;
    worldwright::WorldEditorModel model;
    CHECK(model.newWorld("SculptedWorld"));
    CHECK(model.setTileEnabled(7, 8, true));
    const auto point = Editor::outerVertexPosition(7, 8, 12, 13);
    CHECK(Editor::raise(model, {point, 1.0, 9.5}));

    const auto directory = fs::temp_directory_path() /
        ("worldwright-detail-export-" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch().count()));
    CHECK(fs::create_directories(directory));
    worldwright::WorldExporter exporter;
    CHECK(exporter.exportWorld(model, directory));

    worldwright::AdtWriter writer;
    std::vector<std::uint8_t> expected;
    CHECK(writer.serialize(*model.terrainAt(7, 8), 7, 8, expected));
    std::ifstream file(directory / "SculptedWorld_7_8.adt",
                       std::ios::binary | std::ios::ate);
    CHECK(file.good());
    const auto size = file.tellg();
    std::vector<std::uint8_t> actual(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(actual.data()),
              static_cast<std::streamsize>(actual.size()));
    CHECK(actual == expected);
    std::error_code ignored;
    fs::remove_all(directory, ignored);
}

}  // namespace

int main() {
    testCoordinateMappingRadiusZoomAndFit();
    testBrushValidation();
    testDeterministicContinuousStroke();
    testEditingStateAndExistingLayoutOperations();
    testExportUsesSculptedGeometry();
    std::cout << "Terrain detail checks: " << checks
              << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
