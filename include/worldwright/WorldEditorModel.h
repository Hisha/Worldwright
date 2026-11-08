#ifndef WORLDWRIGHT_WORLD_EDITOR_MODEL_H
#define WORLDWRIGHT_WORLD_EDITOR_MODEL_H

#include "worldwright/TerrainDefinition.h"
#include "worldwright/WorldDefinition.h"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace worldwright {

// Toolkit-independent state used by the graphical editor. WorldDefinition is
// deliberately the authority for tile availability; terrain entries exist
// only while their corresponding WorldDefinition tile is enabled.
class WorldEditorModel {
public:
    WorldEditorModel();

    static bool validateWorldName(const std::string& name,
                                  std::string* errorMessage = nullptr);

    bool newWorld(const std::string& name,
                  std::string* errorMessage = nullptr);
    bool setWorldName(const std::string& name,
                      std::string* errorMessage = nullptr);

    const std::string& worldName() const noexcept;
    const WorldDefinition& world() const noexcept;

    void selectTile(int x, int y);
    void selectRectangle(int firstX, int firstY, int lastX, int lastY);
    void clearSelection() noexcept;
    const std::vector<TileCoord>& selection() const noexcept;
    std::optional<TileCoord> primarySelection() const noexcept;
    bool isSelected(int x, int y) const noexcept;

    bool setSelectedEnabled(bool enabled);
    bool setSelectedElevation(float elevation);
    bool setTileEnabled(int x, int y, bool enabled);
    bool setTilesEnabled(const std::vector<TileCoord>& tiles, bool enabled);

    const TerrainDefinition* terrainAt(int x, int y) const noexcept;
    std::optional<float> elevationAt(int x, int y) const noexcept;

    // Maps a point in grid-content coordinates to a tile. The origin is the
    // top-left corner, x grows right, and y grows down.
    static std::optional<TileCoord> tileAt(double contentX, double contentY,
                                           double tileSize) noexcept;

    // Inclusive, normalized and grid-clipped rectangle coordinates.
    static std::vector<TileCoord> rectangleTiles(int firstX, int firstY,
                                                  int lastX, int lastY);

private:
    static std::size_t tileIndex(int x, int y) noexcept;

    std::string m_worldName;
    WorldDefinition m_world;
    std::unordered_map<std::size_t, TerrainDefinition> m_terrain;
    std::vector<TileCoord> m_selection;
};

}  // namespace worldwright

#endif
