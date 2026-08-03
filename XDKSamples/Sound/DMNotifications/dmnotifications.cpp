//-----------------------------------------------------------------------------
// File: DMNotifications.cpp
//
// Desc: Demonstrates how to use DirectMusic Notification messages to get
//       information about audio playback.
//
// Hist: 04.06.01 - Created
//       03.06.02 - Added audio level meters for April 02 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include <xgraphics.h>
#include <dmusici.h>
#include "dmusicfx.h"
#include "myfactory.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nplayback" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Segment/boundary\nselect" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Play\ncontrol" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Increase\nvolume" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Decrease\nvolume" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);


// Structure to represent boundary flags and description
typedef struct
{
    DMUS_SEGF_FLAGS Flags;
    WCHAR*          strLabel;
} BOUNDARY;

// List of supported boundaries
BOUNDARY g_aBoundaries[] =
{
    { DMUS_SEGF_FLAGS(0), L"Immediately" },
    { DMUS_SEGF_GRID,     L"Grid Boundary" },
    { DMUS_SEGF_BEAT,     L"Beat Boundary" },
    { DMUS_SEGF_MEASURE,  L"Measure Boundary" },
};
static const DWORD NUM_BOUNDARIES = sizeof( g_aBoundaries ) / sizeof( g_aBoundaries[0] );

char* g_strSegments[] = 
{
    "Bach Invention.sgt", 
    "ClaireDeLune.sgt", 
    "BrassAction.sgt",  
    "FurElise.sgt",
};
static const DWORD NUM_SEGMENTS = sizeof( g_strSegments ) / sizeof( g_strSegments[0] );

struct PLAYSTATE
{
    DWORD                       dwSegment;
    IDirectMusicSegment8 *      pSegment;
    IDirectMusicSegmentState8 * pSegState;
    BOOL                        bPlaying;
};

static const FLOAT BEAT_DISPLAY_TIME = 0.1f;    // 100ms display time




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    HRESULT LoadSegment( PLAYSTATE* ps, DWORD dwSegment );

    // Font and help
    CXBFont     m_Font;
    CXBHelp     m_Help;

    BOOL        m_bDrawHelp;

    DWORD       m_dwBoundary;   // Current Boundary
    DWORD       m_dwCurrent;
    FLOAT       m_fVolume;

    FLOAT       m_fBeat;
    WCHAR       m_strSegment[100];
    
    PLAYSTATE   m_aStates[2];

    IDirectMusicPerformance8* m_pDMPerformance;
    IDirectMusicLoader8*      m_pDMLoader;
    IDirectMusicAudioPath8*   m_pMusicAudioPath;
    IDirectMusicGraph8*       m_pDMGraph;
    LPDIRECTSOUND8            m_pDSound;           // DirectSound object

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
    ZeroMemory( m_aStates, sizeof( PLAYSTATE ) * 2 );
    m_dwBoundary = 0;
    m_dwCurrent  = 0;
    m_bDrawHelp = FALSE;
    m_strSegment[0] = 0;
}




