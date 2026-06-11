#include "MiniGeometry.h"
#include <cmath>
#include <cstdio>

namespace Caesura {

// ===========================================================================
// Vertex layout
// ===========================================================================

bgfx::VertexLayout posNormLayout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,   3, bgfx::AttribType::Float)
        .end();
    return layout;
}

bgfx::VertexBufferHandle createVB(const GeometryData& geo) {
    const bgfx::Memory* mem = bgfx::copy(geo.vertices.data(),
        uint32_t(geo.vertices.size() * sizeof(PosNormVertex)));
    return bgfx::createVertexBuffer(mem, posNormLayout());
}

bgfx::IndexBufferHandle createIB(const GeometryData& geo) {
    const bgfx::Memory* mem = bgfx::copy(geo.indices.data(),
        uint32_t(geo.indices.size() * sizeof(uint16_t)));
    return bgfx::createIndexBuffer(mem);
}

// ===========================================================================
// Cube
// ===========================================================================

GeometryData createCubeGeometry() {
    // 24 unique vertices (position + normal), 36 indices
    GeometryData geo;
    geo.vertices = {
        // +X face
        { 0.5f,-0.5f, 0.5f,  1,0,0 }, { 0.5f, 0.5f, 0.5f,  1,0,0 },
        { 0.5f, 0.5f,-0.5f,  1,0,0 }, { 0.5f,-0.5f,-0.5f,  1,0,0 },
        // -X face
        {-0.5f,-0.5f,-0.5f, -1,0,0 }, {-0.5f, 0.5f,-0.5f, -1,0,0 },
        {-0.5f, 0.5f, 0.5f, -1,0,0 }, {-0.5f,-0.5f, 0.5f, -1,0,0 },
        // +Y face
        {-0.5f, 0.5f, 0.5f,  0,1,0 }, { 0.5f, 0.5f, 0.5f,  0,1,0 },
        { 0.5f, 0.5f,-0.5f,  0,1,0 }, {-0.5f, 0.5f,-0.5f,  0,1,0 },
        // -Y face
        {-0.5f,-0.5f,-0.5f,  0,-1,0},{ 0.5f,-0.5f,-0.5f,  0,-1,0},
        { 0.5f,-0.5f, 0.5f,  0,-1,0},{-0.5f,-0.5f, 0.5f,  0,-1,0},
        // +Z face
        {-0.5f,-0.5f, 0.5f,  0,0,1 }, { 0.5f,-0.5f, 0.5f,  0,0,1 },
        { 0.5f, 0.5f, 0.5f,  0,0,1 }, {-0.5f, 0.5f, 0.5f,  0,0,1 },
        // -Z face
        { 0.5f,-0.5f,-0.5f,  0,0,-1},{-0.5f,-0.5f,-0.5f,  0,0,-1},
        {-0.5f, 0.5f,-0.5f,  0,0,-1},{ 0.5f, 0.5f,-0.5f,  0,0,-1},
    };

    uint16_t faces[] = {
         0,1,2,  0,2,3,   4,5,6,  4,6,7,
         8,9,10, 8,10,11, 12,13,14,12,14,15,
        16,17,18,16,18,19,20,21,22,20,22,23,
    };
    geo.indices.assign(faces, faces + 36);
    return geo;
}

// ===========================================================================
// UV Sphere (lat/lon subdivision)
// ===========================================================================

