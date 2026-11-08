#include "worldwright/AdtWriter.h"
#include "worldwright/EditorInteraction.h"
#include "worldwright/WdtReader.h"
#include "worldwright/WorldEditorModel.h"
#include "worldwright/WorldExporter.h"
#include "worldwright/WdtWriter.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
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

class TempDir {
public:
    TempDir() {
        m_path = std::filesystem::temp_directory_path() /
                 ("worldwright-editor-tests-" +
                  std::to_string(std::chrono::steady_clock::now()
                                     .time_since_epoch()
                                     .count()));
        std::filesystem::create_directories(m_path);
    }
    ~TempDir() {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }
    const std::filesystem::path& path() const { return m_path; }

private:
    std::filesystem::path m_path;
};

std::vector<std::uint8_t> readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const auto size = file.tellg();
    if (size < 0) {
        return {};
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
    }
    return bytes;
}

void testNewWorldAndNames() {
    worldwright::WorldEditorModel model;
    std::string error;
    CHECK(model.newWorld("NewWorld", &error));
    CHECK(model.worldName() == "NewWorld");
    CHECK(model.world().empty());
    CHECK(model.selection().empty());
    CHECK(!model.newWorld("bad world", &error));
    CHECK(!error.empty());
    CHECK(model.worldName() == "NewWorld");
    CHECK(!worldwright::WorldEditorModel::validateWorldName("9World"));
    CHECK(worldwright::WorldEditorModel::validateWorldName("World_335a"));
}

void testCoordinateMappingAndRectangle() {
    const auto origin = worldwright::WorldEditorModel::tileAt(0.0, 0.0, 10.0);
    CHECK(origin == (worldwright::TileCoord{0, 0}));
    const auto center =
        worldwright::WorldEditorModel::tileAt(305.0, 319.99, 10.0);
    CHECK(center == (worldwright::TileCoord{30, 31}));
    CHECK(!worldwright::WorldEditorModel::tileAt(-0.1, 0.0, 10.0));
    CHECK(!worldwright::WorldEditorModel::tileAt(640.0, 0.0, 10.0));
    CHECK(!worldwright::WorldEditorModel::tileAt(1.0, 1.0, 0.0));

    const auto rectangle =
        worldwright::WorldEditorModel::rectangleTiles(32, 32, 30, 30);
    CHECK(rectangle.size() == 9);
    CHECK(rectangle.front() == (worldwright::TileCoord{30, 30}));
    CHECK(rectangle.back() == (worldwright::TileCoord{32, 32}));
    const auto clipped =
        worldwright::WorldEditorModel::rectangleTiles(-3, -2, 1, 1);
    CHECK(clipped.size() == 4);
    CHECK(clipped.front() == (worldwright::TileCoord{0, 0}));
}

bool near(double lhs, double rhs, double epsilon = 0.000001) {
    return std::abs(lhs - rhs) <= epsilon;
}

void testMapViewCalculations() {
    const auto initial = worldwright::EditorInteraction::fitWorld(800.0, 600.0);
    CHECK(near(initial.tileSize, 8.625));
    CHECK(near(initial.panX, 124.0));
    CHECK(near(initial.panY, 24.0));

    const auto resized =
        worldwright::EditorInteraction::fitWorld(1000.0, 800.0);
    CHECK(near(resized.tileSize, 11.75));
    CHECK(near(resized.panX, 124.0));
    CHECK(near(resized.panY, 24.0));
    const auto tiny = worldwright::EditorInteraction::fitWorld(100.0, 100.0);
    CHECK(near(tiny.tileSize, 0.8125));
    CHECK(near(tiny.panX, 24.0));
    CHECK(near(tiny.panY, 24.0));

    const worldwright::MapView view{10.0, 20.0, 30.0};
    const auto zoomed =
        worldwright::EditorInteraction::zoomAt(view, 220.0, 180.0, 2.0);
    CHECK(near(zoomed.tileSize, 20.0));
    CHECK(near((220.0 - zoomed.panX) / zoomed.tileSize, 20.0));
    CHECK(near((180.0 - zoomed.panY) / zoomed.tileSize, 15.0));

    const auto minimum =
        worldwright::EditorInteraction::zoomAt(view, 0.0, 0.0, 0.00001);
    const auto maximum =
        worldwright::EditorInteraction::zoomAt(view, 0.0, 0.0, 10000.0);
    CHECK(near(minimum.tileSize,
               worldwright::EditorInteraction::MinTileSize));
    CHECK(near(maximum.tileSize,
               worldwright::EditorInteraction::MaxTileSize));
}

