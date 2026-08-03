//-----------------------------------------------------------------------------
// File: Pan3D.cpp
//
// Desc: This sample demonstrates how to use a panning algorithm and 
//       the DirectSOund SetMixbinVolumes API to 3D position a sound source.
//       It allows the game to use 2D  voices and also utilize N speakers (N>4)
//       for positioning. Two algorithms, of different CPU overhead are provided
//
// Hist: 03.01.02 - New for April XDK release
//       03.06.02 - Added audio level meters for April 02 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include <assert.h>
#include "dsound.h"
#include "dsstdfx.h"
#include "xgmath.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nsound" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Change\nsound" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle source/\nlistener" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nheadphones" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Increase\nvolume" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Decrease\nvolume" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Move object\nin Y" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Move object\nin X/Z" },
    { XBHELP_MISC_CALLOUT, XBHELP_PLACEMENT_1, GLYPH_LEFT_BUTTON GLYPH_RIGHT_BUTTON L" Change Environment" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);




//-----------------------------------------------------------------------------
// Global variables and definitions
//-----------------------------------------------------------------------------

struct VECTOR3
{
    FLOAT x, y, z;
};

struct VECTOR4
{
    FLOAT x, y, z, w;
};

// Pan3d algorithm speaker locations
struct PAN3DSPEAKER 
{
    union 
    {
        VECTOR3 vSpeakerPos;     // Speaker position in 3D space
        VECTOR4 v4SpeakerPos;    // Speaker position in 4D space for orientation transforms
    }; 

    DWORD dwMixBin;           // Speaker mixbin identifier
};



// The table below describes in 3D space the positions of 5 virtual speakers
// that "float" around the listener on the circumference of unity circle
// currently all the speakers are positioned so they exist on the same horizontal
// plane but that they can be portioned anywhere to create interesting panning effects.
// The number of speakers the algorithm can pan between is variable. In this example
// we have chosen to pan through the center channel as well.
PAN3DSPEAKER g_aDefaultSpeakers[] = 
{
    { { -0.7f, 0.0f,  0.7f }, DSMIXBIN_3D_FRONT_LEFT }, 
    { {  0.7f, 0.0f,  0.7f }, DSMIXBIN_3D_FRONT_RIGHT },
    { { -0.7f, 0.0f, -0.7f }, DSMIXBIN_3D_BACK_LEFT }, 
    { {  0.7f, 0.0f, -0.7f }, DSMIXBIN_3D_BACK_RIGHT },
    { {  0.0f, 0.0f,  1.0f }, DSMIXBIN_FRONT_CENTER }
};

const DWORD PAN3D_SPEAKER_COUNT = sizeof( g_aDefaultSpeakers ) / sizeof( g_aDefaultSpeakers[0] );

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


struct I3DL2ENVIRONMENT
{
    WCHAR*          strName;
    DSI3DL2LISTENER ds3dl;
};
    
