//-----------------------------------------------------------------------------
// File: DrawTraingle.h
//
// Desc: Very odd rasterization routine that samples normals by computing 
//       intersections with a mesh.  Since the results of the rasterization
//       will be used as a texture map the usual rasterization rules don't
//       work.
//
// Hist: 08.05.02 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef DRAWTRIANGLE_H
#define DRAWTRIANGLE_H


class RayMesh;



struct SourceVert
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 norm;
    float u, v;
};



void DrawTriangle( D3DXVECTOR3* pDestNorms, int iPitch, const SourceVert* v0, 
                   const SourceVert* v1, const SourceVert* v2, RayMesh* pMesh );



#endif
