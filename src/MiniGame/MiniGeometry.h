#pragma once
#include <cstdint>
#include <vector>
#include <bgfx/bgfx.h>

namespace Caesura {

// ===========================================================================
// MiniGeometry — procedural 3D geometry generators for mini-games
// ===========================================================================
// Generates vertex (PosNormVertex) + index data for common primitives.
// All generators return standalone VB/IB pairs suitable for bgfx submission.

enum class MiniGeoType : uint8_t {
    Cube   = 0,
    Sphere = 1,
    Plane  = 2,
    Quad   = 3,
    Count
};

struct PosNormVertex {
    float x, y, z;
    float nx, ny, nz;
};

struct GeometryData {
    std::vector<PosNormVertex> vertices;
    std::vector<uint16_t>      indices;
};

// -- Generators -----------------------------------------------------------

// Unit cube (1x1x1 centered), 24 unique vertices + 36 indices
GeometryData createCubeGeometry();

// UV sphere with given subdivisions (lat x lon segments)
// radius = 1.0, centered at origin
GeometryData createSphereGeometry(uint32_t segments = 16);

// Flat plane on XZ plane, normal pointing +Y
// w,h = width/depth, centered at origin
GeometryData createPlaneGeometry(float width = 10.0f, float height = 10.0f,
                                  uint32_t subdivX = 1, uint32_t subdivZ = 1);

// Single quad facing +Z (billboard-ready), 1x1 unit
GeometryData createQuadGeometry();

// -- bgfx helpers ---------------------------------------------------------

// Create a bgfx vertex layout for PosNormVertex
bgfx::VertexLayout posNormLayout();

// Create VB + IB from GeometryData
bgfx::VertexBufferHandle createVB(const GeometryData& geo);
bgfx::IndexBufferHandle  createIB(const GeometryData& geo);

} // namespace Caesura
