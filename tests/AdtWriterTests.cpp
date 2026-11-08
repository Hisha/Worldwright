#include "worldwright/AdtWriter.h"
#include "worldwright/TerrainDefinition.h"

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
int checks = 0;
int failures = 0;
void check(bool value, const char* expression, int line) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL line " << line << ": " << expression << '\n'; }
}
#define CHECK(x) check((x), #x, __LINE__)

std::uint32_t u32(const Bytes& d, std::size_t o) {
    return static_cast<std::uint32_t>(d[o]) |
           static_cast<std::uint32_t>(d[o + 1]) << 8 |
           static_cast<std::uint32_t>(d[o + 2]) << 16 |
           static_cast<std::uint32_t>(d[o + 3]) << 24;
}
float f32(const Bytes& d, std::size_t o) {
    const auto bits = u32(d, o); float value = 0;
    std::memcpy(&value, &bits, sizeof(value)); return value;
}
std::string magic(const Bytes& d, std::size_t o) {
    return {static_cast<char>(d[o + 3]), static_cast<char>(d[o + 2]),
            static_cast<char>(d[o + 1]), static_cast<char>(d[o])};
}
struct Chunk { std::string id; std::size_t offset; std::uint32_t size; };
std::vector<Chunk> walk(const Bytes& d) {
    std::vector<Chunk> result; std::size_t o = 0;
    while (o + 8 <= d.size()) {
        const auto size = u32(d, o + 4);
        if (size > d.size() - o - 8) break;
        result.push_back({magic(d, o), o, size}); o += 8 + size;
    }
    return result;
}

class TempDir {
public:
    TempDir() {
        path = std::filesystem::temp_directory_path() /
               ("worldwright-adt-tests-" + std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path);
    }
    ~TempDir() { std::error_code e; std::filesystem::remove_all(path, e); }
    std::filesystem::path path;
};

void testModel() {
    worldwright::TerrainDefinition t;
    CHECK(worldwright::TerrainDefinition::ChunkCount == 256);
    CHECK(worldwright::TerrainDefinition::VerticesPerChunk == 145);
    CHECK(worldwright::TerrainDefinition::SerializedVertexCount == 37120);
    CHECK(worldwright::TerrainDefinition::UniqueVertexCount == 33025);
    CHECK(t.isValid()); CHECK(t.outerHeight(0, 0) == 0.0f);
    CHECK(t.outerHeight(128, 128) == 0.0f); CHECK(t.innerHeight(127, 127) == 0.0f);
    CHECK(std::isnan(t.outerHeight(-1, 0))); CHECK(std::isnan(t.innerHeight(128, 0)));
    CHECK(t.setFlatHeight(100.0f)); CHECK(t.outerHeight(64, 64) == 100.0f);
    CHECK(t.chunkVertexHeight(15, 15, 144) == 100.0f);
    CHECK(t.setFlatHeight(-42.5f)); CHECK(t.innerHeight(0, 0) == -42.5f);
    CHECK(!t.setFlatHeight(std::numeric_limits<float>::infinity()));
    CHECK(!t.setFlatHeight(std::numeric_limits<float>::quiet_NaN()));
    CHECK(t.outerHeight(0, 0) == -42.5f);
}

Bytes serialize(float height, int x = 32, int y = 32) {
    worldwright::TerrainDefinition t; CHECK(t.setFlatHeight(height));
    worldwright::AdtWriter w; Bytes bytes; CHECK(w.serialize(t, x, y, bytes));
    return bytes;
}

void testRawSerialization() {
    const auto d = serialize(100.0f);
    CHECK(d.size() == worldwright::AdtWriter::kSerializedSize);
    const auto chunks = walk(d);
    CHECK(chunks.size() == 266); CHECK(chunks[0].id == "MVER");
    CHECK(chunks[1].id == "MHDR"); CHECK(chunks[2].id == "MCIN");
    CHECK(u32(d, chunks[0].offset + 8) == 18); CHECK(chunks[1].size == 64);
    CHECK(chunks[2].size == 4096);
    const char* expected[] = {"MCIN","MTEX","MMDX","MMID","MWMO","MWID","MDDF","MODF"};
    const auto mhdr = chunks[1].offset + 8;
    for (std::size_t i = 0; i < 8; ++i) {
        CHECK(chunks[i + 2].id == expected[i]);
        CHECK(mhdr + u32(d, mhdr + 4 * (i + 1)) == chunks[i + 2].offset);
    }
    for (std::size_t i = 10; i < chunks.size(); ++i) CHECK(chunks[i].id == "MCNK");

    const auto mcin = chunks[2].offset + 8;
    for (int i = 0; i < 256; ++i) {
        const auto off = u32(d, mcin + i * 16);
        CHECK(off == chunks[10 + i].offset); CHECK(u32(d, mcin + i * 16 + 4) == 1236);
        CHECK(u32(d, mcin + i * 16 + 8) == 0); CHECK(u32(d, mcin + i * 16 + 12) == 0);
        const auto h = off + 8; const int cx = i % 16, cy = i / 16;
        CHECK(u32(d, h + 4) == static_cast<std::uint32_t>(cx));
        CHECK(u32(d, h + 8) == static_cast<std::uint32_t>(cy));
        CHECK(u32(d, h + 12) == 1); CHECK(u32(d, h + 16) == 0);
        const auto mcvt = off + u32(d, h + 20); const auto mcnr = off + u32(d, h + 24);
        CHECK(magic(d, mcvt) == "MCVT"); CHECK(u32(d, mcvt + 4) == 580);
        CHECK(magic(d, mcnr) == "MCNR"); CHECK(u32(d, mcnr + 4) == 435);
        for (int v = 0; v < 145; ++v) CHECK(f32(d, mcvt + 8 + v * 4) == 0.0f);
        for (int v = 0; v < 145; ++v) {
            CHECK(d[mcnr + 8 + v * 3] == 0); CHECK(d[mcnr + 9 + v * 3] == 0);
            CHECK(d[mcnr + 10 + v * 3] == 127);
        }
        CHECK(std::fabs(f32(d, h + 104) + cy * worldwright::AdtWriter::kChunkSize) < 0.01f);
        CHECK(std::fabs(f32(d, h + 108) + cx * worldwright::AdtWriter::kChunkSize) < 0.01f);
        CHECK(f32(d, h + 112) == 100.0f);
        CHECK(off + 8 + u32(d, off + 4) == (i == 255 ? d.size() : chunks[11 + i].offset));
    }
}

