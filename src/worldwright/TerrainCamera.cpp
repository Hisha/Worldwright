#include "worldwright/TerrainCamera.h"

#include <algorithm>
#include <cmath>

namespace worldwright {
namespace {
constexpr double pi = 3.14159265358979323846;
double radians(double degrees) noexcept { return degrees * pi / 180.0; }
}  // namespace

void TerrainCamera::reset() noexcept {
    m_yaw = -45.0;
    m_pitch = -35.0;
}

void TerrainCamera::fit(const TerrainMeshBounds& bounds,
                        double aspectRatio) noexcept {
    if (!bounds.valid) {
        m_target = {};
        m_distance = 1200.0;
        return;
    }
    m_target = {(bounds.minimumX + bounds.maximumX) * 0.5,
                (bounds.minimumY + bounds.maximumY) * 0.5,
                (bounds.minimumZ + bounds.maximumZ) * 0.5};
    const double width = bounds.maximumX - bounds.minimumX;
    const double height = bounds.maximumY - bounds.minimumY;
    const double depth = bounds.maximumZ - bounds.minimumZ;
    const double horizontal = std::max(width, depth) /
                              std::max(0.25, aspectRatio);
    m_distance = std::max(10.0, std::max({horizontal, height * 2.0, depth}) *
                                    1.15);
}

void TerrainCamera::orbit(double yawDegrees, double pitchDegrees) noexcept {
    if (!std::isfinite(yawDegrees) || !std::isfinite(pitchDegrees)) return;
    m_yaw = std::fmod(m_yaw + yawDegrees, 360.0);
    m_pitch = std::clamp(m_pitch + pitchDegrees, -89.0, -5.0);
}

void TerrainCamera::pan(double screenRight, double screenUp) noexcept {
    if (!std::isfinite(screenRight) || !std::isfinite(screenUp)) return;
    const double yawRadians = radians(m_yaw);
    const double pitchRadians = radians(m_pitch);
    const CameraVector3 right{-std::sin(yawRadians), 0.0,
                              std::cos(yawRadians)};
    const CameraVector3 up{-std::cos(yawRadians) * std::sin(pitchRadians),
                           std::cos(pitchRadians),
                           -std::sin(yawRadians) * std::sin(pitchRadians)};
    m_target.x += right.x * screenRight + up.x * screenUp;
    m_target.y += right.y * screenRight + up.y * screenUp;
    m_target.z += right.z * screenRight + up.z * screenUp;
}

void TerrainCamera::zoom(double wheelSteps) noexcept {
    if (!std::isfinite(wheelSteps)) return;
    m_distance = std::clamp(m_distance * std::pow(0.85, wheelSteps), 1.0,
                            1000000.0);
}

CameraVector3 TerrainCamera::position() const noexcept {
    const double yawRadians = radians(m_yaw);
    const double pitchRadians = radians(m_pitch);
    const double horizontal = m_distance * std::cos(pitchRadians);
    return {m_target.x - horizontal * std::cos(yawRadians),
            m_target.y - m_distance * std::sin(pitchRadians),
            m_target.z - horizontal * std::sin(yawRadians)};
}

double TerrainCamera::nearPlane() const noexcept {
    return std::max(0.1, m_distance / 5000.0);
}

double TerrainCamera::farPlane() const noexcept {
    return std::max(1000.0, m_distance * 8.0);
}

}  // namespace worldwright
