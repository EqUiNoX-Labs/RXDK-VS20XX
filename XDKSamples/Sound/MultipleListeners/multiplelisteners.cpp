//-----------------------------------------------------------------------------
// File: MultipleListeners.cpp
//
// Desc: This sample demonstrates how to use create and manipulate multiple
//          independent virtual 3D listeneters using DirectSound
//
// Hist: 02.10.03 - Demonstrates two algorithms for multiple 'virtual' listeners
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include "dsound.h"
#include "dsstdfx.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display help" },
    { XBHELP_START_BUTTON, XBHELP_PLACEMENT_2, L"Reset positions" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nplayback" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Next sound" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle source/\nlisteners" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"All listeners/\nclosest listener" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Increase\nvolume" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Decrease\nvolume" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Move object in Y" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Move object\nin X/Z" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"(L/R) Change virtual\nlistener" },
    { XBHELP_MISC_CALLOUT, XBHELP_PLACEMENT_1, GLYPH_LEFT_BUTTON GLYPH_RIGHT_BUTTON L": Rotate listener" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );



//-----------------------------------------------------------------------------
// Global variables and definitions
//-----------------------------------------------------------------------------

struct D3DVERTEX
{
    D3DXVECTOR3 p;           // position
    D3DCOLOR    c;           // color
};
#define D3DFVF_D3DVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)


// Constants to define our world space
static const LONG XMIN= -10;
static const LONG XMAX = 10;
static const LONG ZMIN = -10;
static const LONG ZMAX = 10;
static const LONG YMIN = 0;
static const LONG YMAX = 5;

// Number of virtual listeners
static const DWORD MAX_LISTENERS = 4;

// Constants for colors
static const DWORD REAL_SOURCE_COLOR = 0xFFFF0000;
static const DWORD VIRTUAL_SOURCE_COLOR[MAX_LISTENERS] = { 0x80FF8000, 0x80FF0080, 0x8040C000, 0x80008080 };
static const DWORD REAL_LISTENER_COLOR = 0x80808080;
static const DWORD VIRTUAL_LISTENER_COLOR[MAX_LISTENERS] = { 0xFFFF8000, 0xFFFF0080, 0xFF00CF00, 0xFF008080 };

// Constants for scaling input
static const FLOAT MOTION_SCALE = 10.0f;
static const FLOAT VOLUME_SCALE = 5.0f;

// Initial positions of virtual listeners
struct INITIAL_POSITION {
    D3DXVECTOR3 pos;
    FLOAT       angle;
};
static const INITIAL_POSITION g_aInitialPositions[MAX_LISTENERS] =
{
    { D3DXVECTOR3(  6.0f, 0.0f, 0.0f ),  0.0f },
    { D3DXVECTOR3(  8.0f, 0.0f, 0.0f ),  0.0f },
    { D3DXVECTOR3(  0.5f, 0.0f, 6.0f ), -1.5f },
    { D3DXVECTOR3( -6.0f, 0.0f, 0.0f ),  3.1f },
};

// List of wav files to cycle through
const WCHAR* g_strMediaDir = L"Media\\Sounds\\";
WCHAR* g_strFileNames[] = 
{
    L"heli.wav",
    L"DockingMono.wav",
    L"EngineStartMono.wav",
    L"MaleDialog1.wav",
    L"MiningMono.wav",
    L"MusicMono.wav",
    L"Dolphin4.wav",
};
const DWORD NUM_SOUNDS = sizeof( g_strFileNames ) / sizeof( g_strFileNames[0] );

static const FLOAT DOPPLER_FACTOR = 2.0f;


//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont                 m_Font;                 // Font object
    CXBHelp                 m_Help;                 // Help object

    // Sound members
    CWaveFile               m_awfSounds[NUM_SOUNDS];// Wave file parsers
    DWORD                   m_dwCurrent;            // Current sound
    BOOL                    m_bPlaying;             // Are we playing?
    LONG                    m_lVolume;              // Current volume
    LPDIRECTSOUND8          m_pDSound;              // DirectSound object
    LPDIRECTSOUNDBUFFER8    m_pDSBuffer[MAX_LISTENERS];            // DirectSoundBuffer
    BYTE *                  m_pbSampleData;         // Sample data from wav
    BOOL                    m_bClosestListener;     // Which algorithm to use

    // Sound source and listener positions
    D3DXVECTOR3             m_vRealSourcePosition;                      // Real source position vector
    D3DXVECTOR3             m_vVirtualSourcePosition[MAX_LISTENERS];    // Position of virtual source (relative to real listener)
    FLOAT                   m_fRealListenerAngle;                       // Real Listener orientation angle in x-z
    FLOAT                   m_fVirtualListenerAngle[MAX_LISTENERS];     // virtual Listener orientation angle 
    D3DXVECTOR3             m_vVirtualListenerPosition[MAX_LISTENERS];  // virtual Listener position vector

    // Matrices used to transform from each virtual listener's
    // coordinate space (determined by their position and orientation) to
    // the real (aka DirectSound) listener's space (based at origin)
    D3DXMATRIX              m_matVirtualListenerToDSoundListener[MAX_LISTENERS];

    // Models for floor, source, and listener
    LPDIRECT3DVERTEXBUFFER8 m_pvbFloor;             // Quad for the floor
    LPDIRECT3DVERTEXBUFFER8 m_pvbSource;            // Quad for the sources
    LPDIRECT3DVERTEXBUFFER8 m_pvbListener;          // Triangle for the listeners
    LPDIRECT3DVERTEXBUFFER8 m_pvbGrid;              // Lines to grid the floor

    D3DCOLOR        m_cSource;                          // Color for sound source
    D3DCOLOR        m_cVirtualListener[MAX_LISTENERS];  // Color for virtual listeners

    BOOL            m_bDrawHelp;                    // Should we draw help?
    BOOL            m_bControlSource;               // Control source (TRUE) or
                                                    // listener (FALSE)
    DWORD           m_dwVirtualListenerID;          // Which virtual listener to control
    DWORD           m_dwClosestVirtualListenerID;   // Which virtual listener is closest to sound source

    HRESULT SwitchToSound( DWORD dwIndex );         // Sets up a different sound

public:
    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();

    HRESULT TogglePlayback( BOOL bPlay );
    HRESULT ResetPositions();

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
// Name: BuildTransform
// Desc: Creates a transform matrix by compositing a translation and
//          a rotation and then takes the inverse
//-----------------------------------------------------------------------------
VOID BuildTransform( D3DXVECTOR3 translate, 
                     FLOAT fYaw, 
                     FLOAT fPitch,
                     FLOAT fRoll,
                     D3DXMATRIX* pmatTransform )
{
    D3DXMATRIX matTranslate;
    D3DXMATRIX matRotate;

    // We're basically creating the inverse of the transformation we'd use
    // to position our virtual listeners in space.
    D3DXMatrixTranslation( &matTranslate, translate.x, translate.y, translate.z );
    D3DXMatrixRotationYawPitchRoll( &matRotate, fYaw, fPitch, fRoll );
    D3DXMatrixMultiply( pmatTransform, &matRotate, &matTranslate );
    D3DXMatrixInverse( pmatTransform, NULL, pmatTransform );
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    m_bDrawHelp = FALSE;

    // Sounds
    m_lVolume = DSBVOLUME_MAX;
    m_pbSampleData = NULL;
    m_bClosestListener = FALSE;

    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}



//-----------------------------------------------------------------------------
// Name: TogglePlayback
// Desc: Toggles playback of all buffers simultaneously
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::TogglePlayback( BOOL bPlay )
{
    if( bPlay )
    {
        // When we start playback, we use the DSBPLAY_SYNCHPLAYBACK flag to 
        // ensure that all of the buffers start playback at exactly the same
        // time.  Otherwise, you could end up with a flange effect if they
        // were off by a couple samples.  That's also why we start playback
        // from the beginning, since we can't synchronize the Stop() calls
        for( DWORD i = 0; i < MAX_LISTENERS; i++ )
        {
            m_pDSBuffer[i]->Play( 0, 0, DSBPLAY_FROMSTART | 
                                        DSBPLAY_LOOPING | 
                                        DSBPLAY_SYNCHPLAYBACK );
        }
        m_pDSound->SynchPlayback();
    }
    else
    {
        for( DWORD i = 0; i < MAX_LISTENERS; i++ )
        {
            m_pDSBuffer[i]->Stop();
        }
    }
    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: ResetPositions
// Desc: Resets the positions of the source and virtual listeners to 
//          initial, default positions
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::ResetPositions()
{
    // Initialize our virtual listeners - we spread their initial positions 
    // out across the X axis
    m_dwVirtualListenerID = 0;
    for( DWORD k = 0; k < MAX_LISTENERS; k++ )
    {
        m_vVirtualListenerPosition[k] = g_aInitialPositions[k].pos;
        m_fVirtualListenerAngle[k] = g_aInitialPositions[k].angle;
        BuildTransform( m_vVirtualListenerPosition[k],
                        m_fVirtualListenerAngle[k],
                        0.0f,
                        0.0f,
                        &m_matVirtualListenerToDSoundListener[k] );
    }

    // Initial real source position
    m_vRealSourcePosition   = D3DXVECTOR3( 0.0f, 0.0f, (FLOAT)ZMAX );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the sample
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create DirectSound
    if( FAILED( DirectSoundCreate( NULL, &m_pDSound, NULL ) ) )
        return E_FAIL;

    // There are 2 options for 3-D sound processing:
    // 1) DirectSoundUseFullHRTF - full hardware HRTF-based processing
    // 2) DirectSoundUseLightHRTF - hardware HRTF processing, but without
    //      any vertical component (azimuth only).  Saves ~60k of memory
    DirectSoundUseFullHRTF();

    // Exaggerate the doppler effect for demonstration purposes.
    m_pDSound->SetDopplerFactor( DOPPLER_FACTOR, DS3D_IMMEDIATE );
    
    // download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc = { GraphI3DL2_I3DL2Reverb, GraphXTalk_XTalk };
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
    DSBUFFERDESC dsbdesc    = {0};
    dsbdesc.dwSize          = sizeof( DSBUFFERDESC );
    dsbdesc.dwFlags         = DSBCAPS_CTRL3D;
    dsbdesc.dwBufferBytes   = 0;
    dsbdesc.lpwfxFormat     = (WAVEFORMATEX*)&wfx;

    // Since we have one virtual source for each virtual listener,
    // we need to create one buffer foreach virtual source
    for( DWORD k = 0; k < MAX_LISTENERS; k++ )
    {
        if( FAILED( m_pDSound->CreateSoundBuffer( &dsbdesc, &m_pDSBuffer[k], NULL ) ) )
            return E_FAIL;
    }

    // Set our initial transform matrices
    D3DXMATRIX matWorld;
    D3DXMatrixIdentity( &matWorld );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    D3DXMATRIX matView;
    D3DXVECTOR3 vEyePt      = D3DXVECTOR3( (FLOAT)XMIN, 45.0f,  ZMAX / 2.0f );
    D3DXVECTOR3 vLookatPt   = D3DXVECTOR3( (FLOAT)XMIN,  0.0f,  ZMAX / 2.0f );
    D3DXVECTOR3 vUpVec      = D3DXVECTOR3( 0.0f,  0.0f,  1.0f );
    D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 4.0f/3.0f, 1.0f, 10000.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Create our vertex buffers
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbFloor );
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbSource );
    m_pd3dDevice->CreateVertexBuffer( 3 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbListener );
    DWORD dwNumGridLines = ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 );
    m_pd3dDevice->CreateVertexBuffer( 2 * dwNumGridLines * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbGrid );
    
    D3DVERTEX * pVertices;

    // Fill the VB for the source object
    m_pvbSource->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( -0.5f, 0.0f, -0.5f );
    pVertices[1].p = D3DXVECTOR3( -0.5f, 0.0f,  0.5f );
    pVertices[2].p = D3DXVECTOR3(  0.5f, 0.0f, -0.5f );
    pVertices[3].p = D3DXVECTOR3(  0.5f, 0.0f,  0.5f );
    m_pvbSource->Unlock();

    // Fill the VB for the listener object
    m_pvbListener->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( -0.5f, 0.0f, -1.0f );
    pVertices[1].p = D3DXVECTOR3(  0.0f, 0.0f,  1.0f );
    pVertices[2].p = D3DXVECTOR3(  0.5f, 0.0f, -1.0f );
    m_pvbListener->Unlock();

    // Fill the VB for the floor
    m_pvbFloor->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( (FLOAT)XMIN, -0.1f, (FLOAT)ZMIN );
    pVertices[0].c = 0x80101010;
    pVertices[1].p = D3DXVECTOR3( (FLOAT)XMIN, -0.1f, (FLOAT)ZMAX ); 
    pVertices[1].c = 0x80101010;
    pVertices[2].p = D3DXVECTOR3( (FLOAT)XMAX, -0.1f, (FLOAT)ZMIN ); 
    pVertices[2].c = 0x80101010;
    pVertices[3].p = D3DXVECTOR3( (FLOAT)XMAX, -0.1f, (FLOAT)ZMAX ); 
    pVertices[3].c = 0x80101010;
    m_pvbFloor->Unlock();

    // Fill the VB for the grid
    LONG  line;
    DWORD vert = 0;
    m_pvbGrid->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    for( line = ZMIN; line <= ZMAX; line++, vert++ )
    {
        pVertices[ vert * 2 ].p     = D3DXVECTOR3( (FLOAT)XMIN, 0, (FLOAT)line ); 
        pVertices[ vert * 2 ].c     = 0xFF00A000;
        pVertices[ vert * 2 + 1 ].p = D3DXVECTOR3( (FLOAT)XMAX, 0, (FLOAT)line ); 
        pVertices[ vert * 2 + 1 ].c = 0xFF00A000;
    }
    for( line = XMIN; line <= XMAX; line++, vert++ )
    {
        pVertices[ vert * 2 ].p     = D3DXVECTOR3( (FLOAT)line, 0, (FLOAT)ZMIN ); 
        pVertices[ vert * 2 ].c     = 0xFF00A000;
        pVertices[ vert * 2 + 1 ].p = D3DXVECTOR3( (FLOAT)line, 0, (FLOAT)ZMAX ); 
        pVertices[ vert * 2 + 1 ].c = 0xFF00A000;
    }
    m_pvbGrid->Unlock();

    // Set up and play our initial sound
    ResetPositions();
    m_dwCurrent = 0;
    SwitchToSound( m_dwCurrent );
    m_bPlaying = TRUE;

    // Start playing each of our virtual sound sources
    TogglePlayback( TRUE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SwitchToSound
// Desc: Switches to the given sound by:
//       1) Stop playback if we're playing
//       2) Reallocate the sample data buffer
//       3) Point the DirectSoundBuffer to the new data
//       4) Restart playback if needed
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SwitchToSound( DWORD dwIndex )
{
    // If we're currently playing, stop, so that we don't crash
    // when we reallocate our buffer
    TogglePlayback( FALSE );

    // Load the wave format from the file
    WAVEFORMATEXTENSIBLE wfx;
    if( FAILED( m_awfSounds[ dwIndex ].GetFormat( &wfx ) ) )
        return E_FAIL;

    // Calling stop doesn't immediately shut down
    // the voice, so point it away from our buffer
    for( DWORD k = 0; k < MAX_LISTENERS; k++ )
    {
        m_pDSBuffer[k]->SetBufferData( NULL, 0 );
        m_pDSBuffer[k]->SetFormat( (WAVEFORMATEX *)&wfx );
    }

    // Find out how big the new sample is
    DWORD dwNewSize;
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

    // Map each of our new buffers
    for( DWORD k = 0; k < MAX_LISTENERS; k++ )
    {
        // Set up values for the new buffer
         m_pDSBuffer[k]->SetBufferData( m_pbSampleData, dwNewSize );
         m_pDSBuffer[k]->SetLoopRegion( dwLoopStart, dwLoopLength );
    }

    // If we were playing before, restart playback now
    if( m_bPlaying )
    {
        TogglePlayback( TRUE );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    D3DXVECTOR3     vRealSourceOld   = m_vRealSourcePosition;
    D3DXVECTOR3     vVirtualListenerOld[MAX_LISTENERS];
    D3DXVECTOR3*    pvControl;
    DWORD           dwPulse = DWORD( ( cosf( m_fAppTime * 6.0f ) + 1.0f ) * 3.5 );

    // Blend in 0-7 bits of pure white to highlight selected object
    D3DCOLOR        cBlend = 0;
    for( ; dwPulse > 0; dwPulse-- )
    {
        cBlend |= 0x01010101;
        cBlend <<= 1;
    }

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Increase/Decrease volume
    m_lVolume += LONG( ( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_BLACK ] - 
                   m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_WHITE ] ) *
                   m_fElapsedTime * 
                   VOLUME_SCALE );

    // Make sure volume is in the appropriate range
    if( m_lVolume < DSBVOLUME_MIN )
        m_lVolume = DSBVOLUME_MIN;
    else if( m_lVolume > DSBVOLUME_MAX )
        m_lVolume = DSBVOLUME_MAX;

    // Toggle sound on and off
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        m_bPlaying = !m_bPlaying;
        TogglePlayback( m_bPlaying );
    }

    // Cycle through sounds
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
    {
        m_dwCurrent = ( m_dwCurrent + 1 ) % NUM_SOUNDS;
        SwitchToSound( m_dwCurrent );
    }

    // Reset positions
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START )
    {
        ResetPositions();
    }

    // Switch which of source vs. listener we are moving
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] )
    {
        m_bControlSource = !m_bControlSource;
    }

    // Toggle closest-listener vs. all-listener.  To make things easy,
    // we just mute our extra buffers by setting DSBHEADROOM_MAX headroom
    // on them.  Normally, a game wouldn't switch modes at runtime, and
    // so wouldn't have created the extra buffers in the first place
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] )
    {
        m_bClosestListener = !m_bClosestListener;
        for( DWORD k = 1; k < MAX_LISTENERS; k++ )
        {
            m_pDSBuffer[k]->SetHeadroom( m_bClosestListener ? DSBHEADROOM_MAX : DSBHEADROOM_MIN );
        }
    }

    // Change which listener we're moving (assuming we're not moving source)
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        m_dwVirtualListenerID = ( m_dwVirtualListenerID + 1 ) % MAX_LISTENERS;
    else if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        m_dwVirtualListenerID = ( m_dwVirtualListenerID + MAX_LISTENERS - 1) % MAX_LISTENERS;

    // Set up our colors
    m_cSource   = REAL_SOURCE_COLOR   | (  m_bControlSource ? cBlend : 0 );
    for( DWORD k = 0; k < MAX_LISTENERS; k++ )
    {
        m_cVirtualListener[k] = VIRTUAL_LISTENER_COLOR[k];
        if( k == m_dwVirtualListenerID && !m_bControlSource )
            m_cVirtualListener[k] |= cBlend;
    }

    // Grab all the old virtual listener positions so we can calculate
    // velocity for them later
    for( DWORD k = 0; k < MAX_LISTENERS; k++ )
    {
        vVirtualListenerOld[k] = m_vVirtualListenerPosition[k];
    }

    // Point to the appropriate vector
    pvControl = m_bControlSource ? &m_vRealSourcePosition : &m_vVirtualListenerPosition[m_dwVirtualListenerID];

    // Move selected object and clamp to the appropriate range
    pvControl->x += m_DefaultGamepad.fX1 * m_fElapsedTime * MOTION_SCALE;
    if( pvControl->x < XMIN )
        pvControl->x = XMIN;
    else if( pvControl->x > XMAX )
        pvControl->x = XMAX;

    pvControl->z += m_DefaultGamepad.fY1 * m_fElapsedTime * MOTION_SCALE;
    if( pvControl->z < ZMIN )
        pvControl->z = ZMIN;
    else if( pvControl->z > ZMAX )
        pvControl->z = ZMAX;

    pvControl->y += m_DefaultGamepad.fY2 * m_fElapsedTime * MOTION_SCALE;
    if( pvControl->y < YMIN )
        pvControl->y = YMIN;
    else if( pvControl->y > YMAX )
        pvControl->y = YMAX;

