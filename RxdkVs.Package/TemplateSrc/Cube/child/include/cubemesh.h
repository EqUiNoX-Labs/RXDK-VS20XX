//------------------------------------------------------------------------------
// cubemesh.h — public interface of the cubemesh library.
//
// Generates the geometry for a colored cube. Exported to the app via
// publicIncludePaths, so main.cpp can #include "cubemesh.h" directly. Pure data —
// no Direct3D or SDK dependency (the app uploads the arrays into D3D buffers).
//------------------------------------------------------------------------------
#pragma once

enum {
    CUBE_VERTEX_COUNT = 8,
    CUBE_INDEX_COUNT = 36 // 12 triangles * 3
};

// One cube vertex: position + packed 0xAARRGGBB diffuse color. Layout matches the
// app's FVF (D3DFVF_XYZ | D3DFVF_DIFFUSE): 3 floats then a 32-bit color.
struct CubeVertex {
    float x, y, z;
    unsigned long color;
};

// Fill the vertex and index arrays for a cube centered at the origin with the
// given half-extent. Each corner gets a distinct color so the faces show smooth
// RGB gradients. verts must hold CUBE_VERTEX_COUNT; indices must hold CUBE_INDEX_COUNT.
void BuildColorCube(CubeVertex* verts, unsigned short* indices, float halfSize);