I3DL2ENVIRONMENT g_aEnvironments[] =
{
    { L"Default",           { DSI3DL2_ENVIRONMENT_PRESET_DEFAULT }        },
    { L"Generic",           { DSI3DL2_ENVIRONMENT_PRESET_GENERIC }        },
    { L"Padded Cell",       { DSI3DL2_ENVIRONMENT_PRESET_PADDEDCELL }     },
    { L"Room",              { DSI3DL2_ENVIRONMENT_PRESET_ROOM }           },
    { L"Bathroom",          { DSI3DL2_ENVIRONMENT_PRESET_BATHROOM }       },
    { L"Living Room",       { DSI3DL2_ENVIRONMENT_PRESET_LIVINGROOM }     },
    { L"Stone Room",        { DSI3DL2_ENVIRONMENT_PRESET_STONEROOM }      },
    { L"Auditorium",        { DSI3DL2_ENVIRONMENT_PRESET_AUDITORIUM }     },
    { L"Concert Hall",      { DSI3DL2_ENVIRONMENT_PRESET_CONCERTHALL }    },
    { L"Cave",              { DSI3DL2_ENVIRONMENT_PRESET_CAVE }           },
    { L"Arena",             { DSI3DL2_ENVIRONMENT_PRESET_ARENA }          },
    { L"Hangar",            { DSI3DL2_ENVIRONMENT_PRESET_HANGAR }         },
    { L"Carpeted Hallway",  { DSI3DL2_ENVIRONMENT_PRESET_CARPETEDHALLWAY }},
    { L"Hallway",           { DSI3DL2_ENVIRONMENT_PRESET_HALLWAY }        },
    { L"Stone Corridor",    { DSI3DL2_ENVIRONMENT_PRESET_STONECORRIDOR }  },
    { L"Alley",             { DSI3DL2_ENVIRONMENT_PRESET_ALLEY }          },
    { L"Forest",            { DSI3DL2_ENVIRONMENT_PRESET_FOREST }         },
    { L"City",              { DSI3DL2_ENVIRONMENT_PRESET_CITY }           },
    { L"Mountains",         { DSI3DL2_ENVIRONMENT_PRESET_MOUNTAINS }      },
    { L"Quarry",            { DSI3DL2_ENVIRONMENT_PRESET_QUARRY }         },
    { L"Plain",             { DSI3DL2_ENVIRONMENT_PRESET_PLAIN }          },
    { L"Parking Lot",       { DSI3DL2_ENVIRONMENT_PRESET_PARKINGLOT }     },
    { L"Sewer Pipe",        { DSI3DL2_ENVIRONMENT_PRESET_SEWERPIPE }      },
    { L"Underwater",        { DSI3DL2_ENVIRONMENT_PRESET_UNDERWATER }     },
};
static const DWORD NUM_ENVIRONMENTS = sizeof( g_aEnvironments ) / sizeof( g_aEnvironments[0] );

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
    FLOAT                   m_fVolume;              // Current volume
    LPDIRECTSOUND8          m_pDSound;              // DirectSound object
    LPDIRECTSOUNDBUFFER8    m_pDSBuffer;            // DirectSoundBuffer
    BYTE*                   m_pbSampleData;         // Sample data from wav
    BOOL                    m_bHeadphones;          // True if headphones enabled

    // Sound source and listener positions
    XGVECTOR3               m_vSourcePosition;      // Source position vector
    XGVECTOR3               m_vListenerPosition;    // Listener position vector
    XGVECTOR3               m_vSoundVelocity;       // Source velocity vector
    XGVECTOR3               m_vListenerVelocity;    // Listener velocity vector

    FLOAT                   m_fListenerAngle;       // Listener orientation angle in x-z
    XGVECTOR3               m_vListenerOrientationTop; // Listener top orientation vector
    XGVECTOR3               m_vListenerOrientationFrontDefault;

    // pan3d algorithm data
    DWORD                   m_dwSpeakerCount;        // number of virtual speakers to pan between
    BOOL                    m_bUseGaussianAlgorithm; // switches between alternate N speaker pans
    XGMATRIX*               m_pTransformMatrix;      // listener transform matrix
    LONG                    m_alMixBinVolumes[DSMIXBIN_LAST];
    PAN3DSPEAKER            m_aSpeakers[PAN3D_SPEAKER_COUNT];   
    LONG                    m_lDopplerPitch;
    FLOAT                   m_flDopplerFactor;
 

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

    DWORD           m_dwEnvironment;                // Environment

    HRESULT SwitchToSound( DWORD dwIndex );         // Sets up a different sound

    VOID    SetMixBinVolumes();                     // Sets output mixbin volumes
    VOID    SetListenerOrientation( XGVECTOR3* pvFront, XGVECTOR3 *pvTop );   // updates virtual speaker coordinates
    VOID    Calculate3D();                          // core 3d calculation function
    VOID    CalculateDopplerShift( XGVECTOR3* pvNormPos );    // calculates doppler shift
    FLOAT   CalculateDistanceAttenuation( XGVECTOR3* pvPos );

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
    m_bDrawHelp = FALSE;
    m_bUseGaussianAlgorithm = TRUE;
    m_dwSpeakerCount = PAN3D_SPEAKER_COUNT;

    // Sounds
    m_fVolume = DSBVOLUME_MAX;
    m_pbSampleData = NULL;
    m_bHeadphones = FALSE;

    // Positions and 3d pan algorithm data
    m_vSourcePosition   = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_vListenerPosition = D3DXVECTOR3( 0.0f, 0.0f, ZMIN );

    m_lDopplerPitch = 0;
    m_flDopplerFactor = DOPPLER_FACTOR;

    memset( m_alMixBinVolumes,0,sizeof(m_alMixBinVolumes) );
    m_pTransformMatrix = (XGMATRIX*)XPhysicalAlloc( sizeof(D3DMATRIX), MAXULONG_PTR, 0, PAGE_READWRITE );
    memcpy( m_aSpeakers,g_aDefaultSpeakers,sizeof(m_aSpeakers)) ;

    // listener default orientation
    m_fListenerAngle = 0.0f;
    m_vListenerOrientationTop = XGVECTOR3( 0.0f, 1.0f, 0.0f );
    m_vListenerOrientationFrontDefault = XGVECTOR3( 0.0f, 0.0f, 1.0f );

    m_dwEnvironment = 0;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the app
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    int i, j;

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create DirectSound
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
    for( i = 0; i < NUM_SOUNDS; i++ )
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

    // Since we are using a pan algorithm we can just allocate a 2D voice!
    dsbdesc.dwFlags       = 0;
    dsbdesc.dwBufferBytes = 0;
    dsbdesc.lpwfxFormat   = (WAVEFORMATEX*)&wfx;

    if( FAILED( m_pDSound->CreateSoundBuffer( &dsbdesc, &m_pDSBuffer, NULL ) ) )
    {
        return E_FAIL;
    }

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
    m_pd3dDevice->CreateVertexBuffer( 3 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbListener );
    m_pd3dDevice->CreateVertexBuffer( 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbGrid );
    
    D3DVERTEX* pVertices;

    // Fill the VB for the listener
    m_pvbListener->Lock( 0, 0, (BYTE**)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( -0.5f, 0.0f, -1.0f ); pVertices[0].c = LISTENER_COLOR; // m_dwListenerColor;
    pVertices[1].p = D3DXVECTOR3(  0.0f, 0.0f,  1.0f ); pVertices[1].c = LISTENER_COLOR; // m_dwListenerColor;
    pVertices[2].p = D3DXVECTOR3(  0.5f, 0.0f, -1.0f ); pVertices[2].c = LISTENER_COLOR; // m_dwListenerColor;

    // Fill the VB for the source
    m_pvbSource->Lock( 0, 0, (BYTE**)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( -0.5f, 0.0f, -0.5f ); pVertices[0].c = SOURCE_COLOR; // m_dwSourceColor;
    pVertices[1].p = D3DXVECTOR3( -0.5f, 0.0f,  0.5f ); pVertices[1].c = SOURCE_COLOR; // m_dwSourceColor;
    pVertices[2].p = D3DXVECTOR3(  0.5f, 0.0f, -0.5f ); pVertices[2].c = SOURCE_COLOR; // m_dwSourceColor;
    pVertices[3].p = D3DXVECTOR3(  0.5f, 0.0f,  0.5f ); pVertices[3].c = SOURCE_COLOR; // m_dwSourceColor

    // Fill the VB for the floor
    m_pvbFloor->Lock( 0, 0, (BYTE**)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( XMIN, 0.0f, ZMIN ); pVertices[0].c = 0xFF101010;
    pVertices[1].p = D3DXVECTOR3( XMIN, 0.0f, ZMAX ); pVertices[1].c = 0xFF101010;
    pVertices[2].p = D3DXVECTOR3( XMAX, 0.0f, ZMIN ); pVertices[2].c = 0xFF101010;
    pVertices[3].p = D3DXVECTOR3( XMAX, 0.0f, ZMAX ); pVertices[3].c = 0xFF101010;
    m_pvbFloor->Unlock();

    // Fill the VB for the grid
    m_pvbGrid->Lock( 0, 0, (BYTE**)&pVertices, 0 );
    for( i = ZMIN, j = 0; i <= ZMAX; i++, j++ )
    {
        pVertices[j*2+0].p = D3DXVECTOR3( XMIN, 0, (FLOAT)i ); pVertices[j*2+0].c = 0xFF00A000;
        pVertices[j*2+1].p = D3DXVECTOR3( XMAX, 0, (FLOAT)i ); pVertices[j*2+1].c = 0xFF00A000;
    }
    for( i = XMIN; i <= XMAX; i++, j++ )
    {
        pVertices[j*2+0].p = D3DXVECTOR3( (FLOAT)i, 0, ZMIN ); pVertices[j*2+0].c = 0xFF00A000;
        pVertices[j*2+1].p = D3DXVECTOR3( (FLOAT)i, 0, ZMAX ); pVertices[j*2+1].c = 0xFF00A000;
    }
    m_pvbGrid->Unlock();

    // Set up and play our initial sound
    m_dwCurrent = 0;
    SwitchToSound( m_dwCurrent );
    m_bPlaying = TRUE;
    m_pDSBuffer->Play( 0, 0, DSBPLAY_LOOPING );

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
    m_awfSounds[ dwIndex ].GetLoopRegion( &dwLoopStart, &dwLoopLength );

    // Set up values for the new buffer
    m_pDSBuffer->SetBufferData( m_pbSampleData, dwNewSize );
    m_pDSBuffer->SetLoopRegion( dwLoopStart, dwLoopLength );
    m_pDSBuffer->SetCurrentPosition( 0 );

    // Set the mixbin volumes for the mixbins associated with the dsound buffer
    SetMixBinVolumes();

    // If we were playing before, restart playback now
    if( m_bPlaying )
        m_pDSBuffer->Play( 0, 0, DSBPLAY_LOOPING );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetMixBinVolumes()
// Desc: Called whenever we switch from a sound or toggle headphones.
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetMixBinVolumes()
{
    DWORD              dwNumMixBins = PAN3D_SPEAKER_COUNT+2;
    DSMIXBINS          dsMixBins;
    DSMIXBINVOLUMEPAIR dsMixBinArray[DSMIXBIN_ASSIGNMENT_MAX];
    DWORD i = 0;

    // Set mixbins. Note that we are sending sound to the
    // crosstalk (3d) so we can use the reverb and crosstalk cancellation for free
    dsMixBins.dwMixBinCount       = dwNumMixBins;
    dsMixBins.lpMixBinVolumePairs = dsMixBinArray;

    dsMixBinArray[i].dwMixBin = DSMIXBIN_3D_FRONT_LEFT;
    dsMixBinArray[i].lVolume = m_alMixBinVolumes[dsMixBinArray[i].dwMixBin];
    i++;
    dsMixBinArray[i].dwMixBin = DSMIXBIN_3D_FRONT_RIGHT;
    dsMixBinArray[i].lVolume  = m_alMixBinVolumes[dsMixBinArray[i].dwMixBin];
    i++;
    dsMixBinArray[i].dwMixBin = DSMIXBIN_FRONT_CENTER;
    dsMixBinArray[i].lVolume  = m_alMixBinVolumes[dsMixBinArray[i].dwMixBin];
    i++;
    dsMixBinArray[i].dwMixBin = DSMIXBIN_3D_BACK_LEFT;
    dsMixBinArray[i].lVolume  = m_alMixBinVolumes[dsMixBinArray[i].dwMixBin];
    i++;
    dsMixBinArray[i].dwMixBin = DSMIXBIN_3D_BACK_RIGHT;
    dsMixBinArray[i].lVolume  = m_alMixBinVolumes[dsMixBinArray[i].dwMixBin];
    i++;
    dsMixBinArray[i].dwMixBin = DSMIXBIN_LOW_FREQUENCY;
    dsMixBinArray[i].lVolume  = m_alMixBinVolumes[dsMixBinArray[i].dwMixBin];
    i++;
    
    // reduce the volume on the reverb 
    dsMixBinArray[i].dwMixBin = DSMIXBIN_I3DL2;
    dsMixBinArray[i].lVolume  = -1800;

    if( m_bHeadphones )
    {
        // when headphones are enabled we only want pan between two speakers
        // This can be changed so you can pan between 4 speakers but at the 
        // end of Calculate3D choose the max volume between the left 
        // (front/back) and right(front/back)
        for( i=2; i < dsMixBins.dwMixBinCount; i++ )
        {
            dsMixBinArray[i].lVolume = -10000;
        }
    }

    m_pDSBuffer->SetMixBins(&dsMixBins);
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    XGVECTOR3  vSourceOld   = m_vSourcePosition;
    XGVECTOR3  vListenerOld = m_vListenerPosition;
    XGVECTOR3* pvControl;
    DWORD      dwPulse = DWORD( ( cosf( m_fAppTime * 6.0f ) + 1.0f ) * 50 );
    D3DCOLOR   cBlend = dwPulse | ( dwPulse << 8 ) | ( dwPulse << 16 );

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
        // tell pan algorithm to only use front speakers if headphones enabled
        m_dwSpeakerCount = m_bHeadphones ? 2: PAN3D_SPEAKER_COUNT;
        SetMixBinVolumes();
    }

    // Changing listener environment settings is expensive, so only do it when we're
    // actually changing the environment.
    BOOL bResetListener = FALSE;
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_LEFT_TRIGGER ] )
    {
        m_dwEnvironment = ( m_dwEnvironment + NUM_ENVIRONMENTS - 1 ) % NUM_ENVIRONMENTS;
        bResetListener = TRUE;
    }
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_RIGHT_TRIGGER ] )
    {
        m_dwEnvironment = ( m_dwEnvironment + 1 ) % NUM_ENVIRONMENTS;
        bResetListener = TRUE;
    }

    if( bResetListener )
    {
        m_pDSound->SetI3DL2Listener( &g_aEnvironments[ m_dwEnvironment ].ds3dl, DS3D_IMMEDIATE );
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

    // Calculate listener orientation in x-z plane
    if( m_vListenerPosition.x != vListenerOld .x ||
        m_vListenerPosition.z != vListenerOld .z )
    {
        XGVECTOR3 vDelta = m_vListenerPosition - vListenerOld;

        XGVec3Normalize(&vDelta,&vDelta);
        m_fListenerAngle = FLOAT( atan2( vDelta.x, vDelta.z ) );

        vDelta.y = 0.0f;

        // set default listener orientation using our private method
        SetListenerOrientation( &vDelta, &m_vListenerOrientationTop);
    }

    // Position the sound and listener in 3D using the new Pan algorithm. 
    m_vListenerVelocity = ( m_vListenerPosition - vListenerOld ) / m_fElapsedTime;
    m_vSoundVelocity    = ( m_vSourcePosition - vSourceOld ) / m_fElapsedTime;

    // buffer volume is still handled by dsound
    m_pDSBuffer->SetVolume( (LONG)m_fVolume );
    
    // Using the current source+listener position, orientation and velocity
    // perform the sound 3d calculations using the panning algorithm        
    Calculate3D();

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
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );

    // Draw the floor
    m_pd3dDevice->SetStreamSource( 0, m_pvbFloor, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Draw the grid
    m_pd3dDevice->SetStreamSource( 0, m_pvbGrid, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_LINELIST, 0, 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) );

    // Draw the source
    {
        D3DXMATRIX matSource;
        D3DXMatrixTranslation( &matSource, 
                               m_vSourcePosition.x,
                               m_vSourcePosition.y,
                               m_vSourcePosition.z );
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &matSource );
        m_pd3dDevice->SetStreamSource( 0, m_pvbSource, sizeof( D3DVERTEX ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
    }

    // Draw the listener
    {
        D3DXMATRIX matListener;
        D3DXMATRIX mat;

        D3DXMatrixTranslation( &matListener, m_vListenerPosition.x, m_vListenerPosition.y, m_vListenerPosition.z );
        D3DXMatrixRotationY( &mat, m_fListenerAngle );
        D3DXMatrixMultiply( &matListener, &mat, &matListener );
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &matListener );
        m_pd3dDevice->SetStreamSource( 0, m_pvbListener, sizeof( D3DVERTEX ) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 1 );
    }

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        WCHAR strBuffer[200];

        m_Font.Begin();

        // Show title
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"Pan3D" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Show status
        m_Font.DrawText( 64, 100, 0xffffffff, L"Current Sound: " );
        m_Font.DrawText( m_bPlaying ? 0xffffff00 : 0xff808000, g_strFileNames[m_dwCurrent] );

        swprintf( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_vSourcePosition.x, m_vSourcePosition.y, m_vSourcePosition.z );
        m_Font.DrawText( 64, 130, 0xffffffff, L"Source: " );
        m_Font.DrawText( m_dwSourceColor, strBuffer );

        swprintf( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_vListenerPosition.x, m_vListenerPosition.y, m_vListenerPosition.z );
        m_Font.DrawText( 64, 160, 0xffffffff, L"Listener: " );
        m_Font.DrawText( m_dwListenerColor, strBuffer );

        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, m_fVolume / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(m_fVolume) - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 190, 0xffffffff, L"Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );
        
        m_Font.DrawText( 64, 220, 0xffffffff, L"Environment: " );
        m_Font.DrawText( 64, 250, 0xffffff00, g_aEnvironments[ m_dwEnvironment ].strName );
   
        m_Font.DrawText( 64, 340, 0xffffffff, L"Headphones: ");
        if( m_bHeadphones )
            m_Font.DrawText( 0xffffff00, L"enabled" );
        else
            m_Font.DrawText( 0xff808000, L"disabled" );

        m_Font.End();

        // Draw the on-screen audio level meters
        XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetListenerOrientation()
