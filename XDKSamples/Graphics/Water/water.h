//-----------------------------------------------------------------------------
// File: Water.h
//
// Desc: Four texture stages are used to render the water: wave, reflection,
//       refraction and Fresnel effect:
//
//       Reflection and refraction textures are prepared already before water
//       rendering (See CReflection and CRefraction).
//       Fresnel texture is the most significant component here. It gives the
//       reflection refraction ratio and an approximate normal per-pixel.
//       A bump texture is used to bump the other three textures with different
//       scale and direction.
//
// Hist: 11.14.02 - Created
//       12.10.02 - Optimized and code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once

#include "waterdefs.h"
#include "refraction.h"
#include "reflection.h"


// The circle time of water. A large number means slow movent.
const FLOAT c_fWaterTime       = 2; // In seconds
const INT   c_nNumWaterTimeDiv = 17; // Need 17 bumpmap textures
const FLOAT c_WaterTimeSpan    = c_fWaterTime / c_nNumWaterTimeDiv;// In seconds

// The max buffer size of water plane mesh
const DWORD c_dwMaxVertices = 1218;
const DWORD c_dwMaxIndexBufferSize   = 4592;




//-----------------------------------------------------------------------------
// Name: class CWater
// Desc: The class for water rendering. To render water, we need 5 textures:
//       fresnel texture, reflection texture, refraction texture , bumpmap
//       texture and fog texture.
//       File 'Water.vsh' and 'Water.psh' define the vertex shader and the
//       pixel shader used in rendering the water. You can refer to the code in
//       the shader files to see how it works.
//-----------------------------------------------------------------------------
class CWater
{
protected:
    // Textures

    LPDIRECT3DTEXTURE8 m_pTexFresnel;
    // Reflection is gotten from g_pApp->m_pReflection->GetTexture()
    // Refraction is gotten from g_pApp->m_pRefraction->GetTexture()

    // Bumpmap texture
    INT                m_nCurBumpID;
    LPDIRECT3DTEXTURE8 m_prgBumpTextures[c_nNumWaterTimeDiv];
    FLOAT              m_fBumpSpeedFactor;
  
    // Bump Scales
    FLOAT              m_fReflectionBumpScale; 
    FLOAT              m_fRefractionBumpScale;
    FLOAT              m_fFresnelBumpScale;
    D3DXVECTOR4        m_vTexCoordScale;

    // Shaders
    DWORD              m_dwPixelShaderHandle;
    DWORD              m_dwVertexShaderHandle;
   
    // Mesh
    LPDIRECT3DINDEXBUFFER8  m_pIB;
    LPDIRECT3DVERTEXBUFFER8 m_pVB;

    DWORD                   m_dwNumVertices;
    DWORD                   m_dwNumIndices;
    
    D3DXMATRIX              m_matWorld;
    D3DXMATRIX              m_matReflectionBump;
    D3DXMATRIX              m_matRefractionBump;
    D3DXMATRIX              m_matFresnelBump;
    D3DXMATRIX              m_matScaleReflection;


protected:
    HRESULT AlterBumpTextureIndex();

    HRESULT CreateShaders();
    HRESULT InitWaterPlaneMesh();
    HRESULT LoadBumpTextures();

    HRESULT MoveWater();
    HRESULT ReleaseBump();
    
public:
    CWater();
    virtual ~CWater();
    HRESULT Initialize();
    HRESULT FrameMove();
    HRESULT Render();
    HRESULT Cleanup();
};
