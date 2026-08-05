//-----------------------------------------------------------------------------
// File: CustomMemoryAllocator.cpp
//
// Desc: This sample demonstrates the use of custom allocators. At the time 
//       of the creating of this sample, only the XACT lib is enable with this 
//       functionality. This is why an XACT sample was chosen for this sample.
//       Eventually all Xbox libs will have this functionality. 
//
// Hist: 09.05.02 - New for October 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>
#include <xact.h>
#include <dsstdfx.h>
#include <assert.h>
#include <deque>
#include "CHeapAlloc.h"
#include "XactSounds.h"         // XACT generated header


// Global variable declarations
CHeapAlloc*     g_pSoundCueHeap  = NULL;        // Heap allocator object


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,   XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,      XBHELP_PLACEMENT_2, L"Play sound\ninstance" },
    { XBHELP_B_BUTTON,      XBHELP_PLACEMENT_2, L"Stop oldest/all\ninstances" },
    { XBHELP_X_BUTTON,      XBHELP_PLACEMENT_1, L"Sources/Listener" },
    { XBHELP_BLACK_BUTTON,  XBHELP_PLACEMENT_1, L"Volume Down" },
    { XBHELP_WHITE_BUTTON,  XBHELP_PLACEMENT_1, L"Volume Up" },
    { XBHELP_DPAD,          XBHELP_PLACEMENT_1, L"Cues/Categories" },
    { XBHELP_RIGHTSTICK,    XBHELP_PLACEMENT_2, L"Move object\nin Y" },
    { XBHELP_LEFTSTICK,     XBHELP_PLACEMENT_2, L"Move object\nin X/Z" },
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )




// Our vertex format
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

#define WALL1_XMIN ( XMIN )
#define WALL1_XMAX ( XMAX )
#define WALL1_ZMIN ( ZMAX - 0.12f * ( ZMAX - ZMIN ) )
#define WALL1_ZMAX ( ZMAX - 0.10f * ( ZMAX - ZMIN ) )

#define WALL2_XMIN ( XMIN + 0.25f * ( XMAX - XMIN ) )
#define WALL2_XMAX ( XMIN + 0.75f * ( XMAX - XMIN ) )
#define WALL2_ZMIN ( ZMAX - 0.42f * ( ZMAX - ZMIN ) )
#define WALL2_ZMAX ( ZMAX - 0.40f * ( ZMAX - ZMIN ) )


// Constants for scaling input
static const FLOAT MOTION_SCALE = 10.0f;
static const FLOAT VOLUME_SCALE = 5.0f;
static const FLOAT DOPPLER_FACTOR = 2.0f;

// In this sample, some sounds are bound to objects
// (such the helicopter's rotor and gun), while others
// are not (e.g., background music).  The following
// enum makes it easier to know which sounds are
// tied to which objects
enum SOURCES_ENUM
{
    SOURCE_DEFAULT = -1,
    SOURCE_TARGET,
    SOURCE_HELICOPTER,
    SOURCE_BYSTANDER,
    SOURCE_MAX
};

// General metadata about cues for this sample
struct CUE_METADATA
{
    WCHAR*                  strDesc;            // Description
    XACT_SOUNDBANK_SAMPLE   nCueIndex;          // Index supplied by XACT header
    BOOL                    bAutorelease;       // Whether this sound is fire+forget
    SOURCES_ENUM            nSourceIndex;       // Source to which this cue is bound
    union 
    {
        std::deque<PXACTSOUNDCUE>*   pqInstances;   // Instances of non-autorelease cues
        UINT                         nInstances;    // Number of instances for autorelease cues
    } u;
};

