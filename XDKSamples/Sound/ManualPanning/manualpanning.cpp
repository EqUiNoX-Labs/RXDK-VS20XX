//-----------------------------------------------------------------------------
// File: ManualPanning.cpp
//
// Desc: The ManualPanning sample demonstrates how to perform panning by 
//       sending a buffer to different mixbins and controlling the mixbin
//       volume.  This sample demonstrates the mixbins for individual speakers,
//       but the process is the same for non-speaker mixbins
//
// Hist: 04.30.01 - New for June release
//       03.06.02 - Added audio level meters for April 02 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include <dsound.h>
#include <xgraphics.h>
#include "dsstdfx.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nplayback" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Change\nsound" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nlooping" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Increase\nvolume" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Decrease\nvolume" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Select\nspeaker" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Change speaker\nvolume" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);




// List of wav files to cycle through
const WCHAR* g_strMediaDir = L"Media\\Sounds\\";
WCHAR* g_strFileNames[] = 
{
    L"Heli.wav",
    L"DockingMono.wav",
    L"EngineStartMono.wav",
    L"MaleDialog1.wav",
    L"MiningMono.wav",
    L"MusicMono.wav",
    L"Dolphin4.wav",
};
const DWORD NUM_SOUNDS = sizeof(g_strFileNames) / sizeof(g_strFileNames[0]);




// Struct containing speaker information
struct OPTION_STRUCT
{
    DWORD  dwMixBinID;
    FLOAT  fVolume;
    WCHAR* strDescription;
};

// List of speakers/mixbins we can configure
OPTION_STRUCT g_aOptions[] =
{
    { DSMIXBIN_FRONT_LEFT,      DSBVOLUME_MAX,  L"Front Left" },
    { DSMIXBIN_FRONT_RIGHT,     DSBVOLUME_MAX,  L"Front Right" },
    { DSMIXBIN_FRONT_CENTER,    DSBVOLUME_MAX,  L"Front Center" },
    { DSMIXBIN_LOW_FREQUENCY,   DSBVOLUME_MAX,  L"Low Frequency" },
    { DSMIXBIN_BACK_LEFT,       DSBVOLUME_MAX,  L"Back Left" },
    { DSMIXBIN_BACK_RIGHT,      DSBVOLUME_MAX,  L"Back Right" },
};
static const DWORD NUM_OPTIONS = sizeof( g_aOptions ) / sizeof( g_aOptions[0] );




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Font and help
    BOOL        m_bDrawHelp;
    CXBFont     m_Font;
    CXBHelp     m_Help;

    CWaveFile               m_awfSounds[NUM_SOUNDS];// Wave file parsers
    DWORD                   m_dwCurrent;            // Current sound
    BOOL                    m_bPlaying;             // Are we playing?
    FLOAT                   m_fVolume;              // Current volume
    LPDIRECTSOUNDBUFFER8    m_pDSBuffer;            // DirectSoundBuffer
    BYTE*                   m_pbSampleData;         // Sample data from wav
    BOOL                    m_bLooping;             // TRUE if loop is enabled
    DWORD                   m_dwOption;             // Selected option
    LPDIRECTSOUND8          m_pDSound;              // DirectSound object
    DSMIXBINVOLUMEPAIR      m_dsmbvp[8];            // Mix bin volumes
    DSMIXBINS               m_dsmb;                 // DSound MixBin struct

    HRESULT SwitchToSound( DWORD dwIndex );         // Sets up a different sound

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

    // Sounds
    m_fVolume = DSBVOLUME_MAX;
    m_pbSampleData = NULL;
    m_dwOption = 0;
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

    // Open up each of our wave files - note that this is just opening
    // the file and finding the important chunks, but doesn't actually
    // load any data
    for( int i = 0; i < NUM_SOUNDS; i++ )
    {
        char strFullPath[MAX_PATH];
        sprintf( strFullPath, "d:\\%S%S", g_strMediaDir, g_strFileNames[i] );
        if( FAILED( m_awfSounds[i].Open( strFullPath ) ) )
            return XBAPPERR_MEDIANOTFOUND;
    }

    // Get the wave format of the first sound to use when creating the
    // DirectSound Buffer.  We no longer care if all the sounds have
    // the same wave format, since we can use SetFormat to change it
    // on the fly.  
    WAVEFORMATEXTENSIBLE wfx;
    if( FAILED( m_awfSounds[0].GetFormat( &wfx ) ) )
        return E_FAIL;

    // Create a sound buffer of 0 size, since we're going to use
    // SetBufferData
    DSBUFFERDESC dsbdesc;
    ZeroMemory( &dsbdesc, sizeof( DSBUFFERDESC ) );
    dsbdesc.dwSize = sizeof( DSBUFFERDESC );

    // If fewer than 256 buffers are in existence at all points during 
    // the game, it may be more efficient not to use LOCDEFER.
    dsbdesc.dwFlags = DSBCAPS_LOCDEFER;
    dsbdesc.dwBufferBytes = 0;
    dsbdesc.lpwfxFormat = (WAVEFORMATEX*)&wfx;
    dsbdesc.lpMixBins = &m_dsmb;

    // We're just going to use the 6 speaker mixbins for this sample,
    // but you can pan to up to 8 mixbins
    m_dsmb.dwMixBinCount = 6;
    m_dsmb.lpMixBinVolumePairs = m_dsmbvp;

    // Our default mapping is to send to every speaker at minimum
    // volume.  This way, any time we switch sounds, we'll be 
    // sending to all speakers.  The correct volumes will be set
    // in the FrameMove() function based on current values
    m_dsmbvp[0].dwMixBin = DSMIXBIN_FRONT_LEFT;
    m_dsmbvp[0].lVolume = DSBVOLUME_MIN;
    m_dsmbvp[1].dwMixBin = DSMIXBIN_FRONT_RIGHT;
    m_dsmbvp[1].lVolume = DSBVOLUME_MIN;
    m_dsmbvp[2].dwMixBin = DSMIXBIN_FRONT_CENTER;
    m_dsmbvp[2].lVolume = DSBVOLUME_MIN;
    m_dsmbvp[3].dwMixBin = DSMIXBIN_LOW_FREQUENCY;
    m_dsmbvp[3].lVolume = DSBVOLUME_MIN;
    m_dsmbvp[4].dwMixBin = DSMIXBIN_BACK_LEFT;
    m_dsmbvp[4].lVolume = DSBVOLUME_MIN;
    m_dsmbvp[5].dwMixBin = DSMIXBIN_BACK_RIGHT;
    m_dsmbvp[5].lVolume = DSBVOLUME_MIN;

    if( FAILED( DirectSoundCreateBuffer( &dsbdesc, &m_pDSBuffer ) ) )
        return E_FAIL;

    // Set up and play our initial sound
    m_dwCurrent = 0;
    m_bPlaying = TRUE;
    m_bLooping = TRUE;
    SwitchToSound( m_dwCurrent );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SwitchToSound()
