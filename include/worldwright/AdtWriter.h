#ifndef WORLDWRIGHT_ADT_WRITER_H
#define WORLDWRIGHT_ADT_WRITER_H

#include "worldwright/TerrainDefinition.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace worldwright {

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
    bool save(const TerrainDefinition& terrain, int tileX, int tileY,
              const std::filesystem::path& outputPath,
              const AdtWriteOptions& options = {});

    AdtWriteError error() const noexcept;
    const std::string& errorMessage() const noexcept;
    static const char* errorName(AdtWriteError error) noexcept;

private:
    bool fail(AdtWriteError error, std::string message);
    void reset() noexcept;

    AdtWriteError m_error = AdtWriteError::None;
    std::string m_errorMessage;
};

}  // namespace worldwright

#endif
