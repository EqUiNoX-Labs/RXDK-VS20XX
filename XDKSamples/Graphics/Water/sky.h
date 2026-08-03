//-----------------------------------------------------------------------------
// File: Sky.h
//
// Desc: This file contains the sky class. 
//
// Hist: 11.14.02 - Created
//       12.10.02 - Optimized and code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#include <XBMesh.h>
#include "WaterDefs.h"




//-----------------------------------------------------------------------------
// Name: class CSky
// Desc: The class for rendering the sky. The sky is a shere that uses a
//       cubemap texture.
//-----------------------------------------------------------------------------
class CSky
{
    CXBMesh                 m_SkyMesh;
    CXBPackedResource       m_xprTextures;

    DWORD                   m_dwVertexShaderHandle;
    DWORD                   m_dwPixelShaderHandle;

    LPDIRECT3DCUBETEXTURE8  m_pTexture;

public:
    HRESULT Initialize();
    HRESULT Render( ICullFrustum* pCullFrustumObject );
};