// Note that our cue metadata is not ordered in the same
// order as the cues themselves (see media/sounds/sample.h)
// This is because there is no guarantee as to the values
// of the cues' indices.  Hence, we can not index this
// array via cue index and putting the metadata in the
// same order gives a false sense of this.
static CUE_METADATA g_aCueMetaData[] =
{
    { L"Ambient Sound", XACT_SOUNDBANK_SAMPLE_AMBIENT,      FALSE,  SOURCE_DEFAULT,     NULL    },
    { L"BG Music 1",    XACT_SOUNDBANK_SAMPLE_MUSIC_1,      TRUE,   SOURCE_DEFAULT,     NULL    },
    { L"BG Music 2",    XACT_SOUNDBANK_SAMPLE_MUSIC_2,      TRUE,   SOURCE_DEFAULT,     NULL    },
    { L"Helicopter",    XACT_SOUNDBANK_SAMPLE_HELICOPTER,   FALSE,  SOURCE_HELICOPTER,  NULL    },
    { L"Weapon",        XACT_SOUNDBANK_SAMPLE_GUNSHOT,      TRUE,   SOURCE_HELICOPTER,  NULL    },
    { L"Oh Yeah!",      XACT_SOUNDBANK_SAMPLE_OHYEAH,       FALSE,  SOURCE_BYSTANDER,   NULL    },
    { L"Explosion",     XACT_SOUNDBANK_SAMPLE_EXPLOSION,    TRUE,   SOURCE_TARGET,      NULL    },
};

// Object names
static WCHAR* g_aObjectNames[] =
{
    L"Target",       // SOURCE_TARGET
    L"Helicopter",   // SOURCE_HELICOPTER
    L"Bystander",    // SOURCE_BYSTANDER
    L"Listener",     // SOURCE_MAX (Listener)
};

// Object colors
static const DWORD g_aObjectColors[] =
{
    0xFFEA1B1B,     // SOURCE_TARGET
    0xFF1BEE20,     // SOURCE_HELICOPTER
    0xFFAAD61B,     // SOURCE_BYSTANDER
    0xFF1B1BEA,     // SOURCE_MAX (Listener)
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Utility Objects
    CXBFont                     m_Font;                         // Font object
    CXBHelp                     m_Help;                         // Help object
    BOOL                        m_bDrawHelp;                    // TRUE to draw help screen

    // XACT Engine
    PXACTENGINE                 m_pXACT;                        // XACT Engine
    PBYTE                       m_pbWaveBank;                   // Wave bank data
    PXACTWAVEBANK               m_pWaveBank;                    // In-Memory wave bank
    PXACTWAVEBANK               m_pStreamingWaveBank;           // Streaming wave bank
    PBYTE                       m_pbSoundBank;                  // Sound bank data
    PXACTSOUNDBANK              m_pSoundBank;                   // XACT Sound Bank
    PXACTSOUNDSOURCE            m_aSoundSources[SOURCE_MAX];    // XACT Sound sources

    // Sound source and listener positions
    D3DXVECTOR3                 m_avSourcePositions[SOURCE_MAX];  // Source positions
    D3DXVECTOR3                 m_vListenerPosition;            // Listener position
    FLOAT                       m_fListenerAngle;               // Listener Orientation in x-z
    D3DXVECTOR3                 m_vListenerOrientationTop;      // Listener top orientation vector

    // Transformation matrices
    D3DXMATRIX                  m_matWorld;                     // World transform
    D3DXMATRIX                  m_matView;                      // View transform
    D3DXMATRIX                  m_matProj;                      // Projection transform

    // Models for floor, source, and listener
    LPDIRECT3DVERTEXBUFFER8     m_pvbFloor;                     // Floor
    LPDIRECT3DVERTEXBUFFER8     m_avbSources[SOURCE_MAX];       // Sources
    LPDIRECT3DVERTEXBUFFER8     m_pvbListener;                  // Listener
    LPDIRECT3DVERTEXBUFFER8     m_pvbGrid;                      // Grid for floor

    // Various State
    FLOAT                       m_fVolume;                      // Volume
    UINT                        m_iCurrentCue;                  // Selected cue

    // Index of source/listener being controlled.
    // 0 through SOURCE_MAX - 1 means gives the index
    // of a source, SOURCE_MAX means the listener is selected.
    UINT                        m_iCurrentObject;

    // Customer allocator objects
    DWORD                       m_dwCueInstanceCount;           // Track number of cues instances created

    // Helper functions
    HRESULT LoadFile( const CHAR* strFilename, VOID** ppvData, DWORD* pdwSize );
    HRESULT InitObjectData();
    HRESULT InitAudio();
    HRESULT UpdateMiscInterfaceElements();
    HRESULT UpdateAudio();
    HRESULT UpdateObjectData();

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
    m_bDrawHelp                 = FALSE;

    // XACT Engine
    m_pXACT                     = NULL;
    m_pbWaveBank                = NULL;
    m_pWaveBank                 = NULL;
    m_pStreamingWaveBank        = NULL;
    m_pbSoundBank               = NULL;
    m_pSoundBank                = NULL;
    ZeroMemory( m_aSoundSources, sizeof( m_aSoundSources ) );

    // Sound source and listener positions;
    m_avSourcePositions[0]      = D3DXVECTOR3( -3.f, 0.f, 0.f );
    m_avSourcePositions[1]      = D3DXVECTOR3(  0.f, 0.f, 3.f );
    m_avSourcePositions[2]      = D3DXVECTOR3(  3.f, 0.f, 0.f );

    m_vListenerPosition         = D3DXVECTOR3( 0.f, 0.f, 0.f );
    m_vListenerOrientationTop   = D3DXVECTOR3( 0.f, 1.f, 0.f );
    m_fListenerAngle            = 0.f;

    // Models
    m_pvbFloor                  = NULL;
    m_pvbListener               = NULL;
    m_pvbGrid                   = NULL;
    ZeroMemory( m_avbSources, sizeof( m_avbSources ) );

    m_fVolume                   = DSBVOLUME_MAX;
    m_iCurrentCue               = 0;
    m_iCurrentObject            = 0;

    // Customer allocator objects
    m_dwCueInstanceCount        = 0;

    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}




