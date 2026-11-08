#include "worldwright/WorldExporter.h"

#include "worldwright/AdtWriter.h"
#include "worldwright/WdtWriter.h"

#include <atomic>
#include <system_error>
#include <utility>

namespace worldwright {
namespace {

std::filesystem::path makeStagingPath(
    const std::filesystem::path& outputDirectory) {
    static std::atomic<unsigned long long> counter{0};
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto candidate =
            outputDirectory /
            (".worldwright-export-" +
             std::to_string(counter.fetch_add(1, std::memory_order_relaxed)));
        std::error_code error;
        if (!std::filesystem::exists(candidate, error) && !error) {
            return candidate;
        }
    }
    return {};
}

class StagingCleanup {
public:
    explicit StagingCleanup(std::filesystem::path path)
        : m_path(std::move(path)) {}
    ~StagingCleanup() {
        std::error_code error;
        std::filesystem::remove_all(m_path, error);
    }

private:
    std::filesystem::path m_path;
};

}  // namespace

std::vector<std::filesystem::path> WorldExporter::outputPaths(
    const WorldEditorModel& model,
    const std::filesystem::path& outputDirectory) {
    std::vector<std::filesystem::path> result;
    result.reserve(model.world().tileCount() + 1);
    result.push_back(outputDirectory / (model.worldName() + ".wdt"));
    for (const auto tile : model.world().enabledTiles()) {
        result.push_back(outputDirectory /
                         (model.worldName() + "_" + std::to_string(tile.x) +
                          "_" + std::to_string(tile.y) + ".adt"));
    }
    return result;
}

bool WorldExporter::exportWorld(const WorldEditorModel& model,
                                const std::filesystem::path& outputDirectory,
                                bool overwriteExisting) {
    m_error = WorldExportError::None;
    m_errorMessage.clear();

    std::string nameError;
    if (!WorldEditorModel::validateWorldName(model.worldName(), &nameError)) {
        return fail(WorldExportError::InvalidWorldName, nameError);
    }

    std::error_code error;
    if (outputDirectory.empty() ||
        !std::filesystem::exists(outputDirectory, error) || error ||
        !std::filesystem::is_directory(outputDirectory, error) || error) {
        return fail(WorldExportError::InvalidDirectory,
                    "Export directory does not exist or is not a directory: " +
                        outputDirectory.string());
    }

    const auto destinations = outputPaths(model, outputDirectory);
    for (const auto& destination : destinations) {
        error.clear();
        const bool exists = std::filesystem::exists(destination, error);
        if (error) {
            return fail(WorldExportError::InvalidDirectory,
                        "Could not inspect output path: " +
                            destination.string() + ": " + error.message());
        }
        if (exists && std::filesystem::is_directory(destination, error)) {
            return fail(WorldExportError::InvalidDirectory,
                        "An output path is a directory: " +
                            destination.string());
        }
        if (exists && !overwriteExisting) {
            return fail(WorldExportError::OutputExists,
                        "Output file already exists: " +
                            destination.string());
        }
    }

    const auto staging = makeStagingPath(outputDirectory);
    if (staging.empty()) {
        return fail(WorldExportError::StagingFailed,
                    "Could not allocate an export staging directory.");
    }
    const auto generated = staging / "generated";
    const auto backup = staging / "backup";
    std::filesystem::create_directories(generated, error);
    if (error) {
        return fail(WorldExportError::StagingFailed,
                    "Could not create export staging directory: " +
                        error.message());
    }
    StagingCleanup cleanup(staging);

    WdtWriteOptions wdtOptions;
    wdtOptions.overwrite = false;
    WdtWriter wdtWriter;
    const auto wdtStage = generated / destinations.front().filename();
    if (!wdtWriter.save(model.world(), wdtStage, wdtOptions)) {
        return fail(WorldExportError::WriteFailed,
                    "Could not generate WDT: " + wdtWriter.errorMessage());
    }

    AdtWriteOptions adtOptions;
    adtOptions.overwrite = false;
    AdtWriter adtWriter;
    const auto tiles = model.world().enabledTiles();
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const auto tile = tiles[i];
        const auto* terrain = model.terrainAt(tile.x, tile.y);
        if (terrain == nullptr) {
            return fail(WorldExportError::WriteFailed,
                        "Enabled tile is missing terrain: (" +
                            std::to_string(tile.x) + "," +
                            std::to_string(tile.y) + ")");
        }
        const auto stagePath = generated / destinations[i + 1].filename();
        if (!adtWriter.save(*terrain, tile.x, tile.y, stagePath, adtOptions)) {
            return fail(WorldExportError::WriteFailed,
                        "Could not generate ADT for tile (" +
                            std::to_string(tile.x) + "," +
                            std::to_string(tile.y) + "): " +
                            adtWriter.errorMessage());
        }
    }

    std::filesystem::create_directories(backup, error);
    if (error) {
        return fail(WorldExportError::StagingFailed,
                    "Could not prepare export backup directory: " +
                        error.message());
    }

    std::vector<bool> hadExisting(destinations.size(), false);
    std::size_t committed = 0;
    const auto rollback = [&]() {
        std::error_code ignored;
        for (std::size_t i = 0; i < committed; ++i) {
            std::filesystem::remove(destinations[i], ignored);
            ignored.clear();
        }
        for (std::size_t i = 0; i < destinations.size(); ++i) {
            if (!hadExisting[i]) {
                continue;
            }
            const auto backupPath = backup / destinations[i].filename();
            if (std::filesystem::exists(backupPath, ignored)) {
                ignored.clear();
                std::filesystem::rename(backupPath, destinations[i], ignored);
            }
            ignored.clear();
        }
    };

    for (std::size_t i = 0; i < destinations.size(); ++i) {
        error.clear();
        hadExisting[i] = std::filesystem::exists(destinations[i], error);
        if (error) {
            rollback();
            return fail(WorldExportError::CommitFailed,
                        "Could not inspect an output during commit: " +
                            error.message());
        }
        if (hadExisting[i]) {
            std::filesystem::rename(destinations[i],
                                    backup / destinations[i].filename(), error);
            if (error) {
                rollback();
                return fail(WorldExportError::CommitFailed,
                            "Could not preserve existing output: " +
                                destinations[i].string() + ": " +
                                error.message());
            }
        }
        std::filesystem::rename(generated / destinations[i].filename(),
                                destinations[i], error);
        if (error) {
            rollback();
            return fail(WorldExportError::CommitFailed,
                        "Could not commit output: " + destinations[i].string() +
                            ": " + error.message());
        }
        ++committed;
    }

    return true;
}

WorldExportError WorldExporter::error() const noexcept {
    return m_error;
}

const std::string& WorldExporter::errorMessage() const noexcept {
    return m_errorMessage;
}

bool WorldExporter::fail(WorldExportError error, std::string message) {
    m_error = error;
    m_errorMessage = std::move(message);
    return false;
}

}  // namespace worldwright
