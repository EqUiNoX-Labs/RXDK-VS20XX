//-----------------------------------------------------------------------------
// File: LightgunMesh.h
//
// Desc: Code for rendering a model of a lightgun
//
// Hist: 10.08.02 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef LIGHTGUNMESH_H
#define LIGHTGUNMESH_H

#include <XBMesh.h>




//-----------------------------------------------------------------------------
// Name: class CLightgunMesh
// Desc: The lightgun mesh. This is overridden from the base class so that we
//       can provide a custom RenderCallback() function.
//-----------------------------------------------------------------------------
class CLightgunMesh : public CXBMesh
{
    // Subsets for within the lightgun mesh
    enum
    {
        LIGHTGUNMESH_SUBSET_TRIGGER,
        LIGHTGUNMESH_SUBSET_BUTTON0,
        LIGHTGUNMESH_SUBSET_BUTTON1,
        LIGHTGUNMESH_SUBSET_BUTTON2,
        LIGHTGUNMESH_SUBSET_BUTTON3,
        LIGHTGUNMESH_SUBSET_DPAD,
        LIGHTGUNMESH_SUBSET_BUTTON5,
        LIGHTGUNMESH_SUBSET_TOP1,
        LIGHTGUNMESH_SUBSET_LIGHTBODY,
        LIGHTGUNMESH_SUBSET_BODY,
        LIGHTGUNMESH_SUBSET_BARREL,
        LIGHTGUNMESH_SUBSET_HANDLE,
        LIGHTGUNMESH_SUBSET_GRIP,
        LIGHTGUNMESH_SUBSET_BARRELREAR,
        LIGHTGUNMESH_SUBSET_TRIGGERGUARD1,
        LIGHTGUNMESH_SUBSET_REARSITE,
        LIGHTGUNMESH_SUBSET_TRIGGERGUARD2,
        LIGHTGUNMESH_SUBSET_FRONTSITE,
        LIGHTGUNMESH_SUBSET_BARRELTIP,
        LIGHTGUNMESH_SUBSET_TOP2,
        LIGHTGUNMESH_SUBSET_LIGHT,
        LIGHTGUNMESH_NUM_SUBSETS
    };

    // Structures for rendering the mesh' subsets
    D3DXMATRIX         m_pSubsetMatrix[LIGHTGUNMESH_NUM_SUBSETS];
    LPDIRECT3DTEXTURE8 m_pSubsetTexture[LIGHTGUNMESH_NUM_SUBSETS];

public:
    // Structures for animating the gamepad controls.
    D3DXMATRIX*        m_pmatWhiteButtonMatrix;
    D3DXMATRIX*        m_pmatBlackButtonMatrix;
    D3DXMATRIX*        m_pmatXButtonMatrix;
    D3DXMATRIX*        m_pmatYButtonMatrix;
    D3DXMATRIX*        m_pmatTriggerMatrix;

    // World matrix, so the app can orient the lightgun
    D3DXMATRIX         m_matWorld;

public:
    HRESULT Create( CHAR* strFilename, CXBPackedResource* pResource );
    BOOL    RenderCallback( DWORD dwSubset, XBMESH_SUBSET* pSubset, DWORD dwFlags );
};




#endif // LIGHTGUNMESH_H
