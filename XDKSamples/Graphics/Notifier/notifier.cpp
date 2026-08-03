//-----------------------------------------------------------------------------
// File: Notifier.cpp
//
// Desc: Illustrates how to use vblank routines and notifiers on the Xbox.
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
    { XBHELP_LEFTSTICK,   XBHELP_PLACEMENT_2, L"Move camera\nX, Z" },
    { XBHELP_RIGHTSTICK,  XBHELP_PLACEMENT_1, L"Move camera Y" },
    { XBHELP_BACK_BUTTON, XBHELP_PLACEMENT_2, L"Display\nhelp" },
    { XBHELP_A_BUTTON,    XBHELP_PLACEMENT_2, L"Toggle\nWaitVBlank" },
};
#define NUM_HELP_CALLOUTS 4




//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
DWORD g_dwVBCount = 0;    // VBlank count, incremented in the callback
DWORD g_dwPBCount = 0;    // Pushbuffer count, incremented in the callback

#define MAX_SPHERE 128    // # of spheres to draw




//-----------------------------------------------------------------------------
// Name: VBlankCallback
// Desc: This routine is called at the beginning of the vertical blank.
//-----------------------------------------------------------------------------
void __cdecl VBlankCallback( D3DVBLANKDATA *pData )
{
    g_dwVBCount++;
}




//-----------------------------------------------------------------------------
// Name: PushBufferCallback
// Desc: This routine is called when the pushbuffer hits the point where
//       the callback was added.
//-----------------------------------------------------------------------------
void __cdecl PushBufferCallback( DWORD Context )
{
    g_dwPBCount++;
}






