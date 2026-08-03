//-----------------------------------------------------------------------------
// File: DebugMouse.cpp
//
// Desc: This sample demonstrates using the debug mouse.
//
// Hist: 04.07.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "mouse.h"
#include <xgraphics.h>
#include <stdio.h>
#include <XBApp.h>
#include <XBInput.h>
#include <XBFont.h>
#include <XBUtil.h>
#include <XBMesh.h>




//-----------------------------------------------------------------------------
#define MAX_BACKGROUND_COLORS 5
DWORD g_dwBackgroundColors[MAX_BACKGROUND_COLORS] = {
    0x00005000,
    0x00500000,
    0x00000050,
    0x00505050,
    0x00000000,
};

#define MAX_HELPFONT_COLORS 3
DWORD g_dwHelpFontColors[MAX_HELPFONT_COLORS] = {
    0xffffff00,
    0xffffffff,
    0xff00ff00,
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Fonts
    CXBFont            m_Font;
    CXBFont            m_ConsoleFont;

    // Plane model details
    CXBPackedResource  m_xprResource;
    CXBMesh            m_PlaneMesh;

    // Misc
    DWORD              m_dwBackgroundColorIndex;
    DWORD              m_dwHelpFontColorIndex;

    BOOL               m_bLeftExtraPressed;
    BOOL               m_bRightExtraPressed;

public:
    HRESULT Initialize();
    HRESULT Render();
    HRESULT FrameMove();

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
// Desc: Application constructor. Sets attributes for the app.
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize scene objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return E_FAIL;

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize the airplanes
    m_PlaneMesh.Create( "Models\\Airplane.xbg", &m_xprResource );

    // Initialize the debug mouse
    if( FAILED( XBInput_InitDebugMouse() ) )
        return E_FAIL;

    // Set the world matrix
    D3DXMATRIX matWorld;
    D3DXMatrixTranslation( &matWorld, 0.0f, 5.0f, 0.0f );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    // Set projection transform
    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 640.0f/480.0f, 0.1f, 100.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Set view position
    D3DXMATRIX matView;
    D3DXMatrixTranslation( &matView, 0.0f, 0.0f, 40.0f);
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    m_dwBackgroundColorIndex = 0;
    m_dwHelpFontColorIndex = 0;

    m_bLeftExtraPressed = m_bRightExtraPressed = FALSE;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene. As this code only changes text, there is no real animation
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Poll the mouse or mice.  Only use data from the LAST mouse that moved.
    DWORD dwMousePortsChanged = XBInput_GetMouseInput();
    XINPUT_STATE CurrentMouseState;
    ZeroMemory( &CurrentMouseState, sizeof(XINPUT_STATE) );

    for( DWORD i=0; i < XGetPortCount(); i++ )
    {
        if( dwMousePortsChanged & ( 1 << i ) ) 
            CurrentMouseState = g_MouseState[i];
    }

    if( dwMousePortsChanged )
    {
        FLOAT fRotX1 = 0.0f, fRotY1 = 0.0f;
        if( CurrentMouseState.DebugMouse.bButtons & 
            XINPUT_DEBUG_MOUSE_LEFT_BUTTON )
        {
            // Rotate the model
            fRotX1 = -CurrentMouseState.DebugMouse.cMickeysX / 64.0f;
            fRotY1 = -CurrentMouseState.DebugMouse.cMickeysY / 64.0f;
        }

        FLOAT fTransX1 = 0.0f, fTransY1 = 0.0f;
        if( CurrentMouseState.DebugMouse.bButtons & 
            XINPUT_DEBUG_MOUSE_RIGHT_BUTTON )
        {
            // Move the model
            fTransX1 = CurrentMouseState.DebugMouse.cMickeysX / 32.0f;
            fTransY1 = -CurrentMouseState.DebugMouse.cMickeysY / 32.0f;
        }

        FLOAT fZoom = 0.0f;
        if( CurrentMouseState.DebugMouse.cWheel )
        {
            // Zoom in and out
            fZoom = -CurrentMouseState.DebugMouse.cWheel * 4.0f;
        }

        if( CurrentMouseState.DebugMouse.bButtons &
            XINPUT_DEBUG_MOUSE_MIDDLE_BUTTON )
        {
            D3DXMATRIX matWorld;
            D3DXMatrixTranslation( &matWorld, 0.0f, 5.0f, 0.0f );
            m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );
        }

        if( CurrentMouseState.DebugMouse.bButtons &
            XINPUT_DEBUG_MOUSE_XBUTTON1 )
        {
            m_bLeftExtraPressed = TRUE;
        }

        if( m_bLeftExtraPressed &&
           !( CurrentMouseState.DebugMouse.bButtons &
            XINPUT_DEBUG_MOUSE_XBUTTON1 ) )
        {
            ++m_dwBackgroundColorIndex;
            if( m_dwBackgroundColorIndex >= MAX_BACKGROUND_COLORS )
                m_dwBackgroundColorIndex = 0;
            m_bLeftExtraPressed = FALSE;
        }

        if( CurrentMouseState.DebugMouse.bButtons &
            XINPUT_DEBUG_MOUSE_XBUTTON2 )
        {
            m_bRightExtraPressed = TRUE;
        }

        if( m_bRightExtraPressed &&
           !( CurrentMouseState.DebugMouse.bButtons &
            XINPUT_DEBUG_MOUSE_XBUTTON2 ) )
        {
            ++m_dwHelpFontColorIndex;
            if( m_dwHelpFontColorIndex >= MAX_HELPFONT_COLORS )
                m_dwHelpFontColorIndex = 0;
            m_bRightExtraPressed = FALSE;
        }

        // If you press all three buttons, reboot the Xbox.
        if( ( CurrentMouseState.DebugMouse.bButtons & 
              XINPUT_DEBUG_MOUSE_LEFT_BUTTON ) &&
            ( CurrentMouseState.DebugMouse.bButtons & 
              XINPUT_DEBUG_MOUSE_RIGHT_BUTTON ) &&
            ( CurrentMouseState.DebugMouse.bButtons &
              XINPUT_DEBUG_MOUSE_MIDDLE_BUTTON ) )
        {
            LD_LAUNCH_DASHBOARD LaunchData = { XLD_LAUNCH_DASHBOARD_MAIN_MENU };
            XLaunchNewImage( NULL, (LAUNCH_DATA*)&LaunchData );
        }

        // Move plane
        D3DXMATRIX matRot, matTrans, matWorld;
        D3DXMatrixRotationYawPitchRoll( &matRot, fRotX1, fRotY1, 0.0f );
        D3DXMatrixTranslation( &matTrans, fTransX1, fTransY1, fZoom );

        // Apply to world matrix.
        D3DXVECTOR3 vOldTranslation;
        D3DXMATRIX oldWorld;
        m_pd3dDevice->GetTransform( D3DTS_WORLD, &oldWorld );

        // Save off old translation.
        vOldTranslation.x = oldWorld._41;
        vOldTranslation.y = oldWorld._42;
        vOldTranslation.z = oldWorld._43;

        // Move plane back to origin of world.
        // And accumulate rotation
        oldWorld._41 = oldWorld._42 = oldWorld._43 = 0.0f;
        D3DXMatrixMultiply( &oldWorld, &oldWorld, &matRot );

        // Move plane back to old position in world.
        // and accumulate translation.
        oldWorld._41 = vOldTranslation.x;
        oldWorld._42 = vOldTranslation.y;
        oldWorld._43 = vOldTranslation.z;
        D3DXMatrixMultiply( &matWorld, &oldWorld, &matTrans );

        m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );
    }

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
    // Clear the scene
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         g_dwBackgroundColors[m_dwBackgroundColorIndex], 
                         1.0f, 0L );

    // Render the plane
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
    m_PlaneMesh.Render();

    // Display title and instructions
    {
        // Draw a rectangle to put the text in
        XBUtil_DrawRect( 60, 248, 570, 415, 0x40000000, 0xff000000 );

        // Draw the title
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"DebugMouse" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        
        // Draw the instructions
        m_Font.SetScaleFactors( 0.75f, 0.75f );
        m_Font.DrawText( 64, 248, g_dwHelpFontColors[m_dwHelpFontColorIndex], 
                         L"Press and hold the Left Mouse Button to rotate the Plane\n"
                         L"Press and hold the Right Mouse Button to translate the Plane\n"
                         L"Rotate the Mouse Wheel to zoom the Plane in and out\n"
                         L"Press the Mouse Wheel to reset the view\n" 
                         L"Press the Left Extra Button to Cycle the background color\n"
                         L"Press the Right Extra Button to Cycle the help font color\n" 
                         L"Press the Left, Right and Mouse Wheel Buttons together to Reboot" );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

