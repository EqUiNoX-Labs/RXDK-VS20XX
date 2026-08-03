//-----------------------------------------------------------------------------
// File: XBFurMesh.h
//
// Desc: 
//
// Hist: 11.11.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include "XBFur.h"




//-----------------------------------------------------------------------------
// Name: class CXBFurMesh
// Desc: 
//-----------------------------------------------------------------------------
class CXBFurMesh
{
public:
    DWORD                   m_dwNumVertices; // Current # of vertices
    D3DVertexBuffer*        m_pVB;           // Vertex buffer
    DWORD                   m_dwNumIndices;  // Current # of indices
    D3DIndexBuffer*         m_pIB;           // Index buffer

    D3DTexture*             m_pTexture;      // Base mesh texture

    FLOAT       m_fFinDotProductThreshold;   // Max dot product of fin's face normal with eye vector

    DWORD       m_dwNumFinBins;              // Number of discretized directions for fin bins

    struct FinBin 
    {
        D3DXVECTOR3 m_vDirection;            // Selection direction for this bin
        FLOAT       m_fDirectionThreshold;   // Cutoff for dot product of eye direction with m_vDirection
        DWORD       m_dwNumFins;             // Number of fins in this bin
        D3DVertexBuffer* m_pFinVB;           // Fin vertex buffer filled with 1 quad per edge
    };
    FinBin* m_rFinBin;
    
    D3DXVECTOR4 m_vShellOffset;              // Current shell offset for drawing fins

    DWORD       m_dwSavedLightingState;      // Saved state for before/after drawing

    D3DXMATRIX  m_matViewProjection;

    D3DXVECTOR3 m_vEyeWorldPos;              // Position of eye in world coords
    D3DXVECTOR3 m_vEyeObjectPos;             // Position of eye in local object coords
    D3DXVECTOR3 m_vEyeDirection;             // Direction from center of object to eye in local object coords

    D3DXVECTOR3 m_vLightWorldPos;            // Position of light in world coords
    D3DXVECTOR3 m_vLightObjectPos;           // Position of light in local object coords
    D3DXVECTOR3 m_vLightDirection;           // Direction from center of object to light in local object coords
    
    CXBFurMesh();
    ~CXBFurMesh();

    HRESULT Initialize( DWORD dwFVF, DWORD dwVertexCount, LPDIRECT3DVERTEXBUFFER8 pVB, DWORD dwIndexCount, LPDIRECT3DINDEXBUFFER8 pIB );
    HRESULT ExtractFins( UINT BinFactor, FLOAT fFinDotProductThreshold, FLOAT fEdgeTextureScale ); // nFinBin = octohedron sphere subdivision = 4*(BinFactor + 1)*(BinFactor +1) + 2
    HRESULT CleanFins(); // deallocate memory

    // State setting and drawing are put in separate functions so that when multiple furry models
    // are rendered (especially with the same fur) we can get some state-setting savings.
    void Begin( D3DXVECTOR3* pvEyePos, D3DXVECTOR3* pvLightPos, D3DXMATRIX* pmatViewProjection );  // set state common to both fins and fur
    void BeginObject( D3DXMATRIX* pmatWorld, D3DXMATRIX* pmatWorldInverse );
    void DrawFins( CXBFur* pFur, DWORD dwFinVS,
                   FLOAT fFinLODFull, // LOD values less than this are fully on
                   FLOAT fFinLODCutoff, // LOD values beyond this are off
                   FLOAT fFinExtraNormalScale ); // fins look better if then stick out a little more than the fur
    void DrawShells( CXBFur* pFur, DWORD dwFurVS, DWORD dwFurPS[3] );
    void EndObject();
    void End();
};


