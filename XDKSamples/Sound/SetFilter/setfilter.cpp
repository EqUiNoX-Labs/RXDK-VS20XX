//-----------------------------------------------------------------------------
// File: SetFilter.cpp
//
// Desc: The SetFilter sample demonstrates how to use the programmable filter
//       block provided by DirectSound.
//
// Hist: 04.30.01 - Created
//       07.18.01 - Added routines for calculating coefficients
//       10.15.01 - Updated Parametric EQ gain
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
#include <math.h>


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nplayback" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Change\nsound" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nDLS2" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nParamEQ" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Increase\nvolume" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Decrease\nvolume" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Select\nparameter" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Change\nparameter" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);




// List of wav files to cycle through
const WCHAR* g_strMediaDir = L"Media\\Sounds\\";
WCHAR* g_strFileNames[] = 
{
    L"noise.wav",
    L"55.wav",
    L"110.wav",
    L"220.wav",
    L"440.wav",
    L"880.wav",
    L"1760.wav",
    L"3520.wav",
    L"7040.wav",
    L"14080.wav",
};
const DWORD NUM_SOUNDS = sizeof(g_strFileNames) / sizeof(g_strFileNames[0]);


struct OPTION_STRUCT
{
    DWORD  dwScale;          // Joystick scale factor
    WCHAR* strDescription;  // Description of option
};

OPTION_STRUCT g_aOptions[] =
{
    { 1000,  L"DLS2 Filter Freq" },
    { 10000, L"DLS2 Resonance" },
    { 1000,  L"ParamEQ Filter Freq" },
    { 10000, L"ParamEQ Filter Gain" },
    { 10000, L"ParamEQ Filter Q" },
};

enum FILTER_OPTION
{
    DLS2_CutoffFrequency,
    DLS2_Resonance,
    PARAMEQ_Frequency,
    PARAMEQ_Gain,
    PARAMEQ_Q,
    NUM_OPTIONS,
};

#define RES_SCALE      1000
#define PEQ_GAIN_SCALE  400




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Font and help
    CXBFont     m_Font;
    CXBHelp     m_Help;
    BOOL        m_bDrawHelp;

    CWaveFile               m_awfSounds[NUM_SOUNDS];// Wave file parsers
    DWORD                   m_dwCurrent;            // Current sound
    BOOL                    m_bPlaying;             // Are we playing?
    FLOAT                   m_fVolume;              // Current volume
    LPDIRECTSOUNDBUFFER8    m_pDSBuffer;            // DirectSoundBuffer
    BYTE*                   m_pbSampleData;         // Sample data from wav
    DSFILTERDESC            m_dsfd;                 // DSFILTERDESC struct
    DWORD                   m_dwParam;              // Selected parameter
    LPDIRECTSOUND8          m_pDSound;              // DirectSound object

    FLOAT                   m_fDLS2Freq;            // DLS2 cutoff frequency
    LONG                    m_lDLS2Resonance;       // DLS2 resonance * RES_SCALE
    FLOAT                   m_fParamEQFreq;
    LONG                    m_lParamEQGain;
    LONG                    m_lParamEQQ;

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
    m_fVolume        = DSBVOLUME_MAX;
    m_pbSampleData   = NULL;
    m_lDLS2Resonance = 0;
    m_fDLS2Freq      = 440.0f;
    m_fParamEQFreq   = 440.0f;
    m_lParamEQGain   = 0;
    m_lParamEQQ      = 0;
}




//-----------------------------------------------------------------------------
// Name: FreqToHardwareCoeff()
// Desc: Calculates coefficient value[0] for DLS2 filter from a frequency
//-----------------------------------------------------------------------------
ULONG FreqToHardwareCoeff( FLOAT fFreq )
{
    FLOAT fNormCutoff = fFreq / 48000.0f;

    // Filter is ineffective out of these ranges, so why
    // bother even trying?
    if( fFreq < 30.0f )
        return 0x8000;
    if( fFreq > 8000.0f )
        return 0x0;

    FLOAT fFC = FLOAT( 2.0f * sin( D3DX_PI * fNormCutoff ) );
    // log(fFC)/log(2)
    LONG lOctaves = LONG( 4096.0f * log( fFC ) / (0.6931 ) );

    return (ULONG)lOctaves & 0xFFFF;
}




