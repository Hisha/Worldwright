#include "worldwright/TerrainDefinition.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace worldwright {
namespace {

constexpr float invalidHeight() noexcept {
    return std::numeric_limits<float>::quiet_NaN();
}

}  // namespace

bool TerrainDefinition::setFlatHeight(float elevation) noexcept {
    if (!std::isfinite(elevation)) {
        return false;
    }
    m_outer.fill(elevation);
    m_inner.fill(elevation);
    return true;
}

float TerrainDefinition::outerHeight(int row, int column) const noexcept {
    if (row < 0 || column < 0 || row >= OuterVerticesPerTileSide ||
        column >= OuterVerticesPerTileSide) {
        return invalidHeight();
    }
    return m_outer[static_cast<std::size_t>(row) *
                       OuterVerticesPerTileSide +
                   static_cast<std::size_t>(column)];
}

float TerrainDefinition::innerHeight(int row, int column) const noexcept {
    if (row < 0 || column < 0 || row >= InnerVerticesPerTileSide ||
        column >= InnerVerticesPerTileSide) {
        return invalidHeight();
    }
    return m_inner[static_cast<std::size_t>(row) *
                       InnerVerticesPerTileSide +
                   static_cast<std::size_t>(column)];
}

float TerrainDefinition::chunkVertexHeight(int chunkX, int chunkY,
                                           int vertexIndex) const noexcept {
    if (chunkX < 0 || chunkY < 0 || chunkX >= ChunksPerSide ||
        chunkY >= ChunksPerSide || vertexIndex < 0 ||
        vertexIndex >= VerticesPerChunk) {
        return invalidHeight();
    }

    // MCVT alternates 9 outer vertices and 8 inner vertices for each of
    // eight cell rows, followed by the final row of 9 outer vertices.
    const int pairRow = vertexIndex / 17;
    const int withinRow = vertexIndex % 17;
    if (pairRow < CellsPerChunkSide) {
        if (withinRow < OuterVerticesPerChunkSide) {
            return outerHeight(chunkY * CellsPerChunkSide + pairRow,
                               chunkX * CellsPerChunkSide + withinRow);
        }
        return innerHeight(chunkY * CellsPerChunkSide + pairRow,
                           chunkX * CellsPerChunkSide +
                               withinRow - OuterVerticesPerChunkSide);
    }

    return outerHeight(chunkY * CellsPerChunkSide + CellsPerChunkSide,
                       chunkX * CellsPerChunkSide + withinRow);
}

bool TerrainDefinition::isValid() const noexcept {
    const auto finite = [](float value) { return std::isfinite(value); };
    return std::all_of(m_outer.begin(), m_outer.end(), finite) &&
           std::all_of(m_inner.begin(), m_inner.end(), finite);
}

}  // namespace worldwright
