#ifndef WORLDWRIGHT_EDITOR_INTERACTION_H
#define WORLDWRIGHT_EDITOR_INTERACTION_H

#include "worldwright/WorldDefinition.h"

#include <vector>

namespace worldwright {

class WorldEditorModel;

enum class EditMode {
    Select,
    PaintTerrain,
    EraseTerrain,
    Pan,
};

struct MapView {
    double tileSize = 12.0;
    double panX = 0.0;
    double panY = 0.0;
};

class EditorInteraction {
public:
    static constexpr double MinTileSize = 2.0;
    static constexpr double MaxTileSize = 96.0;
    static constexpr double FitMargin = 24.0;

    // Fits and centers the complete square world in the supplied viewport.
    static MapView fitWorld(double viewportWidth,
                            double viewportHeight) noexcept;

    // Returns a clamped zoom while keeping the content point under the cursor
    // fixed in viewport coordinates.
    static MapView zoomAt(const MapView& view, double cursorX, double cursorY,
                          double factor) noexcept;

    // Supercover traversal between sampled grid cells. Returned coordinates
    // are unique, ordered, and clipped to the 64 x 64 world.
    static std::vector<TileCoord> gridLine(int firstX, int firstY, int lastX,
                                           int lastY);

    // Only paint and erase modes alter terrain. Select and pan are guaranteed
    // no-ops, which keeps mode behavior independently testable from Qt.
    static bool applyTerrainStroke(WorldEditorModel& model, EditMode mode,
                                   const std::vector<TileCoord>& tiles);
};

}  // namespace worldwright

#endif
