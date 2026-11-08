#ifndef WORLDWRIGHT_TERRAIN_DETAIL_INTERACTION_H
#define WORLDWRIGHT_TERRAIN_DETAIL_INTERACTION_H

#include "worldwright/TerrainHeightEditor.h"
#include "worldwright/WorldDefinition.h"

#include <vector>

namespace worldwright {

// Toolkit-independent view and stroke calculations used by the terrain-detail
// canvas. World coordinates are the same file/tile coordinates used by W005A.
struct TerrainDetailView {
    double pixelsPerUnit = 1.0;
    double panX = 0.0;
    double panY = 0.0;
};

class TerrainDetailInteraction {
public:
    static constexpr double MinPixelsPerUnit = 0.02;
    static constexpr double MaxPixelsPerUnit = 8.0;
    static constexpr double FitMargin = 32.0;

    static TerrainPoint screenToWorld(const TerrainDetailView& view,
                                      TerrainPoint screen) noexcept;
    static TerrainPoint worldToScreen(const TerrainDetailView& view,
                                      TerrainPoint world) noexcept;
    static double brushRadiusPixels(const TerrainDetailView& view,
                                    double radius) noexcept;
    static TerrainDetailView zoomAt(const TerrainDetailView& view,
                                    TerrainPoint cursor, double factor) noexcept;

    // Frames the active ADT plus one ADT of context in every in-world
    // direction. The context can contain enabled neighbors or unavailable
    // terrain and therefore makes edges explicit.
    static TerrainDetailView fitActive(TileCoord active, double viewportWidth,
                                       double viewportHeight) noexcept;

    static bool validBrushParameters(double radius, double strength,
                                     bool smoothing) noexcept;
};

// Incremental, distance-based stroke sampler. Samples are exactly `spacing`
// world units apart along the input polyline, irrespective of how many mouse
// move events partition that path. begin() emits the initial application.
class TerrainStrokeSampler {
public:
    std::vector<TerrainPoint> begin(TerrainPoint point, double spacing);
    std::vector<TerrainPoint> append(TerrainPoint point);
    void reset() noexcept;
    bool active() const noexcept;

private:
    TerrainPoint m_lastInput{};
    double m_spacing = 0.0;
    double m_distanceToNext = 0.0;
    bool m_active = false;
};

}  // namespace worldwright

#endif