#if 1
    // Update the listener rotation from the analog triggers.
    FLOAT fDelta = m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_RIGHT_TRIGGER ];
    fDelta -= m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_LEFT_TRIGGER ];
    fDelta *= m_fElapsedTime / 100;
    m_fVirtualListenerAngle[ m_dwVirtualListenerID ] += fDelta;
    if( fDelta > 360.0f )
    {
        fDelta -= 360.0f;
    }
    else if( fDelta < 0.0f )
    {
        fDelta += 360.0f;
    }
    BuildTransform( m_vVirtualListenerPosition[m_dwVirtualListenerID],
                    m_fVirtualListenerAngle[m_dwVirtualListenerID],
                    0.0f,
                    0.0f,
                    &m_matVirtualListenerToDSoundListener[m_dwVirtualListenerID] );
#else
    // This sample only handles rotating the listener about the Y axis.  In your
    // title, you'll probably want to handle full rotation of your virtual
    // listener.  We just base the orientation around the change in position
    // from the last frame
    if( m_vVirtualListenerPosition[m_dwVirtualListenerID] != vVirtualListenerOld[m_dwVirtualListenerID] )
    {
        D3DXVECTOR3 vDelta = m_vVirtualListenerPosition[m_dwVirtualListenerID] - vVirtualListenerOld[m_dwVirtualListenerID];
        m_fVirtualListenerAngle[m_dwVirtualListenerID] = FLOAT( atan2( vDelta.x, vDelta.z ) );
        BuildTransform( m_vVirtualListenerPosition[m_dwVirtualListenerID],
                        m_fVirtualListenerAngle[m_dwVirtualListenerID],
                        0.0f,
                        0.0f,
                        &m_matVirtualListenerToDSoundListener[m_dwVirtualListenerID] );
    }
