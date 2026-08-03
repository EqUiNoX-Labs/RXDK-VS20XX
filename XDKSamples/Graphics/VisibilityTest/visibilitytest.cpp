//-----------------------------------------------------------------------------
// File: VisibilityTest.cpp
//
// Desc: Illustrates how to do visibility testing on the XBox.
//
//       This sample draws two objects. One is possibly occluded by the other.
//       After starting the vis test on the back object, we wait for the
//       test to complete before either rendering or not rendering the object.
//       In a real game situation, you would do several different things...
//       First, you would intertwine vis tests with actual rendering so the
//       vis test would have time to complete before you check the results.
//       Second when you finally got around to checking the results, if a test
//       had not yet concluded, you would either draw additional geometry, or
//       if no additional geometry was able to be drawn, proceed as if the 
//       object was visible. The point is to not sit around waiting for tests 
//       to conclude. The graphics pipe needs to be busy 100% of the time.
//
//       This sample uses a lo-res version of the object to do the vis test,
//       and only draws the hi-res object is the vis test suggests we should.
//       Another suggestion is to render the real object and check vis test
//       results the following frame. Then use the logic that if an object was
//       not visible in frame n, it probably won't be visible in frame n+1.
//       
// Hist: 11.08.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBResource.h>




//-----------------------------------------------------------------------------
// Help screen definitions
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] =
{
    { XBHELP_LEFTSTICK,   XBHELP_PLACEMENT_2, L"Move wall\nin X, Y" },
    { XBHELP_RIGHTSTICK,  XBHELP_PLACEMENT_2, L"Move wall\nin Z" },
    { XBHELP_BACK_BUTTON, XBHELP_PLACEMENT_1, L"Display help" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts)/sizeof( g_HelpCallouts[0]);




// Geometry for drawing a wall
static FLOAT g_vWallVertices[4][5] =
{
    // Pos.x  Pos.y  Pos.z   tex.u  tex.v
    { -1.0f, -1.0f,  0.0f,   0.0f,  0.0f },
    {  1.0f, -1.0f,  0.0f,   1.0f,  0.0f },
    {  1.0f,  1.0f,  0.0f,   1.0f,  1.0f },
    { -1.0f,  1.0f,  0.0f,   0.0f,  1.0f }
};


    
    
//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont           m_Font;
    CXBHelp           m_Help;
    BOOL              m_bDrawHelp;

    CXBPackedResource m_xprResource;   // Packed resource with our textures

    CXBMesh           m_Sphere;        // An object to do the vis test on
    CXBMesh           m_LoResSphere;   // Lo-res version
    D3DXMATRIX        m_matSphere;

    D3DTexture*       m_pWallTexture;  // A wall to block our object
    D3DXMATRIX        m_matWall;

public:
    HRESULT Initialize();
    HRESULT FrameMove();
    HRESULT Render();

    CXBoxSample();
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
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    // Allow unlimited frame rate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    m_bDrawHelp  = FALSE;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;

    // Create a font
    if( FAILED( hr = m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize the help system
    if( FAILED( hr = m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    m_pWallTexture = m_xprResource.GetTexture( "WallTexture.bmp" );

    // Create the sphere object and it's low res version
    if( FAILED( m_Sphere.Create( "Models\\Sphere.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;
    if( FAILED( m_LoResSphere.Create( "Models\\LoSphere.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Set the view transform
    const D3DXVECTOR3 vEyePos( 0.0f, 0.0f, -10.0f );
    const D3DXVECTOR3 vLookAt( 0.0f, 0.0f,   0.0f );
    const D3DXVECTOR3 vUp    ( 0.0f, 1.0f,   0.0f );
    D3DXMATRIX matView;
    D3DXMatrixLookAtLH( &matView, &vEyePos, &vLookAt, &vUp );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    // Set projection transform
    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 640.0f/480.0f, 0.1f, 1000.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons&XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Move the wall around
    static D3DXVECTOR3 m_vPosition(0,0,-5);
    m_vPosition.x += m_DefaultGamepad.fX1*m_fElapsedTime*2.0f;
    m_vPosition.y += m_DefaultGamepad.fY1*m_fElapsedTime*2.0f;
    m_vPosition.z += m_DefaultGamepad.fY2*m_fElapsedTime*2.0f;
    D3DXMatrixTranslation( &m_matWall, m_vPosition.x, m_vPosition.y, m_vPosition.z );

    // Rotate the sphere
    D3DXMatrixRotationY( &m_matSphere, m_fTime*2.0f );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, 
                         0xff400000, 1.0f, 0L );

    // Restore state that text clobbers
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    // Render the wall
    m_pd3dDevice->SetTexture( 0, m_pWallTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xff8080ff );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWall );
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZ|D3DFVF_TEX1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, g_vWallVertices, 
                                   sizeof(g_vWallVertices[0]) );

    // Do the visibility test on the sphere.
    //
    // Note 1: We would normally send simple test geometry for the vis test
    //         and not the full object.
    // Note 2: We could send multiple objects between the
    //         BeginVisibilityTest() EndVisibilityTest() pair. The call to 
    //         GetVisibilityTestResult() would then return the total number of
    //         pixels drawn by all objects.

    // We don't want color or z buffer updated, so we disable them.
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0x00000000 );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,     FALSE );

    // Begin the vis text
    m_pd3dDevice->BeginVisibilityTest();

    // Render the object. Ideally, this should be a lo-res version
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matSphere );
    m_LoResSphere.Render();
    
    // End the vis test
    m_pd3dDevice->EndVisibilityTest( 0 );

    // Re-enable color and z writes
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,     TRUE );

    // Check the number of pixels that would have been drawn by the visibility
    // test geometry. Note that the vis test will take some time to complete,
    // especially since the GPU and CPU are not synchronized. Ideally, a game
    // would do other stuff here (AI, physics, render other objects, etc.) to
    // allow some time for the vis test to complete. This sample lacks any other
    // work to do, so we are just going to go into a loop. Please do not do this
    // in a game, as it's a blatant waste of CPU time. 
    UINT    dwNumPixelsDrawn;
    HRESULT hr;
    do
    {
        // Loop until we get valid vis test results
        hr = m_pd3dDevice->GetVisibilityTestResult( 0, &dwNumPixelsDrawn, NULL );
    }
    while( hr==D3DERR_TESTINCOMPLETE );

    // If pixels would have been drawn then our object is visible. So, draw the
    // hires version of the object now.
    if( dwNumPixelsDrawn > 0 )
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matSphere );

        // Render the hi-res object.
        m_Sphere.Render();
    }

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"VisibilityTest" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        // Report if sphere was rendered, and if so, show number of pixels
        if( dwNumPixelsDrawn == 0 )
            m_Font.DrawText( 320, 405, 0xffff0000, L"Sphere not rendered.", XBFONT_CENTER_X );
        else
        {
            WCHAR str[80];
            swprintf( str, L" %d", dwNumPixelsDrawn );
            m_Font.DrawText( 320, 380, 0xff00ff00, L"Sphere rendered.", XBFONT_CENTER_X );
            m_Font.DrawText( 320, 405, 0xff00ff00, L"# Pixels:", XBFONT_RIGHT );
            m_Font.DrawText( 320, 405, 0xff00ff00, str, XBFONT_LEFT );
        }
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}



