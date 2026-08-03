//-----------------------------------------------------------------------------
// File: Reflection.h
//
// Desc: Class to render the "above water" model parts into a texture. This
//       refraction texture is used in the final water rendering. See 
//       water.h/cpp for more information
//
// Hist: 11.14.02 - Created
//       12.10.02 - Optimized and code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "WaterDefs.h"
#include "WaterApp.h"
#include "NoneWater.h"




//-----------------------------------------------------------------------------
// Name: class CReflection
// Desc: This class creates a rendertarget and renders the "above water" model
//       parts into a texture.
//-----------------------------------------------------------------------------
class CReflection: public ICullFrustum
{
protected:
    // For ICullFrustum use
    D3DXMATRIX                  m_matView;
    D3DXMATRIX                  m_matViewProj;

    // For rendering into a texture
    LPDIRECT3DTEXTURE8          m_pTexture;
    LPDIRECT3DSURFACE8          m_pSurface;
    LPDIRECT3DSURFACE8          m_pZBuffer;
    D3DVIEWPORT8                m_Viewport;

protected:
    HRESULT RenderToTexture();

public:
    CReflection();
    virtual ~CReflection();

    HRESULT Initialize();
    HRESULT FrameMoveAndUpdateTexture();
    HRESULT Cleanup();

    // Interface from ICullFrustum
    D3DXMATRIX* GetViewMatrix()            { return &m_matView; }

    D3DXMATRIX* GetProjMatrix()            { return g_pApp->GetProjMatrix(); }

    D3DXMATRIX* GetViewProjMultiMatrix()   { return &m_matViewProj; }

    BOOL IsReflected()                     { return TRUE; }
   
    LPDIRECT3DBASETEXTURE8 GetTexture()    { return m_pTexture; }
};

