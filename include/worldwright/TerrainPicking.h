#ifndef WORLDWRIGHT_TERRAIN_PICKING_H
#define WORLDWRIGHT_TERRAIN_PICKING_H

#include "worldwright/TerrainCamera.h"
#include "worldwright/TerrainHeightEditor.h"
#include "worldwright/TerrainMesh.h"

#include <cstddef>
#include <optional>

namespace worldwright {

struct TerrainRay {
    CameraVector3 origin;
    CameraVector3 direction;
};

struct TerrainPickResult {
    TerrainPoint terrain;
    double elevation = 0.0;
    double distance = 0.0;
    TileCoord tile;
    std::size_t triangleFirstIndex = 0;
};

// Toolkit-independent ray construction and intersection against the exact
// render mesh. Render (x,y,z) maps to terrain (x,elevation,y).
class TerrainPicking {
public:
    static TerrainRay screenRay(const TerrainCamera& camera,
                                double normalizedX, double normalizedY,
                                double aspectRatio,
                                double verticalFovDegrees = 45.0) noexcept;
    static std::optional<TerrainPickResult> intersect(
        const TerrainMesh& mesh, const TerrainRay& ray) noexcept;
    static std::optional<double> surfaceElevation(
        const TerrainMesh& mesh, TerrainPoint point) noexcept;
};

}  // namespace worldwright

#endif
