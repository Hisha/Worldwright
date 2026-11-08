#include "worldwright/AdtWriter.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace worldwright {
namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::uint32_t fourcc(char a, char b, char c, char d) {
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(a)) << 24) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 8) |
           static_cast<std::uint32_t>(static_cast<unsigned char>(d));
}

constexpr std::uint32_t kMver = fourcc('M', 'V', 'E', 'R');
constexpr std::uint32_t kMhdr = fourcc('M', 'H', 'D', 'R');
constexpr std::uint32_t kMcin = fourcc('M', 'C', 'I', 'N');
constexpr std::uint32_t kMtex = fourcc('M', 'T', 'E', 'X');
constexpr std::uint32_t kMmdx = fourcc('M', 'M', 'D', 'X');
constexpr std::uint32_t kMmid = fourcc('M', 'M', 'I', 'D');
constexpr std::uint32_t kMwmo = fourcc('M', 'W', 'M', 'O');
constexpr std::uint32_t kMwid = fourcc('M', 'W', 'I', 'D');
constexpr std::uint32_t kMddf = fourcc('M', 'D', 'D', 'F');
constexpr std::uint32_t kModf = fourcc('M', 'O', 'D', 'F');
constexpr std::uint32_t kMcnk = fourcc('M', 'C', 'N', 'K');
constexpr std::uint32_t kMcvt = fourcc('M', 'C', 'V', 'T');
constexpr std::uint32_t kMcnr = fourcc('M', 'C', 'N', 'R');
constexpr std::uint32_t kMcly = fourcc('M', 'C', 'L', 'Y');
constexpr std::uint32_t kMcrf = fourcc('M', 'C', 'R', 'F');
constexpr std::uint32_t kMcal = fourcc('M', 'C', 'A', 'L');
constexpr std::uint32_t kMclq = fourcc('M', 'C', 'L', 'Q');
constexpr std::uint32_t kMcse = fourcc('M', 'C', 'S', 'E');

constexpr std::size_t kMhdrPayloadSize = 64;
constexpr std::size_t kMcinPayloadSize = 256 * 16;
constexpr std::size_t kMcnkHeaderSize = 128;
constexpr std::size_t kMcnrPaddingSize = 13;
constexpr std::uint32_t kDoNotFixAlphaMapFlag = 0x00008000u;
constexpr char kBaseTexture[] =
    "Tileset\\The Blasted Lands\\BlastedLandsBlack.blp";
constexpr int kHalfCellsPerTile =
    TerrainDefinition::InnerVerticesPerTileSide * 2;

struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct HeightPoint {
    int x;
    int y;
    double height;
};

Vector3 operator-(const Vector3& left, const Vector3& right) noexcept {
    return {left.x - right.x, left.y - right.y, left.z - right.z};
}

Vector3 cross(const Vector3& left, const Vector3& right) noexcept {
    return {left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x};
}

double length(const Vector3& value) noexcept {
    return std::sqrt(value.x * value.x + value.y * value.y +
                     value.z * value.z);
}

bool sampleHeight(const AdtTerrainNeighborhood& neighborhood, int halfX,
                  int halfY, double& height) noexcept {
    // Candidate owners are visited in global row-major order. This matches the
    // editor's canonical shared-outer-vertex rule and makes both exporters of
    // a shared edge choose the same value.
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const auto* terrain = neighborhood.terrainAt(offsetX, offsetY);
            if (terrain == nullptr) {
                continue;
            }
            const int localX = halfX - offsetX * kHalfCellsPerTile;
            const int localY = halfY - offsetY * kHalfCellsPerTile;
            if ((localX & 1) == 0 && (localY & 1) == 0 && localX >= 0 &&
                localY >= 0 && localX <= kHalfCellsPerTile &&
                localY <= kHalfCellsPerTile) {
                height = terrain->outerHeight(localY / 2, localX / 2);
                return std::isfinite(height);
            }
            if ((localX & 1) != 0 && (localY & 1) != 0 && localX > 0 &&
                localY > 0 && localX < kHalfCellsPerTile &&
                localY < kHalfCellsPerTile) {
                height = terrain->innerHeight((localY - 1) / 2,
                                              (localX - 1) / 2);
                return std::isfinite(height);
            }
        }
    }
    return false;
}

