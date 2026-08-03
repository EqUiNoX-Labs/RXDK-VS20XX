//-----------------------------------------------------------------------------
// File: DebugKeyboard.cpp
//
// Desc: This sample demonstrates using the debug keyboard.
//
// Hist: 06.11.01 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <stdio.h>
#include <XBApp.h>
#include <XBInput.h>
#include <XBFont.h>
#include <XBUtil.h>
#include <XBMesh.h>
#include "console.h"
#include "commands.h"
#include "keyboard.h"




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

    // Initialize the debug keyboard
    if( FAILED( XBInput_InitDebugKeyboard() ) )
        return E_FAIL;

    // Create a font for the console
    if( FAILED( m_ConsoleFont.Create( "SystemFont.xpr" ) ) )
        return E_FAIL;

    // Initialize the console
    InitConsole( &m_ConsoleFont );

    // Set projection transform
    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 640.0f/480.0f, 0.1f, 100.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Set view position
    D3DXMATRIX matView;
    D3DXMatrixTranslation( &matView, 0.0f, 0.0f, 40.0f);
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene. As this code only changes text, there is no real animation
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    if( FALSE == IsConsoleActive() )
    {
        if( XBInput_GetKeyboardInput() == ESC_KEY )
        {
            ToggleConsole();
        }
    }

    // Move plane
    FLOAT x =  12.0f * cosf(1.57f*m_fAppTime);
    FLOAT z = -12.0f * sinf(1.57f*m_fAppTime);
    FLOAT y =   4.0f;
    D3DXMATRIX matWorld, matTrans;
    D3DXMatrixRotationY( &matWorld, 1.57f*m_fAppTime );
    D3DXMatrixTranslation( &matTrans, x, y, z );
    D3DXMatrixMultiply( &matWorld, &matWorld, &matTrans );
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
    // Clear the scene
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         0x00005000, 1.0f, 0L );

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

    // Display the console
    if( IsConsoleActive() == TRUE )
    {
        ProcessConsole();
        DrawConsole();
    }

    // Display title and instructions
    {
        // Draw a rectangle to put the text in
        XBUtil_DrawRect( 60, 248, 564, 398, 0x40000000, 0xff000000 );

        // Draw the title
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"DebugKeyboard" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        
        // Draw the instructions
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        m_Font.DrawText(  64, 250, 0xffffff00, L"Press the ESC key on the Xbox debug keyboard\n"
                                               L"to open the console window. Console supports\n"
                                               L"help, clear, and reboot. Other text will just be\n"
                                               L"displayed in the console window." );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




