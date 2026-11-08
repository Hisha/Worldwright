#include "worldwright/EditorInteraction.h"

#include "worldwright/WorldEditorModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace worldwright {

MapView EditorInteraction::fitWorld(double viewportWidth,
                                    double viewportHeight) noexcept {
    if (!std::isfinite(viewportWidth) || !std::isfinite(viewportHeight) ||
        viewportWidth <= 0.0 || viewportHeight <= 0.0) {
        return {};
    }
    const double availableWidth = std::max(1.0, viewportWidth - FitMargin * 2.0);
    const double availableHeight =
        std::max(1.0, viewportHeight - FitMargin * 2.0);
    // Fit is allowed below the normal interactive minimum so the complete
    // world remains visible even in an unusually small viewport.
    const double size = std::min(
        std::min(availableWidth / WorldDefinition::GridWidth,
                 availableHeight / WorldDefinition::GridHeight),
        MaxTileSize);
    const double worldWidth = WorldDefinition::GridWidth * size;
    const double worldHeight = WorldDefinition::GridHeight * size;
    return {size, (viewportWidth - worldWidth) / 2.0,
            (viewportHeight - worldHeight) / 2.0};
}

MapView EditorInteraction::zoomAt(const MapView& view, double cursorX,
                                  double cursorY, double factor) noexcept {
    if (!std::isfinite(view.tileSize) || view.tileSize <= 0.0 ||
        !std::isfinite(cursorX) || !std::isfinite(cursorY) ||
        !std::isfinite(factor) || factor <= 0.0) {
        return view;
    }
    const double newSize = std::clamp(view.tileSize * factor, MinTileSize,
                                      MaxTileSize);
    const double contentX = (cursorX - view.panX) / view.tileSize;
    const double contentY = (cursorY - view.panY) / view.tileSize;
    return {newSize, cursorX - contentX * newSize,
            cursorY - contentY * newSize};
}

std::vector<TileCoord> EditorInteraction::gridLine(int firstX, int firstY,
                                                    int lastX, int lastY) {
    std::vector<TileCoord> result;
    const long long dx = std::llabs(static_cast<long long>(lastX) - firstX);
    const long long dy = std::llabs(static_cast<long long>(lastY) - firstY);

    // Mouse coordinates are normally close to the world. This guard prevents
    // pathological off-widget coordinates from causing an unbounded loop.
    if (dx > 100000 || dy > 100000) {
        return result;
    }

    int x = firstX;
    int y = firstY;
    const int stepX = firstX < lastX ? 1 : -1;
    const int stepY = firstY < lastY ? 1 : -1;
    long long traversedX = 0;
    long long traversedY = 0;
    for (;;) {
        if (WorldDefinition::isValidTile(x, y)) {
            result.push_back({x, y});
        }
        if (traversedX == dx && traversedY == dy) {
            break;
        }
        const long long decision =
            (1 + 2 * traversedX) * dy - (1 + 2 * traversedY) * dx;
        if (decision == 0) {
            x += stepX;
            y += stepY;
            ++traversedX;
            ++traversedY;
        } else if (decision < 0) {
            x += stepX;
            ++traversedX;
        } else {
            y += stepY;
            ++traversedY;
        }
    }
    return result;
}

bool EditorInteraction::applyTerrainStroke(
    WorldEditorModel& model, EditMode mode,
    const std::vector<TileCoord>& tiles) {
    if (mode == EditMode::PaintTerrain) {
        return model.setTilesEnabled(tiles, true);
    }
    if (mode == EditMode::EraseTerrain) {
        return model.setTilesEnabled(tiles, false);
    }
    return false;
}

}  // namespace worldwright
