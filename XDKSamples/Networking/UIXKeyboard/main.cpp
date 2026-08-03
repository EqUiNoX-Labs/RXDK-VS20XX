//-----------------------------------------------------------------------------
// File: main.cpp
//
// Desc: Virtual keyboard reference UI
//
// Note: This sample is intended to show appropriate functionality only.
//       Please do not lift the graphics for use in your game. A description
//       of the user research that went into the creation of this sample is
//       located in the XDK documentation at Developing for Xbox - Reference
//       User Interface.
//
// Hist: 02.13.01 - New for March XDK release 
//       03.07.01 - Localized for April XDK release
//       04.10.01 - Updated for May XDK with full translations
//       06.06.01 - Japanese keyboard added
//       07.22.02 - Japanese keyboard (keyboard) added
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xact.h>
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBResource.h>
#include <XBOnline.h>
#include <XBSound.h>
#include "UIXKeyboardFeature.h"
#include "UIXCustomUIPlugin.h"




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application object for load and save game reference UI
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    enum AppState
    {
        STATE_MENU,         // Main menu
        STATE_KEYBOARD,     // Keyboard display
    };

    AppState           m_State;            // Current state of the app

    CXBFont            m_Font16;           // A 16-pt font for the app
    CXBFont            m_Font18;           // A 16-pt font for the keyboard

    ILiveEngine*       m_pUIXEngine;       // The main UIX engine interface
    DWORD              m_UIXWorkFlags;     // Work flags returned from UIX

    UIX_KEYBOARD_DATA  m_UIXKeyboardData;  // Used to communicate with the UIX keyboard

public:
    HRESULT Initialize();
    HRESULT FrameMove();
    HRESULT Render();
};




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CreateSoundSystem()
// Desc: Uses Xact to load some sounds from a wave bank and sound bank
//-----------------------------------------------------------------------------
PXACTENGINE      g_pXactEngine;
PXACTWAVEBANK    g_pWaveBank;
PVOID            g_pWaveBankMemory;
PXACTSOUNDBANK   g_pSoundBank;
PVOID            g_pSoundBankMemory;

