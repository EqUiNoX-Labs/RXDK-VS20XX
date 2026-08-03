//-----------------------------------------------------------------------------
// File: MultiPass.cpp
//
// Desc: This sample demonstrates how to use the multipass audio processing
//       capabilities of DirectSound on the Xbox.  Several standard DirectSound
//       buffers are created, and are set to output to another 3D Buffer, which
//       is then positioned.
//
// Hist: 05.10.01 - New for June Release
//       03.06.02 - Added audio level meters for April 02 XDK release
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
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nsound" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Change\nsound" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle source/\nlistener" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nheadphones" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Increase\nvolume" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Decrease\nvolume" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Move object\nin Y" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Move object\nin X/Z" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);




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
#define XMIN -10
#define XMAX 10
#define ZMIN -10
#define ZMAX 10
#define YMIN 0
#define YMAX 5

// Constants for colors
static const DWORD SOURCE_COLOR   = 0xFFEA1B1B;
static const DWORD LISTENER_COLOR = 0xFF1B1BEA;

// Constants for scaling input
#define MOTION_SCALE 10.0f
#define VOLUME_SCALE  5.0f

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
    DWORD                   m_dwCurrent;            // Current sound
    LPDIRECTSOUND8          m_pDSound;              // DirectSound object
    LPDIRECTSOUNDBUFFER8    m_pDS3DBuffer;          // 3D DirectSoundBuffer

    BOOL                    m_bPlaying[NUM_SOUNDS];     // Is buffer playing?
    FLOAT                   m_fVolume[NUM_SOUNDS];      // Buffer volume
    CWaveFile               m_awfSounds[NUM_SOUNDS];    // Wave file parsers
    LPDIRECTSOUNDBUFFER8    m_apBuffers[NUM_SOUNDS];    // Non-3D Buffers
    BYTE*                   m_pbSampleData[NUM_SOUNDS]; // Sample data from wavs
    BOOL                    m_bHeadphones;              // True if headphones enabled

    // Sound source and listener positions
    D3DXVECTOR3             m_vSourcePosition;      // Source position vector
    D3DXVECTOR3             m_vListenerPosition;    // Listener position vector

    // Transform matrices
    D3DXMATRIX              m_matWorld;             // World transform
    D3DXMATRIX              m_matView;              // View transform
    D3DXMATRIX              m_matProj;              // Projection transform

    // Models for floor, source, and listener
    LPDIRECT3DVERTEXBUFFER8 m_pvbFloor;             // Quad for the floor
    LPDIRECT3DVERTEXBUFFER8 m_pvbSource;            // Quad for the source
    LPDIRECT3DVERTEXBUFFER8 m_pvbListener;          // Quad for the listener
    LPDIRECT3DVERTEXBUFFER8 m_pvbGrid;              // Lines to grid the floor

    D3DCOLOR        m_dwSourceColor;                // Color for sound source
    D3DCOLOR        m_dwListenerColor;              // Color for listener

    BOOL            m_bDrawHelp;                    // Should we draw help?
    BOOL            m_bControlSource;               // Control source (TRUE) or
                                                    // listener (FALSE)

    HRESULT LoadSounds();                           // Loads list of wav files

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
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    m_bDrawHelp   = FALSE;
    m_bHeadphones = FALSE;

    // Set up sounds
    for( int i = 0; i < NUM_SOUNDS; i++ )
    {
        m_fVolume[i]      = DSBVOLUME_MAX;
        m_pbSampleData[i] = NULL;
        m_apBuffers[i]    = NULL;
    }


    // Positions
    m_vSourcePosition   = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_vListenerPosition = D3DXVECTOR3( 0.0f, 0.0f, ZMIN );
}




