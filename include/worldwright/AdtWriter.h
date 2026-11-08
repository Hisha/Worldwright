#ifndef WORLDWRIGHT_ADT_WRITER_H
#define WORLDWRIGHT_ADT_WRITER_H

#include "worldwright/TerrainDefinition.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace worldwright {

// Optional 3x3 terrain view used to calculate normals at tile boundaries.
// Offsets are relative to the tile being serialized. Missing/disabled tiles
// remain null and contribute no mesh faces.
struct AdtTerrainNeighborhood {
    bool setTerrain(int offsetX, int offsetY,
                    const TerrainDefinition* terrain) noexcept;
    const TerrainDefinition* terrainAt(int offsetX,
                                       int offsetY) const noexcept;

private:
    std::array<const TerrainDefinition*, 9> m_terrain{};
};

enum class AdtWriteError {
    None = 0,
    InvalidTerrain,
    InvalidTile,
    InvalidPath,
    OutputExists,
    OpenFailed,
    WriteFailed,
    ReplaceFailed,
};

struct AdtWriteOptions {
    bool overwrite = true;
};

class AdtWriter {
public:
    static constexpr std::uint32_t kVersion = 18;
    static constexpr float kTileSize = TerrainDefinition::TileSize;
    static constexpr float kChunkSize = kTileSize / 16.0f;
    static constexpr float kWorldCenter = kTileSize * 32.0f;
    static constexpr std::uint32_t kMcnkPayloadSize = 1228;
    static constexpr std::uint32_t kMcnkTotalSize = 1236;
    static constexpr std::size_t kSerializedSize =
        4292 + TerrainDefinition::ChunkCount * kMcnkTotalSize;

    bool serialize(const TerrainDefinition& terrain, int tileX, int tileY,
                   std::vector<std::uint8_t>& output);
    bool serialize(const TerrainDefinition& terrain, int tileX, int tileY,
                   const AdtTerrainNeighborhood& neighborhood,
                   std::vector<std::uint8_t>& output);
    bool save(const TerrainDefinition& terrain, int tileX, int tileY,
              const std::filesystem::path& outputPath,
              const AdtWriteOptions& options = {});
    bool save(const TerrainDefinition& terrain, int tileX, int tileY,
              const AdtTerrainNeighborhood& neighborhood,
              const std::filesystem::path& outputPath,
              const AdtWriteOptions& options = {});

    AdtWriteError error() const noexcept;
    const std::string& errorMessage() const noexcept;
    static const char* errorName(AdtWriteError error) noexcept;

private:
    bool serializeImpl(const TerrainDefinition& terrain, int tileX, int tileY,
                       const AdtTerrainNeighborhood& neighborhood,
                       std::vector<std::uint8_t>& output);
    bool saveImpl(const TerrainDefinition& terrain, int tileX, int tileY,
                  const AdtTerrainNeighborhood& neighborhood,
                  const std::filesystem::path& outputPath,
                  const AdtWriteOptions& options);
    bool fail(AdtWriteError error, std::string message);
    void reset() noexcept;

    AdtWriteError m_error = AdtWriteError::None;
    std::string m_errorMessage;
};

}  // namespace worldwright

#endif
