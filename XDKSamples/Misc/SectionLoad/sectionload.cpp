//-----------------------------------------------------------------------------
// File: SectionLoad.cpp
//
// Desc: Illustrates loading and unloading of sections.
//
// Note: Section loading and unloading is something that will be used
//       primarily in large projects where memory restrictions prevent you
//       from having all code and data loaded simultaneously. Furthermore,
//       flagging a section as NOPRELOAD only works if that section cannot
//       be squeezed into a preloaded section. Thus, if you try to write
//       a couple of small routines that are to be loaded and unloaded, they
//       will be squeezed in at the end of a preloaded section and the code
//       would always execute (as they would always be in memory).
//
//       As a result, this sample shows that section loading and unloading
//       work properly by doing the following: If the data section is 
//       loaded, write access to the data area will work just fine and
//       the sample shows the time of the last successful access.
//       If the data section is not loaded, accessing the data area will 
//       generate an exception.
//
// Hist: 09.20.02 - Code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>




//-----------------------------------------------------------------------------
// Help screen definitions
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] =
{
    { XBHELP_BACK_BUTTON, XBHELP_PLACEMENT_2, L"Display\nhelp" },
    { XBHELP_A_BUTTON,    XBHELP_PLACEMENT_2, L"Toggle\nDataSeg\nloaded" },
    { XBHELP_X_BUTTON,    XBHELP_PLACEMENT_2, L"Attempt DataSeg\naccess" },
};
#define NUM_HELP_CALLOUTS 3




//-----------------------------------------------------------------------------
// Put some data in another section
// Note that uninitialized sections will always be flagged as preload
//-----------------------------------------------------------------------------
#define BUFSIZE (32*1024)
#pragma data_seg("dataseg1")
BYTE g_Seg1Data0[BUFSIZE] = {0};
BYTE g_Seg1Data1[BUFSIZE] = {0};
BYTE g_Seg1Data2[BUFSIZE] = {0};
#pragma data_seg()




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont            m_Font;
    CXBHelp            m_Help;
    BOOL               m_bDrawHelp;

    CXBPackedResource  m_xprResource;
    CXBMesh*           m_pPlaneMesh;

    BOOL               m_bDataSegLoaded;
    FLOAT              m_fDataSegAccessTime;
    DWORD              m_dwDataSegChecksum;

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
    // Initialize our stuff
    m_bDrawHelp          = FALSE;

    m_bDataSegLoaded     = FALSE;
    m_fDataSegAccessTime = 0.0f;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    D3DXMATRIX matProj, matView;

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize the help system
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize the airplane mesh
    m_pPlaneMesh = new CXBMesh;
    m_pPlaneMesh->Create( "Models\\Airplane.xbg", &m_xprResource );

    // Set projection transform
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 640.0f/480.0f, 0.1f, 100.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Set view position
    D3DXMatrixTranslation( &matView, 0.0f, 0.0f, 60.0f);
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

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

    // Toggle dataseg load
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        // If not loaded, do the load
        if( !m_bDataSegLoaded )
        {
            VOID* pAddress = XLoadSection( "dataseg1" );
            if( NULL != pAddress )
                m_bDataSegLoaded = TRUE;
            m_fDataSegAccessTime = 0.0f;
        }
        else // Otherwise, free it up
        {
            XFreeSection( "dataseg1" );
            m_bDataSegLoaded     = FALSE;
            m_fDataSegAccessTime = 0.0f;
        }
    }

    // Attempt dataseg read
    if( m_bDataSegLoaded )
    {
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        {
            m_dwDataSegChecksum = 0;
            // Touch each byte of the loaded data segments.
            // Note: THIS WILL CRASH if the data segment is not loaded.
            for( DWORD i=0; i<BUFSIZE; i++ )
            {
                BYTE d0 = g_Seg1Data0[i];
                BYTE d1 = g_Seg1Data1[i];
                BYTE d2 = g_Seg1Data2[i];

                m_dwDataSegChecksum += d0 + d1 + d2;
            }

            m_fDataSegAccessTime = m_fTime;
        }
    }

    // Move airplane
    D3DXMATRIX matWorld, matTrans, matRotate;
    static FLOAT fYRot = 0.0f;
    fYRot += 1.57f*m_fElapsedTime;
    D3DXMatrixTranslation( &matTrans, 20.0f * cosf(fYRot), 0.0f, -20.0f * sinf(fYRot) );
    D3DXMatrixRotationY( &matRotate, fYRot );
    D3DXMatrixMultiply( &matWorld, &matRotate, &matTrans );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

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
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 
                         0x00000040, 1.0f, 0L );

    // Set state
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

    m_pPlaneMesh->Render( 0 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.DrawText(  64, 50, 0xffffffff, L"SectionLoad" );
        m_Font.DrawText( 450, 50, 0xffffff00, m_strFrameRate );
        m_Font.DrawText(  64, 75, 0xffffff00, L"Press A to toggle DataSeg loaded" );

        if( m_bDataSegLoaded )
        {
            m_Font.DrawText( 64, 100, 0xff00ff00, L"DataSeg Loaded" );
            m_Font.DrawText( 64, 125, 0xffffff00, L"Press X to access DataSeg" );
        }
        else
        {
            m_Font.DrawText( 64, 100, 0xffff0000, L"DataSeg NOT Loaded" );
        }

        if( m_fDataSegAccessTime != 0.0f )
        {
            WCHAR str[80];
            swprintf( str, L"Last DataSeg access time: %3.2f ms", m_fDataSegAccessTime );
            m_Font.DrawText( 64, 150, 0xff00ff00, str );
            swprintf( str, L"Current time: %3.2f ms", m_fTime );
            m_Font.DrawText( 64, 175, 0xff00ff00, str );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

