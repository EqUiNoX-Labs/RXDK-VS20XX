//-----------------------------------------------------------------------------
// File: BumpMesh.h
//
// Desc: A subclass of xbmesh that supports texture space bumpmapping.
//
// Hist:  02.01.03 - New for Feb 2003 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef BUMPMESH_H
#define BUMPMESH_H

#include <XBMesh.h>




//-----------------------------------------------------------------------------
// Name: class BumpMesh
// Desc: A subclass of XBMesh that supports texture space bumpmapping.
//-----------------------------------------------------------------------------
class BumpMesh : public CXBMesh
{
public:
    // Constructor/Destructor.
    BumpMesh();
    ~BumpMesh();

    // Calculate the texture space basis for each vertex of the mesh.
    HRESULT CalculateTextureSpaceBasis();

    // Override the mesh callback to set the basis vectors.
    VOID RenderMeshCallback( DWORD dwFrame, XBMESH_FRAME* pFrame, 
                             DWORD dwFlags );

    // Light position to set in RenderMeshCallback.
    D3DXVECTOR3 m_vLightPos;

private:
    // Internal functions to calculate the texture space basis.
    VOID CalculateFrameTextureSpaceBasis( XBMESH_FRAME* pFrame, 
                                          D3DXMATRIX* pParentMat );

    VOID CalculateMeshTextureSpaceBasis( XBMESH_DATA* pMesh, 
                                         LPDIRECT3DVERTEXBUFFER8* pBasisVB, 
                                         D3DXMATRIX* pMat );

    // Additional data from each XBMESH_FRAME.
    // Just a vertex buffer containing the texture space basis vectors.
    LPDIRECT3DVERTEXBUFFER8* m_pMeshBasisVB;
};




#endif