void testStrokeTraversalAndModes() {
    using worldwright::EditMode;
    using worldwright::EditorInteraction;

    const auto single = EditorInteraction::gridLine(7, 9, 7, 9);
    CHECK(single.size() == 1);
    CHECK(single.front() == (worldwright::TileCoord{7, 9}));

    const auto horizontal = EditorInteraction::gridLine(2, 4, 8, 4);
    CHECK(horizontal.size() == 7);
    CHECK(horizontal.front() == (worldwright::TileCoord{2, 4}));
    CHECK(horizontal.back() == (worldwright::TileCoord{8, 4}));

    const auto diagonal = EditorInteraction::gridLine(10, 10, 15, 15);
    CHECK(diagonal.size() == 6);
    for (std::size_t i = 0; i < diagonal.size(); ++i) {
        CHECK(diagonal[i] ==
              (worldwright::TileCoord{10 + static_cast<int>(i),
                                      10 + static_cast<int>(i)}));
    }
    const auto shallow = EditorInteraction::gridLine(0, 0, 5, 2);
    CHECK(shallow.size() == 8);
    CHECK(shallow.front() == (worldwright::TileCoord{0, 0}));
    CHECK(shallow.back() == (worldwright::TileCoord{5, 2}));

    const auto clipped = EditorInteraction::gridLine(-4, 3, 3, 3);
    CHECK(clipped.size() == 4);
    CHECK(clipped.front() == (worldwright::TileCoord{0, 3}));
    CHECK(clipped.back() == (worldwright::TileCoord{3, 3}));

    worldwright::WorldEditorModel model;
    CHECK(EditorInteraction::applyTerrainStroke(model, EditMode::PaintTerrain,
                                                 single));
    CHECK(model.world().hasTile(7, 9));
    CHECK(model.elevationAt(7, 9) == 0.0f);
    model.selectTile(7, 9);
    CHECK(model.setSelectedElevation(42.0f));
    CHECK(!EditorInteraction::applyTerrainStroke(model, EditMode::PaintTerrain,
                                                  single));
    CHECK(model.elevationAt(7, 9) == 42.0f);

    const auto before = model.world().tileCount();
    CHECK(!EditorInteraction::applyTerrainStroke(model, EditMode::Select,
                                                  horizontal));
    CHECK(model.world().tileCount() == before);
    CHECK(!EditorInteraction::applyTerrainStroke(model, EditMode::Pan,
                                                  horizontal));
    CHECK(model.world().tileCount() == before);
    CHECK(EditorInteraction::applyTerrainStroke(model, EditMode::EraseTerrain,
                                                 single));
    CHECK(!model.world().hasTile(7, 9));
}

void testTileEditingAndElevation() {
    worldwright::WorldEditorModel model;
    CHECK(model.newWorld("NewWorld"));
    model.selectRectangle(30, 30, 32, 32);
    CHECK(model.selection().size() == 9);
    CHECK(model.setSelectedEnabled(true));
    CHECK(model.world().tileCount() == 9);
    for (const auto tile : model.selection()) {
        CHECK(model.terrainAt(tile.x, tile.y) != nullptr);
        CHECK(model.elevationAt(tile.x, tile.y) == 0.0f);
    }
    CHECK(model.setSelectedElevation(100.0f));
    for (const auto tile : model.selection()) {
        CHECK(model.elevationAt(tile.x, tile.y) == 100.0f);
        CHECK(model.terrainAt(tile.x, tile.y)->isValid());
    }
    CHECK(!model.setSelectedElevation(
        std::numeric_limits<float>::quiet_NaN()));
    model.selectTile(31, 31);
    CHECK(model.setSelectedEnabled(false));
    CHECK(!model.world().hasTile(31, 31));
    CHECK(model.terrainAt(31, 31) == nullptr);
}

void testNineTileExport() {
    TempDir temp;
    worldwright::WorldEditorModel model;
    CHECK(model.newWorld("NewWorld"));
    model.selectRectangle(30, 30, 32, 32);
    CHECK(model.setSelectedEnabled(true));
    CHECK(model.setSelectedElevation(100.0f));

    worldwright::WorldExporter exporter;
    CHECK(exporter.exportWorld(model, temp.path()));
    const auto paths = worldwright::WorldExporter::outputPaths(model, temp.path());
    CHECK(paths.size() == 10);
    for (const auto& path : paths) {
        CHECK(std::filesystem::is_regular_file(path));
    }
    CHECK(std::filesystem::file_size(temp.path() / "NewWorld.wdt") ==
          worldwright::WdtWriter::kSerializedSize);

    worldwright::WdtReader reader;
    CHECK(reader.load(temp.path() / "NewWorld.wdt"));
    CHECK(reader.tileCount() == 9);
    CHECK(reader.hasTile(30, 30));
    CHECK(reader.hasTile(32, 32));

    const auto* terrain = model.terrainAt(30, 30);
    CHECK(terrain != nullptr);
    worldwright::AdtWriter writer;
    std::vector<std::uint8_t> expected;
    CHECK(writer.serialize(*terrain, 30, 30, expected));
    CHECK(readFile(temp.path() / "NewWorld_30_30.adt") == expected);

    CHECK(!exporter.exportWorld(model, temp.path()));
    CHECK(exporter.error() == worldwright::WorldExportError::OutputExists);
    CHECK(exporter.exportWorld(model, temp.path(), true));
}

void testNineTileExportAfterPainting() {
    TempDir temp;
    worldwright::WorldEditorModel model;
    CHECK(model.newWorld("PaintedWorld"));
    for (int y = 30; y <= 32; ++y) {
        const auto stroke =
            worldwright::EditorInteraction::gridLine(30, y, 32, y);
        CHECK(worldwright::EditorInteraction::applyTerrainStroke(
            model, worldwright::EditMode::PaintTerrain, stroke));
    }
    CHECK(model.world().tileCount() == 9);
    model.selectRectangle(30, 30, 32, 32);
    CHECK(model.setSelectedElevation(100.0f));

    worldwright::WorldExporter exporter;
    CHECK(exporter.exportWorld(model, temp.path()));
    worldwright::WdtReader reader;
    CHECK(reader.load(temp.path() / "PaintedWorld.wdt"));
    CHECK(reader.tileCount() == 9);
    for (int y = 30; y <= 32; ++y) {
        for (int x = 30; x <= 32; ++x) {
            CHECK(reader.hasTile(x, y));
            CHECK(std::filesystem::is_regular_file(
                temp.path() / ("PaintedWorld_" + std::to_string(x) + "_" +
                               std::to_string(y) + ".adt")));
        }
    }
}

}  // namespace

int main() {
    testNewWorldAndNames();
    testCoordinateMappingAndRectangle();
    testMapViewCalculations();
    testStrokeTraversalAndModes();
    testTileEditingAndElevation();
    testNineTileExport();
    testNineTileExportAfterPainting();
    std::cout << "Editor core checks: " << checks
              << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
