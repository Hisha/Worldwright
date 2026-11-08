#include "worldwright/TerrainDetailInteraction.h"

#include <algorithm>
#include <cmath>

namespace worldwright {

TerrainPoint TerrainDetailInteraction::screenToWorld(
    const TerrainDetailView& view, TerrainPoint screen) noexcept {
    if (!std::isfinite(view.pixelsPerUnit) || view.pixelsPerUnit <= 0.0) {
        return {};
    }
    return {(screen.x - view.panX) / view.pixelsPerUnit,
            (screen.y - view.panY) / view.pixelsPerUnit};
}

TerrainPoint TerrainDetailInteraction::worldToScreen(
    const TerrainDetailView& view, TerrainPoint world) noexcept {
    return {view.panX + world.x * view.pixelsPerUnit,
            view.panY + world.y * view.pixelsPerUnit};
}

double TerrainDetailInteraction::brushRadiusPixels(
    const TerrainDetailView& view, double radius) noexcept {
    if (!std::isfinite(view.pixelsPerUnit) || view.pixelsPerUnit <= 0.0 ||
        !std::isfinite(radius) || radius <= 0.0) {
        return 0.0;
    }
    return radius * view.pixelsPerUnit;
}

TerrainDetailView TerrainDetailInteraction::zoomAt(
    const TerrainDetailView& view, TerrainPoint cursor, double factor) noexcept {
    if (!std::isfinite(view.pixelsPerUnit) || view.pixelsPerUnit <= 0.0 ||
        !std::isfinite(cursor.x) || !std::isfinite(cursor.y) ||
        !std::isfinite(factor) || factor <= 0.0) {
        return view;
    }
    const auto world = screenToWorld(view, cursor);
    const double scale = std::clamp(view.pixelsPerUnit * factor,
                                    MinPixelsPerUnit, MaxPixelsPerUnit);
    return {scale, cursor.x - world.x * scale, cursor.y - world.y * scale};
}

TerrainDetailView TerrainDetailInteraction::fitActive(
    TileCoord active, double viewportWidth, double viewportHeight) noexcept {
    if (!WorldDefinition::isValidTile(active.x, active.y) ||
        !std::isfinite(viewportWidth) || !std::isfinite(viewportHeight) ||
        viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return {};
    }
    const int minTileX = std::max(0, active.x - 1);
    const int minTileY = std::max(0, active.y - 1);
    const int maxTileX = std::min(WorldDefinition::GridWidth - 1, active.x + 1);
    const int maxTileY = std::min(WorldDefinition::GridHeight - 1, active.y + 1);
    const double minX = minTileX * TerrainHeightEditor::TileSize;
    const double minY = minTileY * TerrainHeightEditor::TileSize;
    const double spanX = (maxTileX - minTileX + 1) *
                         TerrainHeightEditor::TileSize;
    const double spanY = (maxTileY - minTileY + 1) *
                         TerrainHeightEditor::TileSize;
    const double availableWidth = std::max(1.0, viewportWidth - 2 * FitMargin);
    const double availableHeight = std::max(1.0, viewportHeight - 2 * FitMargin);
    const double scale = std::clamp(std::min(availableWidth / spanX,
                                             availableHeight / spanY),
                                    MinPixelsPerUnit, MaxPixelsPerUnit);
    return {scale, (viewportWidth - spanX * scale) / 2.0 - minX * scale,
            (viewportHeight - spanY * scale) / 2.0 - minY * scale};
}

bool TerrainDetailInteraction::validBrushParameters(double radius,
                                                     double strength,
                                                     bool smoothing) noexcept {
    return std::isfinite(radius) && radius > 0.0 &&
           std::isfinite(strength) && strength > 0.0 &&
           (!smoothing || strength <= 1.0);
}

std::vector<TerrainPoint> TerrainStrokeSampler::begin(TerrainPoint point,
                                                       double spacing) {
    reset();
    if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
        !std::isfinite(spacing) || spacing <= 0.0) {
        return {};
    }
    m_lastInput = point;
    m_spacing = spacing;
    m_distanceToNext = spacing;
    m_active = true;
    return {point};
}

std::vector<TerrainPoint> TerrainStrokeSampler::append(TerrainPoint point) {
    std::vector<TerrainPoint> result;
    if (!m_active || !std::isfinite(point.x) || !std::isfinite(point.y)) {
        return result;
    }
    double dx = point.x - m_lastInput.x;
    double dy = point.y - m_lastInput.y;
    double remaining = std::hypot(dx, dy);
    if (remaining <= 0.0) {
        return result;
    }
    TerrainPoint cursor = m_lastInput;
    while (remaining + 1e-12 >= m_distanceToNext) {
        const double fraction = m_distanceToNext / remaining;
        cursor.x += dx * fraction;
        cursor.y += dy * fraction;
        result.push_back(cursor);
        dx = point.x - cursor.x;
        dy = point.y - cursor.y;
        remaining = std::hypot(dx, dy);
        m_distanceToNext = m_spacing;
    }
    m_distanceToNext -= remaining;
    m_lastInput = point;
    return result;
}

void TerrainStrokeSampler::reset() noexcept {
    m_spacing = 0.0;
    m_distanceToNext = 0.0;
    m_active = false;
}

bool TerrainStrokeSampler::active() const noexcept { return m_active; }

}  // namespace worldwright
