//-----------------------------------------------------------------------------
// File: HeatShimmer.cpp
//
// Desc: Implementation of HeatShimmer sample
//
// Hist: 09.20.02 - Created for October 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBResource.h>
#include <xgraphics.h>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON, XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_LEFTSTICK,   XBHELP_PLACEMENT_2, L"Move in-out\nand left-right" },
    { XBHELP_RIGHTSTICK,  XBHELP_PLACEMENT_2, L"Rotate and\ntilt view" },
    { XBHELP_DPAD,        XBHELP_PLACEMENT_2, L"Switch\ndistortion texture" },
    { XBHELP_Y_BUTTON,    XBHELP_PLACEMENT_2, L"Increase\ndistortion" },
    { XBHELP_X_BUTTON,    XBHELP_PLACEMENT_2, L"Decrease\ndistortion" },
    { XBHELP_B_BUTTON,    XBHELP_PLACEMENT_2, L"Increase\nscroll speed" },
    { XBHELP_A_BUTTON,    XBHELP_PLACEMENT_2, L"Decrease\nscroll speed" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont           m_Font;         // Font object
    CXBHelp           m_Help;         // Help object
    BOOL              m_bDrawHelp;    // TRUE to draw help screen
    CXBPackedResource m_xprResources; // Packed resources
  
    VOID SetDefaultStates();         // Sets render defaults
    VOID RenderZDistortionTexture(); // Makes Z*distortion texture
    VOID RenderHeatShimmer();        // Renders shimmer
    
    // Textures
    D3DTexture* m_pDepthBufferTexture;         // Depth buffer texture as RGBA
    D3DTexture* m_pBackBufferTexture;          // Back buffer texture
    D3DSurface* m_pOffscreenBackBuffer;        // Second back buffer surface
    D3DTexture* m_pOffscreenBackBufferTexture; // Second back buffer texture
    D3DSurface* m_pZDistortionBuffer;          // Z*distortion buffer
    D3DTexture* m_pZDistortionBufferTexture;   // Z*distortion buffer texture
    D3DTexture* m_pOffsetTextures[3];          // Offset maps

    // Matrices
    XGMATRIX    m_matWorld;
    XGMATRIX    m_matView;
    XGMATRIX    m_matProj;
    XGMATRIX    m_matSkyBox;      

    // Camera parameters
    FLOAT       m_fYaw;
    FLOAT       m_fPitch;
    FLOAT       m_fZoom;
    XGVECTOR3   m_vPosition;
    XGVECTOR3   m_vForward;

    // Meshes
    CXBMesh     m_SkyBoxObject;         // The skybox geometry
    CXBMesh     m_TerrainObject;        // The terrain geometry

    // Distortion window parameters
    INT         m_iOffsetTextureIndex;  // Current distortion texture index
    FLOAT       m_fScrollX;             // Distortion scroll direction, X
    FLOAT       m_fScrollY;             // Distortion scroll direction, Y
    FLOAT       m_fDistort;             // Distortion amount
    FLOAT       m_fScrollSpeed;         // Current scroll speed (dpixels/dt)
    FLOAT       m_fScreenWidth;         // Screen width
    FLOAT       m_fScreenHeight;        // Screen height
    FLOAT       m_fHeights[4];          // Screen space heights of distortion quads
    FLOAT       m_fDistortionScales[4]; // Distortion scale at quad intersections

    // Pixel shaders
    DWORD       m_dwShimmerPixelShader;
    DWORD       m_dwZDistortPixelShader;

public:
    CXBoxSample();

    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();
};




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
    m_bDrawHelp = FALSE;

    // Set presentation interval to be immediate for better frame rate testing
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // Set back buffer count to 1
    m_d3dpp.BackBufferCount = 1;

    m_fScreenWidth  = (FLOAT)m_d3dpp.BackBufferWidth;
    m_fScreenHeight = (FLOAT)m_d3dpp.BackBufferHeight;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load resources from the packed resource file
    if( FAILED( m_xprResources.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load the geometry models
    if( FAILED( m_SkyBoxObject.Create( "Models\\Sky.xbg", &m_xprResources ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    if( FAILED( m_TerrainObject.Create( "Models\\Desert_Stage.xbg", &m_xprResources ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create displacment map textures
    m_pOffsetTextures[0] = m_xprResources.GetTexture( "Shimmer0" );
    m_pOffsetTextures[1] = m_xprResources.GetTexture( "Shimmer1" );
    m_pOffsetTextures[2] = m_xprResources.GetTexture( "Shimmer2" );

    // Create Z*distortion texture texture with same size
    m_pd3dDevice->CreateTexture( 256, 256, 1, 0, D3DFMT_X8R8G8B8, 0, &m_pZDistortionBufferTexture );
    m_pZDistortionBufferTexture->GetSurfaceLevel( 0, &m_pZDistortionBuffer );

    // Create the pixel shaders
    if( FAILED ( XBUtil_CreatePixelShader( "Shaders\\Shimmer.xpu", &m_dwShimmerPixelShader ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    if( FAILED ( XBUtil_CreatePixelShader( "Shaders\\ZDistort.xpu", &m_dwZDistortPixelShader ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Set up back buffer texture
    D3DSURFACE_DESC desc;
    m_pBackBuffer->GetDesc( &desc );
    m_pBackBufferTexture = new D3DTexture;
    XGSetTextureHeader( desc.Width, desc.Height, 1, 0,
                        desc.Format, 0, m_pBackBufferTexture, m_pBackBuffer->Data,
                        desc.Width * XGBytesPerPixelFromFormat(desc.Format) );

    // Create a second back buffer texture to render into
    m_pd3dDevice->CreateTexture( desc.Width, desc.Height, 1, 0,
                                 desc.Format, 0, &m_pOffscreenBackBufferTexture );
    m_pOffscreenBackBufferTexture->GetSurfaceLevel( 0, &m_pOffscreenBackBuffer );
    
    // Set up depth buffer texture
    m_pDepthBuffer->GetDesc( &desc );
    m_pDepthBufferTexture = new D3DTexture;
    XGSetTextureHeader( desc.Width, desc.Height, 1, 0,
                        D3DFMT_LIN_B8G8R8A8 , 0, m_pDepthBufferTexture, m_pDepthBuffer->Data,
                        desc.Width * XGBytesPerPixelFromFormat(D3DFMT_LIN_B8G8R8A8) );
    
    // Initialize camera
    m_fYaw   =  5.35f;
    m_fPitch = -0.20f;
    m_fZoom  = 10.00f;
    m_vPosition = XGVECTOR3( -0.76f, 10.8f, -58.1f );
    XGMatrixIdentity( &m_matWorld );
    XGMatrixIdentity( &m_matView );
    XGMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/4, 640.0f/480.0f, 5.0f, 2000.0f );

    // Initialize distortion window parameters
    m_fScrollY            = 0.0f;
    m_fScrollX            = 0.0f;
    m_fDistort            = 5.0f;
    m_fScrollSpeed        = 15.0f;
    m_iOffsetTextureIndex = 0;
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
        m_bDrawHelp = !m_bDrawHelp;

    // Camera matrix
    XGMATRIX Camera;
    XGVECTOR3* pLeft     = (XGVECTOR3*)&Camera[4*0];
    XGVECTOR3* pForward  = (XGVECTOR3*)&Camera[4*2];
    XGVECTOR3* pPosition = (XGVECTOR3*)&Camera[4*3];

    // Rotation
    FLOAT fRotationScale = 1.0f * m_fElapsedTime;
    FLOAT fDeltaYaw = fRotationScale * m_DefaultGamepad.fX2;
    FLOAT fDeltaPitch = -fRotationScale * m_DefaultGamepad.fY2;
    m_fYaw += fDeltaYaw;
    m_fPitch += fDeltaPitch;
    XGMatrixRotationYawPitchRoll( &Camera, m_fYaw, m_fPitch, 0.0f );

    // Translation
    FLOAT fTranslationScale = 10.0f*m_fElapsedTime;
    D3DXVECTOR3 vLeft( pLeft->x, 0.0f, pLeft->z );
    D3DXVec3Normalize( &vLeft, &vLeft );
    D3DXVECTOR3 vForward( pForward->x, 0.0f, pForward->z );
    D3DXVec3Normalize( &vForward, &vForward );
    FLOAT fDeltaX = fTranslationScale * m_DefaultGamepad.fX1;
    m_vPosition += vLeft * fTranslationScale * m_DefaultGamepad.fX1;
    m_vPosition += vForward * fTranslationScale * m_DefaultGamepad.fY1;
    m_vPosition.y = 15.0f;
    *pPosition = m_vPosition;

    // Update view matrix
    FLOAT fDet;
    XGMatrixInverse( &m_matView, &fDet, &Camera );
    assert(fDet > 0.0f);

    // Set the skybox view transform (which retains the view orientation,
    // but not the translation)
    m_matSkyBox     = m_matView;
    m_matSkyBox._41 = 0.0f; 
    m_matSkyBox._42 = 0.0f; 
    m_matSkyBox._43 = 0.0f;

    // Distortion texture
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( ++m_iOffsetTextureIndex > 2 )
            m_iOffsetTextureIndex = 0;
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( --m_iOffsetTextureIndex < 0 )
            m_iOffsetTextureIndex = 2;
    }
    
    // Distortion scale
    const FLOAT fDistortionScale = 0.02f;
    if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_X] > 10 )
    {
        m_fDistort -= fDistortionScale; 
        if( m_fDistort < 0 )
            m_fDistort = 0;
    }
    if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_Y] > 10 )
    {
        m_fDistort += fDistortionScale; 
    }

    // Distortion scroll rate
    const FLOAT fScrollScale = 0.05f;
    if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_A] > 10 )
    {
        m_fScrollSpeed -= fScrollScale; 
        if( m_fScrollSpeed < 0 )
            m_fScrollSpeed = 0.0f;
    }
    if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_B] > 10 )
    {
        m_fScrollSpeed += fScrollScale; 
    }

    // Fudge scroll to compensate for moving camera
    const FLOAT fScrollAngleScaleX = 480.0f/640.0f;
    const FLOAT fScrollAngleScaleY = 640.0f/480.0f;
    const FLOAT fScrollTransScaleX = 1.0f/130.0f;
    m_fScrollY += fDeltaPitch * fScrollAngleScaleY + m_fElapsedAppTime * (m_fScrollSpeed/m_fScreenHeight);
    m_fScrollY -= floorf( m_fScrollY );
    m_fScrollX += fDeltaX * fScrollTransScaleX + fDeltaYaw * fScrollAngleScaleX;
    m_fScrollX -= floorf( m_fScrollX );
    FLOAT fViewAngle = pForward->y;

    // Update distortion scales
    FLOAT fDistortionScales[4] = {0.0f, 1.0f, 1.0f, 0.0f};
    for( DWORD i = 0;i < 4; i++ )
        m_fDistortionScales[i] = fDistortionScales[i] * m_fDistort;
    
    // Update distortion window quads
    FLOAT fHeights[4]          = {0.25f, 0.45f, 0.55f, 0.82f};
    FLOAT fHeightOffset = fViewAngle * m_fScreenWidth/m_fScreenHeight;
    for( DWORD i = 0; i < 4; i++ )
        m_fHeights[i] = fHeights[i] + fHeightOffset;
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetDefaultStates()
// Desc: Set render state defaults
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetDefaultStates()
{
    // Set world\view\projection matrices
    m_pd3dDevice->SetTransform( D3DTS_WORLD,      &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,       &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Texture
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTexture( 0, NULL );
    
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1 );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTexture( 1, NULL );

    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_TEXCOORDINDEX, 2 );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTexture( 2, NULL );
    
    // Depth
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESS );

    // Alpha test and blend
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Lighting
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_TWOSIDEDLIGHTING, FALSE );

    // Fog
    FLOAT FogStart =  10.0f;
    FLOAT FogEnd   = 200.0f;
    m_pd3dDevice->SetRenderState( D3DRS_FOGENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_FOGSTART, *((DWORD*)(&FogStart)) );
    m_pd3dDevice->SetRenderState( D3DRS_FOGEND, *((DWORD*)(&FogEnd)) );
    DWORD dwFogColor = XGCOLOR( 1.0f, 1.0f, 235.0f/255.0f, 0.0f );
    m_pd3dDevice->SetRenderState( D3DRS_FOGCOLOR, dwFogColor );

    // Vertex/pixel shaders
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetVertexShader( NULL );
}




//-----------------------------------------------------------------------------
// Name: RenderZDistortionTexture()
// Desc: Makes a texture with R=DistortionX*Z, G= DistortionY*Z
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderZDistortionTexture()
{   
    // Get distortion texture size
    D3DSURFACE_DESC desc;
    m_pZDistortionBufferTexture->GetLevelDesc( 0, &desc );
    FLOAT fTextureWidth  = (FLOAT)desc.Width;
    FLOAT fTextureHeight = (FLOAT)desc.Height;

    // Create quad
    struct
    {
        D3DXVECTOR4 Pos;
        D3DXVECTOR2 UV0;
        D3DXVECTOR2 UV1;
    } Verts[4];
    for( UINT i = 0; i < 4; i++ )
    {
        FLOAT Width  = (i%2) ? 1.0f : 0.0f;
        FLOAT Height = (i<2) ? m_fHeights[0] - 1.0f : m_fHeights[3] + 1.0f;

        Verts[i].Pos.x = Width  * fTextureWidth;
        Verts[i].Pos.y = Height * fTextureHeight;
        Verts[i].Pos.z = 0.0f;
        Verts[i].Pos.w = 1.0f;
        Verts[i].UV0.x = Width  + m_fScrollX;
        Verts[i].UV0.y = Height + m_fScrollY;
        Verts[i].UV1.x = Width  * m_fScreenWidth;
        Verts[i].UV1.y = Height * m_fScreenHeight;

        // Screen/texture space offset
        Verts[i].Pos.x -= 0.5f;
        Verts[i].Pos.y -= 0.5f;
    }

    // Textures
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTexture( 0, m_pOffsetTextures[m_iOffsetTextureIndex] );

    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetTexture( 1, m_pDepthBufferTexture );
    
    // Disable Z
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE , FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ); 

    // Pixel/vertex shaders
    m_pd3dDevice->SetPixelShader( m_dwZDistortPixelShader );
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_TEX2 );

    // Render quad
    m_pd3dDevice->DrawVerticesUP( D3DPT_QUADSTRIP, 4, Verts, sizeof(Verts[0]) );
}