GeometryData createSphereGeometry(uint32_t segments) {
    GeometryData geo;
    uint32_t latSteps = segments;
    uint32_t lonSteps = segments * 2;

    // Vertices: poles + rings
    // Top pole
    geo.vertices.push_back({0, 1, 0, 0, 1, 0});

    // Rings (excluding poles)
    for (uint32_t lat = 1; lat < latSteps; ++lat) {
        float theta = float(lat) * 3.14159265f / float(latSteps);
        float sinT = sinf(theta);
        float cosT = cosf(theta);

        for (uint32_t lon = 0; lon < lonSteps; ++lon) {
            float phi = float(lon) * 2.0f * 3.14159265f / float(lonSteps);
            float sinP = sinf(phi);
            float cosP = cosf(phi);

            float nx = sinT * cosP;
            float ny = cosT;
            float nz = sinT * sinP;
            geo.vertices.push_back({nx, ny, nz, nx, ny, nz});
        }
    }

    // Bottom pole
    geo.vertices.push_back({0, -1, 0, 0, -1, 0});

    // Indices
    // Top cap
    for (uint32_t lon = 0; lon < lonSteps; ++lon) {
        uint16_t next = (lon + 1) % lonSteps;
        geo.indices.push_back(0);
        geo.indices.push_back(uint16_t(1 + lon));
        geo.indices.push_back(uint16_t(1 + next));
    }

    // Middle rings
    for (uint32_t lat = 0; lat < latSteps - 2; ++lat) {
        uint32_t rowA = 1 + lat * lonSteps;
        uint32_t rowB = 1 + (lat + 1) * lonSteps;
        for (uint32_t lon = 0; lon < lonSteps; ++lon) {
            uint16_t next = (lon + 1) % lonSteps;
            geo.indices.push_back(uint16_t(rowA + lon));
            geo.indices.push_back(uint16_t(rowB + lon));
            geo.indices.push_back(uint16_t(rowA + next));
            geo.indices.push_back(uint16_t(rowB + lon));
            geo.indices.push_back(uint16_t(rowB + next));
            geo.indices.push_back(uint16_t(rowA + next));
        }
    }

    // Bottom cap
    uint16_t bottomIdx = uint16_t(geo.vertices.size() - 1);
    uint32_t bottomRow = 1 + (latSteps - 2) * lonSteps;
    for (uint32_t lon = 0; lon < lonSteps; ++lon) {
        uint16_t next = (lon + 1) % lonSteps;
        geo.indices.push_back(uint16_t(bottomRow + lon));
        geo.indices.push_back(bottomIdx);
        geo.indices.push_back(uint16_t(bottomRow + next));
    }

    printf("[MiniGeometry] Sphere: %zu verts, %zu tris (segments=%u)\n",
           geo.vertices.size(), geo.indices.size() / 3, segments);
    return geo;
}

// ===========================================================================
// Plane (XZ plane, normal +Y)
// ===========================================================================

GeometryData createPlaneGeometry(float width, float depth,
                                  uint32_t subdivX, uint32_t subdivZ) {
    GeometryData geo;
    float hw = width * 0.5f;
    float hd = depth * 0.5f;

    uint32_t vertsX = subdivX + 1;
    uint32_t vertsZ = subdivZ + 1;

    for (uint32_t iz = 0; iz < vertsZ; ++iz) {
        for (uint32_t ix = 0; ix < vertsX; ++ix) {
            float x = -hw + float(ix) / float(subdivX) * width;
            float z = -hd + float(iz) / float(subdivZ) * depth;
            geo.vertices.push_back({x, 0, z, 0, 1, 0});
        }
    }

    for (uint32_t iz = 0; iz < subdivZ; ++iz) {
        for (uint32_t ix = 0; ix < subdivX; ++ix) {
            uint16_t a = uint16_t(iz * vertsX + ix);
            uint16_t b = uint16_t(a + 1);
            uint16_t c = uint16_t((iz + 1) * vertsX + ix);
            uint16_t d = uint16_t(c + 1);
            geo.indices.push_back(a); geo.indices.push_back(c); geo.indices.push_back(b);
            geo.indices.push_back(c); geo.indices.push_back(d); geo.indices.push_back(b);
        }
    }

    printf("[MiniGeometry] Plane: %zu verts, %zu tris\n",
           geo.vertices.size(), geo.indices.size() / 3);
    return geo;
}

// ===========================================================================
// Quad (single quad facing +Z, 1x1)
// ===========================================================================

GeometryData createQuadGeometry() {
    GeometryData geo;
    // Two triangles, facing +Z (normal 0,0,1)
    geo.vertices = {
        {-0.5f, -0.5f, 0, 0,0,1},
        { 0.5f, -0.5f, 0, 0,0,1},
        { 0.5f,  0.5f, 0, 0,0,1},
        {-0.5f,  0.5f, 0, 0,0,1},
    };
    geo.indices = {0,1,2, 0,2,3};
    return geo;
}

} // namespace Caesura
