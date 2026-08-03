//-----------------------------------------------------------------------------
// File: XACTMarkers.cpp
//
// Desc: XACTMarkers is a sample that demonstrates how to use XACT markers
//       and notification features.
//
// Hist: 3.11.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include <xgraphics.h>
#include <xact.h>
#include "XactSounds.h"




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help"  },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Restart Play"  },
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )




//-----------------------------------------------------------------------------
// Global variable declarations
//-----------------------------------------------------------------------------
WCHAR* g_rgWordArray[] = 
{
    L"And ",
    L"the ",
    L"receiving ",
    L"team ",
    L"comes ",
    L"up ",
    L"with ",
    L"the ",
    L"football. ",
    L"That ",
    L"should ",
    L"do ",
    L"it, ",
    L"they ",
    L"didn't ",
    L"recover ",
    L"the ",
    L"on-side ",
    L"attempt. ",
};
const DWORD NUM_WORDS           = sizeof( g_rgWordArray ) / sizeof( g_rgWordArray[0] );
const DWORD NUM_WORDS_PER_LINE  = 7;
const FLOAT LINE_SPACING        = 50.0f;
const FLOAT WORD_SPACING        = 64.0f;




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont                         m_Font;                 // Font object
    CXBHelp                         m_Help;                 // Help object
    BOOL                            m_bDrawHelp;            // TRUE to draw help screen

    LPDIRECTSOUND8                  m_pDSound;              // DirectSound object
    IXACTEngine*                    m_pXACT;                // XACT Engine instance

    BYTE*                           m_pbWaveBank;           // Wave Bank data
    PXACTWAVEBANK                   m_pWaveBank;            // XACT Wave Bank

    BYTE*                           m_pbSoundBank;          // Sound Bank data
    IXACTSoundBank*                 m_pSoundBank;           // XACT Sound Bank
    PXACTSOUNDCUE                   m_pSoundCue;            // Handle to a playing cue

    XACT_NOTIFICATION_DESCRIPTION   m_xactNotificationDesc; // Marker notifications

    DWORD                           m_dwWordCount;          // Number of words to draw

public:
    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();

    // Utility sample methods
    HRESULT ResetState();
    VOID    DrawWords();

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
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
    m_bDrawHelp     = FALSE;
    m_pDSound       = NULL;
    m_pXACT         = NULL;
    m_pbWaveBank    = NULL;
    m_pWaveBank     = NULL;
    m_pbSoundBank   = NULL;
    m_pSoundBank    = NULL;
    m_pSoundCue     = NULL;
    m_dwWordCount   = 0;
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

    // Create the XACT runtime engine
    XACT_RUNTIME_PARAMETERS xrParams;
    xrParams.dwMax2DHwVoices        = 128;
    xrParams.dwMax3DHwVoices        = 0;
    xrParams.dwMaxConcurrentStreams = 16;
    xrParams.dwMaxNotifications     = 0;
    if( FAILED( XACTEngineCreate( &xrParams, &m_pXACT ) ) )
    {
        return E_FAIL;
    }

    // Load the in memory wave bank
    DWORD dwFileSize;
    if( FAILED( XBUtil_LoadFile( "D:\\media\\sounds\\XactSounds_memory.xwb", (VOID **)&m_pbWaveBank, &dwFileSize ) ) )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Register the in memory wave bank with XACT
    if( FAILED( m_pXACT->RegisterWaveBank( m_pbWaveBank, dwFileSize, &m_pWaveBank ) ) )
    {
        return E_FAIL;
    }

    // Load the sound bank
    if( FAILED( XBUtil_LoadFile( "D:\\media\\sounds\\XactSounds.xsb", (VOID **)&m_pbSoundBank, &dwFileSize ) ) )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Register the sound bank with XACT
    if( FAILED( m_pXACT->CreateSoundBank( m_pbSoundBank, dwFileSize, &m_pSoundBank ) ) )
    {
        return E_FAIL;
    }

    // Initialize XACT notification struct for MARKER notification
    m_xactNotificationDesc.wType              = eXACTNotification_Marker;
    m_xactNotificationDesc.wFlags             = XACT_FLAG_NOTIFICATION_PERSIST | XACT_FLAG_NOTIFICATION_USE_SOUNDCUE_INDEX;
    m_xactNotificationDesc.u.pSoundBank       = m_pSoundBank;
    m_xactNotificationDesc.dwSoundCueIndex    = XACT_SOUNDBANK_MARKERS_XACT_MARKER;
    m_xactNotificationDesc.hEvent             = NULL;

    // Register a MARKER notification with the XACT engine.
    // This will allow us to monitor when markers are hit during cue.
    if( FAILED( m_pXACT->RegisterNotification( &m_xactNotificationDesc ) ) )
        return E_FAIL;

    // Un-set the persist flag, for use later when checking
    // for marker notification fire.
    m_xactNotificationDesc.wFlags &= ~( XACT_FLAG_NOTIFICATION_PERSIST );

    // Reset state for drawing word array
    ResetState();

    // Initialize DSound for our audio level meters
    DirectSoundCreate( NULL, &m_pDSound, NULL );

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
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Reset state for drawing word array
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        ResetState();
    }

    // Check for MARKER notification
    XACT_NOTIFICATION xactNotification_MARKER = {0};
    if( m_pXACT->GetNotification( &m_xactNotificationDesc, &xactNotification_MARKER ) == S_OK )
    {
        // Word marker fired, increment for next word
        m_dwWordCount++;
    }

    // Pump XACT's work queue
    XACTEngineDoWork();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background and clear the zbuffer
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48,  36, 0xFFFFFFFF, L"XACTMarkers" );
        m_Font.DrawText( 450, 50, 0xFFFFFFFF, m_strFrameRate );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.End();

        // Draw words from word array according to the number of 
        // markers which have fired thus far.    
        DrawWords();

        // Update the on-screen audio level meters
        XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample::ResetState()
// Desc: Reset the state for the drawing word array
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::ResetState()
{
    // Reset the word count
    m_dwWordCount = 0;

    // Play the sound cue. This cue is on a layer, so restarting
    // the play will stop the old before attempting to play the new.
    if( FAILED( m_pSoundBank->Play( XACT_SOUNDBANK_MARKERS_XACT_MARKER, NULL, XACT_FLAG_SOUNDCUE_AUTORELEASE, NULL ) ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample::DrawWords()
// Desc: Draw words from word array according to the number of markers which 
//       have fired thus far.
//-----------------------------------------------------------------------------
VOID CXBoxSample::DrawWords()
{
    FLOAT fLineSpace = LINE_SPACING;

    // Loop through the word array and display as many words
    // as we have recieved markers for. 
    for ( DWORD dwIndex = 0; dwIndex < m_dwWordCount; dwIndex++ )
    {
        // Adjust to next line
        if ( !( dwIndex % NUM_WORDS_PER_LINE ) )
        {
            // Draw an empty string to reset to beginning of new line
            fLineSpace += LINE_SPACING;
            m_Font.DrawText( WORD_SPACING,  fLineSpace, 0xFF00FF00, L"" );
        }

        // Draw the word
        m_Font.DrawText( 0xFF00FF00, g_rgWordArray[dwIndex] );
    }
}
