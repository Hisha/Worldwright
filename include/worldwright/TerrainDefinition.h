#ifndef WORLDWRIGHT_TERRAIN_DEFINITION_H
#define WORLDWRIGHT_TERRAIN_DEFINITION_H

#include <array>
#include <cstddef>

namespace worldwright {

// Geometry for one ADT tile. Heights are absolute WoW world elevations.
// The two arrays preserve WoW's staggered grid without duplicating vertices
// shared by adjacent MCNK chunks: a 129x129 outer grid and a 128x128 inner
// grid. Serialization expands these into 256 groups of 145 MCVT values.
class TerrainDefinition {
public:
    static constexpr int ChunksPerSide = 16;
    static constexpr int ChunkCount = ChunksPerSide * ChunksPerSide;
    static constexpr int CellsPerChunkSide = 8;
    static constexpr int OuterVerticesPerChunkSide = 9;
    static constexpr int OuterVerticesPerTileSide =
        ChunksPerSide * CellsPerChunkSide + 1;
    static constexpr int InnerVerticesPerTileSide =
        ChunksPerSide * CellsPerChunkSide;
    static constexpr int VerticesPerChunk = 145;
    static constexpr std::size_t SerializedVertexCount =
        static_cast<std::size_t>(ChunkCount) * VerticesPerChunk;
    static constexpr std::size_t UniqueOuterVertexCount =
        static_cast<std::size_t>(OuterVerticesPerTileSide) *
        OuterVerticesPerTileSide;
    static constexpr std::size_t UniqueInnerVertexCount =
        static_cast<std::size_t>(InnerVerticesPerTileSide) *
        InnerVerticesPerTileSide;
    static constexpr std::size_t UniqueVertexCount =
        UniqueOuterVertexCount + UniqueInnerVertexCount;

    TerrainDefinition() = default;

    // Rejects NaN and infinity without changing the current terrain.
    bool setFlatHeight(float elevation) noexcept;

    float outerHeight(int row, int column) const noexcept;
    float innerHeight(int row, int column) const noexcept;
    float chunkVertexHeight(int chunkX, int chunkY,
                            int vertexIndex) const noexcept;
    bool isValid() const noexcept;

private:
    std::array<float, UniqueOuterVertexCount> m_outer{};
    std::array<float, UniqueInnerVertexCount> m_inner{};
};

}  // namespace worldwright

#endif