HRESULT CreateSoundSystem( CHAR* strSoundBankFile, CHAR* strWaveBankFile )
{
    HRESULT hr;
    DWORD dwWaveBankSize;
    DWORD dwSoundBankSize;

    // Read the wave bank and the sound bank media files into memory
    if( FAILED( XBUtil_LoadFile( strWaveBankFile, &g_pWaveBankMemory, &dwWaveBankSize ) ) )
        return E_FAIL;

    if( FAILED( XBUtil_LoadFile( strSoundBankFile, &g_pSoundBankMemory, &dwSoundBankSize ) ) )
        return E_FAIL;

    // Create the Xact engine
    XACT_RUNTIME_PARAMETERS XactParams;
    ZeroMemory( &XactParams, sizeof(XactParams) );
    XactParams.dwMaxConcurrentStreams = 16;
    XactParams.dwMax2DHwVoices        = 40;
    XactParams.dwMax3DHwVoices        = 10;

    hr = XACTEngineCreate( &XactParams, &g_pXactEngine );
    if( FAILED(hr) ) 
        return E_FAIL;

    // Register the wave bank
    hr = g_pXactEngine->RegisterWaveBank( g_pWaveBankMemory, dwWaveBankSize, &g_pWaveBank );
    if( FAILED(hr) ) 
    {
        g_pXactEngine->Release();
        return E_FAIL;
    }

    // Register the sound bank
    hr = g_pXactEngine->CreateSoundBank( g_pSoundBankMemory, dwSoundBankSize, &g_pSoundBank );
    if( FAILED(hr) ) 
    {
        g_pXactEngine->Release();
        return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Sets up the virtual keyboard example
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Start the app in the menu state
    m_State = STATE_MENU;

    // Arial 18, bold, plain for keyboard
    if( FAILED( m_Font18.Create( "Font18.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Arial 16, bold, oulined for normal text
    if( FAILED( m_Font16.Create( "Font16.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load app sounds using Xact
    if( FAILED( CreateSoundSystem( "d:\\Media\\UIXKeyboard.xsb", 
                                   "d:\\Media\\UIXKeyboard.xwb" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize UIX stuff
    {
        HRESULT hr;

        // Init the online library
        hr = XOnlineStartup( NULL );
        if( FAILED(hr) ) 
            return hr;

        // Create a UI plugin object
        CUIPlugin* pUIPlugin = new CUIPlugin();
        hr = pUIPlugin->Initialize( &m_Font16 );
        if( FAILED(hr) ) 
            return hr;

        // Create an audio plugin object
        ITitleAudioPlugin* pAudioPlugin;
        hr = UIXCreateAudioPlugin( g_pXactEngine, g_pSoundBank, &pAudioPlugin );
        if( FAILED(hr) ) 
            return hr;

        // Create the live engine
        hr = UIXCreateLiveEngine( "d:\\Media\\UIXKeyboard.uix", XGetLanguage(), &m_pUIXEngine );
        if( FAILED(hr) ) 
            return hr;

        // Setup the plugins and enable the desired features
        m_pUIXEngine->SetUIPlugin( pUIPlugin );
        m_pUIXEngine->SetAudioPlugin( pAudioPlugin );
        m_pUIXEngine->EnableFeature( UIX_KEYBOARD_FEATURE );
    }

    // Create an ITitleFontRenderer wrapper around a CXBFont
    static UIXFontWrapper KeyboardFont( &m_Font18 );

    // Initialize the data structure for communicating with the UIX Keyboard
    m_UIXKeyboardData.pKeyboardFont     = &KeyboardFont;
    m_UIXKeyboardData.strBuffer         = new WCHAR[64];
    m_UIXKeyboardData.strBuffer[0]      = L'\0';
    m_UIXKeyboardData.dwBufferSize      = 64;
    m_UIXKeyboardData.dwCursorPos       = 0;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame; the entry point for animating the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    if( m_State == STATE_MENU )
    {
        switch( m_DefaultGamepad.Event )
        {
            case XBGAMEPAD_START:
            case XBGAMEPAD_A:
            {
                // For this sample, accept input from all controllers. A game
                // would probably limit input to a specific port here.
                m_UIXKeyboardData.dwLockInputToPort = UIX_INVALID_VALUE;

                // Start the UIX keyboard feature
                m_pUIXEngine->StartFeature( UIX_KEYBOARD_FEATURE, &m_UIXKeyboardData );
                m_State = STATE_KEYBOARD;
                break;
            }
        }
    }

    else // m_State == STATE_KEYBOARD
    {
        // Provide input to UIX
        for( DWORD i = 0; i < XGetPortCount(); i++ ) 
        {
            m_pUIXEngine->SetInput( i, g_Gamepads[i].hDevice ? &g_InputStates[i] : NULL );
        }

        // Let the UIX engine do work
        m_pUIXEngine->DoWork( &m_UIXWorkFlags );

        // Check if the UIX feature exitted
        if( m_UIXWorkFlags & UIX_DOWORK_FEATURE_EXIT )
        {
            // We have little that needs to be done here, because our keyboard
            // kept any entered text in our m_UIXKeyboardData structure.
            // Alternatively, we could have authored the feature to return this
            // information in a UIX_EXIT_INFO structure.

            // Switch back the the menu
            m_State = STATE_MENU;
        }
    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the viewport, zbuffer, and stencil buffer
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0xff1010c0, 1.0f, 0L );

    switch( m_State )
    {
        case STATE_MENU:        
        {
            m_Font16.SetScaleFactors( 1.2f, 1.2f );
            m_Font16.DrawText( 48.0f, 36.0f, 0xffeaeaea, L"UIXKeyboard" );
            m_Font16.SetScaleFactors( 1.0f, 1.0f );

            m_Font16.DrawText( 320.0f, 220.0f, 0xffffffff, 
                               L"Press " GLYPH_A_BUTTON L" to start keyboard", 
                               XBFONT_CENTER_X );
            break;
        }

        case STATE_KEYBOARD:    
            // Render UIX stuff
            if( m_UIXWorkFlags & UIX_DOWORK_NEED_TO_RENDER )
                m_pUIXEngine->Render( m_pBackBuffer );
            break;
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


