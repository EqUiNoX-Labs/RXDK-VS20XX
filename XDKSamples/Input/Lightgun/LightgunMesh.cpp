//-----------------------------------------------------------------------------
// File: LightgunMesh.cpp
//
// Desc: Code for rendering a model of a lightgun
//
// Hist: 10.08.02 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <XBMesh.h>
#include <XBResource.h>
#include "LightgunMesh.h"




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Creates the lightgun mesh from an XBG file and packed resources.
//-----------------------------------------------------------------------------
HRESULT CLightgunMesh::Create( CHAR* strFilename, CXBPackedResource* pResource )
{
    // Load the object using the base class
    if( FAILED( CXBMesh::Create( strFilename ) ) )
        return E_FAIL;

    // World matrix, so the app can orient the lightgun
    D3DXMatrixIdentity( &m_matWorld );

    // Initialize the local matrix for each subset
    for( DWORD i=0; i< LIGHTGUNMESH_NUM_SUBSETS; i++ )
    {
        D3DXMatrixIdentity( &m_pSubsetMatrix[i] );
    }
    m_pmatWhiteButtonMatrix = &m_pSubsetMatrix[LIGHTGUNMESH_SUBSET_BUTTON0];
    m_pmatBlackButtonMatrix = &m_pSubsetMatrix[LIGHTGUNMESH_SUBSET_BUTTON1];
    m_pmatXButtonMatrix     = &m_pSubsetMatrix[LIGHTGUNMESH_SUBSET_BUTTON2];
    m_pmatYButtonMatrix     = &m_pSubsetMatrix[LIGHTGUNMESH_SUBSET_BUTTON3];
    m_pmatTriggerMatrix     = &m_pSubsetMatrix[LIGHTGUNMESH_SUBSET_TRIGGER];

    // Assign textures to the various lightgun body parts
    D3DTexture* pShinyBlackTexture  = pResource->GetTexture( "ShinyBlack" );
    D3DTexture* pMatteBlackTexture  = pResource->GetTexture( "MatteBlack" );
    D3DTexture* pWhiteGlassTexture  = pResource->GetTexture( "WhiteGlass" );
    D3DTexture* pBlueGlassTexture   = pResource->GetTexture( "BlueGlass" );
    D3DTexture* pYellowGlassTexture = pResource->GetTexture( "YellowGlass" );
    D3DTexture* pRedGlassTexture    = pResource->GetTexture( "RedGlass" );
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_TRIGGER]       = pShinyBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_BUTTON0]       = pWhiteGlassTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_BUTTON1]       = pShinyBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_BUTTON2]       = pBlueGlassTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_BUTTON3]       = pYellowGlassTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_DPAD]          = pShinyBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_BUTTON5]       = pWhiteGlassTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_TOP1]          = pMatteBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_LIGHTBODY]     = pMatteBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_BODY]          = pMatteBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_BARREL]        = pMatteBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_HANDLE]        = pMatteBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_GRIP]          = pMatteBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_BARRELREAR]    = pMatteBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_TRIGGERGUARD1] = pShinyBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_REARSITE]      = pShinyBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_TRIGGERGUARD2] = pShinyBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_FRONTSITE]     = pShinyBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_BARRELTIP]     = pShinyBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_TOP2]          = pMatteBlackTexture;
    m_pSubsetTexture[LIGHTGUNMESH_SUBSET_LIGHT]         = pRedGlassTexture;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderCallback()
// Desc: Overridden from the base class so that we can animate and highlight
//       individual mesh subsets before rendering them.
//-----------------------------------------------------------------------------
BOOL CLightgunMesh::RenderCallback( DWORD dwSubset, XBMESH_SUBSET* pSubset,
                                    DWORD dwFlags )
{
    // Set matrix
    D3DXMATRIX mat;
    D3DXMatrixMultiply( &mat, &m_pSubsetMatrix[dwSubset], &m_matWorld );
    D3DDevice::SetTransform( D3DTS_WORLD, &mat );

    // Set material
    D3DMATERIAL8 mtrl;
    XBUtil_InitMaterial( mtrl, 1.0f, 1.0f, 1.0f, 1.0f );
    D3DDevice::SetMaterial( &mtrl );

    // Set texture
    D3DDevice::SetTexture( 0, m_pSubsetTexture[dwSubset] );

    return TRUE;
};