// Desc: Called whenever the listener orientation changes. The Pan3D algorithm
//       relies on a table of cartesian coordinates that describe N virtual speakers
//       floating around the listener. Each time the listener orientation changes
//       the plane that describes the speakers changes thus the need to transform
//       the speaker locations
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetListenerOrientation(XGVECTOR3 *pvFront, XGVECTOR3 *pvTop)
{
    XGVECTOR4 v4SpeakerPos;
    XGVECTOR3 vDefaultSpeakerPos;
    
    // Recalculate the speaker locations.
    // Assume front vector is normalized. Use the top orientation 
    // vector and the angle of the listener to calculate 
    // a transform matrix we need to multiply our speaker matrix by
    XGMatrixRotationAxis( m_pTransformMatrix, pvTop, m_fListenerAngle );
    
    // Now transform the speaker positions
    for(DWORD i = 0; i < PAN3D_SPEAKER_COUNT; i++) 
    {
        vDefaultSpeakerPos.x = g_aDefaultSpeakers[i].vSpeakerPos.x;
        vDefaultSpeakerPos.y = g_aDefaultSpeakers[i].vSpeakerPos.y;
        vDefaultSpeakerPos.z = g_aDefaultSpeakers[i].vSpeakerPos.z;

        XGVec3Transform(&v4SpeakerPos, &vDefaultSpeakerPos, m_pTransformMatrix);

        m_aSpeakers[i].vSpeakerPos.x = v4SpeakerPos.x;
        m_aSpeakers[i].vSpeakerPos.y = v4SpeakerPos.y;
        m_aSpeakers[i].vSpeakerPos.z = v4SpeakerPos.z;

    }            
}