//-----------------------------------------------------------------------------
// Name: dBToHardwareCoeff()
// Desc: Calculates coefficient value[1] for DLS2 filter from resonance in dB
//-----------------------------------------------------------------------------
ULONG dBToHardwareCoeff( LONG lResonance )
{
    FLOAT fResonance = (FLOAT)lResonance;

    if( fResonance > 22.5f )
        fResonance = 22.5f;

    DOUBLE fQ = pow( 10.0, -0.05 * fResonance);
    ULONG dwQ = (ULONG)( 0x8000 * fQ );
    if( dwQ > 0xFFFF )
        dwQ = 0xFFFF;

    return (ULONG)dwQ;
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
    if( FAILED( DirectSoundCreateBuffer( &dsbdesc, &m_pDSBuffer ) ) )
        return E_FAIL;

    // Enable DLS2 lowpass filter to start with
    m_dwParam = 0;
    ZeroMemory( &m_dsfd, sizeof( DSFILTERDESC ) );
    m_dsfd.dwMode = DSFILTER_MODE_DLS2;
    m_dsfd.adwCoefficients[0] = FreqToHardwareCoeff( m_fDLS2Freq );
    m_dsfd.adwCoefficients[1] = dBToHardwareCoeff( m_lDLS2Resonance / RES_SCALE );
    m_dsfd.adwCoefficients[2] = FreqToHardwareCoeff( FLOAT(m_fParamEQFreq) );
    m_dsfd.adwCoefficients[3] = ( m_lParamEQGain >= 0 ) ? m_lParamEQGain : 0xC000 - m_lParamEQGain;
    m_dsfd.dwQCoefficient = m_lParamEQQ / RES_SCALE;
    m_pDSBuffer->SetFilter( &m_dsfd );


    // Set up and play our initial sound
    m_dwCurrent = 0;
    m_bPlaying = TRUE;
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

    // Changing the format resets the buffer, so reset our filter
    m_pDSBuffer->SetFilter( &m_dsfd );

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
        m_pDSBuffer->Play( 0, 0, DSBPLAY_LOOPING );

    return S_OK;
}




