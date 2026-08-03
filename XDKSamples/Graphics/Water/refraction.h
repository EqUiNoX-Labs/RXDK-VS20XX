//-----------------------------------------------------------------------------
// File: Refraction.h
//
// Desc: Class to render the "below water" model parts. The refraction 
//       texture is used in the final water rendering. See CWater for more 
//       information.
//
// Hist: 11.14.02 - Created
//       12.10.02 - Optimized and code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#include "WaterDefs.h"
#include "WaterApp.h"
#include "NoneWater.h"


//The fog texture and its width and height
const INT c_nFogTextureWidth  = 32;
const INT c_nFogTextureHeight = 2;




//-----------------------------------------------------------------------------
// Name: class CRefraction
// Desc: This class creates a rendertarget and renders the "below water" part
//       of the non-water scene into texture.
//-----------------------------------------------------------------------------
class CRefraction: public ICullFrustum
{
protected:
    // For rendering into a texture
    LPDIRECT3DTEXTURE8          m_pTexture;
    LPDIRECT3DSURFACE8          m_pSurface;
    LPDIRECT3DSURFACE8          m_pZBuffer;
    D3DVIEWPORT8                m_Viewport;

    //The pointer to the effect is used to set the parameters of fog.
    LPD3DXEFFECT                m_pRefractionEffect;
    //For fog
    LPDIRECT3DTEXTURE8          m_pWaterFogTex;
    //Linear fog's parameters
    D3DXVECTOR3                 m_vWaterFogFarColor;

protected:
    HRESULT RenderToTexture();

public:
    CRefraction();
    virtual ~CRefraction();

    HRESULT Initialize();
    HRESULT FrameMoveAndUpdateTexture();
    HRESULT Cleanup();

    //Interface from ICullFrustum
    D3DXMATRIX* GetViewMatrix()              { return g_pApp->GetViewMatrix(); }

    D3DXMATRIX* GetProjMatrix()              { return g_pApp->GetProjMatrix(); }

    D3DXMATRIX* GetViewProjMultiMatrix()     { return g_pApp->GetViewProjMultiMatrix(); }

    BOOL IsReflected()                       { return FALSE; }

    LPDIRECT3DBASETEXTURE8 GetTexture()      { return m_pTexture; }
};
