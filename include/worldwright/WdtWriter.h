#ifndef WORLDWRIGHT_WDT_WRITER_H
#define WORLDWRIGHT_WDT_WRITER_H

#include "worldwright/WorldDefinition.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace worldwright {

// Failure categories reported by WdtWriter::save().
enum class WdtWriteError {
    None = 0,
    InvalidPath,
    OutputExists,
    OpenFailed,
    WriteFailed,
    ReplaceFailed,
};

struct WdtWriteOptions {
    // When false, save() refuses to replace an existing destination. The CLI
    // defaults this to false and enables it only with --overwrite.
    bool overwrite = true;
};

// Serializes a WorldDefinition into a World of Warcraft 3.3.5a terrain WDT.
//
// The generated file contains exactly three chunks in this order:
//   MVER (version 18), MPHD (32-byte header), MAIN (4096 eight-byte entries).
// It is deterministic: the same WorldDefinition always produces byte-identical
// output, with no timestamps, random data, or host-dependent fields.
class WdtWriter {
public:
    static constexpr std::uint32_t kVersion = 18;

    static constexpr std::uint32_t kMphdPayloadSize = 32;
    static constexpr std::uint32_t kMainEntrySize = 8;
    static constexpr std::size_t kMainEntryCount = WorldDefinition::TileCapacity;
    static constexpr std::size_t kMainPayloadSize =
        kMainEntryCount * static_cast<std::size_t>(kMainEntrySize);

    static constexpr std::size_t kMverChunkSize = 8 + 4;
    static constexpr std::size_t kMphdChunkSize = 8 + kMphdPayloadSize;
    static constexpr std::size_t kMainChunkSize = 8 + kMainPayloadSize;
    static constexpr std::size_t kSerializedSize =
        kMverChunkSize + kMphdChunkSize + kMainChunkSize;

    // Pure in-memory serialization. The returned bytes are exactly what save()
    // writes. This is useful for tests and for future export pipelines.
    static std::vector<std::uint8_t> serialize(const WorldDefinition& world);

    // Serializes and writes a complete WDT. The write goes to a temporary file
    // in the destination directory and is then renamed into place. On failure
    // the previous destination (if any) is left untouched and the temporary
    // file is removed.
    bool save(const WorldDefinition& world,
              const std::filesystem::path& outputPath,
              const WdtWriteOptions& options = {});

    WdtWriteError error() const noexcept;
    const std::string& errorMessage() const noexcept;

    static const char* errorName(WdtWriteError error) noexcept;

private:
    bool fail(WdtWriteError error, std::string message);
    void reset() noexcept;

    WdtWriteError m_error = WdtWriteError::None;
    std::string m_errorMessage;
};

}  // namespace worldwright

#endif  // WORLDWRIGHT_WDT_WRITER_H
