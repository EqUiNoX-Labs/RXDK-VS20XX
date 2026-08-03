//-----------------------------------------------------------------------------
// File: EnvelopeGenerator.cpp
//
// Desc: The EnvelopeGenerator sample demonstrates how to control looping and
//       Envelope Generators in DirectSound
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
#include <stddef.h> // For offsetof
#include "dsstdfx.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nplayback" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Change\nsound" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nLooping" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_1, L"Change EG" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Increase\nvolume" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Decrease\nvolume" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Select\nparameter" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Change value" },
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


// Struct for changing parameters of DSENVELOPEDESC
struct OPTION_STRUCT
{
    DWORD   dwOffset;       // Offset into EnvelopeDesc of field being changed
    LONG    dwMinValue;     // Minimum value of option
    LONG    dwMaxValue;     // Maximum value of option
    BOOL    bEG2;           // TRUE if only available in EG2 (DSEG_MULTI)
    WCHAR*  strDescription;  // Description of option
    FLOAT   fCurrentValue;  // Current value as a float
};

// List of parameters we can change
OPTION_STRUCT g_aOptions[] =
{
    { offsetof( DSENVELOPEDESC, dwDelay ),      0,  0xFFF, FALSE,  L"Delay" },
    { offsetof( DSENVELOPEDESC, dwAttack ),     0,  0xFFF, FALSE,  L"Attack" },
    { offsetof( DSENVELOPEDESC, dwHold ),       0,  0xFFF, FALSE,  L"Hold" },
    { offsetof( DSENVELOPEDESC, dwDecay ),      0,  0xFFF, FALSE,  L"Decay" },
    { offsetof( DSENVELOPEDESC, dwSustain ),    0,  0xFF,  FALSE,  L"Sustain" },
    { offsetof( DSENVELOPEDESC, dwRelease ),    0,  0xFFF, FALSE,  L"Release" },
    { offsetof( DSENVELOPEDESC, lPitchScale ),  -128, 127, TRUE,   L"Pitch Scale" },
    { offsetof( DSENVELOPEDESC, lFilterCutOff), -128, 127, TRUE,   L"Filter Cutoff" },
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
    BYTE *                  m_pbSampleData;         // Sample data from wav
    DSENVELOPEDESC          m_adsedEG[2];           // Envelope Generate settings
    DWORD                   m_dwParam;              // Selected parameter
    DWORD                   m_dwEG;                 // Selected EG
    BOOL                    m_bLooping;             // True if loop is enabled
    LPDIRECTSOUND8          m_pDSound;              // DirectSound object

    // Acceleration factor for controller input
    FLOAT                   m_fAcceleration;

    HRESULT SwitchToSound( DWORD dwIndex );         // Sets up a different sound
    HRESULT DrawEnvelope();                         // Draw the envelope graph

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
    
    m_fAcceleration = 1.0f;
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

    // Initialize EnvelopeGenerators
    m_dwEG = 0;
    m_dwParam = 0;
    ZeroMemory( m_adsedEG, 2 * sizeof( DSENVELOPEDESC ) );
    m_adsedEG[0].dwEG = DSEG_AMPLITUDE;
    m_adsedEG[0].dwMode = DSEG_MODE_DELAY;
    m_adsedEG[1].dwEG = DSEG_MULTI;
    m_adsedEG[1].dwMode = DSEG_MODE_DELAY;
    m_adsedEG[0].dwSustain = 0xFF;
    g_aOptions[4].fCurrentValue = 255.0f;


    // Create a sound buffer of 0 size, since we're going to use
    // SetBufferData
    DSBUFFERDESC dsbdesc;
    ZeroMemory( &dsbdesc, sizeof( DSBUFFERDESC ) );
    dsbdesc.dwSize = sizeof( DSBUFFERDESC );

    // If fewer than 256 buffers are in existence at all points during 
    // the game, it may be more efficient not to use LOCDEFER.
    dsbdesc.dwFlags = DSBCAPS_LOCDEFER;
    dsbdesc.dwBufferBytes = 0;
    dsbdesc.lpwfxFormat = (WAVEFORMATEX *)&wfx;
    if( FAILED( DirectSoundCreateBuffer( &dsbdesc, &m_pDSBuffer ) ) )
        return E_FAIL;

    // Set up and play our initial sound
    // If we play the buffer with the EGs disabled (as they are by
    // default), then changing the EG's after playback started 
    // wouldn't have any affect.  Therefore, set the EG's BEFORE
    // starting playback
    m_pDSBuffer->SetEG( &m_adsedEG[0] );
    m_pDSBuffer->SetEG( &m_adsedEG[1] );
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
    m_awfSounds[ dwIndex ].GetLoopRegionBytes( &dwLoopStart, &dwLoopLength );

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
        {
            m_pDSBuffer->StopEx( 0, DSBSTOPEX_ENVELOPE );
        }
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

    // Cycle through EnvelopeDesc parameters
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        do {
            m_dwParam = ( m_dwParam + NUM_OPTIONS - 1 ) % NUM_OPTIONS;
        } while ( g_aOptions[ m_dwParam ].bEG2 && m_dwEG == 0 );
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        do {
            m_dwParam = ( m_dwParam + 1 ) % NUM_OPTIONS;
        } while ( g_aOptions[ m_dwParam ].bEG2 && m_dwEG == 0 );
    }

    // Switch between EGs
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] )
    {
        m_dwEG ^= 1;

        // If we were on an EG2-only param, change to first param
        if( m_dwEG == 0 && g_aOptions[ m_dwParam ].bEG2 )
        {
            m_dwParam = 0;
        }
        
        // Reload the float values from the EG struct
        for( DWORD i = 0; i < NUM_OPTIONS; i++ )
        {
            g_aOptions[i].fCurrentValue = FLOAT( *(LONG*)( (BYTE *)&m_adsedEG[ m_dwEG ] + g_aOptions[i].dwOffset ) );
        }
    }

    // Adjust value of currently selected parameter, making
    // sure to keep it within the appropriate range
    FLOAT*  pfValue = (FLOAT *)&g_aOptions[ m_dwParam ].fCurrentValue;
    FLOAT   fDelta = m_DefaultGamepad.fX2 * m_fElapsedTime * 100 * m_fAcceleration;
    if( fDelta < 0.01f && fDelta > -0.01f )
    {
        // Reset acceleration
        m_fAcceleration = 1.0f;
    }
    else
    {
#if 0
        if( fDelta < 0 && *pfValue + fDelta < g_aOptions[ m_dwParam ].dwMinValue )
            fDelta = g_aOptions[ m_dwParam ].dwMinValue - *pfValue;
        else if( fDelta > 0 && *pfValue + fDelta > g_aOptions[ m_dwParam ].dwMaxValue )
            fDelta = g_aOptions[ m_dwParam ].dwMaxValue - *pfValue;
#endif // 0
        // Adjust acceleration factor
        m_fAcceleration += 1.0f * m_fElapsedTime;
    }

    // Write out the new value
    (*pfValue) += fDelta;
    LONG*   plValue = (LONG*)((BYTE*)&m_adsedEG[m_dwEG] + g_aOptions[m_dwParam].dwOffset);
    if( (*pfValue) != (*plValue) )
    {
        if( (*pfValue) < g_aOptions[ m_dwParam ].dwMinValue )
        {
            (*pfValue) = FLOAT( g_aOptions[ m_dwParam ].dwMinValue );
        }
        else if( (*pfValue) > g_aOptions[ m_dwParam].dwMaxValue )
        {
            (*pfValue) = FLOAT( g_aOptions[ m_dwParam ].dwMaxValue );
        }
        (*plValue) = (LONG)(*pfValue);
    }

    // Toggle looping
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] )
    {
        m_bLooping = !m_bLooping;

        // If we were playing, make another call to play to change looping
        if( m_bPlaying )
            m_pDSBuffer->Play( 0, 0, m_bLooping ? DSBPLAY_LOOPING : 0 );
    }


    // To use Filtercutoff in the MULTI EG, we need to set the DLS2 lowpass filter in 
    // the hardware to some reasonable values otherwise when we try to change the 
    // FilterCutoff, nothing will happen. FilterCutoff is relative to the resonant 
    // frequency we set below.  We set both pairs of coefficients to the same value
    // to handle stereo buffers, since we don't plan to use ParamEQ.
    // However, we don't want to use the filter unless we're actually using the
    // FilterCutOff in the EG, otherwise we'll lowpass filter ALL sounds
    DSFILTERDESC dsfd = {0};

    if( m_adsedEG[1].lFilterCutOff == 0 )
        dsfd.dwMode = DSFILTER_MODE_BYPASS;
    else
        dsfd.dwMode = DSFILTER_MODE_DLS2;
    dsfd.adwCoefficients[0] = 0xE283; // ~4kHz
    dsfd.adwCoefficients[1] = 0x8FF5; // ~3dB
    dsfd.adwCoefficients[2] = 0xE283; // ~4kHz
    dsfd.adwCoefficients[3] = 0x8FF5; // ~3dB
    m_pDSBuffer->SetFilter( &dsfd );

    // Update settings
    m_pDSBuffer->SetVolume( LONG(m_fVolume) );
    m_pDSBuffer->SetEG( &m_adsedEG[0] );
    m_pDSBuffer->SetEG( &m_adsedEG[1] );

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
    RenderGradientBackground( 0xff402040, 0xff404040 );

    // Draw a graph of the envelope
    DrawEnvelope();

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        WCHAR strBuffer[200];

        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"EnvelopeGenerator" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Show status
        swprintf( strBuffer, L"%s %s", g_strFileNames[ m_dwCurrent ], m_bLooping ? L"(looping)" : L"" );
        m_Font.DrawText( 64, 80, 0xffffffff, L"Current Sound: " );
        m_Font.DrawText( m_bPlaying ? 0xffffff00 : 0xff808000, strBuffer );
        
        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, m_fVolume / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(m_fVolume) - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 110, 0xffffffff, L"Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        for( DWORD i = 0; i < sizeof( g_aOptions ) / sizeof( g_aOptions[0] ); i++ )
        {
            // For each valid option, print out name and value
            if( m_dwEG != 0 || !g_aOptions[i].bEG2 )
            {
                swprintf( strBuffer, L"EG%d %s: ", m_dwEG + 1, g_aOptions[i].strDescription );
                m_Font.DrawText( 64, FLOAT(160 + i * 30), i == m_dwParam ? 0xffffffff : 0xff808080, strBuffer );

                swprintf( strBuffer, L"%d", *(LONG *)((BYTE *)&m_adsedEG[ m_dwEG ] + g_aOptions[i].dwOffset) );
                m_Font.DrawText( i == m_dwParam ? 0xffffff00 : 0xff808000, strBuffer );

                if( i == m_dwParam )
                    m_Font.DrawText( 64, FLOAT(160 + i * 30), 0xffffffff, GLYPH_RIGHT_TICK, XBFONT_RIGHT );
            }
        }
        m_Font.End();

        // Draw the on-screen audio level meters
        XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




