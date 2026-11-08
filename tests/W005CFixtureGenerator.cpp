#include "worldwright/TerrainHeightEditor.h"
#include "worldwright/WorldEditorModel.h"
#include "worldwright/WorldExporter.h"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " OUTPUT_DIRECTORY\n";
        return 2;
    }
    const std::filesystem::path output = argv[1];
    worldwright::WorldEditorModel model;
    if (!model.newWorld("NewWorld")) return 1;
    for (int y = 30; y <= 32; ++y)
        for (int x = 30; x <= 32; ++x)
            if (!model.setTileEnabled(x, y, true)) return 1;

    using worldwright::TerrainHeightEditor;
    if (!TerrainHeightEditor::raise(
            model, {{31.0 * TerrainHeightEditor::TileSize,
                     30.5 * TerrainHeightEditor::TileSize},
                    130.0, 90.0}) ||
        !TerrainHeightEditor::lower(
            model, {{31.5 * TerrainHeightEditor::TileSize,
                     31.0 * TerrainHeightEditor::TileSize},
                    95.0, 65.0}) ||
        !TerrainHeightEditor::raise(
            model, {{31.0 * TerrainHeightEditor::TileSize,
                     31.0 * TerrainHeightEditor::TileSize},
                    145.0, 120.0}) ||
        !TerrainHeightEditor::smooth(
            model, {{31.0 * TerrainHeightEditor::TileSize,
                     31.0 * TerrainHeightEditor::TileSize},
                    155.0, 0.4})) {
        std::cerr << "could not construct sculpted fixture\n";
        return 1;
    }

    worldwright::WorldExporter exporter;
    if (!exporter.exportWorld(model, output)) {
        std::cerr << exporter.errorMessage() << '\n';
        return 1;
    }
    return 0;
}