Vector3 wowPosition(const HeightPoint& point) noexcept {
    constexpr double halfCell =
        static_cast<double>(TerrainDefinition::TileSize) /
        kHalfCellsPerTile;
    // Editor column -> negative WoW Y; editor row -> negative WoW X.
    return {-point.y * halfCell, -point.x * halfCell, point.height};
}

Vector3 terrainNormal(const AdtTerrainNeighborhood& neighborhood, int halfX,
                      int halfY) noexcept {
    Vector3 sum;
    const bool inner = (halfX & 1) != 0 && (halfY & 1) != 0;
    const int cellXs[2] = {inner ? halfX - 1 : halfX - 2, halfX};
    const int cellYs[2] = {inner ? halfY - 1 : halfY - 2, halfY};
    const int cellCount = inner ? 1 : 2;
    for (int cellYIndex = 0; cellYIndex < cellCount; ++cellYIndex) {
        const int cellY = cellYs[cellYIndex];
        for (int cellXIndex = 0; cellXIndex < cellCount; ++cellXIndex) {
            const int cellX = cellXs[cellXIndex];
            if ((cellX & 1) != 0 || (cellY & 1) != 0) {
                continue;
            }
            HeightPoint points[5] = {{cellX, cellY, 0.0},
                                     {cellX + 2, cellY, 0.0},
                                     {cellX + 2, cellY + 2, 0.0},
                                     {cellX, cellY + 2, 0.0},
                                     {cellX + 1, cellY + 1, 0.0}};
            bool complete = true;
            for (auto& point : points) {
                if (!sampleHeight(neighborhood, point.x, point.y,
                                  point.height)) {
                    complete = false;
                    break;
                }
            }
            if (!complete) {
                continue;
            }

            // center/TL/BL, center/TR/TL, center/BR/TR, center/BL/BR
            constexpr int triangles[4][3] = {
                {4, 0, 3}, {4, 1, 0}, {4, 2, 1}, {4, 3, 2}};
            for (const auto& triangle : triangles) {
                const auto& a = points[triangle[0]];
                const auto& b = points[triangle[1]];
                const auto& c = points[triangle[2]];
                const bool containsTarget =
                    (a.x == halfX && a.y == halfY) ||
                    (b.x == halfX && b.y == halfY) ||
                    (c.x == halfX && c.y == halfY);
                if (!containsTarget) {
                    continue;
                }
                const auto pa = wowPosition(a);
                const auto face = cross(wowPosition(b) - pa,
                                        wowPosition(c) - pa);
                if (std::isfinite(face.x) && std::isfinite(face.y) &&
                    std::isfinite(face.z) && face.z > 0.0) {
                    sum.x += face.x;
                    sum.y += face.y;
                    sum.z += face.z;
                }
            }
        }
    }

    const double magnitude = length(sum);
    if (!std::isfinite(magnitude) || magnitude <= 0.0) {
        return {0.0, 0.0, 1.0};
    }
    return {sum.x / magnitude, sum.y / magnitude, sum.z / magnitude};
}

std::int8_t encodeNormalComponent(double component) noexcept {
    if (!std::isfinite(component)) {
        return 0;
    }
    const auto quantized = static_cast<int>(std::lround(component * 127.0));
    return static_cast<std::int8_t>(std::max(-127, std::min(127, quantized)));
}

void appendNormal(Bytes& output, const Vector3& normal) {
    for (const double component : {normal.x, normal.y, normal.z}) {
        output.push_back(static_cast<std::uint8_t>(
            encodeNormalComponent(component)));
    }
}

void vertexHalfCoordinates(int chunkX, int chunkY, int vertex, int& halfX,
                           int& halfY) noexcept {
    const int pairRow = vertex / 17;
    const int withinRow = vertex % 17;
    halfY = chunkY * 16 + pairRow * 2;
    if (pairRow < TerrainDefinition::CellsPerChunkSide &&
        withinRow >= TerrainDefinition::OuterVerticesPerChunkSide) {
        halfX = chunkX * 16 +
                (withinRow - TerrainDefinition::OuterVerticesPerChunkSide) *
                    2 +
                1;
        ++halfY;
    } else {
        halfX = chunkX * 16 + withinRow * 2;
    }
}

