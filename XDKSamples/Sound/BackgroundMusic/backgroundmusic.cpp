//-----------------------------------------------------------------------------
// File: BackgroundMusic.cpp
//
// Desc: The BackgroundMusic sample demonstrates how to play background WMA
//       files, combining game WMA assets with user soundtracks stored on the
//       Xbox hard drive.  Please see the documentation for this sample for
//       further information.
//
// Hist: 08.20.01 - New for October XDK
//       03.06.02 - Added audio level meters for April 02 XDK release
//       10.07.02 - Cleaned up for November 02 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include <xgraphics.h>
#include "dsstdfx.h"
#include "MusicManager.h"




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nplayback" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Next\nsoundtrack" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Next song" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Pause\nplayback" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Recreate\nMusicManager" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Random\nsong" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Volume" },
    { XBHELP_MISC_CALLOUT, XBHELP_PLACEMENT_2, L"\406 Toggle random\n\407 Toggle global" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts)/sizeof( g_HelpCallouts[0]);




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Font and help
    CXBFont         m_Font;
    CXBHelp         m_Help;
    BOOL            m_bDrawHelp;

    LPDIRECTSOUND8  m_pDSound;          // DirectSound object
    CMusicManager*  m_pMusicManager;    // Music Manager

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
    m_bDrawHelp     = FALSE;
    m_pMusicManager = NULL;
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

    if( FAILED( DirectSoundCreate( NULL, &m_pDSound, NULL ) ) )
        return E_FAIL;

    // download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\media\\dsstdfx.bin", 
                                            &EffectLoc, 
                                            XAUDIO_DOWNLOADFX_EXTERNFILE, 
                                            NULL ) ) )
        return E_FAIL;

    // Create MusicManager
    m_pMusicManager = new CMusicManager();
    if( !m_pMusicManager )
        return E_OUTOFMEMORY;

    // Initialize MusicManager
    if( FAILED( m_pMusicManager->Initialize() ) )
        return E_FAIL;

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

    // Toggle playback
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        if( m_pMusicManager->GetStatus() == MM_PLAYING )
            m_pMusicManager->Stop();
        else
            m_pMusicManager->Play();
    }

    // Switch to next soundtrack
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        m_pMusicManager->NextSoundtrack();
    }

    // Switch to next song
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
    {
        m_pMusicManager->NextSong();
    }

    // Select a random song
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
    {
        BOOL bWasPlaying = ( m_pMusicManager->GetStatus() == MM_PLAYING ) ? TRUE : FALSE;
        BOOL bWasPaused  = ( m_pMusicManager->GetStatus() == MM_PAUSED ) ? TRUE : FALSE;

        if( bWasPlaying || bWasPaused )
            m_pMusicManager->Stop();

        m_pMusicManager->RandomSong( m_pMusicManager->GetGlobal() );

        if( bWasPlaying || bWasPaused )
            m_pMusicManager->Play();
        if( bWasPaused )
            m_pMusicManager->Pause();
    }

    // Pause playback
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
    {
        if( m_pMusicManager->GetStatus() == MM_PLAYING )
        {
            m_pMusicManager->Pause();
        }
        else if( m_pMusicManager->GetStatus() == MM_PAUSED )
        {
            m_pMusicManager->Play();
        }
    }

    // Destroy the MusicManager and create a new one
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
    {
        delete m_pMusicManager;
        m_pMusicManager = new CMusicManager();
        if( !m_pMusicManager )
            return E_OUTOFMEMORY;

        if( FAILED( m_pMusicManager->Initialize() ) )
            return E_FAIL;
    }

    // Select between random and sequential play
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] )
    {
        m_pMusicManager->SetRandom( !m_pMusicManager->GetRandom() );
    }

    // Select between global and local play
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] )
    {
        m_pMusicManager->SetGlobal( !m_pMusicManager->GetGlobal() );
    }

    FLOAT fVolume = m_pMusicManager->GetVolume();
    fVolume += 2000 * m_DefaultGamepad.fY1 * m_fElapsedTime;
    if( fVolume < DSBVOLUME_MIN )
        fVolume = DSBVOLUME_MIN;
    if( fVolume > DSBVOLUME_MAX )
        fVolume = DSBVOLUME_MAX;
    m_pMusicManager->SetVolume( fVolume );

    // Let dsound and music manager do some work
    DirectSoundDoWork();
    m_pMusicManager->Process();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"BackgroundMusic" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        WCHAR strSoundtrack[MAX_SOUNDTRACK_NAME];
        WCHAR strSong[MAX_SONG_NAME];
        DWORD dwLength;
        WCHAR strBuffer[100];

        // Get information about what song/soundtrack is currently selected
        m_pMusicManager->GetCurrentInfo( strSoundtrack, strSong, &dwLength );
        m_Font.DrawText( 64, 80, 0xffffffff, L"Soundtrack: " );
        m_Font.DrawText( 0xffffff00, strSoundtrack );

        m_Font.DrawText( 64, 110, 0xffffffff, L"Song: " );
        m_Font.DrawText( 0xffffff00, strSong );

        FLOAT fPos = m_pMusicManager->GetPlaybackPosition();
        swprintf( strBuffer, L"%02d:%02d / %02d:%02d", DWORD(fPos / 60), 
                                                       (DWORD)fPos % 60, 
                                                       ( dwLength / 60000 ), 
                                                       ( dwLength / 1000 ) % 60 );
        m_Font.DrawText( 64, 140, 0xffffffff, L"Position: " );
        switch( m_pMusicManager->GetStatus() )
        {
            case MM_PLAYING:
                m_Font.DrawText( 0xffffff00, strBuffer );
                break;

            case MM_PAUSED:
                m_Font.DrawText( 0xff808080, strBuffer );
                m_Font.DrawText( 0xff808080, L" (paused)" );
                break;

            default:
                m_Font.DrawText( 0xff808080, strBuffer );
                m_Font.DrawText( 0xff808080, L" (stopped)" );
                break;
        }

        m_Font.DrawText( 64, 170, 0xffffffff, L"Mode: " );
        m_Font.DrawText( 0xffffff00, m_pMusicManager->GetRandom() ? L"Random" : L"Sequential" );
        m_Font.DrawText( 0xffffff00, L" " );
        m_Font.DrawText( 0xffffff00, m_pMusicManager->GetGlobal() ? L"Global" : L"Local" );

        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, m_pMusicManager->GetVolume() / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(m_pMusicManager->GetVolume()) - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 200, 0xffffffff, L"Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        // Display some help text
        if( m_pMusicManager->GetStatus() == MM_PAUSED )
            m_Font.DrawText( 320, 320, 0xffffffff, L"Press \400 or \401 to play current song", XBFONT_CENTER_X );
        else if( m_pMusicManager->GetStatus() == MM_PLAYING )
        {
            m_Font.DrawText( 320, 320, 0xffffffff, L"Press \400 to stop current song", XBFONT_CENTER_X );
            m_Font.DrawText( 320, 350, 0xffffffff, L"Press \401 to pause current song", XBFONT_CENTER_X );
        }
        else
            m_Font.DrawText( 320, 320, 0xffffffff, L"Press \400 to play current song", XBFONT_CENTER_X );

        m_Font.DrawText( 320, 380, 0xffffffff, L"Press \412\413 for Help", XBFONT_CENTER_X );

        m_Font.End();

        // Draw the on-screen audio level meters
        XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
