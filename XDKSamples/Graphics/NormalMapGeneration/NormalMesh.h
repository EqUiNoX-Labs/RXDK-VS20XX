//-----------------------------------------------------------------------------
// File: NormalMesh.h
//
// Desc: A subclass of xbmesh that supports calculating a normal map for the 
//       mesh by shooting rays from the surface along the normal of the mesh 
//       and intersecting the rays with another mesh where the normal is 
//       sampled.
//
// Hist: 08.05.02 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef NORMALMESH_H
#define NORMALMESH_H


class RayMesh;



//-----------------------------------------------------------------------------
// Name: class NormalMesh
// Desc: A subclass of CXBMesh that support calculating normal maps from a high
//       resolution model for the mesh.
//-----------------------------------------------------------------------------
class NormalMesh : public CXBMesh
{
public:
    // Calculate the normal map for this mesh using pHighMesh.
    void CalculateNormalMap( RayMesh* pHighMesh, int iOverSample = 1 );

private:
    // Internal fucntions to calculate the normal map for frames and mesh data.
    void CalculateFrameNormalMap( XBMESH_FRAME* pFrame, D3DXMATRIX* pParentMat );
    void CalculateMeshNormalMap( XBMESH_DATA* pMesh, D3DXMATRIX* pMat );
    
    RayMesh* m_pHighMesh;
    int m_iOverSample;
};



#endif
