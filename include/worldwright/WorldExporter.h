#ifndef WORLDWRIGHT_WORLD_EXPORTER_H
#define WORLDWRIGHT_WORLD_EXPORTER_H

#include "worldwright/WorldEditorModel.h"

#include <filesystem>
#include <string>
#include <vector>

namespace worldwright {

enum class WorldExportError {
    None = 0,
    InvalidWorldName,
    InvalidDirectory,
    OutputExists,
    StagingFailed,
    WriteFailed,
    VerificationFailed,
    CommitFailed,
};

class WorldExporter {
public:
    // Returns all destination paths, WDT first and ADTs in row-major order.
    static std::vector<std::filesystem::path> outputPaths(
        const WorldEditorModel& model,
        const std::filesystem::path& outputDirectory);

    bool exportWorld(const WorldEditorModel& model,
                     const std::filesystem::path& outputDirectory,
                     bool overwriteExisting = false);

    WorldExportError error() const noexcept;
    const std::string& errorMessage() const noexcept;

private:
    bool fail(WorldExportError error, std::string message);

    WorldExportError m_error = WorldExportError::None;
    std::string m_errorMessage;
};

}  // namespace worldwright

#endif
