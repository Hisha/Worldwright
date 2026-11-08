#ifndef WORLDWRIGHT_TERRAIN_HEIGHT_EDITOR_H
#define WORLDWRIGHT_TERRAIN_HEIGHT_EDITOR_H

#include "worldwright/TerrainDefinition.h"

namespace worldwright {

class WorldEditorModel;

struct TerrainPoint {
    double x = 0.0;
    double y = 0.0;
};

struct TerrainBrush {
    TerrainPoint center;
    double radius = 0.0;
    double strength = 0.0;
};

// Qt-independent height operations over enabled WorldEditorModel tiles.
// Editing space follows WDT/file tile coordinates: X increases with tileX and
// outer-grid columns, Y with tileY and outer-grid rows. One tile spans
// [tile * TileSize, (tile + 1) * TileSize]. Outer vertices lie on whole cell
// intersections; inner vertices are offset by half a cell on both axes.
class TerrainHeightEditor {
public:
    static constexpr double TileSize = TerrainDefinition::TileSize;
    static constexpr double CellSize =
        TileSize / TerrainDefinition::InnerVerticesPerTileSide;

    // Strength is an elevation delta. It must be finite and non-negative.
    static bool raise(WorldEditorModel& model,
                      const TerrainBrush& brush);
    static bool lower(WorldEditorModel& model,
                      const TerrainBrush& brush);

    // Strength is a blend fraction in [0, 1]. Each vertex blends toward the
    // snapshot average of directly connected vertices of the other grid.
    static bool smooth(WorldEditorModel& model,
                       const TerrainBrush& brush);

    // Linear falloff: max(0, 1 - distance / radius).
    static double influence(double distance, double radius) noexcept;

    static TerrainPoint outerVertexPosition(int tileX, int tileY, int row,
                                            int column) noexcept;
    static TerrainPoint innerVertexPosition(int tileX, int tileY, int row,
                                            int column) noexcept;
};

}  // namespace worldwright

#endif