//-----------------------------------------------------------------------------
// Name: LoadSegment()
// Desc: Transitions the playstate to a different segment.  Stops playback
//       if it's currently playing, releases current segment, loads and 
//       downloads new segment, and restarts playback if needed.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::LoadSegment( PLAYSTATE* ps, DWORD dwSegment )
{
    // Shut down the current segment
    if( ps->bPlaying )
    {
        m_pDMPerformance->StopEx( ps->pSegState, 0, 0 );
        ps->pSegState->Release();
        ps->pSegState = NULL;
    }

    if( ps->pSegment )
    {
        ps->pSegment->Release();
        ps->pSegment = NULL;
    }

    // Load the new segment
    if( FAILED( m_pDMLoader->LoadObjectFromFile( CLSID_DirectMusicSegment, 
                                                 IID_IDirectMusicSegment8,
                                                 g_strSegments[ dwSegment ], 
                                                 (VOID **)&ps->pSegment ) ) )
        return E_FAIL;

    ps->dwSegment = dwSegment;
    if( ps->bPlaying )
    {
        m_pDMPerformance->PlaySegmentEx( ps->pSegment, 
                                         NULL, 
                                         NULL, 
                                         0, 
                                         0, 
                                         &ps->pSegState, 
                                         NULL, 
                                         NULL );
    }

    return S_OK;
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
    if( FAILED( XAudioDownloadEffectsImage( "d:\\media\\dmusicfx.bin",
                                            &EffectLoc,
                                            XAUDIO_DOWNLOADFX_EXTERNFILE,
                                            NULL ) ) )
        return E_FAIL;

    // Initialize DMusic
    IDirectMusicHeap* pNormalHeap;
    DirectMusicCreateDefaultHeap( &pNormalHeap );

    IDirectMusicHeap* pPhysicalHeap;
    DirectMusicCreateDefaultPhysicalHeap( &pPhysicalHeap );

    DirectMusicInitializeEx( pNormalHeap, pPhysicalHeap, MyFactory );

    pNormalHeap->Release();
    pPhysicalHeap->Release();

    // Create loader object
    DirectMusicCreateInstance( CLSID_DirectMusicLoader, NULL, 
                               IID_IDirectMusicLoader8, (VOID**)&m_pDMLoader );

    // Create performance object
    DirectMusicCreateInstance( CLSID_DirectMusicPerformance, NULL,
                               IID_IDirectMusicPerformance8, (VOID**)&m_pDMPerformance );

    // Initialize the performance with the standard audio path.
    // The flags (final) argument allows us to specify whether or not we want
    // DirectMusic to create a thread on our behalf to process music, using 
    // DMUS_INITAUDIO_NOTHREADS.  The default is for DirectMusic to create its
    // own thread; DMUS_INITAUDIO_NOTHREADS tells DirectMusic not to do this, 
    // and the app will periodically call DirectMusicDoWork().  For software 
    // emulation on alpha hardware, it's generally better to have DirectMusic
    // create its own thread. On real hardware, periodically calling 
    // DirectMusicDoWork may provide a better option.
    m_pDMPerformance->InitAudioX( DMUS_APATH_SHARED_STEREOPLUSREVERB, 96, 128, 0 );

    // Tell DirectMusic where the default search path is
    m_pDMLoader->SetSearchDirectory( GUID_DirectMusicAllTypes, 
                                   "D:\\Media\\Sounds", FALSE );
    
    // Load primary segment
    m_pDMLoader->LoadObjectFromFile( CLSID_DirectMusicSegment, IID_IDirectMusicSegment8, 
                                   g_strSegments[0], (VOID**)&m_aStates[0].pSegment);
    // Load secondary segment
    m_pDMLoader->LoadObjectFromFile( CLSID_DirectMusicSegment, IID_IDirectMusicSegment8, 
                                   g_strSegments[0], (VOID**)&m_aStates[1].pSegment );

    // Play segment on the default audio path
    m_pDMPerformance->PlaySegmentEx( m_aStates[0].pSegment, NULL, NULL, 0, 
                                   0, &m_aStates[0].pSegState, NULL, NULL );
    m_aStates[0].bPlaying = TRUE;

    // Get default (music) audiopath.
    m_pDMPerformance->GetDefaultAudioPath( &m_pMusicAudioPath );

    // Max volume for music
    m_fVolume = DSBVOLUME_MAX;
    m_pMusicAudioPath->SetVolume( (LONG)m_fVolume, 0 );

    // Set up to receive the notifications we're interested in
    GUID guid = GUID_NOTIFICATION_SEGMENT;
    m_pDMPerformance->AddNotificationType( guid );
    guid = GUID_NOTIFICATION_MEASUREANDBEAT;
    m_pDMPerformance->AddNotificationType( guid );

    return S_OK;
}