void testDeterminismAndCoordinates() {
    const auto a = serialize(-20.0f, 0, 0); const auto b = serialize(-20.0f, 0, 0);
    CHECK(a == b);
    const auto first = u32(a, 92); const auto last = u32(a, 92 + 255 * 16);
    CHECK(std::fabs(f32(a, first + 8 + 104) - worldwright::AdtWriter::kWorldCenter) < .01f);
    CHECK(std::fabs(f32(a, first + 8 + 108) - worldwright::AdtWriter::kWorldCenter) < .01f);
    CHECK(f32(a, last + 8 + 112) == -20.0f);
    const auto edge = serialize(0.0f, 63, 63); CHECK(!edge.empty());
    CHECK(f32(edge, u32(edge, 92) + 8 + 104) < 0.0f);
}

void testErrorsAndFiles() {
    worldwright::TerrainDefinition t; worldwright::AdtWriter w; Bytes d{1};
    CHECK(!w.serialize(t, -1, 0, d)); CHECK(d.empty());
    CHECK(w.error() == worldwright::AdtWriteError::InvalidTile);
    CHECK(!w.serialize(t, 64, 63, d));
    TempDir temp; worldwright::AdtWriteOptions noReplace; noReplace.overwrite = false;
    CHECK(!w.save(t, 0, 0, temp.path / "missing" / "x.adt", noReplace));
    const auto output = temp.path / "x.adt"; CHECK(w.save(t, 0, 0, output, noReplace));
    CHECK(std::filesystem::file_size(output) == worldwright::AdtWriter::kSerializedSize);
    CHECK(!w.save(t, 0, 0, output, noReplace));
    CHECK(w.error() == worldwright::AdtWriteError::OutputExists);
    worldwright::AdtWriteOptions replace; replace.overwrite = true;
    CHECK(w.save(t, 63, 63, output, replace));
    std::size_t temps = 0;
    for (const auto& e : std::filesystem::directory_iterator(temp.path))
        if (e.path().filename().string().find(".worldwright-") != std::string::npos) ++temps;
    CHECK(temps == 0);
}

float absoluteOuter(const Bytes& d, int chunkX, int chunkY, int row, int column) {
    const int index = chunkY * 16 + chunkX;
    const auto off = u32(d, 92 + index * 16); const auto h = off + 8;
    const auto mcvt = off + u32(d, h + 20) + 8;
    const int vertex = row == 8 ? 136 + column : row * 17 + column;
    return f32(d, h + 112) + f32(d, mcvt + vertex * 4);
}

void testNeighborSeams() {
    const auto northwest = serialize(100.0f, 31, 31);
    const auto east = serialize(100.0f, 32, 31);
    const auto south = serialize(100.0f, 31, 32);
    const auto southeast = serialize(100.0f, 32, 32);
    const auto nwEastChunk = u32(northwest, 92 + 15 * 16);
    const auto eastWestChunk = u32(east, 92);
    CHECK(std::fabs((f32(northwest, nwEastChunk + 8 + 108) -
                     8 * worldwright::AdtWriter::kChunkSize / 8) -
                    f32(east, eastWestChunk + 8 + 108)) < .01f);
    const auto nwSouthChunk = u32(northwest, 92 + 240 * 16);
    const auto southNorthChunk = u32(south, 92);
    CHECK(std::fabs((f32(northwest, nwSouthChunk + 8 + 104) -
                     8 * worldwright::AdtWriter::kChunkSize / 8) -
                    f32(south, southNorthChunk + 8 + 104)) < .01f);
    for (int row = 0; row < 9; ++row)
        CHECK(absoluteOuter(northwest, 15, 7, row, 8) ==
              absoluteOuter(east, 0, 7, row, 0));
    for (int column = 0; column < 9; ++column)
        CHECK(absoluteOuter(northwest, 7, 15, 8, column) ==
              absoluteOuter(south, 7, 0, 0, column));
    CHECK(absoluteOuter(northwest, 15, 15, 8, 8) == 100.0f);
    CHECK(absoluteOuter(east, 0, 15, 8, 0) == 100.0f);
    CHECK(absoluteOuter(south, 15, 0, 0, 8) == 100.0f);
    CHECK(absoluteOuter(southeast, 0, 0, 0, 0) == 100.0f);
}
}

int main() {
    testModel(); testRawSerialization(); testDeterminismAndCoordinates();
    testErrorsAndFiles(); testNeighborSeams();
    std::cout << "ADT writer checks: " << checks << ", failures: " << failures << '\n';
    return failures == 0 ? 0 : 1;
}
