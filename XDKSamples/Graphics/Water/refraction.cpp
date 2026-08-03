//-----------------------------------------------------------------------------
// File: Refraction.cpp
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
#include "WaterDefs.h"
#include "Refraction.h"
#include "NoneWater.h" 
#include "ResMan.h"


//Water Fog texture
const CHAR* c_strFogTextureFileName = "Water\\Textures\\Waterfog.xpr";




//-----------------------------------------------------------------------------
// Name: CRefraction()
// Desc: Contructor
//-----------------------------------------------------------------------------
CRefraction::CRefraction()
{
    m_pZBuffer          = NULL;
    m_pSurface          = NULL;
    m_pTexture          = NULL;
    m_pRefractionEffect = NULL;
    m_pWaterFogTex      = NULL;
    m_vWaterFogFarColor = D3DXVECTOR3( 47 / 255.0f, 74 / 255.0f, 66 / 255.0f );
}




//-----------------------------------------------------------------------------
// Name: ~CRefraction()
// Desc: Desructor
//-----------------------------------------------------------------------------
CRefraction::~CRefraction()
{
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Create the render target and its texture
//-----------------------------------------------------------------------------
HRESULT CRefraction::Initialize()
{
    HRESULT hr;
        
    // Create texture for render
    if( FAILED( hr = g_pd3dDevice->CreateTexture( RTEXTURE_WIDTH, RTEXTURE_HEIGHT, 1,
                                                  D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, 
                                                  D3DPOOL_DEFAULT, &m_pTexture ) ) )
        return hr;

    // Create render surface
    m_pTexture->GetSurfaceLevel( 0, &m_pSurface );

    // Create a depthbuffer
    if( FAILED( hr = g_pd3dDevice->CreateImageSurface( RTEXTURE_WIDTH, RTEXTURE_HEIGHT,
                                                       D3DFMT_LIN_D24S8, &m_pZBuffer ) ) )
        return hr;

    // Set up the viewport to use when rendering into this surface
    m_Viewport.X      = 0;
    m_Viewport.Y      = 0;
    m_Viewport.Width  = RTEXTURE_WIDTH;
    m_Viewport.Height = RTEXTURE_HEIGHT;
    m_Viewport.MinZ   = 0.0f;
    m_Viewport.MaxZ   = 1.0f;

    // Create fog texture
    static CXBPackedResource xpr;
    xpr.Create( c_strFogTextureFileName );
    m_pWaterFogTex = xpr.GetTexture( 0UL );

    if( g_pApp->m_pResMan ) 
    {
        // Get the effect
        m_pRefractionEffect = g_pApp->m_pResMan->GetEffectsFile( "refraction.fx" );
        if( m_pRefractionEffect )
        {
            // Set fog parameters into effect
            D3DXVECTOR4 WFgC( m_vWaterFogFarColor.x,
                              m_vWaterFogFarColor.y,
                              m_vWaterFogFarColor.z,
                              1 );
           m_pRefractionEffect->SetVector( FCC_WFGC, &WFgC );
           m_pRefractionEffect->SetTexture( FCC_FOGT, m_pWaterFogTex );
        }
        else 
        {
            return E_FAIL;
        }
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: FrameMoveAndUpdateTexture()
// Desc: Frame move and render the current scene into a texture
//-----------------------------------------------------------------------------
HRESULT CRefraction::FrameMoveAndUpdateTexture()
{
    if( m_pRefractionEffect )
    {
        D3DXVECTOR4 vFogScale( 1.0f / g_pApp->GetViewPosition().y / c_nFogTextureWidth,
                               1.0f / c_nFogTextureHeight,
                               1.0f,
                               1.0f );
        m_pRefractionEffect->SetVector( FCC_FOGS, &vFogScale );
    }

    RenderToTexture();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderToTexture()
// Desc: Render the current scene into the reflection texture.
//-----------------------------------------------------------------------------
HRESULT CRefraction::RenderToTexture()
{
    // Render only if camera is changed
    if ( !g_pApp->IsCameraChanged() )
        return S_OK;

    // Save state
    D3DSurface*  pRenderTargetOld;
    D3DSurface*  pZStencilOld;
    D3DVIEWPORT8 ViewportOld;
    g_pd3dDevice->GetRenderTarget( &pRenderTargetOld );
    g_pd3dDevice->GetDepthStencilSurface( &pZStencilOld );
    g_pd3dDevice->GetViewport( &ViewportOld );

    // Set new rendertarget
    g_pd3dDevice->SetRenderTarget( m_pSurface, m_pZBuffer );
    g_pd3dDevice->SetViewport( &m_Viewport );

    // Render the scene into the texture
    g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET,
                         D3DCOLOR_COLORVALUE( m_vWaterFogFarColor.x,
                                              m_vWaterFogFarColor.y,
                                              m_vWaterFogFarColor.z, 1.0f ), 
                         1.0f, NULL );
    
    g_pApp->m_pNonWater->Render( RF_BELOW_WATER );

    // Restore state
    g_pd3dDevice->SetRenderTarget( pRenderTargetOld, pZStencilOld );
    g_pd3dDevice->SetViewport( &ViewportOld );
    SAFE_RELEASE( pRenderTargetOld );
    SAFE_RELEASE( pZStencilOld );

    return S_OK;
}





//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Free Resourses
//-----------------------------------------------------------------------------
HRESULT CRefraction::Cleanup()
{
    SAFE_RELEASE( m_pWaterFogTex );
    SAFE_RELEASE( m_pZBuffer );
    SAFE_RELEASE( m_pSurface );
    SAFE_RELEASE( m_pTexture );

    return S_OK;
}