//-----------------------------------------------------------------------------
// Name: LoadFile()
// Desc: Loads a file from disk using aysnchronous, unbuffered I/O.  This
//       routine allocates a buffer using malloc, which it is then the
//       caller's responsibility to free.  Since unbuffered I/O requires
//       sector-multiple reads, the buffer may contain some extra slop
//       space at the end.  The pdwSize parameter returned reflects the
//       amount of VALID data in the buffer, not the size of the allocation
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::LoadFile( const CHAR* strFilename, VOID** ppvData, DWORD* pdwSize )
{
    // Open up the file for unbuffered, asynchronous IO
    HANDLE hFile = CreateFile( strFilename, GENERIC_READ, FILE_SHARE_READ, 
                               NULL, OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Determine how large the file is (assuming size is less than 4GB)
    DWORD dwFileSize = GetFileSize( hFile, NULL );
    *pdwSize = dwFileSize;
    assert( dwFileSize != -1 );

    // Get the sector size of the drive we're reading from and round
    // our file size up to a sector multiple
    DWORD dwSectorSize = XGetDiskSectorSize( "D:\\" );
    dwFileSize += dwSectorSize - 1;
    dwFileSize /= dwSectorSize;
    dwFileSize *= dwSectorSize;

    // Allocate memory to load the wave bank in
    *ppvData = malloc( dwFileSize );
    if( !*ppvData )
    {
        CloseHandle( hFile );
        return E_OUTOFMEMORY;
    }

    // Kick off the read
    OVERLAPPED overlapped = {0};
    if( FAILED( ReadFile( hFile, *ppvData, dwFileSize, NULL, &overlapped ) ) )
    {
        CloseHandle( hFile );
        return E_FAIL;
    }

    // Wait for the result to complete
    DWORD dwBytesRead;
    GetOverlappedResult( hFile, &overlapped, &dwBytesRead, TRUE );
    assert( dwBytesRead == *pdwSize );

    // Close the file
    CloseHandle( hFile );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitObjectData()
// Desc: Initializes 3D elements
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitObjectData()
{
    // Setup the transform matrices
    D3DXVECTOR3 vEyePt    = D3DXVECTOR3( XMIN, 45.f, ZMAX / 2.f );
    D3DXVECTOR3 vLookatPt = D3DXVECTOR3( XMIN,  0.f, ZMAX / 2.f );
    D3DXVECTOR3 vUpVec    = D3DXVECTOR3(  0.f,  0.f,        1.f );

    D3DXMatrixIdentity( &m_matWorld );
    D3DXMatrixLookAtLH( &m_matView, &vEyePt, &vLookatPt, &vUpVec );
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI / 4, 4.f / 3.f, 1.f, 10000.f );

    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Create our vertex buffers
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbFloor );
    m_pd3dDevice->CreateVertexBuffer( 3 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbListener );
    m_pd3dDevice->CreateVertexBuffer( 2 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbGrid );

    for( LONG i = 0; i < SOURCE_MAX; ++i )
    {
        m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_avbSources[i] );
    }

    // Fill the listener
    D3DVERTEX* pVertices;
    m_pvbListener->Lock( 0, 0, (PBYTE *)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( -0.5f, 0.f, -1.f ); pVertices[0].c = g_aObjectColors[SOURCE_MAX];
    pVertices[1].p = D3DXVECTOR3(   0.f, 0.f,  1.f ); pVertices[1].c = g_aObjectColors[SOURCE_MAX];
    pVertices[2].p = D3DXVECTOR3(  0.5f, 0.f, -1.f ); pVertices[2].c = g_aObjectColors[SOURCE_MAX];
    m_pvbListener->Unlock();

    // Fill the sources
    for( LONG i = 0; i < SOURCE_MAX; ++i )
    {
        m_avbSources[i]->Lock( 0, 0, (PBYTE *)&pVertices, 0 );
        pVertices[0].p = D3DXVECTOR3( -0.5f, 0.f, -0.5f ); pVertices[0].c = g_aObjectColors[i];
        pVertices[1].p = D3DXVECTOR3( -0.5f, 0.f,  0.5f ); pVertices[1].c = g_aObjectColors[i];
        pVertices[2].p = D3DXVECTOR3(  0.5f, 0.f, -0.5f ); pVertices[2].c = g_aObjectColors[i];
        pVertices[3].p = D3DXVECTOR3(  0.5f, 0.f,  0.5f ); pVertices[3].c = g_aObjectColors[i];
        m_avbSources[i]->Unlock();
    }

    // Fill the floor
    m_pvbFloor->Lock( 0, 0, (PBYTE *)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( XMIN, 0.f, ZMIN ); pVertices[0].c = 0xFF101010;
    pVertices[1].p = D3DXVECTOR3( XMIN, 0.f, ZMAX ); pVertices[1].c = 0xFF101010;
    pVertices[2].p = D3DXVECTOR3( XMAX, 0.f, ZMIN ); pVertices[2].c = 0xFF101010;
    pVertices[3].p = D3DXVECTOR3( XMAX, 0.f, ZMAX ); pVertices[3].c = 0xFF101010;
    m_pvbFloor->Unlock();

    // Fill in the grid
    m_pvbGrid->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    LONG j = 0;     // carried into the second loop, which relied on VC6 for-scope
    for( LONG i = ZMIN; i <= ZMAX; i++, j++ )
    {
        pVertices[j*2+0].p = D3DXVECTOR3( XMIN, 0, (FLOAT)i ); pVertices[j*2+0].c = 0xFF00A000;
        pVertices[j*2+1].p = D3DXVECTOR3( XMAX, 0, (FLOAT)i ); pVertices[j*2+1].c = 0xFF00A000;
    }
    for( LONG i = XMIN; i <= XMAX; i++, j++ )
    {
        pVertices[j*2+0].p = D3DXVECTOR3( (FLOAT)i, 0, ZMIN ); pVertices[j*2+0].c = 0xFF00A000;
        pVertices[j*2+1].p = D3DXVECTOR3( (FLOAT)i, 0, ZMAX ); pVertices[j*2+1].c = 0xFF00A000;
    }
    m_pvbGrid->Unlock();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitAudio()
// Desc: Performs audio initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitAudio()
{
    // Tell DirectSound to use Full HRTF
    DirectSoundUseFullHRTF();

    // Create the XACT Engine
    XACT_RUNTIME_PARAMETERS xrParams = {0};
    xrParams.dwMax2DHwVoices         = 128;
    xrParams.dwMax3DHwVoices         = 32;
    xrParams.dwMaxConcurrentStreams  = 20;
    xrParams.dwMaxNotifications      = 0;
    if( FAILED( XACTEngineCreate( &xrParams, &m_pXACT ) ) )
        return E_FAIL;

    // Load the in memory wave bank and register it with XACT
    DWORD dwFileSize;
    if( FAILED( LoadFile( "D:\\media\\sounds\\XactSounds_memory.xwb", (VOID **)&m_pbWaveBank, &dwFileSize ) ) )
        return XBAPPERR_MEDIANOTFOUND;
    if( FAILED( m_pXACT->RegisterWaveBank( m_pbWaveBank, dwFileSize, &m_pWaveBank ) ) )
        return E_FAIL;

    // Register the streaming wave bank with XACT
    HANDLE hStreamingWaveBank = CreateFile( "D:\\media\\sounds\\XactSounds_streaming.xwb",
                                            GENERIC_READ, FILE_SHARE_READ, NULL,
                                            OPEN_EXISTING,
                                            FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
                                            NULL );
    if( INVALID_HANDLE_VALUE == hStreamingWaveBank )
        return XBAPPERR_MEDIANOTFOUND;

    XACT_WAVEBANK_STREAMING_PARAMETERS wbParams = {0};
    wbParams.hFile                        = hStreamingWaveBank;
    wbParams.dwOffset                     = 0;
    wbParams.dwPacketSizeInMilliSecs      = 100;
    wbParams.dwPrimePacketSizeInMilliSecs = 50;
    if( FAILED( m_pXACT->RegisterStreamedWaveBank( &wbParams, &m_pStreamingWaveBank ) ) )
        return E_FAIL;

    // Load the sound bank and register it with XACT
    if( FAILED( LoadFile( "D:\\media\\sounds\\XactSounds.xsb", (VOID **)&m_pbSoundBank, &dwFileSize ) ) )
        return XBAPPERR_MEDIANOTFOUND;
    if( FAILED( m_pXACT->CreateSoundBank( m_pbSoundBank, dwFileSize, &m_pSoundBank ) ) )
        return E_FAIL;

    // Register a stop notification for each cue index, so we 
    // can do our instance tracking
    XACT_NOTIFICATION_DESCRIPTION xactNotificationDesc;

    ZeroMemory( &xactNotificationDesc, sizeof( xactNotificationDesc ) );
    xactNotificationDesc.wType        = eXACTNotification_Stop;
    xactNotificationDesc.wFlags       = XACT_FLAG_NOTIFICATION_PERSIST |              // Always keep this notification registered (i.e., don't just notify once)
                                        XACT_FLAG_NOTIFICATION_USE_SOUNDCUE_INDEX;    // Apply to this sound cue index
    xactNotificationDesc.u.pSoundBank = m_pSoundBank;
    xactNotificationDesc.hEvent       = NULL;

    for( LONG i = 0; i < XACT_SOUNDBANK_SAMPLE_CUE_COUNT; ++i )
    {
        xactNotificationDesc.dwSoundCueIndex = g_aCueMetaData[i].nCueIndex;
        if( FAILED( m_pXACT->RegisterNotification( &xactNotificationDesc ) ) )
            return E_FAIL;
    }

    // Load our DSP image and register it with XACT
    VOID* pDSPImage = NULL;
    if( FAILED( LoadFile( "d:\\media\\dsstdfx.bin", &pDSPImage, &dwFileSize ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    DSEFFECTIMAGELOC dseil;
    dseil.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    dseil.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( m_pXACT->DownloadEffectsImage( pDSPImage, dwFileSize, &dseil, NULL ) ) )
        return E_FAIL;

    // Free our copy of the DSP imgae
    free( pDSPImage );

    // Set the listener's initial values
    m_pXACT->SetListenerOrientation( 0.f, 0.f, 1.f,
                                     m_vListenerOrientationTop.x,
                                     m_vListenerOrientationTop.y,
                                     m_vListenerOrientationTop.z,
                                     DS3D_IMMEDIATE );
    m_pXACT->SetListenerPosition( m_vListenerPosition.x,
                                  m_vListenerPosition.y,
                                  m_vListenerPosition.z,
                                  DS3D_IMMEDIATE );
    m_pXACT->SetListenerVelocity( 0.f, 0.f, 0.f, DS3D_IMMEDIATE );

    // Create the sound sources
    for( LONG i = 0; i < SOURCE_MAX; ++i )
    {
        if( FAILED( m_pXACT->CreateSoundSource( XACT_FLAG_SOUNDSOURCE_3D, &m_aSoundSources[i] ) ) )
            return E_FAIL;

        m_aSoundSources[i]->SetPosition( m_avSourcePositions[i].x, m_avSourcePositions[i].y, m_avSourcePositions[i].z, DS3D_IMMEDIATE );
        m_aSoundSources[i]->SetVelocity( 0.f, 0.f, 0.f, DS3D_IMMEDIATE );
    }

    // Create deques for the sound cue instances
    for( LONG i = 0; i < XACT_SOUNDBANK_SAMPLE_CUE_COUNT; ++i )
    {
        if( g_aCueMetaData[i].bAutorelease )
            g_aCueMetaData[i].u.nInstances = 0;
        else
        {
            g_aCueMetaData[i].u.pqInstances = new std::deque<PXACTSOUNDCUE>();
            if( !g_aCueMetaData[i].u.pqInstances )
                return E_OUTOFMEMORY;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a custom heap object for SoundCues
    g_pSoundCueHeap = new CHeapAlloc();
    if ( NULL == g_pSoundCueHeap )
        return E_OUTOFMEMORY;

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize 3D elements
    if( FAILED( InitObjectData() ) )
        return E_FAIL;

    // Initialize Audio elements
    if( FAILED( InitAudio() ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateMiscInterfaceElements()
// Desc: Updates various miscellaneous user interface elements (help, volume,
//       cue / source / listener selection)
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::UpdateMiscInterfaceElements()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
        m_bDrawHelp = !m_bDrawHelp;

    // Increase/Decrease volume
    m_fVolume += ( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_WHITE ] - 
                   m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_BLACK ] ) *
                   m_fElapsedTime * VOLUME_SCALE;

    // Make sure volume is in the appropriate range
    if( m_fVolume < DSBVOLUME_MIN )
        m_fVolume = DSBVOLUME_MIN;
    else if( m_fVolume > DSBVOLUME_MAX )
        m_fVolume = DSBVOLUME_MAX;

    // XACT doesn't provide the programmer with cue-specific volume
    // control.  Instead, the programmer controls the volume of
    // categories.  In this sample, we leave the volume mixing up to the
    // sound designer (pre-defined by the XACT project files) and
    // just control a global volume slider.
    m_pXACT->SetMasterVolume( XACT_CATEGORY_INDEX_UNUSED, (LONG)m_fVolume );

    // Cycle through the cues
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        m_iCurrentCue = ( m_iCurrentCue + XACT_SOUNDBANK_SAMPLE_CUE_COUNT - 1 ) % XACT_SOUNDBANK_SAMPLE_CUE_COUNT;
    else if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        m_iCurrentCue = ( m_iCurrentCue + 1 ) % XACT_SOUNDBANK_SAMPLE_CUE_COUNT;

    // Cycle through the sources / listeners
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        m_iCurrentObject = ( m_iCurrentObject + 1 ) % ( SOURCE_MAX + 1 );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateAudio()
// Desc: Starts and stops cues depending on the user's command
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::UpdateAudio()
{
    // Handle all pending XACT Notifications
    XACT_NOTIFICATION xactNotification;
    while( m_pXACT->GetNotification( NULL, &xactNotification ) == S_OK )
    {
        // This sample only handles a single notification and therefore
        // we don't do much validation as to the type of notification
        // we receive.  However, if more notifications are registered,
        // more thorough validation of the notification description would
        // be required.

        // Another option is to ask for only specific types of notifications
        // when calling GetNotification.  Providing the same
        // XACT_NOTIFICATION_DESCRIPTION structure as the one used to
        // register the notification guarantees the only notification
        // returned by GetNotification() matches the registered notification

        // We should only get stop notifications for our single soundbank
        assert( xactNotification.Header.u.pSoundBank == m_pSoundBank &&
                xactNotification.Header.wType        == eXACTNotification_Stop );

        // Look through the array of cue metadata to find this cue
        // since our array is not arranged by cue index
        for( LONG i = 0; i < XACT_SOUNDBANK_SAMPLE_CUE_COUNT; ++i )
        {
            if( g_aCueMetaData[i].nCueIndex == xactNotification.Header.dwSoundCueIndex )
            {
                // If the cue was autorelease, update our count of the
                // number of active instances.
                // If the cue was non-autorelease, it still might have
                // stopped without us explicitly telling it to do so.
                // Therefore, we need to remove our pointer to the instance
                // if we still have it
                if( g_aCueMetaData[i].bAutorelease )
                    g_aCueMetaData[i].u.nInstances--;
                else
                {
                    std::deque<PXACTSOUNDCUE>::iterator iInstance;

                    // Iterate through the deque.  If we find the
                    // instance, remove it and break
                    for( iInstance = g_aCueMetaData[i].u.pqInstances->begin(); iInstance != g_aCueMetaData[i].u.pqInstances->end(); ++iInstance )
                    {
                        if( *iInstance == xactNotification.Header.pSoundCue )
                        {
                            // We need to call Stop() on the soundcue so that
                            // the memory associated with it is freed
                            m_pSoundBank->Stop( XACT_SOUNDCUE_INDEX_UNUSED, 0, *iInstance );
                            g_aCueMetaData[i].u.pqInstances->erase( iInstance );
                            break;
                        }
                    }
                }

                break;
            }
        }
    }

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        // Start playing an instance of this cue with
        // properties defined by its metadata above
        DWORD               dwFlags;
        PXACTSOUNDSOURCE    pSoundSource = NULL;
        PXACTSOUNDCUE*      ppSoundCue = NULL;
        PXACTSOUNDCUE       pTempSoundCue = NULL;

        // If it's an autoreleased cue, don't save a pointer
        if( g_aCueMetaData[m_iCurrentCue].bAutorelease )
        {
            ppSoundCue  = NULL;
            dwFlags     = XACT_FLAG_SOUNDCUE_AUTORELEASE;
        }
        else
        {
            ppSoundCue  = &pTempSoundCue;
            dwFlags     = 0;
        }

        // Bind this cue to the proper source
        if( g_aCueMetaData[m_iCurrentCue].nSourceIndex == SOURCE_DEFAULT )
            pSoundSource = NULL;
        else
            pSoundSource = m_aSoundSources[ g_aCueMetaData[m_iCurrentCue].nSourceIndex ];

        // Play an instance of the cue
        if( FAILED( m_pSoundBank->Play( g_aCueMetaData[m_iCurrentCue].nCueIndex, pSoundSource, dwFlags, ppSoundCue ) ) )
            return E_FAIL;

        // If we saved a pointer to the instance, place it in the instance deque
        // Otherwise, increment the count of playing instances of that cue
        if( ppSoundCue )
            g_aCueMetaData[m_iCurrentCue].u.pqInstances->push_back( *ppSoundCue );
        else
            g_aCueMetaData[m_iCurrentCue].u.nInstances++;
    }
    else if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
    {
        // If this is an autoreleased sound, stop playing all instances of it.
        // If not, stop playing the oldest instance
        if( g_aCueMetaData[m_iCurrentCue].bAutorelease )
        {
            // Only stop the cue if there is an instance to stop
            if( g_aCueMetaData[m_iCurrentCue].u.nInstances > 0 )
            {
                // Just call Stop on the whole index - we'll get stop notifications
                // for each instance and use that to update nInstances
                m_pSoundBank->Stop( g_aCueMetaData[m_iCurrentCue].nCueIndex, 0, NULL );
            }
        }
        else
        {
            // Only stop the cue if there is an instance to stop
            if( g_aCueMetaData[m_iCurrentCue].u.pqInstances->size() )
            {
                // Explicitly stop the oldest instance of the cue
                m_pSoundBank->Stop( XACT_SOUNDCUE_INDEX_UNUSED, 0, g_aCueMetaData[m_iCurrentCue].u.pqInstances->front() );
                g_aCueMetaData[m_iCurrentCue].u.pqInstances->pop_front();
            }
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateObjectData()
// Desc: Updates position and velocity of listener and sound sources
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::UpdateObjectData()
{
    // Point to the controlled object and save its current position
    // (so that we can calculate velocity and orientation later)
    D3DXVECTOR3  vOldPosition;
    D3DXVECTOR3* pvControl;

    if( m_iCurrentObject == SOURCE_MAX )
        pvControl = &m_vListenerPosition;
    else
        pvControl = &m_avSourcePositions[m_iCurrentObject];

    vOldPosition = *pvControl;

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
    if( m_iCurrentObject == SOURCE_MAX &&
        ( m_vListenerPosition.x != vOldPosition.x ||
          m_vListenerPosition.z != vOldPosition.z ) )
    {
        D3DXVECTOR3 vDelta = m_vListenerPosition - vOldPosition;
        m_fListenerAngle = FLOAT( atan2( vDelta.x, vDelta.z ) );

        // set default listener orientation
        m_pXACT->SetListenerOrientation( vDelta.x, 0.0f, vDelta.z,
                                         m_vListenerOrientationTop.x,
                                         m_vListenerOrientationTop.y,
                                         m_vListenerOrientationTop.z,
                                         DS3D_DEFERRED );
    }

    // Change the position of the current object if it has changed
    if( *pvControl != vOldPosition )
    {
        if( m_iCurrentObject == SOURCE_MAX )
        {
            m_pXACT->SetListenerPosition( pvControl->x, pvControl->y, pvControl->z, DS3D_DEFERRED );
        }
        else
        {
            m_aSoundSources[m_iCurrentObject]->SetPosition( pvControl->x, pvControl->y, pvControl->z, DS3D_DEFERRED );
        }
    }

    // Set the velocity of the current object
    D3DXVECTOR3 vVelocity = ( *pvControl - vOldPosition ) / m_fElapsedTime;
    if( m_iCurrentObject == SOURCE_MAX )
    {
        m_pXACT->SetListenerVelocity( vVelocity.x, vVelocity.y, vVelocity.z, DS3D_DEFERRED );
    }
    else
    {
        m_aSoundSources[m_iCurrentObject]->SetVelocity( vVelocity.x, vVelocity.y, vVelocity.z, DS3D_DEFERRED );
    }

    // Commit position/velocity changes
    m_pXACT->CommitDeferredSettings();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Let XACT do work
    XACTEngineDoWork();

    // Update help, volume, cue selection, and source/listener selection
    if( FAILED( UpdateMiscInterfaceElements() ) )
        return E_FAIL;

    // Update the audio being played (start/stop cues)
    if( FAILED( UpdateAudio() ) )
        return E_FAIL;

    // Update internal object data, as well as XACT as to the position
    // and velocity of the sources and listeners
    if( FAILED( UpdateObjectData() ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
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
    m_pd3dDevice->SetStreamSource( 0, m_pvbFloor, sizeof(D3DVERTEX) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Draw the grid
    m_pd3dDevice->SetStreamSource( 0, m_pvbGrid, sizeof(D3DVERTEX) );
    m_pd3dDevice->DrawPrimitive( D3DPT_LINELIST, 0, 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) );

    // Draw the sources
    for( LONG i = 0; i < SOURCE_MAX; ++i )
    {
        D3DXMATRIX matSource;
        D3DXMatrixTranslation( &matSource, 
                               m_avSourcePositions[i].x,
                               m_avSourcePositions[i].y,
                               m_avSourcePositions[i].z );
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &matSource );
        m_pd3dDevice->SetStreamSource( 0, m_avbSources[i], sizeof(D3DVERTEX) );
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
        m_pd3dDevice->SetStreamSource( 0, m_pvbListener, sizeof(D3DVERTEX) );
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 1 );
    }

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        WCHAR strBuffer[200];

        m_Font.Begin();

        // Show frame rate
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"Custom Memory Allocator (Using XACT)" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Show status
        m_Font.DrawText( 64, 100, 0xffffffff, L"Current Object: " );
        m_Font.DrawText( g_aObjectColors[m_iCurrentObject], g_aObjectNames[m_iCurrentObject] );

        if( m_iCurrentObject == SOURCE_MAX )
            swprintf( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_vListenerPosition.x, 
                                                           m_vListenerPosition.y, 
                                                           m_vListenerPosition.z );
        else
            swprintf( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_avSourcePositions[m_iCurrentObject].x,
                                                           m_avSourcePositions[m_iCurrentObject].y, 
                                                           m_avSourcePositions[m_iCurrentObject].z );
        m_Font.DrawText( 64, 130, 0xffffffff, L"Object Position: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        if( g_aCueMetaData[m_iCurrentCue].nSourceIndex == SOURCE_DEFAULT )
            swprintf( strBuffer, L" [None]" );
        else
            swprintf( strBuffer, L" [%s]", g_aObjectNames[ g_aCueMetaData[m_iCurrentCue].nSourceIndex ] );
        m_Font.DrawText( 64, 190, 0xffffffff, L"Current Cue:" );
        m_Font.DrawText( 64, 215, 0xffffff00, g_aCueMetaData[m_iCurrentCue].strDesc );
        m_Font.DrawText( 0xffffff00, strBuffer );

        swprintf( strBuffer, L"%d", g_aCueMetaData[m_iCurrentCue].bAutorelease ?
                                       g_aCueMetaData[m_iCurrentCue].u.nInstances :
                                       g_aCueMetaData[m_iCurrentCue].u.pqInstances->size() );
        m_Font.DrawText( 64, 250, 0xffffffff, L"Instances: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, m_fVolume / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(m_fVolume) - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 360, 0xffffffff, L"Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