#define ENV_XMIN 320
#define ENV_XMAX 580
#define ENV_YMIN 200
#define ENV_YMAX 400
#define ENV_XRANGE ( ENV_XMAX - ENV_XMIN )
#define ENV_YRANGE ( ENV_YMAX - ENV_YMIN )

//-----------------------------------------------------------------------------
// Name: DrawEnvelope()
// Desc: Draws a graph of the envelope
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DrawEnvelope()
{
    D3DXVECTOR4 ap[7];
    WAVEFORMATEXTENSIBLE wfx;
    DWORD dwSize;
    FLOAT fDuration;

    // Calculate EG values in seconds
    FLOAT fDelay  = m_adsedEG[ m_dwEG ].dwDelay  * 512 / 48000.0f;
    FLOAT fAttack = m_adsedEG[ m_dwEG ].dwAttack * 512 / 48000.0f;
    FLOAT fHold   = m_adsedEG[ m_dwEG ].dwHold   * 512 / 48000.0f;
    FLOAT fDecay  = m_adsedEG[ m_dwEG ].dwDecay  * 512 / 48000.0f;
    FLOAT fSustain= m_adsedEG[ m_dwEG ].dwSustain / 255.0f;
    FLOAT fRelease= m_adsedEG[ m_dwEG ].dwRelease * 512 / 48000.0f;

    // Calculate length of sound in seconds
    m_awfSounds[ m_dwCurrent ].GetFormat( &wfx  );
    m_awfSounds[ m_dwCurrent ].GetDuration( &dwSize );
    fDuration = ((float)dwSize) / (float)wfx.Format.nAvgBytesPerSec;

    // Scale over the total length of the envelope
    FLOAT fTotal = fDelay + fAttack + fHold + fDecay + fDuration + fRelease;
    
    // Set vertices for graph
    ap[0] = D3DXVECTOR4( ENV_XMIN, ENV_YMAX, 0.0f, 1.0f );
    ap[1] = D3DXVECTOR4( ap[0].x + fDelay / fTotal * ENV_XRANGE, ENV_YMAX, 0.0f, 1.0f );
    ap[2] = D3DXVECTOR4( ap[1].x + fAttack / fTotal * ENV_XRANGE, ENV_YMIN, 0.0f, 1.0f );
    ap[3] = D3DXVECTOR4( ap[2].x + fHold / fTotal * ENV_XRANGE, ENV_YMIN, 0.0f, 1.0f );
    ap[4] = D3DXVECTOR4( ap[3].x + fDecay / fTotal * ( 1.0f - fSustain ) * ENV_XRANGE, ENV_YMAX - fSustain * ENV_YRANGE, 0.0f, 1.0f );
    ap[5] = D3DXVECTOR4( ENV_XMAX - fRelease / fTotal * fSustain * ENV_XRANGE, ENV_YMAX - fSustain * ENV_YRANGE, 0.0f, 1.0f );
    ap[6] = D3DXVECTOR4( ENV_XMAX, ENV_YMAX, 0.0f, 1.0f );

    // Draw the graph
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xFFFFFF00 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, 6, ap, sizeof( D3DXVECTOR4 ) );

    return S_OK;
}