//-----------------------------------------------------------------------------
// Name: Calculate3D()
// Desc: The function implements a N speaker pan algorithm. It requires no 
//       cartesian to polar transformations and is computationally efficient.
//       This can be used as an alternative to hw 3D (hrtf) since
//       1) allows you to specify arbitrary mixbins/speakers as the pan 
//          destinations
//       2) Does not require a HW 3D voice
//       3) it can do N (where N >4) pan unlike HRTF which is limited to 4 
//          currently
//       4) Its fast and will not block other APU hw functions since it minimizes
//          hw access
//       The idea is to use a table that describes N virtual speakers that float
//       around the listener at all times. The speakers exist on a unity circle
//       (listener being the center) and their coordinates get transformed based
//       on the listener orientation. The volume for each speaker is calculated
//       based on the distance of the normalized sound position 
//       in relation to the speaker. That value is passed through a Gaussian 
//       function to create a smooth roll-off. In the sample the mixbin 
//       destination are actually the crosstalk input mixbins so you get the 
//       filtering and time delay that enhances the 3D "feel". This can be 
//       trivially changed.
//       The algorithm currently does not distance attenuation. This can be 
//       implemented again easily in the function below using linear or log 
//       roll-off
//-----------------------------------------------------------------------------
VOID CXBoxSample::Calculate3D()
{
    static const float      flAlpha             = 0.20f;
    LONG                    lVolumeBase         = -3000; 
    float                   flVolumeRange       = (DSBVOLUME_MAX-DSBVOLUME_MIN)+(FLOAT)lVolumeBase;
    const PAN3DSPEAKER*     aSpeakers           = m_aSpeakers;
    XGVECTOR3               vNormPos;
    XGVECTOR3               vSpeakerNorm;
    FLOAT                   flSpeakerMag;
    FLOAT                   flSpeakerFactor;
    LONG                    lVolume;
    LONG                    lDistanceVolume;
    DWORD                   i;
    DSMIXBINS               dsMixBins;
    DSMIXBINVOLUMEPAIR      dsMixBinArray[8];
    
    dsMixBins.dwMixBinCount = 0;
    dsMixBins.lpMixBinVolumePairs = dsMixBinArray;
    
    // First create a normalized vector that point from the listener to 
    // the source
    vNormPos = m_vSourcePosition - m_vListenerPosition;    

    // Calculate distance contribution to volume
    lDistanceVolume = (LONG)( lVolumeBase * CalculateDistanceAttenuation( &vNormPos ) );

    XGVec3Normalize( &vNormPos,(const XGVECTOR3 *)&vNormPos );

    // calculate doppler shift
    CalculateDopplerShift( &vNormPos );

    // Calculate volume for each speaker.  We're calculating volume for 
    // each speaker, regardless of speaker config so that processing takes
    // the same amount of time.
    // Distance and rolloff attenuation are taken care of in a different
    // function using different data members, so this is strictly pan. 
    // Note that this particular implementation calculates speaker volumes
    // independently of each other. For example when the source is directly in front
    // of the listener, we don't bleed volume from the front left and right speakers

    for( i = 0; i < m_dwSpeakerCount; i++ )
    {
        if( m_bUseGaussianAlgorithm ) 
        {
            // Alternate algorithm with smoother transitions but more cpu intensive
            // (still faster than using DSound with FullHrtf)
            vSpeakerNorm.x = vNormPos.x - aSpeakers[i].vSpeakerPos.x;
            vSpeakerNorm.y = vNormPos.y - aSpeakers[i].vSpeakerPos.y;
            vSpeakerNorm.z = vNormPos.z - aSpeakers[i].vSpeakerPos.z;
            flSpeakerMag = XGVec3Length( &vSpeakerNorm );
            flSpeakerFactor = 1.0f - (float)exp( -pow( 2.0f, flSpeakerMag ) * flAlpha );
        } 
        else
        {
            vSpeakerNorm.x = ( vNormPos.x - aSpeakers[i].vSpeakerPos.x ) / 2;
            vSpeakerNorm.y = ( vNormPos.y - aSpeakers[i].vSpeakerPos.y ) / 2;
            vSpeakerNorm.z = ( vNormPos.z - aSpeakers[i].vSpeakerPos.z ) / 2;
            
            flSpeakerMag =(FLOAT)( vSpeakerNorm.x * vSpeakerNorm.x + 
                                   vSpeakerNorm.y * vSpeakerNorm.y + 
                                   vSpeakerNorm.z * vSpeakerNorm.z );
            flSpeakerFactor = flSpeakerMag;
        }

        assert( ( flSpeakerFactor >= 0.0f ) && ( flSpeakerFactor <= 1.0f ) );
        lVolume = (long)(flSpeakerFactor * -flVolumeRange)+lDistanceVolume;
        assert( ( lVolume >= DSBVOLUME_MIN ) && ( lVolume <= DSBVOLUME_MAX ) );
        
        if( lVolume != m_alMixBinVolumes[aSpeakers[i].dwMixBin] )
        {
            m_alMixBinVolumes[aSpeakers[i].dwMixBin] = lVolume;

            dsMixBinArray[dsMixBins.dwMixBinCount].lVolume = lVolume;
            dsMixBinArray[dsMixBins.dwMixBinCount].dwMixBin = aSpeakers[i].dwMixBin;
            dsMixBins.dwMixBinCount++;
            
        }
    }

    if( dsMixBins.dwMixBinCount ) 
    {
        // Apply volumes to dsound
        m_pDSBuffer->SetMixBinVolumes( &dsMixBins );
    }
}