//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Font, help, and resources
    CXBFont     m_Font;
    CXBHelp     m_Help;
    CXBPackedResource m_xprResource;

    // Options
    BOOL        m_bDrawHelp;
    BOOL        m_bWaitVBlank;

    // Camera position
    D3DXVECTOR3 m_vCameraPos;
    D3DXVECTOR3 m_vCameraRot;

    // A bunch of spheres to draw
    CXBMesh     m_Sphere;
    D3DXVECTOR3 m_vSpherePosition[MAX_SPHERE];
    D3DXVECTOR3 m_vSphereRotation[MAX_SPHERE];
    D3DXMATRIX  m_matSphere[MAX_SPHERE];

    // An airplane to draw
    CXBMesh     m_Airplane;
    D3DXVECTOR3 m_vAirplanePosition;
    D3DXVECTOR3 m_vAirplaneRotation;
    D3DXMATRIX  m_matAirplane;

    // Fence times
    FLOAT       m_fFenceStartTime;
    FLOAT       m_fFenceStopTime;

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

    // initialize our stuff
    m_bDrawHelp  = FALSE;
    m_bWaitVBlank = FALSE;

    m_vCameraPos = D3DXVECTOR3( 0.0f, 0.0f,-15.0f );
    m_vCameraRot = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
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

    // Load the packed resource
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Set projection transform
    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 640.0f/480.0f, 0.1f, 1000.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Set the view matrix based on the camera position
    D3DXMATRIX matView;
    D3DXMatrixTranslation( &matView, -m_vCameraPos.x, -m_vCameraPos.y, -m_vCameraPos.z );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    // Create the sphere objects
    m_Sphere.Create( "Models\\Sphere.xbg", &m_xprResource );
    D3DXMatrixScaling( &m_Sphere.GetFrame(0)->m_matTransform, 2.5f, 2.5f, 2.5f );

    for( DWORD i=0; i<MAX_SPHERE; i++ )
    {
        m_vSpherePosition[i] = D3DXVECTOR3( (float)rand()*100.0f/32768.0f-50.0f, 
                                                  (float)rand()*100.0f/32768.0f-50.0f, 
                                                  30.0f+(float)rand()*100.0f/32768.0f );
        m_vSphereRotation[i] = D3DXVECTOR3( 0.0f, (FLOAT)i, 0.0f );
    }

    // Load up the plane object
    m_Airplane.Create( "Models\\Airplane.xbg", &m_xprResource );
    m_vAirplanePosition = D3DXVECTOR3( 0.0f, 0.0f, 10.0f );
    m_vAirplaneRotation = D3DXVECTOR3( 0.0f, 0.0f,  0.0f );

    // Register our vblank callback
    g_pd3dDevice->SetVerticalBlankCallback( VBlankCallback );

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
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle vblank wait
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        m_bWaitVBlank = !m_bWaitVBlank;

    // Rotate the spheres...
    for( DWORD i=0; i<MAX_SPHERE; i++ )
    {
        m_vSphereRotation[i].y += m_fElapsedTime*1.0f;

        D3DXMATRIX mat, matTranslate, matRotate;
        D3DXMatrixRotationYawPitchRoll( &matRotate, m_vSphereRotation[i].y, m_vSphereRotation[i].x, m_vSphereRotation[i].z );
        D3DXMatrixTranslation( &matTranslate, m_vSpherePosition[i].x, m_vSpherePosition[i].y, m_vSpherePosition[i].z );
        D3DXMatrixMultiply( &m_matSphere[i], &matRotate, &matTranslate );
    }

    // Rotate the plane
    {
        m_vAirplaneRotation.y += m_fElapsedTime*0.5f;
        m_vAirplaneRotation.z += m_fElapsedTime;

        D3DXMATRIX mat, matTranslate, matRotate;
        D3DXMatrixRotationYawPitchRoll( &matRotate, m_vAirplaneRotation.y, m_vAirplaneRotation.x, m_vAirplaneRotation.z );
        D3DXMatrixTranslation( &matTranslate, m_vAirplanePosition.x, m_vAirplanePosition.y, m_vAirplanePosition.z );
        D3DXMatrixMultiply( &m_matAirplane, &matRotate, &matTranslate );
    }



    // Adjust the camera
    D3DXMATRIX matView;
    m_vCameraPos.x += m_DefaultGamepad.fX1*m_fElapsedTime*8.0f;
    m_vCameraPos.y += m_DefaultGamepad.fY2*m_fElapsedTime*8.0f;
    m_vCameraPos.z += m_DefaultGamepad.fY1*m_fElapsedTime*8.0f;

    D3DXMatrixTranslation( &matView, -m_vCameraPos.x, -m_vCameraPos.y, -m_vCameraPos.z);
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

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
    FLOAT fBaseTime = XBUtil_Timer(TIMER_GETABSOLUTETIME);

    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 
                         0xff400000, 1.0f, 0L );

    // Restore state that text clobbers
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );

    // Render some spheres
    for( DWORD i=0; i<MAX_SPHERE/4; i++ )
    {
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matSphere[i] );
        m_Sphere.Render();
    }

    // Do a callback at this point
    m_pd3dDevice->InsertCallback( D3DCALLBACK_READ, (D3DCALLBACK)PushBufferCallback, 0 );

    // Render some more spheres
    for( i=MAX_SPHERE/4; i<MAX_SPHERE; i++ )
    {
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matSphere[i] );
        m_Sphere.Render();
    }

    // Insert a fence here
    m_fFenceStartTime = XBUtil_Timer(TIMER_GETABSOLUTETIME) - fBaseTime;
    DWORD dwFence0 = m_pd3dDevice->InsertFence();

    // Render the plane
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matAirplane );
    m_Airplane.Render();

    // Wait for the fence
    m_pd3dDevice->BlockOnFence(dwFence0);
    m_fFenceStopTime = XBUtil_Timer(TIMER_GETABSOLUTETIME) - fBaseTime;

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        WCHAR str[80];
    
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"Notifier" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        swprintf( str, L"%d", g_dwVBCount );
        m_Font.DrawText( 64, 70, 0xffffffff, L"VBCount: " );
        m_Font.DrawText( 0xffffff00, str );
        swprintf( str, L"%d", g_dwPBCount );
        m_Font.DrawText( 64, 95, 0xffffffff, L"PBCount: " );
        m_Font.DrawText( 0xffffff00, str );

        swprintf( str, L"%5.4f", m_fFenceStartTime );
        m_Font.DrawText( 64, 120, 0xffffffff, L"Fence Start: " );
        m_Font.DrawText( 0xffffff00, str );
        swprintf( str, L"%5.4f", m_fFenceStopTime );
        m_Font.DrawText( 0xffffffff, L", Stop: " );
        m_Font.DrawText( 0xffffff00, str );

        if( m_bWaitVBlank )
            m_Font.DrawText( 64, 145, 0xffffffff, L"WaitVBlank" );

        m_Font.End();
    }

    // If waiting for the vblank, then block until we get it
    if( m_bWaitVBlank )
        m_pd3dDevice->BlockUntilVerticalBlank();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




