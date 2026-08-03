//-----------------------------------------------------------------------------
// File: XActInteractiveAudio.cpp
//
// Desc: Demonstrates XACT's interactive audio capabilites
//
// Hist: 5.15.03 - New for June 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>
#include <xact.h>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Change control\nvariable" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Play cue" },
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont     m_Font;             // Font object
    CXBHelp     m_Help;             // Help object
    BOOL        m_bDrawHelp;        // TRUE to draw help screen

    // XACT Engine
    PXACTENGINE                 m_pXACT;                        // XACT Engine
    PXACTWAVEBANK               m_pStreamingWaveBank;           // Streaming wave bank
    PBYTE                       m_pbSoundBank;                  // Sound bank data
    PXACTSOUNDBANK              m_pSoundBank;                   // XACT Sound Bank
    PXACTSOUNDCUE               m_pSoundCue;                    // XACT Sound cue
    
    FLOAT                       m_fVariableValue;               // XIA variable
    FLOAT                       m_fVariableValueLast;           // Previous value

public:
    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();

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
    m_bDrawHelp = FALSE;

    // XACT Engine
    m_pXACT                     = NULL;
    m_pStreamingWaveBank        = NULL;
    m_pbSoundBank               = NULL;
    m_pSoundBank                = NULL;
    m_pSoundCue                 = NULL;

    m_fVariableValue            = 0.0f;
    m_fVariableValueLast        = m_fVariableValue;
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

    // Tell DirectSound to use Full HRTF
    DirectSoundUseFullHRTF();

    // Initialize the XACT runtime parameters
    XACT_RUNTIME_PARAMETERS xrParams = {0};
    xrParams.dwMax2DHwVoices         = 128;
    xrParams.dwMax3DHwVoices         = 32;
    xrParams.dwMaxConcurrentStreams  = 20;
    xrParams.dwMaxNotifications      = 0;

    // Create the XACT runtime engine
    if( FAILED( XACTEngineCreate( &xrParams, &m_pXACT ) ) )
    {
        return E_FAIL;
    }

    // Open the streaming wave bank file
    HANDLE hStreamingWaveBank = CreateFile( "D:\\media\\sounds\\XactSounds_streaming.xwb",
                                            GENERIC_READ, 
                                            FILE_SHARE_READ, 
                                            NULL,
                                            OPEN_EXISTING, 
                                            FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, 
                                            NULL );
    if( INVALID_HANDLE_VALUE == hStreamingWaveBank )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Initialize the streaming wave bank parameters
    XACT_WAVEBANK_STREAMING_PARAMETERS wbParams = {0};
    wbParams.hFile                        = hStreamingWaveBank;
    wbParams.dwOffset                     = 0;
    wbParams.dwPacketSizeInMilliSecs      = 400;
    wbParams.dwPrimePacketSizeInMilliSecs = 400;

    // Register the streaming wave bank with XACT
    if( FAILED( m_pXACT->RegisterStreamedWaveBank( &wbParams, 
                                                   &m_pStreamingWaveBank ) ) )
    {
        return E_FAIL;
    }

    // Load the sound bank file
    DWORD dwFileSize;
    if( FAILED( XBUtil_LoadFile( "D:\\media\\sounds\\XactSounds.xsb", 
                                 (VOID **)&m_pbSoundBank, 
                                 &dwFileSize ) ) )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Create the sound bank using the XACT engine
    if( FAILED( m_pXACT->CreateSoundBank( m_pbSoundBank,
                                          dwFileSize,
                                          &m_pSoundBank ) ) )
    {
        return E_FAIL;
    }

    // For this sample, we assume there's just one interactive audio
    // cue in the sound bank, at index zero
    m_pSoundBank->Play( 0, NULL, 0, &m_pSoundCue );

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

    // Use the A button to toggle playback.  Since interactive
    // audio cues will never stop on their own, we don't have 
    // to worry about listening for notifications
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        if( m_pSoundCue )
        {
            m_pSoundBank->Stop( 0, 0, NULL );
            m_pSoundCue = NULL;
        }
        else
        {
            m_pSoundBank->Play( 0, NULL, 0, &m_pSoundCue );
        }
    }

    // Update the control variable based on the right thumbstick
    m_fVariableValue += m_DefaultGamepad.fX2 * 3000.0f * m_fElapsedTime;
    if( m_fVariableValue < 0.0f )
        m_fVariableValue = 0.0f;
    else if( m_fVariableValue > 10000.0f )
        m_fVariableValue = 10000.0f;
    
    if( m_fVariableValue != m_fVariableValueLast &&
        m_DefaultGamepad.fX2 == 0.0f )
    {
        m_pXACT->SetVariable( 0, (WORD)m_fVariableValue, 0 );
        m_pXACT->CommitDeferredSettings();
        m_fVariableValueLast = m_fVariableValue;
    }

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
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"XActInteractiveAudio" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        m_Font.DrawText( 48, 100, 0xffffffff, L"Interactive audio features are determined almost" );
        m_Font.DrawText( 48, 125, 0xffffffff, L"entirely by the sound designer in the XACT project" );
        m_Font.DrawText( 48, 150, 0xffffffff, L"file.  All the programmer has to do is change the" );
        m_Font.DrawText( 48, 175, 0xffffffff, L"control variable in response to game state" );

        WCHAR str[256];
        DWORD dwColor = ( m_pSoundCue != NULL ) ? 0xffffff00 : 0xff808080;
        swprintf( str, L"Control variable: " GLYPH_LEFT_TICK L" %d " GLYPH_RIGHT_TICK, (WORD)m_fVariableValue );
        m_Font.DrawText( 48, 240, dwColor, str );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