//-----------------------------------------------------------------------------
// Name: LoadSounds()
// Desc: Creates a DirectSound Buffer for each wav file and reads the sample
//       data from the wav file.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::LoadSounds()
{
    // Create buffers for each of our sound effects and load
    // the sample data from the wav files
    for( int i = 0; i < NUM_SOUNDS; i++ )
    {
        WAVEFORMATEXTENSIBLE wfx;

        char strFullPath[MAX_PATH];
        sprintf( strFullPath, "d:\\%S%S", g_strMediaDir, g_strFileNames[i] );
        if( FAILED( m_awfSounds[i].Open( strFullPath ) ) )
            return XBAPPERR_MEDIANOTFOUND;

        // Allocate space for the wave format
        if( FAILED( m_awfSounds[i].GetFormat( &wfx ) ) )
            return E_FAIL;

        // Create a sound buffer of 0 size, since we're going to use
        // SetBufferData
        DSBUFFERDESC dsbdesc = {0};
        dsbdesc.dwSize = sizeof( DSBUFFERDESC );
        
        // If fewer than 256 buffers are in existence at all points during 
        // the game, it may be more efficient not to use LOCDEFER.
        // dsbdesc.dwFlags = 0;
        dsbdesc.dwFlags = DSBCAPS_LOCDEFER;
        dsbdesc.dwBufferBytes = 0;
        dsbdesc.lpwfxFormat = (WAVEFORMATEX*)&wfx;

        if( FAILED( m_pDSound->CreateSoundBuffer( &dsbdesc, &m_apBuffers[i], NULL ) ) )
            return E_FAIL;

        // Find out how big the sample is
        DWORD dwDuration;
        m_awfSounds[i].GetDuration( &dwDuration );

        // Allocate a buffer for this sound
        m_pbSampleData[i] = (BYTE *)XPhysicalAlloc( dwDuration, MAXULONG_PTR, 0, PAGE_READWRITE | PAGE_NOCACHE );
        if( !m_pbSampleData[i] )
            return E_OUTOFMEMORY;

        // Read sample data from the file
        m_awfSounds[i].ReadSample( 0, m_pbSampleData[i], dwDuration, &dwDuration );

        // Check for embedded loop points
        DWORD dwLoopStartSample;;
        DWORD dwLoopLengthSamples;
        DWORD cbLoopStart;
        DWORD cbLoopLength;
        if( SUCCEEDED( m_awfSounds[i].GetLoopRegion( &dwLoopStartSample, &dwLoopLengthSamples ) ) )
        {
            // We need to convert the loop points from sample counts to
            // byte offsets, but it's slightly different between PCM and ADPCM
            if( wfx.Format.wFormatTag == WAVE_FORMAT_XBOX_ADPCM ||
                ( wfx.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && 
                wfx.SubFormat == KSDATAFORMAT_SUBTYPE_XBOX_ADPCM ) )
            {
                // For ADPCM, calculate # of blocks and multiply that
                // by bytes per block.  Xbox ADPCM is always 64 samples
                // per block.
                cbLoopStart = dwLoopStartSample / 64 * wfx.Format.nBlockAlign;
                cbLoopLength = dwLoopLengthSamples / 64 * wfx.Format.nBlockAlign;
            }
            else
            {
                // For PCM, multiply by bytes per sample
                DWORD cbBytesPerSample = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
                cbLoopStart = dwLoopStartSample * cbBytesPerSample;
                cbLoopLength = dwLoopLengthSamples * cbBytesPerSample;
            }
        }
        else
        {
            // Otherwise, just loop the whole file
            cbLoopStart = 0;
            cbLoopLength = dwDuration;
        }

        // Set up values for the new buffer
        m_apBuffers[i]->SetBufferData( m_pbSampleData[i], dwDuration );
        m_apBuffers[i]->SetLoopRegion( cbLoopStart, cbLoopLength );
        m_apBuffers[i]->SetCurrentPosition( 0 );

        // Set initial values for the buffer
        m_bPlaying[i] = FALSE;
        m_fVolume[i] = DSBVOLUME_MAX;
        m_apBuffers[i]->SetVolume( (LONG)m_fVolume[i] );
        m_apBuffers[i]->SetOutputBuffer( m_pDS3DBuffer );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the app
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

    // If the application doesn't care about vertical HRTF positioning,
    // calling DirectSoundUseLightHRTF can save about 60k of memory.
    // DirectSoundUseLightHRTF();
    DirectSoundUseFullHRTF();

    // download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\media\\dsstdfx.bin", 
                                            &EffectLoc, 
                                            XAUDIO_DOWNLOADFX_EXTERNFILE, 
                                            NULL ) ) )
        return E_FAIL;

    // Set up 3d Buffer desc
    DSBUFFERDESC dsbd;
    ZeroMemory( &dsbd, sizeof( DSBUFFERDESC ) );
    dsbd.dwSize = sizeof( DSBUFFERDESC );
    dsbd.dwFlags = DSBCAPS_MIXIN | DSBCAPS_CTRL3D;
    dsbd.lpwfxFormat = NULL;
    if( FAILED( m_pDSound->CreateSoundBuffer( &dsbd, &m_pDS3DBuffer, NULL ) ) )
        return E_FAIL;

    // Create and initialize our individual sound buffers
    if( FAILED( LoadSounds() ) )
        return E_FAIL;

    // Set the transform matrices
    D3DXVECTOR3 vEyePt      = D3DXVECTOR3( XMIN, 45.0f,  ZMAX / 2.0f );
    D3DXVECTOR3 vLookatPt   = D3DXVECTOR3( XMIN,  0.0f,  ZMAX / 2.0f );
    D3DXVECTOR3 vUpVec      = D3DXVECTOR3( 0.0f,  0.0f,  1.0f );
    D3DXMatrixIdentity( &m_matWorld );
    D3DXMatrixLookAtLH( &m_matView, &vEyePt, &vLookatPt, &vUpVec );
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/4, 4.0f/3.0f, 1.0f, 10000.0f );

    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Create our vertex buffers
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbFloor );
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbSource );
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbListener );
    m_pd3dDevice->CreateVertexBuffer( 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbGrid );
    
    D3DVERTEX * pVertices;

    // Fill the VB for the floor
    m_pvbFloor->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( XMIN, 0.0f, ZMIN ); pVertices[0].c = 0xFF101010;
    pVertices[1].p = D3DXVECTOR3( XMIN, 0.0f, ZMAX ); pVertices[1].c = 0xFF101010;
    pVertices[2].p = D3DXVECTOR3( XMAX, 0.0f, ZMIN ); pVertices[2].c = 0xFF101010;
    pVertices[3].p = D3DXVECTOR3( XMAX, 0.0f, ZMAX ); pVertices[3].c = 0xFF101010;
    m_pvbFloor->Unlock();

    // Fill the VB for the grid
    m_pvbGrid->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    for( int i = ZMIN, j = 0; i <= ZMAX; i++, j++ )
    {
        pVertices[ j * 2 ].p     = D3DXVECTOR3( XMIN, 0, (FLOAT)i ); pVertices[ j * 2 ].c     = 0xFF00A000;
        pVertices[ j * 2 + 1 ].p = D3DXVECTOR3( XMAX, 0, (FLOAT)i ); pVertices[ j * 2 + 1 ].c = 0xFF00A000;
    }
    for( i = XMIN; i <= XMAX; i++, j++ )
    {
        pVertices[ j * 2 ].p     = D3DXVECTOR3( (FLOAT)i, 0, ZMIN ); pVertices[ j * 2 ].c     = 0xFF00A000;
        pVertices[ j * 2 + 1 ].p = D3DXVECTOR3( (FLOAT)i, 0, ZMAX ); pVertices[ j * 2 + 1 ].c = 0xFF00A000;
    }
    m_pvbGrid->Unlock();

    // Set up and play our initial sound
    m_dwCurrent = 0;
    m_bPlaying[0] = TRUE;
    m_apBuffers[0]->Play( 0, 0, DSBPLAY_LOOPING );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    D3DVERTEX*   pVertices;
    D3DXVECTOR3  vSourceOld   = m_vSourcePosition;
    D3DXVECTOR3  vListenerOld = m_vListenerPosition;
    D3DXVECTOR3* pvControl;
    DWORD        dwPulse = DWORD( ( cosf( m_fAppTime * 6.0f ) + 1.0f ) * 50 );
    D3DCOLOR     cBlend = dwPulse | ( dwPulse << 8 ) | ( dwPulse << 16 );

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Increase/Decrease volume
    m_fVolume[m_dwCurrent] += ( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_WHITE ] - 
                                m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_BLACK ] ) *
                              m_fElapsedTime * VOLUME_SCALE;

    // Make sure volume is in the appropriate range
    if( m_fVolume[m_dwCurrent] < DSBVOLUME_MIN )
        m_fVolume[m_dwCurrent] = DSBVOLUME_MIN;
    else if( m_fVolume[m_dwCurrent] > DSBVOLUME_MAX )
        m_fVolume[m_dwCurrent] = DSBVOLUME_MAX;

    // Toggle sound on and off
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        if( m_bPlaying[m_dwCurrent] )
            m_apBuffers[m_dwCurrent]->Stop( );
        else
            m_apBuffers[m_dwCurrent]->Play( 0, 0, DSBPLAY_LOOPING );

        m_bPlaying[m_dwCurrent] = !m_bPlaying[m_dwCurrent];
    }

    // Cycle through sounds
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
    {
        m_dwCurrent = ( m_dwCurrent + 1 ) % NUM_SOUNDS;
    }

    // Switch which of source vs. listener we are moving
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] )
    {
        m_bControlSource = !m_bControlSource;
    }

    // Toggle headphones
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] )
    {
        m_bHeadphones = !m_bHeadphones;
        m_pDSound->EnableHeadphones( m_bHeadphones );
    }

    // Set up our colors
    m_dwSourceColor   = SOURCE_COLOR   | (  m_bControlSource ? cBlend : 0 );
    m_dwListenerColor = LISTENER_COLOR | ( !m_bControlSource ? cBlend : 0 );

    // Point to the appropriate vector
    pvControl = m_bControlSource ? &m_vSourcePosition : &m_vListenerPosition;

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

    // Update source/listener vertex buffers
    m_pvbSource->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    pVertices[0].p = m_vSourcePosition + D3DXVECTOR3( -0.5f, 0.0f, -0.5f ); pVertices[0].c = m_dwSourceColor;
    pVertices[1].p = m_vSourcePosition + D3DXVECTOR3( -0.5f, 0.0f,  0.5f ); pVertices[1].c = m_dwSourceColor;
    pVertices[2].p = m_vSourcePosition + D3DXVECTOR3(  0.5f, 0.0f, -0.5f ); pVertices[2].c = m_dwSourceColor;
    pVertices[3].p = m_vSourcePosition + D3DXVECTOR3(  0.5f, 0.0f,  0.5f ); pVertices[3].c = m_dwSourceColor;
    m_pvbSource->Lock( 0, 0, (BYTE **)&pVertices, 0 );

    m_pvbListener->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    pVertices[0].p = m_vListenerPosition + D3DXVECTOR3( -0.5f, 0.0f, -0.5f ); pVertices[0].c = m_dwListenerColor;
    pVertices[1].p = m_vListenerPosition + D3DXVECTOR3( -0.5f, 0.0f,  0.5f ); pVertices[1].c = m_dwListenerColor;
    pVertices[2].p = m_vListenerPosition + D3DXVECTOR3(  0.5f, 0.0f, -0.5f ); pVertices[2].c = m_dwListenerColor;
    pVertices[3].p = m_vListenerPosition + D3DXVECTOR3(  0.5f, 0.0f,  0.5f ); pVertices[3].c = m_dwListenerColor;
    m_pvbListener->Lock( 0, 0, (BYTE **)&pVertices, 0 );

    // Position the sound and listener in 3D. 
    // We use DS3D_DEFERRED so that all the changes will 
    // be committed at once.
    // We scale the velocities by 2 so that doppler effect
    // is a bit more noticeable.
    D3DXVECTOR3 vListenerVelocity = 2.0f * ( m_vListenerPosition - vListenerOld ) / m_fElapsedTime;
    D3DXVECTOR3 vSoundVelocity = 2.0f * ( m_vSourcePosition - vSourceOld ) / m_fElapsedTime;

    // Source position/velocity/volume
    m_pDS3DBuffer->SetPosition( m_vSourcePosition.x, m_vSourcePosition.y, m_vSourcePosition.z, DS3D_DEFERRED );
    m_pDS3DBuffer->SetVelocity( vSoundVelocity.x, vSoundVelocity.y, vSoundVelocity.z, DS3D_DEFERRED );
    m_apBuffers[m_dwCurrent]->SetVolume( (LONG)m_fVolume[m_dwCurrent] );

    // Listener position/velocity
    m_pDSound->SetPosition( m_vListenerPosition.x, m_vListenerPosition.y, m_vListenerPosition.z, DS3D_DEFERRED  );
    m_pDSound->SetVelocity( vListenerVelocity.x, vListenerVelocity.y, vListenerVelocity.z, DS3D_DEFERRED );

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

    RenderGradientBackground( 0xFF404040, 0xFF606060 );

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetVertexShader( D3DFVF_D3DVERTEX );

    // Draw the floor
    m_pd3dDevice->SetStreamSource( 0, m_pvbFloor, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Draw the grid
    m_pd3dDevice->SetStreamSource( 0, m_pvbGrid, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_LINELIST, 0, 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) );

    // Draw the source
    m_pd3dDevice->SetStreamSource( 0, m_pvbSource, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Draw the listener
    m_pd3dDevice->SetStreamSource( 0, m_pvbListener, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        WCHAR strBuffer[200];

        m_Font.Begin();

        // Show title
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"MultiPass" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Show status
        m_Font.DrawText( 64, 100, 0xffffffff, L"Current Sound: " );
        m_Font.DrawText( m_bPlaying[m_dwCurrent] ? 0xffffff00 : 0xff808000, g_strFileNames[m_dwCurrent] );

        swprintf( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_vSourcePosition.x, m_vSourcePosition.y, m_vSourcePosition.z );
        m_Font.DrawText( 64, 130, 0xffffffff, L"Source: " );
        m_Font.DrawText( m_dwSourceColor, strBuffer );

        swprintf( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_vListenerPosition.x, m_vListenerPosition.y, m_vListenerPosition.z );
        m_Font.DrawText( 64, 160, 0xffffffff, L"Listener: " );
        m_Font.DrawText( m_dwListenerColor, strBuffer );

        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, m_fVolume[m_dwCurrent] / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(m_fVolume[m_dwCurrent]) - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 190, 0xffffffff,  L"Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        m_Font.DrawText( 64, 220, 0xffffffff, L"Headphones: " );
        m_Font.DrawText( 0xffffff00, m_bHeadphones ? L"enabled" : L"disabled" );

        m_Font.End();
    }

    // Draw the on-screen audio level meters
    XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




