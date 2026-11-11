#ifndef WORLDWRIGHT_TERRAIN_CAMERA_H
#define WORLDWRIGHT_TERRAIN_CAMERA_H

#include "worldwright/TerrainMesh.h"

namespace worldwright {

struct CameraVector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

// Toolkit-independent orbit camera state. Angles are degrees, distance and
// target are terrain world units.
class TerrainCamera {
public:
    void reset() noexcept;
    void fit(const TerrainMeshBounds& bounds, double aspectRatio = 1.0) noexcept;
    void orbit(double yawDegrees, double pitchDegrees) noexcept;
    void pan(double screenRight, double screenUp) noexcept;
    void zoom(double wheelSteps) noexcept;

    CameraVector3 position() const noexcept;
    CameraVector3 target() const noexcept { return m_target; }
    double yaw() const noexcept { return m_yaw; }
    double pitch() const noexcept { return m_pitch; }
    double distance() const noexcept { return m_distance; }
    double nearPlane() const noexcept;
    double farPlane() const noexcept;

private:
    CameraVector3 m_target{};
    double m_yaw = -45.0;
    double m_pitch = -35.0;
    double m_distance = 1200.0;
};

}  // namespace worldwright

#endif