//-----------------------------------------------------------------------------
// Name: CalculateDistanceAttenuation()
// Desc: The function calculates a distance factor between 0 and 1.0 used
//       to calculate the volume contribution due to distance.
//       It currently uses a linear conversion.
//-----------------------------------------------------------------------------
FLOAT CXBoxSample::CalculateDistanceAttenuation( XGVECTOR3* pvPos )
{
    FLOAT flMaxDistance = XMAX-XMIN;
    FLOAT flMinDistance = 0.0f;
    FLOAT flDistance = 0.0f;

    // calculate vector length
    flDistance = XGVec3Length(pvPos);

    // normalize distance between 0.0 and 1.0
    flDistance /= ( flMaxDistance - flMinDistance );

    // return distance factor
    return flDistance;
}




//-----------------------------------------------------------------------------
// Name: RatioToPitch()
// Desc: The function converts a frequency ration to pitch octave scale 
//-----------------------------------------------------------------------------
LONG RatioToPitch( FLOAT flRatio )
{
    const FLOAT fl4096  = 4096.0f;
    LONG        lPitch;

    assert(flRatio);

    __asm 
    {
        fld     fl4096
        fld     flRatio
        fyl2x
        fistp   lPitch
    }

    return lPitch;
}




//-----------------------------------------------------------------------------
// Name: CalculateDopplerShift()
// Desc: The function calculates the doppler shift based on the sound/listener
//       velocity          
//-----------------------------------------------------------------------------
VOID CXBoxSample::CalculateDopplerShift( XGVECTOR3* pvNormPos )
{
    static const FLOAT flSpeedOfSound = 342.0f;
    FLOAT flVelocity;
    LONG  lDopplerPitch;

    flVelocity = (m_vSoundVelocity.x - m_vListenerVelocity.x) * pvNormPos->x +
                 (m_vSoundVelocity.y - m_vListenerVelocity.y) * pvNormPos->y +
                 (m_vSoundVelocity.z - m_vListenerVelocity.z) * pvNormPos->z;
    flVelocity *= m_flDopplerFactor;

    if( !flVelocity )
    {
        lDopplerPitch = 0;
    }
    else if( flVelocity >= flSpeedOfSound )
    {
        lDopplerPitch = DSBPITCH_MIN;
    }
    else if( flVelocity <= -flSpeedOfSound )
    {
        lDopplerPitch = 4096;
    }
    else
    {
        lDopplerPitch = RatioToPitch( 1.0f - flVelocity / flSpeedOfSound );
    }

    if( lDopplerPitch != m_lDopplerPitch ) 
    {
        m_pDSBuffer->SetPitch(lDopplerPitch);
        m_lDopplerPitch = lDopplerPitch;
    }
}

