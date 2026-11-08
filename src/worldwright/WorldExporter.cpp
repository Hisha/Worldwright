#include "worldwright/WorldExporter.h"

#include "worldwright/AdtWriter.h"
#include "worldwright/WdtWriter.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
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

using Bytes = std::vector<std::uint8_t>;

struct BoundaryVertex {
    float height = 0.0f;
    std::array<std::int8_t, 3> normal{};
};

struct SerializedBoundaries {
    std::array<BoundaryVertex, 129> north{};
    std::array<BoundaryVertex, 129> south{};
    std::array<BoundaryVertex, 129> west{};
    std::array<BoundaryVertex, 129> east{};
};

bool range(std::size_t offset, std::size_t length, std::size_t size) noexcept {
    return offset <= size && length <= size - offset;
}

std::uint32_t readU32(const Bytes& bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) |
           static_cast<std::uint32_t>(bytes[offset + 1]) << 8 |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 16 |
           static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

float readFloat(const Bytes& bytes, std::size_t offset) noexcept {
    const auto bits = readU32(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool hasMagic(const Bytes& bytes, std::size_t offset,
              const char* name) noexcept {
    return range(offset, 4, bytes.size()) &&
           bytes[offset] == static_cast<std::uint8_t>(name[3]) &&
           bytes[offset + 1] == static_cast<std::uint8_t>(name[2]) &&
           bytes[offset + 2] == static_cast<std::uint8_t>(name[1]) &&
           bytes[offset + 3] == static_cast<std::uint8_t>(name[0]);
}

bool readBoundaries(const std::filesystem::path& path,
                    SerializedBoundaries& boundaries, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "could not open " + path.string();
        return false;
    }
    const std::istreambuf_iterator<char> end;
    Bytes bytes((std::istreambuf_iterator<char>(file)), end);
    if (file.bad()) {
        error = "could not read " + path.string();
        return false;
    }
    const auto fail = [&](const std::string& reason) {
        error = path.filename().string() + ": " + reason;
        return false;
    };
    if (!range(0, 28, bytes.size()) || !hasMagic(bytes, 0, "MVER") ||
        !hasMagic(bytes, 12, "MHDR"))
        return fail("invalid ADT prefix");
    const std::size_t mcin = 20u + readU32(bytes, 24);
    if (!range(mcin, 8 + 4096, bytes.size()) ||
        !hasMagic(bytes, mcin, "MCIN") || readU32(bytes, mcin + 4) != 4096)
        return fail("invalid MCIN reference or size");

    for (int chunk = 0; chunk < 256; ++chunk) {
        const std::size_t entry = mcin + 8 + static_cast<std::size_t>(chunk) * 16;
        const std::size_t mcnk = readU32(bytes, entry);
        const std::uint32_t total = readU32(bytes, entry + 4);
        if (total < 136 || !range(mcnk, total, bytes.size()) ||
            !hasMagic(bytes, mcnk, "MCNK"))
            return fail("invalid MCNK reference at index " +
                        std::to_string(chunk));
        const std::size_t header = mcnk + 8;
        const std::size_t mcvt = mcnk + readU32(bytes, header + 20);
        const std::size_t mcnr = mcnk + readU32(bytes, header + 24);
        if (!range(mcvt, 588, bytes.size()) || !hasMagic(bytes, mcvt, "MCVT") ||
            readU32(bytes, mcvt + 4) != 580 ||
            !range(mcnr, 443, bytes.size()) || !hasMagic(bytes, mcnr, "MCNR") ||
            readU32(bytes, mcnr + 4) != 435)
            return fail("invalid MCVT/MCNR at MCNK index " +
                        std::to_string(chunk));
        const float base = readFloat(bytes, header + 112);
        const int chunkX = chunk % 16;
        const int chunkY = chunk / 16;
        for (int vertex = 0; vertex < 145; ++vertex) {
            const int pairRow = vertex / 17;
            const int within = vertex % 17;
            if ((pairRow < 8 && within >= 9)) continue;
            const int row = chunkY * 8 + pairRow;
            const int column = chunkX * 8 + within;
            if (row != 0 && row != 128 && column != 0 && column != 128)
                continue;
            const float height =
                base + readFloat(bytes, mcvt + 8 +
                                           static_cast<std::size_t>(vertex) * 4);
            if (!std::isfinite(base) || !std::isfinite(height))
                return fail("non-finite boundary height at (" +
                            std::to_string(column) + "," +
                            std::to_string(row) + ")");
            BoundaryVertex value;
            value.height = height;
            for (std::size_t component = 0; component < 3; ++component)
                value.normal[component] = static_cast<std::int8_t>(
                    bytes[mcnr + 8 + static_cast<std::size_t>(vertex) * 3 +
                          component]);
            if (row == 0) boundaries.north[column] = value;
            if (row == 128) boundaries.south[column] = value;
            if (column == 0) boundaries.west[row] = value;
            if (column == 128) boundaries.east[row] = value;
        }
    }
    return true;
}

bool verifySeam(const SerializedBoundaries& first,
                const SerializedBoundaries& second, bool eastWest,
                TileCoord firstTile, TileCoord secondTile,
                const std::filesystem::path& firstPath,
                const std::filesystem::path& secondPath, std::string& error) {
    constexpr float tolerance = 0.0001f;
    const auto& firstEdge = eastWest ? first.east : first.south;
    const auto& secondEdge = eastWest ? second.west : second.north;
    for (std::size_t vertex = 0; vertex < firstEdge.size(); ++vertex) {
        const float difference =
            std::fabs(firstEdge[vertex].height - secondEdge[vertex].height);
        if (difference <= tolerance &&
            firstEdge[vertex].normal == secondEdge[vertex].normal)
            continue;
        std::ostringstream message;
        message << "serialized seam mismatch between "
                << firstPath.filename().string() << " (" << firstTile.x << ','
                << firstTile.y << ") and " << secondPath.filename().string()
                << " (" << secondTile.x << ',' << secondTile.y << ") on the "
                << (eastWest ? "east/west" : "south/north")
                << " border at vertex " << vertex << ": heights "
                << firstEdge[vertex].height << " and "
                << secondEdge[vertex].height << ", difference=" << difference
                << "; normals (" << +firstEdge[vertex].normal[0] << ','
                << +firstEdge[vertex].normal[1] << ','
                << +firstEdge[vertex].normal[2] << ") and ("
                << +secondEdge[vertex].normal[0] << ','
                << +secondEdge[vertex].normal[1] << ','
                << +secondEdge[vertex].normal[2] << ')';
        error = message.str();
        return false;
    }
    return true;
}

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
        AdtTerrainNeighborhood neighborhood;
        for (int offsetY = -1; offsetY <= 1; ++offsetY) {
            for (int offsetX = -1; offsetX <= 1; ++offsetX) {
                neighborhood.setTerrain(
                    offsetX, offsetY,
                    model.terrainAt(tile.x + offsetX, tile.y + offsetY));
            }
        }
        if (!adtWriter.save(*terrain, tile.x, tile.y, neighborhood, stagePath,
                            adtOptions)) {
            return fail(WorldExportError::WriteFailed,
                        "Could not generate ADT for tile (" +
                            std::to_string(tile.x) + "," +
                            std::to_string(tile.y) + "): " +
                            adtWriter.errorMessage());
        }
    }

    // Inspect the staged bytes, not the source model. This runs before the
    // transaction commits any destination and includes all shared corners as
    // the endpoints of both applicable border checks.
    std::vector<SerializedBoundaries> boundaries(tiles.size());
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        std::string verificationError;
        const auto path = generated / destinations[i + 1].filename();
        if (!readBoundaries(path, boundaries[i], verificationError)) {
            return fail(WorldExportError::VerificationFailed,
                        "Could not verify exported ADT: " + verificationError);
        }
    }
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const auto tile = tiles[i];
        for (const TileCoord neighbor :
             {TileCoord{tile.x + 1, tile.y}, TileCoord{tile.x, tile.y + 1}}) {
            if (!WorldDefinition::isValidTile(neighbor.x, neighbor.y)) continue;
            const auto found = std::find(tiles.begin(), tiles.end(), neighbor);
            if (found == tiles.end()) continue;
            const std::size_t neighborPosition =
                static_cast<std::size_t>(std::distance(tiles.begin(), found));
            std::string verificationError;
            if (!verifySeam(boundaries[i], boundaries[neighborPosition],
                            neighbor.x != tile.x, tile, neighbor,
                            generated / destinations[i + 1].filename(),
                            generated /
                                destinations[neighborPosition + 1].filename(),
                            verificationError)) {
                return fail(WorldExportError::VerificationFailed,
                            verificationError);
            }
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