#define VOLUME_SCALE 5.0f
//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    DMUS_NOTIFICATION_PMSG* pPMsg;
    PLAYSTATE* ps = &m_aStates[ m_dwCurrent ];

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Toggle playback
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        if( ps->bPlaying )
        {
            m_pDMPerformance->StopEx( ps->pSegState, 0, g_aBoundaries[ m_dwBoundary ].Flags );
        }
        else
        {
            DWORD dwFlags = ( m_dwCurrent == 0 ) ? 0 : DMUS_SEGF_SECONDARY;

            m_pDMPerformance->PlaySegmentEx( ps->pSegment, NULL, NULL, 
                                             g_aBoundaries[ m_dwBoundary ].Flags | dwFlags, 
                                             0, &ps->pSegState, NULL, NULL );
            if( m_dwCurrent != 0 && g_aBoundaries[ m_dwBoundary ].Flags != 0 )
                swprintf( m_strSegment, L"Secondary Segment Pending" );
        }
        if( m_dwCurrent == 0 )
            ps->bPlaying ^= 1;
    }

    // Toggle primary vs. secondary segment
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
    {
        m_dwCurrent = ( m_dwCurrent + 1 ) % 2;
    }

    // Select boundary
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_dwBoundary = ( m_dwBoundary + NUM_BOUNDARIES - 1 ) % NUM_BOUNDARIES;
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_dwBoundary = ( m_dwBoundary + 1 ) % NUM_BOUNDARIES;
    }

    // Select song
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT  )
    {
        LoadSegment( ps, ( ps->dwSegment - 1 ) % NUM_SEGMENTS );
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        LoadSegment( ps, ( ps->dwSegment + 1 ) % NUM_SEGMENTS );
    }

    // Increase/Decrease volume
    m_fVolume += ( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_WHITE ] - 
                   m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_BLACK ] ) *
                 m_fElapsedTime * VOLUME_SCALE;

    // Make sure volume is in the appropriate range
    if( m_fVolume < DSBVOLUME_HW_MIN )
        m_fVolume = DSBVOLUME_HW_MIN;
    else if( m_fVolume > DSBVOLUME_MAX )
        m_fVolume = DSBVOLUME_MAX;
    m_pMusicAudioPath->SetVolume( (LONG)m_fVolume, 0 );

    // Track time since the last beat
    m_fBeat -= m_fElapsedTime;
    if( m_fBeat < 0.0f )
        m_fBeat = 0.0f;

    // Check for notifications
    while( S_OK == m_pDMPerformance->GetNotificationPMsg( &pPMsg ) )
    {
        IDirectMusicSegmentState8 * pSegState = NULL;

        if( SUCCEEDED( pPMsg->punkUser->QueryInterface( IID_IDirectMusicSegmentState8, (void **)&pSegState ) ) )

        // We're interested in segment notifications on the secondary 
        // segment, and beat notifications on the primary segment
        if( pPMsg->guidNotificationType == GUID_NOTIFICATION_SEGMENT )
        {
            if( pSegState == m_aStates[1].pSegState )
            {
                // Got a segment notification for the secondary...
                switch( pPMsg->dwNotificationOption )
                {
                    case DMUS_NOTIFICATION_SEGABORT:
                        swprintf( m_strSegment, L"Secondary Segment Aborted" );
                        m_aStates[1].bPlaying = FALSE;
                        break;
                    case DMUS_NOTIFICATION_SEGALMOSTEND:
                        swprintf( m_strSegment, L"Secondary Segment almost at end" );
                        break;
                    case DMUS_NOTIFICATION_SEGLOOP:
                        swprintf( m_strSegment, L"Secondary Segment has looped" );
                        break;
                    case DMUS_NOTIFICATION_SEGSTART:
                        swprintf( m_strSegment, L"Secondary Segment Playing" );
                        m_aStates[1].bPlaying = TRUE;
                        break;
                    case DMUS_NOTIFICATION_SEGEND:
                        swprintf( m_strSegment, L"Secondary Segment Completed" );
                        m_aStates[1].bPlaying = FALSE;
                        break;
                }
            }
            else if( pSegState == m_aStates[0].pSegState && 
                     pPMsg->dwNotificationOption == DMUS_NOTIFICATION_SEGEND )
            {
                // Primary segment ended - restart
                m_pDMPerformance->PlaySegmentEx( m_aStates[0].pSegment, NULL, NULL, 0, 
                                                 0, &m_aStates[0].pSegState, NULL, NULL );
            }
        }
        else if( pSegState == m_aStates[0].pSegState &&
                 pPMsg->guidNotificationType == GUID_NOTIFICATION_MEASUREANDBEAT )
        {
            // Got a beat notification for the primary segment
            m_fBeat = BEAT_DISPLAY_TIME;
        }

        m_pDMPerformance->FreePMsg( (DMUS_PMSG*)pPMsg );
    }


    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff408040, 0xff404040 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        WCHAR strBuffer[100];

        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"DMNotifications" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 64, 100, 0xffffffff, L"Primary segment: " );
        m_Font.DrawText( m_dwCurrent==0 ? 0xffffffff : 0x00000000, GLYPH_LEFT_TICK );
        swprintf( strBuffer, L"%S", g_strSegments[ m_aStates[0].dwSegment ] );
        m_Font.DrawText( m_aStates[0].bPlaying ? 0xffffff00 : 0xff808000, strBuffer );
        m_Font.DrawText( m_dwCurrent==0 ? 0xffffffff : 0x00000000, GLYPH_RIGHT_TICK );
        
        m_Font.DrawText( 64, 130, 0xffffffff, L"Secondary segment: " );
        m_Font.DrawText( m_dwCurrent==1 ? 0xffffffff : 0x00000000, GLYPH_LEFT_TICK );
        swprintf( strBuffer, L"%S", g_strSegments[ m_aStates[1].dwSegment ] );
        m_Font.DrawText( m_aStates[1].bPlaying ? 0xffffff00 : 0xff808000, strBuffer );
        m_Font.DrawText( m_dwCurrent==1 ? 0xffffffff : 0x00000000, GLYPH_RIGHT_TICK );
        
        m_Font.DrawText( 64, 160, 0xffffffff, L"Boundary: " );
        m_Font.DrawText( 0xffffff00, g_aBoundaries[m_dwBoundary].strLabel );

        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, m_fVolume / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(m_fVolume) - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 190, 0xffffffff, L"Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        // Display beat/segment notifications
        FLOAT fAlpha = 0.2f + 0.8f * ( m_fBeat / BEAT_DISPLAY_TIME );
        m_Font.DrawText( 200, 240, 0x00ffffff | ( DWORD( 0xFF * fAlpha ) << 24 ), L"Primary Beat" );
        m_Font.DrawText( 200, 280, 0xffffffff, m_strSegment );
        m_Font.End();
    }

    // Draw the on-screen audio level meters
    XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