//-----------------------------------------------------------------------------
// Name: RenderHeatShimmer()
// Desc: Render shimmered back buffer copy into back buffer using
//       Distortion*Z texture
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderHeatShimmer()
{
    // Copy bottom and top rects
    RECT Rect;
    Rect.left   = 0;
    Rect.top    = 0;
    Rect.right  = (LONG)m_fScreenWidth;
    Rect.bottom = (LONG)min( m_fHeights[0] + 1.0f, m_fScreenHeight );
    if( Rect.bottom > 0 )
    {
        m_pd3dDevice->CopyRects( m_pOffscreenBackBuffer, &Rect, 1, m_pBackBuffer, NULL );
    }

    Rect.top    = LONG( max( m_fHeights[3], 0.0f ) );
    Rect.bottom = LONG( m_fScreenHeight );
    if( Rect.top < LONG( m_fScreenHeight) )
    {
        m_pd3dDevice->CopyRects( m_pOffscreenBackBuffer, &Rect, 1, m_pBackBuffer, NULL );
    }

    // Create quads
    struct
    {
        D3DXVECTOR4 Pos;
        D3DXVECTOR2 UV0;
        D3DXVECTOR3 UV1;
        D3DXVECTOR3 UV2;
    } Verts[8];
    for( DWORD i = 0; i < 8; i++ )
    {   
        FLOAT fWidth  = i%2 ? 1.0f : 0.0f;
        FLOAT fHeight = m_fHeights[i/2];
        FLOAT fDistortionScale = m_fDistortionScales[i/2];
        
        Verts[i].Pos.x = m_fScreenWidth  * fWidth  - 0.5f;
        Verts[i].Pos.y = m_fScreenHeight * fHeight - 0.5f;
        Verts[i].Pos.z = 0.0f;
        Verts[i].Pos.w = 1.0f;

        // Tex coords for referencing the distortion texture
        Verts[i].UV0.x = fWidth;
        Verts[i].UV0.y = fHeight;

        // Tex coords for the 3x2 matrix transform for the backbuffer lookup
        Verts[i].UV1.x = m_fScreenWidth * fWidth;
        Verts[i].UV1.y = fDistortionScale;
        Verts[i].UV1.z = 0.0f;

        Verts[i].UV2.x = m_fScreenHeight * fHeight;
        Verts[i].UV2.y = 0.0f;
        Verts[i].UV2.z = fDistortionScale;
    }

    // Textures
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTexture( 0, m_pZDistortionBufferTexture );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetTexture( 2, m_pOffscreenBackBufferTexture );
    
    // Disable Z
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,      FALSE ); 
    
    // Set pixel/vertex shaders
    m_pd3dDevice->SetPixelShader( m_dwShimmerPixelShader );
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_TEX3 |
                                                   D3DFVF_TEXCOORDSIZE2(0) |
                                                   D3DFVF_TEXCOORDSIZE3(1) |
                                                   D3DFVF_TEXCOORDSIZE3(2) );

    // Draw vertices
    m_pd3dDevice->DrawVerticesUP( D3DPT_QUADSTRIP, 8, Verts, sizeof(Verts[0]) );

    // Restore state
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );
    m_pd3dDevice->SetTexture( 2, NULL );
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the color and depth buffer
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         0x00000000, 1.0f, 0L );

    // Set second back buffer texture as render targert
    m_pd3dDevice->SetRenderTarget( m_pOffscreenBackBuffer, m_pDepthBuffer );
   
    // Render skybox and terrain
    SetDefaultStates();

    // Render the skybox
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,  &m_matSkyBox );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    m_SkyBoxObject.Render();

    // Render Terrain
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,  TRUE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_CURRENT );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );
    m_TerrainObject.Render();

    // Set Z*distortion texture as render target, render Z*distortion
    m_pd3dDevice->SetRenderTarget( m_pZDistortionBuffer, NULL );

    SetDefaultStates();
    RenderZDistortionTexture();
   
    // Reset render target to back buffer, render distortion and
    // copy undistorted regions
    m_pd3dDevice->SetRenderTarget( m_pBackBuffer, m_pDepthBuffer );
    
    SetDefaultStates();
    RenderHeatShimmer();
            
    // Show title, distortion parameters, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"HeatShimmer" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        WCHAR str[200];
        swprintf( str, L"%.2f Pixels", m_fDistort );
        m_Font.DrawText(  64,  75, 0xffffffff, L"Distortion Max: " );
        m_Font.DrawText( 0xffffff00, str );
        swprintf( str, L"%.2f Pixels/Sec", m_fScrollSpeed );
        m_Font.DrawText(  64, 100, 0xffffffff, L"Scroll Speed: " );
        m_Font.DrawText( 0xffffff00, str );
        const WCHAR* strTextureNames[3] = 
        {
            L"Waves",
            L"Noise",
            L"Twirl"
        };
        swprintf( str, L"\"%s\"", strTextureNames[m_iOffsetTextureIndex] );
        m_Font.DrawText( 64, 125, 0xffffffff, L"Texture: " );
        m_Font.DrawText( 0xffffff00, str );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