#endif // 0

    // Calculate the position of each virtual source by transforming the 
    // position of the real source into the virtual listener's space
    float fSmallestDistance = DS3D_DEFAULTMAXDISTANCE;
    for( DWORD k = 0; k < MAX_LISTENERS; k++ )
    {
        // transform source position (relative to actual listener)
        D3DXVec3TransformCoord( &m_vVirtualSourcePosition[k],
                                &m_vRealSourcePosition,
                                &m_matVirtualListenerToDSoundListener[k] );

        FLOAT fDistance = D3DXVec3Length( &m_vVirtualSourcePosition[k] );
        if( fDistance < fSmallestDistance )
        {
            fSmallestDistance = fDistance;
            m_dwClosestVirtualListenerID = k;
        }

        m_pDSBuffer[k]->SetVolume( m_lVolume );
    }

    // Calculate the velocity of the real source.  Velocity of each virtual
    // source is dependent on virtual listener position and velocity.
    D3DXVECTOR3 vRealSourceVelocity = ( m_vRealSourcePosition - vRealSourceOld ) / m_fElapsedTime;

    // If we're in closest-listener mode, we just set the position and
    // velocity of one buffer according to whichever virtual listener
    // was closest to the source
    if( m_bClosestListener )
    {
        D3DXVECTOR3* pPos       = &m_vVirtualSourcePosition[ m_dwClosestVirtualListenerID ];

        D3DXVECTOR3  vVirtualListenerVelocity = m_vVirtualListenerPosition[ m_dwClosestVirtualListenerID ] -
                                                vVirtualListenerOld[ m_dwClosestVirtualListenerID ];
        D3DXVECTOR3  vVirtualSourceVelocity   = vRealSourceVelocity - vVirtualListenerVelocity;

        m_pDSBuffer[0]->SetPosition( pPos->x, pPos->y, pPos->z, DS3D_DEFERRED );
        m_pDSBuffer[0]->SetVelocity( vVirtualSourceVelocity.x, vVirtualSourceVelocity.y, vVirtualSourceVelocity.z, DS3D_DEFERRED );
    }
    else
    {
        // In all-listener mode, we set the position and velocity of each
        // buffer according to its corresponding virtual source
        for( DWORD k = 0; k < MAX_LISTENERS; k++ )
        {
            D3DXVECTOR3 vVirtualListenerVelocity = ( m_vVirtualListenerPosition[k] - vVirtualListenerOld [k]) / m_fElapsedTime;

            // Source position/velocity/volume - note listener position/velocity remain unchanged
            m_pDSBuffer[k]->SetPosition( m_vVirtualSourcePosition[k].x, m_vVirtualSourcePosition[k].y, m_vVirtualSourcePosition[k].z, DS3D_DEFERRED );

            D3DXVECTOR3 vVirtualSourceVelocity = vRealSourceVelocity - vVirtualListenerVelocity;
            m_pDSBuffer[k]->SetVelocity( vVirtualSourceVelocity.x, vVirtualSourceVelocity.y, vVirtualSourceVelocity.z, DS3D_DEFERRED );
        }
    }

    // Commit position/velocity changes
    m_pDSound->CommitDeferredSettings();

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
    // Pump DirectSound's work queue
    DirectSoundDoWork();

    // Clear the viewport
    RenderGradientBackground( 0xFF408040, 0xFF404040 );

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    m_pd3dDevice->SetVertexShader( D3DFVF_D3DVERTEX );

    // Draw the floor
    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matIdentity );
    m_pd3dDevice->SetStreamSource( 0, m_pvbFloor, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
    
    // Draw the grid
    m_pd3dDevice->SetStreamSource( 0, m_pvbGrid, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_LINELIST, 0, 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) );

    // Set color op to read from TFACTOR for drawing all our source
    // and listener objects
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR );

    // Draw the real source
    D3DXMATRIX matSource;
    D3DXMatrixTranslation( &matSource, 
                            m_vRealSourcePosition.x,
                            m_vRealSourcePosition.y,
                            m_vRealSourcePosition.z );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matSource );
    m_pd3dDevice->SetStreamSource( 0, m_pvbSource, sizeof( D3DVERTEX ) );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, m_cSource );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Draw the real listener
    D3DXMATRIX matListener;
    D3DXMatrixIdentity( &matListener );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matListener );
    m_pd3dDevice->SetStreamSource( 0, m_pvbListener, sizeof( D3DVERTEX ) );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, REAL_LISTENER_COLOR );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 1 );

    // Draw the virtual listeners
    for( DWORD k = 0; k < MAX_LISTENERS; k++ )
    {
        D3DXMATRIX matListener;
        D3DXMATRIX mat;

        D3DXMatrixTranslation( &matListener, m_vVirtualListenerPosition[k].x, m_vVirtualListenerPosition[k].y, m_vVirtualListenerPosition[k].z );
        D3DXMatrixRotationY( &mat, m_fVirtualListenerAngle[k] );
        D3DXMatrixMultiply( &matListener, &mat, &matListener );
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &matListener );
        m_pd3dDevice->SetStreamSource( 0, m_pvbListener, sizeof( D3DVERTEX ) );
        m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, m_cVirtualListener[k] );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 1 );

    }

    // Draw the virtual sources.  If we're in closest-listener mode,
    // then we only draw the virtual source corresponding to the closest
    // virtual listener
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    for( DWORD k = 0; k < MAX_LISTENERS; k++ )
    {
        if( !m_bClosestListener || k == m_dwClosestVirtualListenerID )
        {
            D3DXMATRIX matSource;
            D3DXMatrixTranslation( &matSource, 
                                m_vVirtualSourcePosition[k].x,
                                m_vVirtualSourcePosition[k].y,
                                m_vVirtualSourcePosition[k].z );
            m_pd3dDevice->SetTransform( D3DTS_WORLD, &matSource );
            m_pd3dDevice->SetStreamSource( 0, m_pvbSource, sizeof( D3DVERTEX ) );
            m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, VIRTUAL_SOURCE_COLOR[k] );
            m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
        }
    }

    // Draw matching lines to help visualize:
    // one line from current virtual listener to real source
    // and one line from real listener to corresponding virtual source
    DWORD dwListener = m_bClosestListener ? m_dwClosestVirtualListenerID : m_dwVirtualListenerID;
    D3DVERTEX vLines[4];
    vLines[0].p = m_vVirtualListenerPosition[ dwListener ];
    vLines[1].p = m_vRealSourcePosition;
    vLines[2].p = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    vLines[3].p = m_vVirtualSourcePosition[ dwListener ];
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matIdentity );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, m_cVirtualListener[ dwListener ] );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 2, vLines, sizeof( D3DVERTEX ) );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        WCHAR szBuff[200];

        m_Font.Begin();
        // Show frame rate
        m_Font.DrawText(  64, 50, 0xffffffff, L"Multiple Virtual Listener 3DSound" );
        m_Font.DrawText( 450, 50, 0xffffff00, m_strFrameRate );

        // Show status
        swprintf( szBuff, L"Current Sound: %s", g_strFileNames[ m_dwCurrent ] );
        m_Font.DrawText( 64, 100, m_bPlaying ? 0xFFFFFFFF : 0xFF404040, szBuff );
        swprintf( szBuff, 
                  L"Source: <%0.1f, %0.1f, %0.1f>", 
                  m_vRealSourcePosition.x, 
                  m_vRealSourcePosition.y, 
                  m_vRealSourcePosition.z );
        m_Font.DrawText( 64, 130, m_cSource, szBuff );
        swprintf( szBuff, 
                  L"Listener %i: <%0.1f, %0.1f, %0.1f>", 
                  m_dwVirtualListenerID, 
                  m_vVirtualListenerPosition[m_dwVirtualListenerID].x, 
                  m_vVirtualListenerPosition[m_dwVirtualListenerID].y, 
                  m_vVirtualListenerPosition[m_dwVirtualListenerID].z );
        m_Font.DrawText( 64, 160, m_cVirtualListener[m_dwVirtualListenerID], szBuff );

        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = (FLOAT)pow( 10.f, ((FLOAT) m_lVolume) / 2000.0f ) * 100.f;
        swprintf( szBuff, L"Volume: %ddB (%0.0f%%)", ( m_lVolume - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 190, 0xFFFFFF00, szBuff );

        if( m_bClosestListener )
        {
            swprintf( szBuff, L"Closest listener (%d)", m_dwClosestVirtualListenerID );
            m_Font.DrawText( 64, 340, m_cVirtualListener[ m_dwClosestVirtualListenerID ], szBuff );
        }
        else
            m_Font.DrawText( 64, 340, 0xFF888800, L"All listeners");

        m_Font.End();

        // Draw the on-screen audio level meters
        XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