// Desc: Switches to the given sound by:
//       1) Stop playback if we're playing
//       2) Reallocate the sample data buffer
//       3) Point the DirectSoundBuffer to the new data
//       4) Restart playback if needed
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SwitchToSound( DWORD dwIndex )
{
    DWORD dwNewSize;

    // If we're currently playing, stop, so that we don't crash
    // when we reallocate our buffer
    if( m_bPlaying )
    {
        m_pDSBuffer->Stop();
    }

    // Calling stop doesn't immediately shut down
    // the voice, so point it away from our buffer
    m_pDSBuffer->SetBufferData( NULL, 0 );

    // Load the wave format from the file
    WAVEFORMATEXTENSIBLE wfx;
    if( FAILED( m_awfSounds[ dwIndex ].GetFormat( &wfx ) ) )
        return E_FAIL;
    m_pDSBuffer->SetFormat( (WAVEFORMATEX *)&wfx );

    // Changing formats re-sets the mixbins to default, so we need
    // to call SetMixBins again with our mixbin settings
    m_pDSBuffer->SetMixBins( &m_dsmb );

    // Find out how big the new sample is
    m_awfSounds[ dwIndex ].GetDuration( &dwNewSize );

    // Set our allocation to that size
    if( m_pbSampleData )
        delete[] m_pbSampleData;
    m_pbSampleData = new BYTE[ dwNewSize ];
    if( !m_pbSampleData )
        return E_OUTOFMEMORY;

    // Read sample data from the file
    m_awfSounds[ dwIndex ].ReadSample( 0, m_pbSampleData, dwNewSize, &dwNewSize );

    // Check for embedded loop points
    DWORD dwLoopStart  = 0;
    DWORD dwLoopLength = dwNewSize;
    m_awfSounds[ dwIndex ].GetLoopRegion( &dwLoopStart, &dwLoopLength );

    // Set up values for the new buffer
    m_pDSBuffer->SetBufferData( m_pbSampleData, dwNewSize );
    m_pDSBuffer->SetLoopRegion( dwLoopStart, dwLoopLength );
    m_pDSBuffer->SetCurrentPosition( 0 );

    // If we were playing before, restart playback now
    if( m_bPlaying )
    {
        m_pDSBuffer->Play( 0, 0, m_bLooping ? DSBPLAY_LOOPING : 0 );
    }

    return S_OK;
}




