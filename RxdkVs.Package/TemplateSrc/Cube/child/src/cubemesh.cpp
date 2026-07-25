//------------------------------------------------------------------------------
// cubemesh.cpp — implementation of the cubemesh library.
//
// Builds to out/cubemesh.lib (type: "library"); linked into the cube app, which
// references it via projectReferences: ["libs/cubemesh"].
//------------------------------------------------------------------------------
#include "cubemesh.h"

namespace {

// 0xAARRGGBB from a corner's sign along each axis: +X adds red, +Y green, +Z blue.
unsigned long CornerColor(float x, float y, float z)
{
    unsigned long c = 0xFF000000UL;
    if (x > 0.0f) { c |= 0x00FF0000UL; }
    if (y > 0.0f) { c |= 0x0000FF00UL; }
    if (z > 0.0f) { c |= 0x000000FFUL; }
    return c;
}

} // namespace

void BuildColorCube(CubeVertex* verts, unsigned short* indices, float h)
{
    const float px[CUBE_VERTEX_COUNT] = { -h,  h,  h, -h, -h,  h,  h, -h };
    const float py[CUBE_VERTEX_COUNT] = { -h, -h,  h,  h, -h, -h,  h,  h };
    const float pz[CUBE_VERTEX_COUNT] = { -h, -h, -h, -h,  h,  h,  h,  h };

    for (int i = 0; i < CUBE_VERTEX_COUNT; ++i) {
        verts[i].x = px[i];
        verts[i].y = py[i];
        verts[i].z = pz[i];
        verts[i].color = CornerColor(px[i], py[i], pz[i]);
    }

    // 12 triangles (two per face). Culling is disabled by the app, so winding is
    // not significant.
    static const unsigned short kIndices[CUBE_INDEX_COUNT] = {
        0, 1, 2,  0, 2, 3, // front  (z = -h)
        4, 6, 5,  4, 7, 6, // back   (z = +h)
        4, 5, 1,  4, 1, 0, // bottom (y = -h)
        3, 2, 6,  3, 6, 7, // top    (y = +h)
        4, 0, 3,  4, 3, 7, // left   (x = -h)
        1, 5, 6,  1, 6, 2  // right  (x = +h)
    };
    for (int i = 0; i < CUBE_INDEX_COUNT; ++i) {
        indices[i] = kIndices[i];
    }
}
