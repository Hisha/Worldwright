#include "worldwright/AdtWriter.h"
#include "worldwright/TerrainHeightEditor.h"
#include "worldwright/WorldEditorModel.h"
#include "worldwright/WorldExporter.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;
constexpr float kHeightTolerance = 0.0001f;
int checks = 0;
int failures = 0;

void check(bool value, const char* expression, int line) {
    ++checks;
    if (!value) {
        ++failures;
        std::cerr << "FAIL line " << line << ": " << expression << '\n';
    }
}
#define CHECK(value) check((value), #value, __LINE__)

class TempDir {
public:
    TempDir() {
        path = std::filesystem::temp_directory_path() /
               ("worldwright-w005c-" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path);
    }
    ~TempDir() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

bool readFile(const std::filesystem::path& path, Bytes& bytes) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    bytes.assign(std::istreambuf_iterator<char>(file),
                 std::istreambuf_iterator<char>());
    return !file.bad();
}

bool range(std::size_t offset, std::size_t length, std::size_t size) {
    return offset <= size && length <= size - offset;
}

std::uint32_t u32(const Bytes& data, std::size_t offset) {
    return static_cast<std::uint32_t>(data[offset]) |
           static_cast<std::uint32_t>(data[offset + 1]) << 8 |
           static_cast<std::uint32_t>(data[offset + 2]) << 16 |
           static_cast<std::uint32_t>(data[offset + 3]) << 24;
}

void putU32(Bytes& data, std::size_t offset, std::uint32_t value) {
    for (int byte = 0; byte < 4; ++byte)
        data[offset + static_cast<std::size_t>(byte)] =
            static_cast<std::uint8_t>(value >> (byte * 8));
}

