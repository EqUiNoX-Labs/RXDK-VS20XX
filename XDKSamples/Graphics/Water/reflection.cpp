//-----------------------------------------------------------------------------
// File: Reflection.cpp
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
#include "Reflection.h"
#include "NoneWater.h" 
#include "ResMan.h"
 



//-----------------------------------------------------------------------------
// Name: CReflection()
// Desc: Contructor
//-----------------------------------------------------------------------------
CReflection::CReflection()
{
    m_pTexture = NULL;
    m_pSurface = NULL;
    m_pZBuffer = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CReflection()
// Desc: Destructor
//-----------------------------------------------------------------------------
CReflection::~CReflection()
{
    Cleanup();
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Create the render target and its texture
//-----------------------------------------------------------------------------
HRESULT CReflection::Initialize()
{
    HRESULT hr;

    // Create texture for render
    if( FAILED( hr = g_pd3dDevice->CreateTexture( RTEXTURE_WIDTH, RTEXTURE_HEIGHT, 1,
                                                  D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, 
                                                  D3DPOOL_DEFAULT, &m_pTexture ) ) )
        return hr;

    // Get a surface for the texture
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

    return hr;
}




//-----------------------------------------------------------------------------
// Name: FrameMoveAndUpdateTexture()
// Desc: Frame move and render the current scene into a texture
//-----------------------------------------------------------------------------
HRESULT CReflection::FrameMoveAndUpdateTexture()
{
    // Get current view matrix
    D3DXMATRIX* matViewSaved = g_pApp->GetViewMatrix();
   
    // Reflect camera in X-Z plane mirror
    D3DXMATRIX matView, matReflect;
    D3DXPLANE plane;

    D3DXVECTOR3 vec0(0, 0, 0), vec1(0, 1, 0);
    D3DXPlaneFromPointNormal( &plane, &vec0, &vec1 );
    D3DXMatrixReflect( &matReflect, &plane );
    D3DXMatrixMultiply( &m_matView, &matReflect, matViewSaved );
    D3DXMatrixMultiply( &m_matViewProj, &m_matView, GetProjMatrix() );

    RenderToTexture();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderToTexture()
// Desc: Render the current scene into the reflection texture. 
//-----------------------------------------------------------------------------
HRESULT CReflection::RenderToTexture()
{
    HRESULT hr = S_OK;

    // Render only if camera is changed
    if( !g_pApp->IsCameraChanged() )
        return hr;

    // Render the nonewater part into the reflection texture
    D3DXVECTOR3 &vPos = g_pApp->GetViewPosition();
    D3DXVECTOR4 Light( vPos.x, vPos.y, vPos.z, 1 );

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
    g_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                         0x00000000, 1.0f, NULL );
    g_pApp->m_pNonWater->Render( RF_ABOVE_WATER );

    // Restore state
    g_pd3dDevice->SetRenderTarget( pRenderTargetOld, pZStencilOld );
    g_pd3dDevice->SetViewport( &ViewportOld );
    SAFE_RELEASE( pRenderTargetOld );
    SAFE_RELEASE( pZStencilOld );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Free Resources
//-----------------------------------------------------------------------------
HRESULT CReflection::Cleanup()
{
    SAFE_RELEASE( m_pTexture );
    SAFE_RELEASE( m_pSurface );
    SAFE_RELEASE( m_pZBuffer );

    return S_OK;
}