#define VOLUME_SCALE 5.0f
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
            m_pDSBuffer->Play( 0, 0, DSBPLAY_LOOPING );

        m_bPlaying = !m_bPlaying;
    }

    // Cycle through sounds
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
    {
        m_dwCurrent = ( m_dwCurrent + 1 ) % NUM_SOUNDS;
        SwitchToSound( m_dwCurrent );
    }

    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_dwParam == 0 )
            m_dwParam = NUM_OPTIONS - 1;
        else
            m_dwParam = ( m_dwParam - 1 );
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_dwParam = ( m_dwParam + 1 ) % NUM_OPTIONS;
    }

    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] )
    {
        m_dsfd.dwMode ^= DSFILTER_MODE_DLS2;
    }

    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] )
    {
        m_dsfd.dwMode ^= DSFILTER_MODE_PARAMEQ;
    }

    // Adjust value of currently selected parameter
    FLOAT fDelta = m_DefaultGamepad.fX2 * m_fElapsedTime * g_aOptions[ m_dwParam ].dwScale;
    switch( m_dwParam )
    {
        case DLS2_CutoffFrequency:
            m_fDLS2Freq += fDelta;
            if( m_fDLS2Freq < 0.0f )
                m_fDLS2Freq = 0.0f;
            else if( m_fDLS2Freq > 8000 )
                m_fDLS2Freq = 8000;
            break;
        case DLS2_Resonance:
            m_lDLS2Resonance += LONG( fDelta );
            if( m_lDLS2Resonance < 0 )
                m_lDLS2Resonance = 0;
            else if( m_lDLS2Resonance > 22 * RES_SCALE )
                m_lDLS2Resonance = 22 * RES_SCALE;
            break;
        case PARAMEQ_Frequency:
            m_fParamEQFreq += fDelta;
            if( m_fParamEQFreq < 0 )
                m_fParamEQFreq = 0;
            else if( m_fParamEQFreq > 8000 )
                m_fParamEQFreq = 8000;
            break;
        case PARAMEQ_Gain:
            // ParamEQ Gain operates a bit differently than we thought:
            // 0x0000-0x7FFF is a continuous increase in gain.  Then there's
            // about 0x2000 worth of attenuation we can do, but that
            // corresponds to the values 0xC000-0xDFFF
            m_lParamEQGain += LONG( fDelta );
            if( m_lParamEQGain < -0x1FFF )
                m_lParamEQGain = -0x1FFF;
            else if( m_lParamEQGain > 0x7FFF )
                m_lParamEQGain = 0x7FFF;
            break;
        case PARAMEQ_Q:
            m_lParamEQQ += LONG( fDelta );
            if( m_lParamEQQ < 0 )
                m_lParamEQQ = 0;
            else if( m_lParamEQQ / RES_SCALE > 7 )
                m_lParamEQQ = 7 * RES_SCALE;
            break;
    }

    // Note!!!:
    // For stereo buffers, where you can only use DLS2 OR ParamEQ, you must 
    // set both pairs of coefficients (one for each channel).
    m_dsfd.adwCoefficients[0] = FreqToHardwareCoeff( m_fDLS2Freq );
    m_dsfd.adwCoefficients[1] = dBToHardwareCoeff( m_lDLS2Resonance / RES_SCALE );
    m_dsfd.adwCoefficients[2] = FreqToHardwareCoeff( FLOAT(m_fParamEQFreq) );
    m_dsfd.adwCoefficients[3] = ( m_lParamEQGain >= 0 ) ? m_lParamEQGain : 0xC000 - m_lParamEQGain;
    m_dsfd.dwQCoefficient = m_lParamEQQ / RES_SCALE;

    m_pDSBuffer->SetVolume( (LONG)m_fVolume );
    m_pDSBuffer->SetFilter( &m_dsfd );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Pump DirectSound's work queue
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
        m_Font.DrawText( 48, 36, 0xffffffff, L"SetFilter" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Show status
        swprintf( strBuffer, L"Current Sound: %s", g_strFileNames[ m_dwCurrent ] );
        m_Font.DrawText( 64, 100, 0xffffffff, L"Current Sound: " );
        m_Font.DrawText( m_bPlaying ? 0xffffff00 : 0xff808000, g_strFileNames[m_dwCurrent] );
        
        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, m_fVolume / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(m_fVolume) - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 130, 0xffffffff, L"Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        m_Font.DrawText( 64, 160, 0xffffffff, L"DLS2: " );
        m_Font.DrawText( 0xffffff00, m_dsfd.dwMode & DSFILTER_MODE_DLS2 ? L"Enabled" : L"Disabled" );
        m_Font.DrawText( 250, 160, 0xffffffff, L"ParamEQ: " );
        m_Font.DrawText( 0xffffff00, m_dsfd.dwMode & DSFILTER_MODE_PARAMEQ ? L"Enabled" : L"Disabled" );

        for( DWORD i = 0; i < NUM_OPTIONS; i++ )
        {
            if( i == m_dwParam  )
                m_Font.DrawText( 88, FLOAT(190 + i * 30), 0xffffffff, GLYPH_RIGHT_TICK, XBFONT_RIGHT );

            swprintf( strBuffer, L"%s: ", g_aOptions[i].strDescription );
            m_Font.DrawText( 88, FLOAT(190 + i * 30), i == m_dwParam ? 0xffffffff : 0xff808080, strBuffer );

            switch( i )
            {
                case DLS2_CutoffFrequency:
                    swprintf( strBuffer, L"%0.2fHz", m_fDLS2Freq );
                    break;
                case DLS2_Resonance:
                    swprintf( strBuffer, L"%lddB", m_lDLS2Resonance / RES_SCALE );
                    break;
                case PARAMEQ_Frequency:
                    swprintf( strBuffer, L"%0.2fHz", m_fParamEQFreq );
                    break;
                case PARAMEQ_Gain:
                    swprintf( strBuffer, L"%ld", m_lParamEQGain );
                    break;
                case PARAMEQ_Q:
                    swprintf( strBuffer, L"%ld", m_lParamEQQ / RES_SCALE );
                    break;
            }
            m_Font.DrawText( i == m_dwParam ? 0xffffff00 : 0xff808000, strBuffer );
        }

        m_Font.End();
    }

    // Draw the on-screen audio level meters
    XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