float f32(const Bytes& data, std::size_t offset) {
    const auto bits = u32(data, offset);
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

bool magic(const Bytes& data, std::size_t offset, const char* expected) {
    return range(offset, 4, data.size()) &&
           data[offset] == static_cast<std::uint8_t>(expected[3]) &&
           data[offset + 1] == static_cast<std::uint8_t>(expected[2]) &&
           data[offset + 2] == static_cast<std::uint8_t>(expected[1]) &&
           data[offset + 3] == static_cast<std::uint8_t>(expected[0]);
}

struct EncodedNormal {
    std::int8_t x = 0;
    std::int8_t y = 0;
    std::int8_t z = 0;
    bool operator==(const EncodedNormal& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct ParsedAdt {
    std::vector<float> outer = std::vector<float>(129 * 129,
                                                   std::numeric_limits<float>::quiet_NaN());
    std::vector<float> inner = std::vector<float>(128 * 128,
                                                   std::numeric_limits<float>::quiet_NaN());
    std::vector<EncodedNormal> outerNormals =
        std::vector<EncodedNormal>(129 * 129);
    std::vector<EncodedNormal> innerNormals =
        std::vector<EncodedNormal>(128 * 128);
    std::vector<bool> outerSeen = std::vector<bool>(129 * 129, false);
    std::vector<bool> innerSeen = std::vector<bool>(128 * 128, false);
};

bool parseAdt(const Bytes& data, const std::string& name, ParsedAdt& parsed,
              std::string& error) {
    const auto fail = [&](const std::string& reason) {
        error = name + ": " + reason;
        return false;
    };
    if (!range(0, 20, data.size()) || !magic(data, 0, "MVER") ||
        u32(data, 4) != 4 || u32(data, 8) != 18 ||
        !magic(data, 12, "MHDR") || u32(data, 16) != 64)
        return fail("invalid MVER/MHDR prefix");
    const std::uint32_t mcinRelative = u32(data, 24);
    const std::size_t mcin = 20u + mcinRelative;
    if (!range(mcin, 8 + 4096, data.size()) || !magic(data, mcin, "MCIN") ||
        u32(data, mcin + 4) != 4096)
        return fail("MCIN reference or payload is invalid");

    for (int chunk = 0; chunk < 256; ++chunk) {
        const std::size_t entry = mcin + 8 + static_cast<std::size_t>(chunk) * 16;
        const std::size_t mcnk = u32(data, entry);
        const std::uint32_t declaredTotal = u32(data, entry + 4);
        if (declaredTotal < 136 || !range(mcnk, declaredTotal, data.size()) ||
            !magic(data, mcnk, "MCNK") ||
            u32(data, mcnk + 4) + 8 != declaredTotal)
            return fail("MCIN entry " + std::to_string(chunk) +
                        " has an invalid MCNK reference/size");
        const std::size_t header = mcnk + 8;
        if (!range(header, 128, data.size()))
            return fail("MCNK " + std::to_string(chunk) + " header is truncated");
        const std::size_t mcvt = mcnk + u32(data, header + 20);
        const std::size_t mcnr = mcnk + u32(data, header + 24);
        if (!range(mcvt, 8 + 580, data.size()) || !magic(data, mcvt, "MCVT") ||
            u32(data, mcvt + 4) != 580)
            return fail("MCNK " + std::to_string(chunk) +
                        " has an invalid MCVT offset or payload size");
        if (!range(mcnr, 8 + 435, data.size()) || !magic(data, mcnr, "MCNR") ||
            u32(data, mcnr + 4) != 435)
            return fail("MCNK " + std::to_string(chunk) +
                        " has an invalid MCNR offset or payload size");
        const float base = f32(data, header + 112);
        if (!std::isfinite(base))
            return fail("MCNK " + std::to_string(chunk) + " base height is non-finite");

        const int chunkX = chunk % 16;
        const int chunkY = chunk / 16;
        for (int vertex = 0; vertex < 145; ++vertex) {
            const float delta = f32(data, mcvt + 8 + static_cast<std::size_t>(vertex) * 4);
            const float height = base + delta;
            if (!std::isfinite(delta) || !std::isfinite(height))
                return fail("MCNK " + std::to_string(chunk) + " vertex " +
                            std::to_string(vertex) + " is non-finite");
            const auto byteAt = [&](std::size_t component) {
                return static_cast<std::int8_t>(
                    data[mcnr + 8 + static_cast<std::size_t>(vertex) * 3 + component]);
            };
            const EncodedNormal normal{byteAt(0), byteAt(1), byteAt(2)};
            const double normalLength = std::sqrt(
                static_cast<double>(normal.x) * normal.x +
                static_cast<double>(normal.y) * normal.y +
                static_cast<double>(normal.z) * normal.z);
            if (normal.z < 0 || normalLength < 125.0 || normalLength > 128.5)
                return fail("MCNK " + std::to_string(chunk) + " vertex " +
                            std::to_string(vertex) + " has an invalid normal");

            const int pairRow = vertex / 17;
            const int within = vertex % 17;
            if (pairRow < 8 && within >= 9) {
                const int row = chunkY * 8 + pairRow;
                const int column = chunkX * 8 + within - 9;
                const std::size_t index = static_cast<std::size_t>(row) * 128 + column;
                if (parsed.innerSeen[index] &&
                    (std::fabs(parsed.inner[index] - height) > kHeightTolerance ||
                     !(parsed.innerNormals[index] == normal)))
                    return fail("duplicate inner vertex mismatch");
                parsed.inner[index] = height;
                parsed.innerNormals[index] = normal;
                parsed.innerSeen[index] = true;
            } else {
                const int row = chunkY * 8 + pairRow;
                const int column = chunkX * 8 + within;
                const std::size_t index = static_cast<std::size_t>(row) * 129 + column;
                if (parsed.outerSeen[index] &&
                    (std::fabs(parsed.outer[index] - height) > kHeightTolerance ||
                     !(parsed.outerNormals[index] == normal))) {
                    std::ostringstream message;
                    message << "intra-ADT outer vertex mismatch at (" << column
                            << ',' << row << ")";
                    return fail(message.str());
                }
                parsed.outer[index] = height;
                parsed.outerNormals[index] = normal;
                parsed.outerSeen[index] = true;
            }
        }
    }
    if (std::find(parsed.outerSeen.begin(), parsed.outerSeen.end(), false) !=
            parsed.outerSeen.end() ||
        std::find(parsed.innerSeen.begin(), parsed.innerSeen.end(), false) !=
            parsed.innerSeen.end())
        return fail("not all terrain vertices were reconstructed");
    error.clear();
    return true;
}

bool compareSource(const ParsedAdt& parsed,
                   const worldwright::TerrainDefinition& source,
                   const std::string& name, std::string& error) {
    for (int row = 0; row < 129; ++row) {
        for (int column = 0; column < 129; ++column) {
            const float actual = parsed.outer[static_cast<std::size_t>(row) * 129 + column];
            const float expected = source.outerHeight(row, column);
            if (std::fabs(actual - expected) > kHeightTolerance) {
                std::ostringstream message;
                message << name << ": outer (" << column << ',' << row
                        << ") serialized=" << actual << " source=" << expected
                        << " difference=" << std::fabs(actual - expected);
                error = message.str();
                return false;
            }
        }
    }
    for (int row = 0; row < 128; ++row) {
        for (int column = 0; column < 128; ++column) {
            const float actual = parsed.inner[static_cast<std::size_t>(row) * 128 + column];
            const float expected = source.innerHeight(row, column);
            if (std::fabs(actual - expected) > kHeightTolerance) {
                std::ostringstream message;
                message << name << ": inner (" << column << ',' << row
                        << ") serialized=" << actual << " source=" << expected
                        << " difference=" << std::fabs(actual - expected);
                error = message.str();
                return false;
            }
        }
    }
    return true;
}

bool compareVerticalSeam(const ParsedAdt& west, const ParsedAdt& east,
                         const std::string& westName,
                         const std::string& eastName, std::string& error) {
    for (int row = 0; row < 129; ++row) {
        const std::size_t wi = static_cast<std::size_t>(row) * 129 + 128;
        const std::size_t ei = static_cast<std::size_t>(row) * 129;
        const float difference = std::fabs(west.outer[wi] - east.outer[ei]);
        if (difference > kHeightTolerance ||
            !(west.outerNormals[wi] == east.outerNormals[ei])) {
            std::ostringstream message;
            message << westName << " east / " << eastName << " west seam at row "
                    << row << ": heights " << west.outer[wi] << " vs "
                    << east.outer[ei] << ", difference=" << difference
                    << ", normals (" << +west.outerNormals[wi].x << ','
                    << +west.outerNormals[wi].y << ','
                    << +west.outerNormals[wi].z << ") vs ("
                    << +east.outerNormals[ei].x << ','
                    << +east.outerNormals[ei].y << ','
                    << +east.outerNormals[ei].z << ')';
            error = message.str();
            return false;
        }
    }
    return true;
}

bool compareHorizontalSeam(const ParsedAdt& north, const ParsedAdt& south,
                           const std::string& northName,
                           const std::string& southName, std::string& error) {
    for (int column = 0; column < 129; ++column) {
        const std::size_t ni = 128 * 129 + static_cast<std::size_t>(column);
        const std::size_t si = static_cast<std::size_t>(column);
        const float difference = std::fabs(north.outer[ni] - south.outer[si]);
        if (difference > kHeightTolerance ||
            !(north.outerNormals[ni] == south.outerNormals[si])) {
            std::ostringstream message;
            message << northName << " south / " << southName
                    << " north seam at column " << column << ": heights "
                    << north.outer[ni] << " vs " << south.outer[si]
                    << ", difference=" << difference;
            error = message.str();
            return false;
        }
    }
    return true;
}

std::filesystem::path adtPath(const std::filesystem::path& directory, int x,
                              int y) {
    return directory / ("W005C_" + std::to_string(x) + "_" +
                        std::to_string(y) + ".adt");
}

bool loadParsed(const std::filesystem::path& path, ParsedAdt& parsed,
                std::string& error) {
    Bytes bytes;
    if (!readFile(path, bytes)) {
        error = "could not read " + path.string();
        return false;
    }
    return parseAdt(bytes, path.filename().string(), parsed, error);
}

void testFlatAndPlaneNormals() {
    worldwright::TerrainDefinition flat;
    worldwright::AdtWriter writer;
    Bytes bytes;
    CHECK(writer.serialize(flat, 32, 32, bytes));
    ParsedAdt parsed;
    std::string error;
    CHECK(parseAdt(bytes, "flat", parsed, error));
    if (!error.empty()) std::cerr << error << '\n';
    CHECK(std::all_of(parsed.outerNormals.begin(), parsed.outerNormals.end(),
                      [](const EncodedNormal& normal) {
                          return normal == EncodedNormal{0, 0, 127};
                      }));
    CHECK(std::all_of(parsed.innerNormals.begin(), parsed.innerNormals.end(),
                      [](const EncodedNormal& normal) {
                          return normal == EncodedNormal{0, 0, 127};
                      }));

    worldwright::TerrainDefinition plane;
    constexpr float slope = 0.5f;
    constexpr float halfCell = worldwright::TerrainDefinition::TileSize / 256.0f;
    for (int row = 0; row < 129; ++row)
        for (int column = 0; column < 129; ++column)
            CHECK(plane.setOuterHeight(row, column,
                                       column * 2 * halfCell * slope));
    for (int row = 0; row < 128; ++row)
        for (int column = 0; column < 128; ++column)
            CHECK(plane.setInnerHeight(row, column,
                                       (column * 2 + 1) * halfCell * slope));
    CHECK(writer.serialize(plane, 32, 32, bytes));
    parsed = ParsedAdt{};
    CHECK(parseAdt(bytes, "plane", parsed, error));
    if (!error.empty()) std::cerr << error << '\n';
    const EncodedNormal expected{0, 57, 114};
    CHECK(parsed.outerNormals[64 * 129 + 64] == expected);
    CHECK(parsed.innerNormals[64 * 128 + 64] == expected);
}

void testEditorScenariosAndSeams() {
    TempDir first;
    worldwright::WorldEditorModel model;
    CHECK(model.newWorld("W005C"));
    for (int y = 31; y <= 33; ++y)
        for (int x = 31; x <= 33; ++x)
            CHECK(model.setTileEnabled(x, y, true));

    // Raised feature crossing an east/west ADT boundary.
    CHECK(worldwright::TerrainHeightEditor::raise(
        model, {{32.0 * worldwright::TerrainHeightEditor::TileSize,
                 31.5 * worldwright::TerrainHeightEditor::TileSize},
                90.0, 45.0}));
    // Lowered depression crossing a north/south boundary.
    CHECK(worldwright::TerrainHeightEditor::lower(
        model, {{32.5 * worldwright::TerrainHeightEditor::TileSize,
                 32.0 * worldwright::TerrainHeightEditor::TileSize},
                75.0, 30.0}));
    // Four-ADT corner feature.
    CHECK(worldwright::TerrainHeightEditor::raise(
        model, {{32.0 * worldwright::TerrainHeightEditor::TileSize,
                 32.0 * worldwright::TerrainHeightEditor::TileSize},
                110.0, 55.0}));
    CHECK(worldwright::TerrainHeightEditor::smooth(
        model, {{32.0 * worldwright::TerrainHeightEditor::TileSize,
                 32.0 * worldwright::TerrainHeightEditor::TileSize},
                125.0, 0.35}));

    worldwright::WorldExporter exporter;
    CHECK(exporter.exportWorld(model, first.path));
    if (!exporter.errorMessage().empty())
        std::cerr << exporter.errorMessage() << '\n';

    std::vector<ParsedAdt> parsed(9);
    std::string error;
    for (int y = 31; y <= 33; ++y) {
        for (int x = 31; x <= 33; ++x) {
            const std::size_t index = static_cast<std::size_t>(y - 31) * 3 + x - 31;
            CHECK(loadParsed(adtPath(first.path, x, y), parsed[index], error));
            if (!error.empty()) std::cerr << error << '\n';
            const auto* source = model.terrainAt(x, y);
            CHECK(source != nullptr);
            if (source != nullptr) {
                CHECK(compareSource(parsed[index], *source,
                                    adtPath(first.path, x, y).filename().string(),
                                    error));
                if (!error.empty()) std::cerr << error << '\n';
            }
        }
    }
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 2; ++column) {
            const std::size_t west = static_cast<std::size_t>(row) * 3 + column;
            CHECK(compareVerticalSeam(parsed[west], parsed[west + 1],
                                      "west", "east", error));
            if (!error.empty()) std::cerr << error << '\n';
        }
    }
    for (int row = 0; row < 2; ++row) {
        for (int column = 0; column < 3; ++column) {
            const std::size_t north = static_cast<std::size_t>(row) * 3 + column;
            CHECK(compareHorizontalSeam(parsed[north], parsed[north + 3],
                                        "north", "south", error));
            if (!error.empty()) std::cerr << error << '\n';
        }
    }

    // All four serialized copies of the central shared corner agree.
    const std::size_t nw = 0, ne = 1, sw = 3, se = 4;
    const float corner = parsed[nw].outer.back();
    CHECK(std::fabs(corner - parsed[ne].outer[128 * 129]) <= kHeightTolerance);
    CHECK(std::fabs(corner - parsed[sw].outer[128]) <= kHeightTolerance);
    CHECK(std::fabs(corner - parsed[se].outer[0]) <= kHeightTolerance);
    CHECK(parsed[nw].outerNormals.back() == parsed[ne].outerNormals[128 * 129]);
    CHECK(parsed[nw].outerNormals.back() == parsed[sw].outerNormals[128]);
    CHECK(parsed[nw].outerNormals.back() == parsed[se].outerNormals[0]);

    // The far mixed/untouched tile stays flat while edited tiles do not.
    CHECK(std::all_of(parsed[8].outer.begin(), parsed[8].outer.end(),
                      [](float height) { return height == 0.0f; }));
    CHECK(std::any_of(parsed[4].outerNormals.begin(), parsed[4].outerNormals.end(),
                      [](const EncodedNormal& normal) {
                          return normal.x != 0 || normal.y != 0;
                      }));

    // A second production export is byte-identical file-for-file.
    TempDir second;
    CHECK(exporter.exportWorld(model, second.path));
    for (const auto& path : worldwright::WorldExporter::outputPaths(model, first.path)) {
        Bytes a, b;
        CHECK(readFile(path, a));
        CHECK(readFile(second.path / path.filename(), b));
        CHECK(a == b);
    }
}

void testMalformedRejection() {
    worldwright::TerrainDefinition terrain;
    worldwright::AdtWriter writer;
    Bytes valid;
    CHECK(writer.serialize(terrain, 32, 32, valid));
    ParsedAdt parsed;
    std::string error;

    Bytes malformed = valid;
    putU32(malformed, 92, std::numeric_limits<std::uint32_t>::max());
    CHECK(!parseAdt(malformed, "invalid MCIN", parsed, error));

    malformed.assign(valid.begin(), valid.end() - 200);
    parsed = ParsedAdt{};
    CHECK(!parseAdt(malformed, "truncated MCNK", parsed, error));

    malformed = valid;
    const std::size_t firstMcnk = u32(malformed, 92);
    putU32(malformed, firstMcnk + 8 + 20,
           std::numeric_limits<std::uint32_t>::max());
    parsed = ParsedAdt{};
    CHECK(!parseAdt(malformed, "invalid MCVT offset", parsed, error));

    malformed = valid;
    const std::size_t mcvt = firstMcnk + u32(malformed, firstMcnk + 8 + 20);
    putU32(malformed, mcvt + 4, 576);
    parsed = ParsedAdt{};
    CHECK(!parseAdt(malformed, "invalid MCVT size", parsed, error));

    malformed = valid;
    putU32(malformed, 92 + 4, std::numeric_limits<std::uint32_t>::max());
    parsed = ParsedAdt{};
    CHECK(!parseAdt(malformed, "out-of-bounds MCNK", parsed, error));
}

void testExporterRejectsSerializedSeamMismatch() {
    TempDir output;
    worldwright::WorldEditorModel model;
    CHECK(model.newWorld("W005C"));
    CHECK(model.setTileEnabled(31, 31, true));
    CHECK(model.setTileEnabled(32, 31, true));
    auto* west = model.mutableTerrainAt(31, 31);
    CHECK(west != nullptr);
    if (west != nullptr) CHECK(west->setOuterHeight(64, 128, 10.0f));
    worldwright::WorldExporter exporter;
    CHECK(!exporter.exportWorld(model, output.path));
    CHECK(exporter.error() == worldwright::WorldExportError::VerificationFailed);
    CHECK(exporter.errorMessage().find("east/west") != std::string::npos);
    CHECK(exporter.errorMessage().find("difference=") != std::string::npos);
    CHECK(!std::filesystem::exists(output.path / "W005C.wdt"));
}

}  // namespace

int main() {
    testFlatAndPlaneNormals();
    testEditorScenariosAndSeams();
    testMalformedRejection();
    testExporterRejectsSerializedSeamMismatch();
    std::cout << "W005C integrity checks: " << checks
              << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
