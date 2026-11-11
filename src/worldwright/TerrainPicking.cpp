#include "worldwright/TerrainPicking.h"

#include "worldwright/TerrainDefinition.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace worldwright {
namespace {

constexpr double kEpsilon = 1e-9;
constexpr double kPi = 3.14159265358979323846;

CameraVector3 add(CameraVector3 a, CameraVector3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
CameraVector3 subtract(CameraVector3 a, CameraVector3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
CameraVector3 multiply(CameraVector3 value, double scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}
double dot(CameraVector3 a, CameraVector3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
CameraVector3 cross(CameraVector3 a, CameraVector3 b) noexcept {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
CameraVector3 normalized(CameraVector3 value) noexcept {
    const double length = std::sqrt(dot(value, value));
    if (!std::isfinite(length) || length <= kEpsilon) return {};
    return multiply(value, 1.0 / length);
}

CameraVector3 vectorOf(const TerrainMeshVertex& vertex) noexcept {
    return {vertex.x, vertex.y, vertex.z};
}

bool intersectsBounds(const TerrainRay& ray, const TerrainMeshBounds& bounds,
                      double maximumDistance) noexcept {
    if (!bounds.valid) return false;
    double entry = 0.0;
    double exit = maximumDistance;
    const double origins[] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const double directions[] = {ray.direction.x, ray.direction.y,
                                 ray.direction.z};
    const double minimums[] = {bounds.minimumX, bounds.minimumY,
                               bounds.minimumZ};
    const double maximums[] = {bounds.maximumX, bounds.maximumY,
                               bounds.maximumZ};
    for (int axis = 0; axis < 3; ++axis) {
        if (std::abs(directions[axis]) <= kEpsilon) {
            if (origins[axis] < minimums[axis] - kEpsilon ||
                origins[axis] > maximums[axis] + kEpsilon) return false;
            continue;
        }
        double first = (minimums[axis] - origins[axis]) / directions[axis];
        double last = (maximums[axis] - origins[axis]) / directions[axis];
        if (first > last) std::swap(first, last);
        entry = std::max(entry, first);
        exit = std::min(exit, last);
        if (entry > exit) return false;
    }
    return exit >= 0.0;
}

std::optional<double> triangleDistance(const TerrainMesh& mesh,
                                       std::size_t firstIndex,
                                       const TerrainRay& ray) noexcept {
    if (firstIndex + 2 >= mesh.indices.size()) return std::nullopt;
    const auto ia = mesh.indices[firstIndex];
    const auto ib = mesh.indices[firstIndex + 1];
    const auto ic = mesh.indices[firstIndex + 2];
    if (ia >= mesh.vertices.size() || ib >= mesh.vertices.size() ||
        ic >= mesh.vertices.size()) return std::nullopt;
    const CameraVector3 a = vectorOf(mesh.vertices[ia]);
    const CameraVector3 edge1 = subtract(vectorOf(mesh.vertices[ib]), a);
    const CameraVector3 edge2 = subtract(vectorOf(mesh.vertices[ic]), a);
    const CameraVector3 p = cross(ray.direction, edge2);
    const double determinant = dot(edge1, p);
    if (std::abs(determinant) <= kEpsilon) return std::nullopt;
    const double inverse = 1.0 / determinant;
    const CameraVector3 fromA = subtract(ray.origin, a);
    const double u = dot(fromA, p) * inverse;
    if (u < -kEpsilon || u > 1.0 + kEpsilon) return std::nullopt;
    const double v = dot(ray.direction, cross(fromA, edge1)) * inverse;
    if (v < -kEpsilon || u + v > 1.0 + kEpsilon) return std::nullopt;
    const double distance = dot(edge2, cross(fromA, edge1)) * inverse;
    if (!std::isfinite(distance) || distance < -kEpsilon) return std::nullopt;
    return std::max(0.0, distance);
}

}  // namespace

TerrainRay TerrainPicking::screenRay(const TerrainCamera& camera,
                                     double normalizedX,
                                     double normalizedY,
                                     double aspectRatio,
                                     double verticalFovDegrees) noexcept {
    const auto origin = camera.position();
    if (!std::isfinite(normalizedX) || !std::isfinite(normalizedY) ||
        !std::isfinite(aspectRatio) || aspectRatio <= 0.0 ||
        !std::isfinite(verticalFovDegrees) || verticalFovDegrees <= 0.0 ||
        verticalFovDegrees >= 180.0) return {origin, {}};
    const auto forward = normalized(subtract(camera.target(), origin));
    const auto right = normalized(cross(forward, {0.0, 1.0, 0.0}));
    const auto up = normalized(cross(right, forward));
    const double tangent = std::tan(verticalFovDegrees * kPi / 360.0);
    const auto direction = normalized(add(
        forward, add(multiply(right, normalizedX * aspectRatio * tangent),
                     multiply(up, normalizedY * tangent))));
    return {origin, direction};
}

std::optional<TerrainPickResult> TerrainPicking::intersect(
    const TerrainMesh& mesh, const TerrainRay& inputRay) noexcept {
    TerrainRay ray = inputRay;
    ray.direction = normalized(ray.direction);
    if (dot(ray.direction, ray.direction) <= kEpsilon) return std::nullopt;
    double nearest = std::numeric_limits<double>::infinity();
    std::optional<TerrainPickResult> result;
    for (const auto& range : mesh.tileRanges) {
        if (!intersectsBounds(ray, range.bounds, nearest)) continue;
        const std::size_t end = std::min(mesh.indices.size(),
                                         range.firstIndex + range.indexCount);
        for (std::size_t index = range.firstIndex; index + 2 < end;
             index += 3) {
            const auto distance = triangleDistance(mesh, index, ray);
            if (!distance.has_value() || *distance >= nearest) continue;
            nearest = *distance;
            const auto position = add(ray.origin, multiply(ray.direction, nearest));
            result = TerrainPickResult{{position.x, position.z}, position.y,
                                       nearest, range.tile, index};
        }
    }
    return result;
}

std::optional<double> TerrainPicking::surfaceElevation(
    const TerrainMesh& mesh, TerrainPoint point) noexcept {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return std::nullopt;
    constexpr double cell = TerrainDefinition::TileSize /
                            TerrainDefinition::InnerVerticesPerTileSide;
    std::optional<double> best;
    for (const auto& range : mesh.tileRanges) {
        const double minX = range.tile.x * TerrainDefinition::TileSize;
        const double minZ = range.tile.y * TerrainDefinition::TileSize;
        if (point.x < minX - kEpsilon ||
            point.x > minX + TerrainDefinition::TileSize + kEpsilon ||
            point.y < minZ - kEpsilon ||
            point.y > minZ + TerrainDefinition::TileSize + kEpsilon) continue;
        const int column = std::clamp(
            static_cast<int>(std::floor((point.x - minX) / cell)), 0,
            TerrainDefinition::InnerVerticesPerTileSide - 1);
        const int row = std::clamp(
            static_cast<int>(std::floor((point.y - minZ) / cell)), 0,
            TerrainDefinition::InnerVerticesPerTileSide - 1);
        TerrainRay ray{{point.x, range.bounds.maximumY + 1.0, point.y},
                       {0.0, -1.0, 0.0}};
        const std::size_t cellFirst = range.firstIndex +
            static_cast<std::size_t>(row *
                TerrainDefinition::InnerVerticesPerTileSide + column) * 12;
        for (std::size_t offset = 0; offset < 12; offset += 3) {
            const auto distance = triangleDistance(mesh, cellFirst + offset, ray);
            if (!distance.has_value()) continue;
            const double elevation = ray.origin.y - *distance;
            if (!best.has_value() || elevation > *best) best = elevation;
        }
    }
    return best;
}

}  // namespace worldwright