#define VOLUME_SCALE 5.0f
//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Check if buffer is still playing
    DWORD dwStatus;
    m_pDSBuffer->GetStatus( &dwStatus );
    m_bPlaying = dwStatus & DSBSTATUS_PLAYING;

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Increase/Decrease volume
    m_fVolume += ( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_WHITE ] - 
                   m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_BLACK ] ) *
                 m_fElapsedTime * VOLUME_SCALE;

    // Make sure volume is in the appropriate range
    if( m_fVolume < DSBVOLUME_MIN )
        m_fVolume = DSBVOLUME_MIN;
    else if( m_fVolume > DSBVOLUME_MAX )
        m_fVolume = DSBVOLUME_MAX;

    // Toggle sound on and off
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        if( m_bPlaying )
            m_pDSBuffer->Stop( );
        else
        {
            // Start playback at beginning of buffer
            m_pDSBuffer->SetCurrentPosition( 0 );
            m_pDSBuffer->Play( 0, 0, m_bLooping ? DSBPLAY_LOOPING : 0 );
            m_bPlaying = TRUE;
        }
    }

    // Cycle through sounds
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
    {
        m_dwCurrent = ( m_dwCurrent + 1 ) % NUM_SOUNDS;
        SwitchToSound( m_dwCurrent );
    }

    // Cycle through speakers
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_dwOption = ( m_dwOption + NUM_OPTIONS - 1 ) % NUM_OPTIONS;
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_dwOption = ( m_dwOption + 1 ) % NUM_OPTIONS;
    }

    // Adjust selected speaker volume
    g_aOptions[ m_dwOption ].fVolume += m_DefaultGamepad.fX2 * m_fElapsedTime * 1500;
    if( g_aOptions[ m_dwOption ].fVolume < DSBVOLUME_MIN )
        g_aOptions[ m_dwOption ].fVolume = DSBVOLUME_MIN;
    else if( g_aOptions[ m_dwOption ].fVolume > DSBVOLUME_MAX )
        g_aOptions[ m_dwOption ].fVolume = DSBVOLUME_MAX;

    // Toggle looping
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] )
    {
        m_bLooping = !m_bLooping;

        // If we were playing, make another call to play to change looping
        if( m_bPlaying )
            m_pDSBuffer->Play( 0, 0, m_bLooping ? DSBPLAY_LOOPING : 0 );
    }

    // Set overall buffer volume
    m_pDSBuffer->SetVolume( (LONG)m_fVolume );
    
    // Build up a bitmap with active speakers and an array 
    // of active speaker volumes
    DSMIXBINS dsmixbins;
    DSMIXBINVOLUMEPAIR dsmbvp[DSMIXBIN_ASSIGNMENT_MAX];

    dsmixbins.dwMixBinCount = 0;
    dsmixbins.lpMixBinVolumePairs = dsmbvp;

    for( int i = 0; i < sizeof( g_aOptions ) / sizeof( g_aOptions[0] ); i++ )
    {
        // If this speaker is more than minimum volume, 
        if( g_aOptions[i].fVolume > DSBVOLUME_MIN )
        {
            // Add it to the bitmap and volume array
            dsmbvp[dsmixbins.dwMixBinCount].dwMixBin = g_aOptions[i].dwMixBinID;
            dsmbvp[dsmixbins.dwMixBinCount].lVolume = (LONG)g_aOptions[i].fVolume;

            dsmixbins.dwMixBinCount++;
        }
    }

    // Set all speaker volumes
    m_pDSBuffer->SetMixBinVolumes( &dsmixbins );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    DirectSoundDoWork();

    // Draw a gradient filled background
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        WCHAR strBuffer[200];

        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"ManualPanning" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Show status
        swprintf( strBuffer, L"%s%s", g_strFileNames[ m_dwCurrent ], m_bLooping ? L" (Looping)" : L"" );
        m_Font.DrawText( 64, 100, 0xffffffff, L"Current Sound: " );
        m_Font.DrawText( m_bPlaying ? 0xffffff00 : 0xff808000, strBuffer );
        
        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, m_fVolume / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(m_fVolume) - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 130, 0xffffffff, L"Master Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        m_Font.DrawText( 64, 160, 0xffffffff, L"Speaker Select: " );

        for( DWORD i = 0; i < sizeof( g_aOptions ) / sizeof( g_aOptions[0] ); i++ )
        {
            DWORD dwColor1 = ( m_dwOption == i ) ? 0xffffffff : 0xff808080;
            DWORD dwColor2 = ( m_dwOption == i ) ? 0xffffff00 : 0xff808000;
            
            // Show percentage and volume (rounded to nearest dB)
            fPercent = powf( 10, g_aOptions[i].fVolume / 2000.0f ) * 100;
            swprintf( strBuffer, L"%s: ", g_aOptions[i].strDescription );
            m_Font.DrawText( 88, 190.0f + i * 30, dwColor1, strBuffer );

            swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(g_aOptions[i].fVolume) - 50 ) / 100,
                                                    fPercent );
            m_Font.DrawText( dwColor2, strBuffer );
            
            if( m_dwOption == i )
                m_Font.DrawText( 88, 190.0f + i * 30, 0xffffffff, GLYPH_RIGHT_TICK, XBFONT_RIGHT );
        }
        m_Font.End();
    }

    // Draw the on-screen audio level meters
    XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