void appendU32(Bytes& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
    out.push_back(static_cast<std::uint8_t>(value >> 16));
    out.push_back(static_cast<std::uint8_t>(value >> 24));
}

void patchU32(Bytes& out, std::size_t offset, std::uint32_t value) {
    out[offset] = static_cast<std::uint8_t>(value);
    out[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    out[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    out[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

void appendFloat(Bytes& out, float value) {
    static_assert(sizeof(float) == sizeof(std::uint32_t),
                  "ADT requires 32-bit IEEE-754 floats");
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    appendU32(out, bits);
}

void patchFloat(Bytes& out, std::size_t offset, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    patchU32(out, offset, bits);
}

std::size_t beginChunk(Bytes& out, std::uint32_t id) {
    const std::size_t start = out.size();
    appendU32(out, id);
    appendU32(out, 0);
    return start;
}

void endChunk(Bytes& out, std::size_t start) {
    patchU32(out, start + 4,
             static_cast<std::uint32_t>(out.size() - start - 8));
}

std::size_t appendEmptyChunk(Bytes& out, std::uint32_t id) {
    const auto start = beginChunk(out, id);
    endChunk(out, start);
    return start;
}

std::filesystem::path parentDirectory(const std::filesystem::path& path) {
    return path.parent_path().empty() ? std::filesystem::path(".")
                                      : path.parent_path();
}

std::filesystem::path makeTempPath(const std::filesystem::path& outputPath) {
    static std::atomic<unsigned long long> counter{0};
    const auto directory = parentDirectory(outputPath);
    const auto base = outputPath.filename().string();
    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto serial = counter.fetch_add(1, std::memory_order_relaxed);
        const auto candidate = directory / ("." + base + ".worldwright-" +
                                             std::to_string(serial) + ".tmp");
        std::error_code error;
        if (!std::filesystem::exists(candidate, error)) {
            return candidate;
        }
    }
    return {};
}

std::string describePath(const std::filesystem::path& path) {
    return path.empty() ? std::string("<empty path>") : path.string();
}

}  // namespace

bool AdtTerrainNeighborhood::setTerrain(
    int offsetX, int offsetY, const TerrainDefinition* terrain) noexcept {
    if (offsetX < -1 || offsetX > 1 || offsetY < -1 || offsetY > 1) {
        return false;
    }
    m_terrain[static_cast<std::size_t>(offsetY + 1) * 3 +
              static_cast<std::size_t>(offsetX + 1)] = terrain;
    return true;
}

const TerrainDefinition* AdtTerrainNeighborhood::terrainAt(
    int offsetX, int offsetY) const noexcept {
    if (offsetX < -1 || offsetX > 1 || offsetY < -1 || offsetY > 1) {
        return nullptr;
    }
    return m_terrain[static_cast<std::size_t>(offsetY + 1) * 3 +
                     static_cast<std::size_t>(offsetX + 1)];
}

bool AdtWriter::serialize(const TerrainDefinition& terrain, int tileX,
                          int tileY, std::vector<std::uint8_t>& output) {
    AdtTerrainNeighborhood neighborhood;
    neighborhood.setTerrain(0, 0, &terrain);
    return serializeImpl(terrain, tileX, tileY, neighborhood, output);
}

bool AdtWriter::serialize(const TerrainDefinition& terrain, int tileX,
                          int tileY,
                          const AdtTerrainNeighborhood& neighborhood,
                          std::vector<std::uint8_t>& output) {
    auto completeNeighborhood = neighborhood;
    completeNeighborhood.setTerrain(0, 0, &terrain);
    return serializeImpl(terrain, tileX, tileY, completeNeighborhood, output);
}

bool AdtWriter::serializeImpl(const TerrainDefinition& terrain, int tileX,
                              int tileY,
                              const AdtTerrainNeighborhood& neighborhood,
                              std::vector<std::uint8_t>& output) {
    reset();
    output.clear();
    if (!terrain.isValid()) {
        return fail(AdtWriteError::InvalidTerrain,
                    "terrain contains a non-finite elevation");
    }
    for (int offsetY = -1; offsetY <= 1; ++offsetY) {
        for (int offsetX = -1; offsetX <= 1; ++offsetX) {
            const auto* neighbor = neighborhood.terrainAt(offsetX, offsetY);
            if (neighbor != nullptr && !neighbor->isValid()) {
                return fail(AdtWriteError::InvalidTerrain,
                            "normal neighborhood contains a non-finite "
                            "elevation at offset (" +
                                std::to_string(offsetX) + "," +
                                std::to_string(offsetY) + ")");
            }
        }
    }
    if (tileX < 0 || tileY < 0 || tileX >= 64 || tileY >= 64) {
        return fail(AdtWriteError::InvalidTile,
                    "tile coordinate is out of range (expected 0-63): (" +
                        std::to_string(tileX) + "," +
                        std::to_string(tileY) + ")");
    }

    output.reserve(kSerializedSize);

    auto chunk = beginChunk(output, kMver);
    appendU32(output, kVersion);
    endChunk(output, chunk);

    const std::size_t mhdrStart = beginChunk(output, kMhdr);
    const std::size_t mhdrPayload = mhdrStart + 8;
    output.insert(output.end(), kMhdrPayloadSize, 0);
    endChunk(output, mhdrStart);

    const std::size_t mcinStart = beginChunk(output, kMcin);
    const std::size_t mcinPayload = mcinStart + 8;
    output.insert(output.end(), kMcinPayloadSize, 0);
    endChunk(output, mcinStart);

    const auto setMhdrOffset = [&](std::size_t fieldOffset,
                                   std::size_t target) {
        patchU32(output, mhdrPayload + fieldOffset,
                 static_cast<std::uint32_t>(target - mhdrPayload));
    };
    setMhdrOffset(4, mcinStart);

    const std::size_t mtexStart = beginChunk(output, kMtex);
    output.insert(output.end(), kBaseTexture,
                  kBaseTexture + sizeof(kBaseTexture));
    endChunk(output, mtexStart);
    setMhdrOffset(8, mtexStart);

    const std::size_t mmdxStart = appendEmptyChunk(output, kMmdx);
    setMhdrOffset(12, mmdxStart);
    const std::size_t mmidStart = appendEmptyChunk(output, kMmid);
    setMhdrOffset(16, mmidStart);
    const std::size_t mwmoStart = appendEmptyChunk(output, kMwmo);
    setMhdrOffset(20, mwmoStart);
    const std::size_t mwidStart = appendEmptyChunk(output, kMwid);
    setMhdrOffset(24, mwidStart);
    const std::size_t mddfStart = appendEmptyChunk(output, kMddf);
    setMhdrOffset(28, mddfStart);
    const std::size_t modfStart = appendEmptyChunk(output, kModf);
    setMhdrOffset(32, modfStart);

    for (int chunkY = 0; chunkY < TerrainDefinition::ChunksPerSide;
         ++chunkY) {
        for (int chunkX = 0; chunkX < TerrainDefinition::ChunksPerSide;
             ++chunkX) {
            const int index =
                chunkY * TerrainDefinition::ChunksPerSide + chunkX;
            const std::size_t mcnkStart = beginChunk(output, kMcnk);
            const std::size_t header = mcnkStart + 8;
            output.insert(output.end(), kMcnkHeaderSize, 0);

            patchU32(output, header, kDoNotFixAlphaMapFlag);
            patchU32(output, header + 4, static_cast<std::uint32_t>(chunkX));
            patchU32(output, header + 8, static_cast<std::uint32_t>(chunkY));
            patchU32(output, header + 12, 1);  // one base texture layer

            float baseHeight = std::numeric_limits<float>::max();
            for (int vertex = 0; vertex < TerrainDefinition::VerticesPerChunk;
                 ++vertex) {
                baseHeight = std::min(
                    baseHeight,
                    terrain.chunkVertexHeight(chunkX, chunkY, vertex));
            }

            const auto relativeOffset = [&](std::size_t absolute) {
                return static_cast<std::uint32_t>(absolute - mcnkStart);
            };

            const std::size_t mcvtStart = beginChunk(output, kMcvt);
            patchU32(output, header + 20, relativeOffset(mcvtStart));
            for (int vertex = 0; vertex < TerrainDefinition::VerticesPerChunk;
                 ++vertex) {
                appendFloat(output,
                            terrain.chunkVertexHeight(chunkX, chunkY, vertex) -
                                baseHeight);
            }
            endChunk(output, mcvtStart);

            const std::size_t mcnrStart = beginChunk(output, kMcnr);
            patchU32(output, header + 24, relativeOffset(mcnrStart));
            for (int vertex = 0; vertex < TerrainDefinition::VerticesPerChunk;
                 ++vertex) {
                int halfX = 0;
                int halfY = 0;
                vertexHalfCoordinates(chunkX, chunkY, vertex, halfX, halfY);
                appendNormal(output,
                             terrainNormal(neighborhood, halfX, halfY));
            }
            endChunk(output, mcnrStart);
            output.insert(output.end(), kMcnrPaddingSize, 0);

            const std::size_t mclyStart = beginChunk(output, kMcly);
            patchU32(output, header + 28, relativeOffset(mclyStart));
            appendU32(output, 0);  // MTEX index
            appendU32(output, 0);  // layer flags
            appendU32(output, 0);  // MCAL offset (unused by base layer)
            appendU32(output, 0);  // ground effect ID
            endChunk(output, mclyStart);

            const std::size_t mcrfStart = appendEmptyChunk(output, kMcrf);
            patchU32(output, header + 32, relativeOffset(mcrfStart));

            const std::size_t mcalStart = appendEmptyChunk(output, kMcal);
            patchU32(output, header + 36, relativeOffset(mcalStart));
            patchU32(output, header + 40, 8);  // includes chunk header

            const std::size_t mclqStart = appendEmptyChunk(output, kMclq);
            patchU32(output, header + 96, relativeOffset(mclqStart));
            patchU32(output, header + 100, 8);

            const std::size_t mcseStart = appendEmptyChunk(output, kMcse);
            patchU32(output, header + 88, relativeOffset(mcseStart));

            const float positionX =
                kWorldCenter - (static_cast<float>(tileY) * kTileSize +
                                static_cast<float>(chunkY) * kChunkSize);
            const float positionY =
                kWorldCenter - (static_cast<float>(tileX) * kTileSize +
                                static_cast<float>(chunkX) * kChunkSize);
            patchFloat(output, header + 104, positionX);
            patchFloat(output, header + 108, positionY);
            patchFloat(output, header + 112, baseHeight);

            endChunk(output, mcnkStart);
            if (output.size() - mcnkStart != kMcnkTotalSize) {
                output.clear();
                return fail(AdtWriteError::InvalidTerrain,
                            "internal MCNK size invariant failed");
            }

            const std::size_t entry = mcinPayload +
                                      static_cast<std::size_t>(index) * 16;
            patchU32(output, entry,
                     static_cast<std::uint32_t>(mcnkStart));
            patchU32(output, entry + 4, kMcnkTotalSize);
        }
    }

    if (output.size() != kSerializedSize) {
        output.clear();
        return fail(AdtWriteError::InvalidTerrain,
                    "internal ADT size invariant failed");
    }
    return true;
}

bool AdtWriter::save(const TerrainDefinition& terrain, int tileX, int tileY,
                     const std::filesystem::path& outputPath,
                     const AdtWriteOptions& options) {
    AdtTerrainNeighborhood neighborhood;
    neighborhood.setTerrain(0, 0, &terrain);
    return saveImpl(terrain, tileX, tileY, neighborhood, outputPath, options);
}

bool AdtWriter::save(const TerrainDefinition& terrain, int tileX, int tileY,
                     const AdtTerrainNeighborhood& neighborhood,
                     const std::filesystem::path& outputPath,
                     const AdtWriteOptions& options) {
    auto completeNeighborhood = neighborhood;
    completeNeighborhood.setTerrain(0, 0, &terrain);
    return saveImpl(terrain, tileX, tileY, completeNeighborhood, outputPath,
                    options);
}

bool AdtWriter::saveImpl(const TerrainDefinition& terrain, int tileX,
                         int tileY,
                         const AdtTerrainNeighborhood& neighborhood,
                         const std::filesystem::path& outputPath,
                         const AdtWriteOptions& options) {
    reset();
    if (outputPath.empty() || !outputPath.has_filename()) {
        return fail(AdtWriteError::InvalidPath,
                    "output path does not name a file: " +
                        describePath(outputPath));
    }

    std::error_code error;
    const auto directory = parentDirectory(outputPath);
    if (!std::filesystem::exists(directory, error) || error ||
        !std::filesystem::is_directory(directory, error) || error) {
        return fail(AdtWriteError::InvalidPath,
                    "output directory does not exist or is not a directory: " +
                        describePath(directory));
    }
    const bool destinationExists = std::filesystem::exists(outputPath, error);
    if (error || (destinationExists &&
                  std::filesystem::is_directory(outputPath, error))) {
        return fail(AdtWriteError::InvalidPath,
                    "output path is not a writable file path: " +
                        describePath(outputPath));
    }
    if (destinationExists && !options.overwrite) {
        return fail(AdtWriteError::OutputExists,
                    "output file already exists (use --overwrite to replace it): " +
                        describePath(outputPath));
    }

    Bytes bytes;
    if (!serializeImpl(terrain, tileX, tileY, neighborhood, bytes)) {
        return false;
    }
    const auto tempPath = makeTempPath(outputPath);
    if (tempPath.empty()) {
        return fail(AdtWriteError::OpenFailed,
                    "could not allocate a temporary file name in: " +
                        describePath(directory));
    }
    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) {
            return fail(AdtWriteError::OpenFailed,
                        "could not create temporary file: " +
                            describePath(tempPath));
        }
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        file.flush();
        file.close();
        if (!file) {
            std::error_code removeError;
            std::filesystem::remove(tempPath, removeError);
            return fail(AdtWriteError::WriteFailed,
                        "could not write complete ADT data to: " +
                            describePath(tempPath));
        }
    }

#if defined(_WIN32)
    if (destinationExists) {
        std::filesystem::remove(outputPath, error);
        if (error) {
            const std::string replaceError = error.message();
            std::error_code removeError;
            std::filesystem::remove(tempPath, removeError);
            return fail(AdtWriteError::ReplaceFailed,
                        "could not remove existing output file: " +
                            describePath(outputPath) + ": " + replaceError);
        }
    }
#endif
    std::filesystem::rename(tempPath, outputPath, error);
    if (error) {
        std::error_code removeError;
        std::filesystem::remove(tempPath, removeError);
        return fail(AdtWriteError::ReplaceFailed,
                    "could not replace output file: " +
                        describePath(outputPath) + ": " + error.message());
    }
    return true;
}

AdtWriteError AdtWriter::error() const noexcept { return m_error; }
const std::string& AdtWriter::errorMessage() const noexcept {
    return m_errorMessage;
}

const char* AdtWriter::errorName(AdtWriteError error) noexcept {
    switch (error) {
        case AdtWriteError::None: return "None";
        case AdtWriteError::InvalidTerrain: return "InvalidTerrain";
        case AdtWriteError::InvalidTile: return "InvalidTile";
        case AdtWriteError::InvalidPath: return "InvalidPath";
        case AdtWriteError::OutputExists: return "OutputExists";
        case AdtWriteError::OpenFailed: return "OpenFailed";
        case AdtWriteError::WriteFailed: return "WriteFailed";
        case AdtWriteError::ReplaceFailed: return "ReplaceFailed";
    }
    return "Unknown";
}

bool AdtWriter::fail(AdtWriteError error, std::string message) {
    m_error = error;
    m_errorMessage = std::move(message);
    return false;
}

void AdtWriter::reset() noexcept {
    m_error = AdtWriteError::None;
    m_errorMessage.clear();
}

}  // namespace worldwright
