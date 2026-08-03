//-----------------------------------------------------------------------------
// File: XActWMAPlayList.cpp
//
// Desc: XActWMAPlayList is a sample that demonstrates how to use XACT to
//       manage user created soundtracks
//
// Hist: 06.07.02 - New for the July 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>
#include <dsstdfx.h>
#include <xact.h>
#include <vector>
#include "XactSounds.h"

using namespace std;


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nplayback" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Next\nsoundtrack" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Prev\nsoundtrack" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_1, L"Prev/next song" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_1, L"Volume\ndown" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Volume\nup" },
    { XBHELP_MISC_CALLOUT, XBHELP_PLACEMENT_2, GLYPH_LEFT_BUTTON L" Trigger: Toggle random song\n"
                                               GLYPH_RIGHT_BUTTON L" Trigger: Toggle random soundtrack" },
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )




//-----------------------------------------------------------------------------
// Audio engine information
//-----------------------------------------------------------------------------

// Volume scaling factor
static const FLOAT VOLUME_SCALE = 5.0f;

// Current state of audio playback
enum MM_STATE 
{
    MM_STOPPED,
    MM_PAUSED,
    MM_PLAYING,
};


// Here is our game soundtrack including WMA files we ship
// with our game.  If needed, this concept could be extended
// to include several different game soundtracks

// Structure representing a soundtrack song (used for both
// game provided soundtracks, as well as user created
// soundtracks)
struct MM_SONG
{
    WCHAR*  strName;
    CHAR*   strFilename;
    DWORD   dwLength;
};

// Structure representing a game provided soundtrack
// All game provided soundtracks must consist of
// WMA file(s) in directories.  Soundtrack directories
// should be devoid of other filetypes, since XACT
// enumerates all files
struct MM_GAMESOUNDTRACK
{
    WCHAR*  strName;
    CHAR*   strDir;
    UINT    uNumSongs;
};

// In this sample, we only care about one game soundtrack
// We also know we have 1 song in this soundtrack.
// If need be, the number of songs can be obtained by using
// FindFirstFile() and FindNextFile() at runtime
MM_GAMESOUNDTRACK g_aGameSoundtracks[] =
{
    { L"Game Soundtrack 1", "d:\\media\\sounds\\soundtrack", 1 },
};

#define NUM_GAME_SOUNDTRACKS ( sizeof(g_aGameSoundtracks) / sizeof(g_aGameSoundtracks[0]) )

// We will reserve one particular cue for soundtracks
// This cue should not have anything important bound to it
CONST CHAR g_strSoundtrackCue[] = "Cue1";

// A soundtrack category enables us to control various aspects
// of multiple cues simultaneously (i.e. volume).
#define SOUNDTRACK_CATEGORY XACT_CATEGORY_BGMUSIC




//-----------------------------------------------------------------------------
// Name: class CSoundtrack
// Desc: Abstraction layer for soundtracks that help merge together game
//       soundtracks and user soundtracks stored on the Xbox hard drive
//-----------------------------------------------------------------------------
class CSoundtrack
{
public:
    CSoundtrack() {}

    VOID    GetSoundtrackName( WCHAR* strName ) { wcscpy( strName, m_strName ); }
    UINT    GetSongCount() { return m_uSongCount; }

    WCHAR       m_strName[MAX_SOUNDTRACK_NAME];
    UINT        m_uSongCount;
    BOOL        m_bGameSoundtrack;
    union 
    {
        UINT    m_uSoundtrackID;
        UINT    m_uSoundtrackIndex;
    };
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Utility objects
    CXBFont                 m_Font;                             // Font object
    CXBHelp                 m_Help;                             // Help object
    BOOL                    m_bDrawHelp;                        // TRUE to draw help screen

    // State variables
    BOOL                    m_bGlobal;                          // TRUE to randomize among soundtracks
    BOOL                    m_bRandom;                          // TRUE to randomize among songs
    FLOAT                   m_fVolume;                          // Volume level
    MM_STATE                m_mmState;                          // Current engine state
    WCHAR                   m_strCurrentSoundtrackName[MAX_SOUNDTRACK_NAME];  // Current soundtrack's name
    WCHAR                   m_strCurrentSongName[MAX_SONG_NAME];  // Current song's name
    DWORD                   m_dwCurrentSongLength;              // Current song's length

    // Music information
    vector<CSoundtrack>     m_vSoundtracks;                     // Vector of soundtracks
    UINT                    m_uCurrentSoundtrack;               // Currently selected soundtrack

    // Timing information
    LARGE_INTEGER           m_liLastTime;                       // Used to keep track of time passing between frames
    LARGE_INTEGER           m_liPlayTime;                       // Used to keep track of play time of the current song
    LARGE_INTEGER           m_liPerfCounterFrequency;           // Performance counter frequency

    // XACT Engine
    IXACTEngine*            m_pXACT;                            // XACT Engine
    BYTE*                   m_pbSoundBank;                      // Sound Bank data
    PXACTSOUNDBANK          m_pSoundBank;                       // XACT Sound Bank
    DWORD                   m_dwSoundCueIndex;                  // Current sound cue index
    PXACTWMAPLAYLIST        m_pWMAPlaylist;                     // WMA Playlist for soundtracks

    HRESULT Play();                                             // Start playing
    HRESULT Stop();                                             // Stop playback
    HRESULT Pause();                                            // Pause playback
    MM_STATE GetStatus() { return m_mmState; }                  // Returns current playback status
    HRESULT SetRandom( BOOL bRandom );                          // Change random mode
    BOOL    GetRandom() { return m_bRandom; }                   // Get random mode
    HRESULT SetGlobal( BOOL bGlobal );                          // Toggle global mode
    BOOL    GetGlobal() { return m_bGlobal; }                   // Get global mode
    HRESULT SetVolume( FLOAT fVolume );                         // Set volume level
    FLOAT   GetVolume() { return m_fVolume; }                   // Get volume level

    HRESULT GetCurrentInfo( WCHAR * strSoundtrack, WCHAR * strSong, DWORD * pdwLength );  // Retrieves info about the current song
    HRESULT NextSoundtrack();                                   // Switch to next soundtrack
    HRESULT NextSong();                                         // Switch to next song
    HRESULT PrevSoundtrack();                                   // Switch to prev soundtrack
    HRESULT PrevSong();                                         // Switch to prev song

    HRESULT LoadSoundtracks();                                  // Fill our soundtrack cache
    HRESULT SelectSoundtrack( DWORD dwSoundtrack, BOOL fInitialize = FALSE ); // Switch to a soundtrack

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
CXBoxSample::CXBoxSample() :
            CXBApplication(),
            m_vSoundtracks()
{
    m_bDrawHelp                             = FALSE;

    m_mmState                               = MM_STOPPED;
    m_bRandom                               = FALSE;
    m_bGlobal                               = TRUE;
    m_fVolume                               = DSBVOLUME_MAX;
    m_liLastTime.QuadPart                   = 0;
    m_liPlayTime.QuadPart                   = 0;
    m_liPerfCounterFrequency.QuadPart       = 0;

    m_pXACT                                 = NULL;
    m_pbSoundBank                           = NULL;
    m_pSoundBank                            = NULL;
    m_pWMAPlaylist                          = NULL;

    m_uCurrentSoundtrack                    = 0;

    m_strCurrentSoundtrackName[0]           = 0;
    m_strCurrentSongName[0]                 = 0;
    m_dwCurrentSongLength                   = 0;
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

    // Get the performance counter's frequency
    QueryPerformanceFrequency( &m_liPerfCounterFrequency );

    // Initialize the XACT Engine

    // Create the XACT runtime engine
    // Note: We only really need one concurrent stream here, but
    // concurrent streams are also used for auditioning, so we'll
    // just leave a few extra.
    // The dwMaxConcurrentStreams parameter may be removed in a future
    // release of the XACT audio library, but it's here for the July 2002 XDK
    XACT_RUNTIME_PARAMETERS xrParams;
    xrParams.dwMax2DHwVoices        = 128;
    xrParams.dwMax3DHwVoices        = 32;
    xrParams.dwMaxConcurrentStreams = 16;
    xrParams.dwMaxNotifications     = 0;
    if( FAILED( XACTEngineCreate( &xrParams, &m_pXACT ) ) )
        return E_FAIL;

    // Register our (almost) empty soundbank with XACT
    //
    // All we really need is a soundbank with a single empty cue (i.e., one
    // that does not bind to any audio files), however, XACT does not allow
    // soundbanks consisting only of empty cues.  Empty cues can reside among
    // other functioning cues, but not alone.
    //
    // The XactSounds.xsb soundbank in this sample has a single cue that binds to
    // a small audio file in a wavebank.  Note that we have not loaded a
    // wavebank anywhere in this code.  This is because we are going to use the
    // cue in this soundbank for a WMA playlist, and we will never play the
    // audio to which the cue refers
    DWORD dwFileSize;
    if( FAILED( XBUtil_LoadFile( "d:\\media\\sounds\\XactSounds.xsb", (VOID **)&m_pbSoundBank, &dwFileSize ) ) )
        return E_FAIL;
    if( FAILED( m_pXACT->CreateSoundBank( m_pbSoundBank, dwFileSize, &m_pSoundBank ) ) )
        return E_FAIL;

    // Download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\media\\dsstdfx.bin", 
                                            &EffectLoc, 
                                            XAUDIO_DOWNLOADFX_EXTERNFILE, 
                                            NULL ) ) )
        return E_FAIL;

    // Find the index of the cue we've reserved for soundtracks
    if( FAILED( m_pSoundBank->GetSoundCueIndexFromFriendlyName( g_strSoundtrackCue, &m_dwSoundCueIndex ) ) )
        return E_FAIL;

    // Register a start notification with the XACT engine This will allow us
    // to monitor when a new song begins (so we can restart our play timer)
    XACT_NOTIFICATION_DESCRIPTION xactNotificationDesc;

    ZeroMemory( &xactNotificationDesc, sizeof( xactNotificationDesc ) );
    xactNotificationDesc.wType              = eXACTNotification_Start;
    xactNotificationDesc.wFlags             = XACT_FLAG_NOTIFICATION_PERSIST |              // Always keep this notification registered (i.e., don't just notify once)
                                              XACT_FLAG_NOTIFICATION_USE_SOUNDCUE_INDEX;    // Only notify when it pertains to this sound cue index
    xactNotificationDesc.u.pSoundBank       = m_pSoundBank;
    xactNotificationDesc.dwSoundCueIndex    = m_dwSoundCueIndex;
    xactNotificationDesc.hEvent             = NULL;

    if( FAILED( m_pXACT->RegisterNotification( &xactNotificationDesc ) ) )
        return E_FAIL;

    // Load up soundtrack information
    if( FAILED( LoadSoundtracks() ) )
        return E_FAIL;

    // Initialize the random number generator
    srand( GetTickCount() );

    // Select the first soundtrack
    SelectSoundtrack( 0, TRUE );

    // Play the first song
    Play();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Let XACT do some work
    XACTEngineDoWork();

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

        // Reset the play timer if a new song has started on the cue
        // Also, cache the song's information

        // Note that we cache the song's information here rather than
        // call GetCurrentInfo every frame.  See GetCurrentInfo's comment
        // header for more information.
        if( xactNotification.Header.dwSoundCueIndex == m_dwSoundCueIndex &&
            xactNotification.Header.u.pSoundBank    == m_pSoundBank &&
            xactNotification.Header.wType           == eXACTNotification_Start )
        {
            m_liPlayTime.QuadPart = 0;
            GetCurrentInfo( m_strCurrentSoundtrackName, m_strCurrentSongName, &m_dwCurrentSongLength );
        }
    }

    // Add to the play time of the current song if it has been playing
    if( GetStatus() == MM_PLAYING )
    {
        LARGE_INTEGER liTemp;

        QueryPerformanceCounter( &liTemp );

        m_liPlayTime.QuadPart += liTemp.QuadPart - m_liLastTime.QuadPart;
        m_liLastTime           = liTemp;
    }

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
        m_bDrawHelp = !m_bDrawHelp;

    // Increase/Decrease volume
    FLOAT fVolumeDelta;
    fVolumeDelta = ( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_WHITE ] - 
                     m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_BLACK ] ) *
                   m_fElapsedTime * VOLUME_SCALE;

    if( fVolumeDelta != 0 )
        SetVolume( GetVolume() + fVolumeDelta );

    // Pause / Unpause
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        if( GetStatus() == MM_PLAYING )
            Pause();
        else
            Play();
    }

    // Next/Prev Soundtrack
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        NextSoundtrack();
    else if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] )
        PrevSoundtrack();

    // Next/Prev Song
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        NextSong();
    else if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        PrevSong();

    // Toggle random selection
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_LEFT_TRIGGER ] )
        SetRandom( !GetRandom() );

    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_RIGHT_TRIGGER ] )
        SetGlobal( !GetGlobal() );

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
        m_Font.DrawText( 48, 36, 0xffffffff, L"XActWMAPlayList" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        WCHAR strBuffer[100];

        swprintf( strBuffer, L"%s", m_strCurrentSoundtrackName );
        m_Font.DrawText( 64, 80, 0xffffffff, L"Soundtrack: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        swprintf( strBuffer, L"%s", m_strCurrentSongName );
        m_Font.DrawText( 64, 110, 0xffffffff, L"Song: " );
        m_Font.DrawText( ( GetStatus() == MM_PLAYING ) ? 0xffffff00 : 0xff808000, strBuffer );
        if( GetStatus() != MM_PLAYING )
            m_Font.DrawText( 0xff808000, L" (not playing)" );

        swprintf( strBuffer, L"%s", GetRandom() ? L"Random" : L"Sequential" );
        m_Font.DrawText( 64, 170, 0xffffffff, L"Song Selection: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        swprintf( strBuffer, L"%s", GetGlobal() ? L"Random" : L"Sequential" );
        m_Font.DrawText( 64, 200, 0xffffffff, L"Soundtrack Selection: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        LARGE_INTEGER liSeconds, liMinutes;

        liSeconds.QuadPart = m_liPlayTime.QuadPart / m_liPerfCounterFrequency.QuadPart;
        liMinutes.QuadPart = liSeconds.QuadPart / 60;
        liSeconds.QuadPart = liSeconds.QuadPart - liMinutes.QuadPart * 60;
        swprintf( strBuffer, L"%02I64d : %02I64d", liMinutes.QuadPart, liSeconds.QuadPart );
        m_Font.DrawText( 64, 230, 0xffffffff, L"Play Time: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, GetVolume() / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(GetVolume()) - 50 ) / 100, fPercent );
        m_Font.DrawText( 64, 260, 0xffffffff, L"Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: LoadSoundtracks()
// Desc: Loads soundtrack info - both game provided soundtracks and user
//       created soundtracks are enumerated
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::LoadSoundtracks()
{
    // Add each game provided soundtrack to the soundtrack vector
    for( UINT i = 0; i < NUM_GAME_SOUNDTRACKS; ++i )
    {
        CSoundtrack sndtrk;

        sndtrk.m_bGameSoundtrack    = TRUE;
        sndtrk.m_uSoundtrackIndex   = i;
        sndtrk.m_uSongCount         = g_aGameSoundtracks[i].uNumSongs;
        wcscpy( sndtrk.m_strName, g_aGameSoundtracks[i].strName );

        m_vSoundtracks.push_back( sndtrk );
    }

    // Add each user provided soundtrack to the soundtrack vector
    XSOUNDTRACK_DATA stData;
    HANDLE hSoundtrack = XFindFirstSoundtrack( &stData );
    if( INVALID_HANDLE_VALUE != hSoundtrack )
    {
        do
        {
            // Ignore empty soundtracks
            if( stData.uSongCount > 0 )
            {
                CSoundtrack sndtrk;

                sndtrk.m_bGameSoundtrack    = FALSE;
                sndtrk.m_uSoundtrackID      = stData.uSoundtrackId;
                sndtrk.m_uSongCount         = stData.uSongCount;
                wcscpy( sndtrk.m_strName, stData.szName );

                m_vSoundtracks.push_back( sndtrk );
            }
        } while( XFindNextSoundtrack( hSoundtrack, &stData ) );
    }

    XFindClose(hSoundtrack);
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SelectSoundtrack()
// Desc: Changes to the specified soundtrack and creates a WMA Playlist from it
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SelectSoundtrack( DWORD dwSoundtrack, BOOL bInitialize )
{
    // If we're not being called from Initialize(), check to make
    // sure we're not selecting the same soundtrack again
    if( !bInitialize && ( dwSoundtrack == m_uCurrentSoundtrack ) )
        return S_FALSE;

    // Clear out any previous playlist
    if( m_pWMAPlaylist )
    {
        m_pWMAPlaylist->Release();
        m_pWMAPlaylist = NULL;
    }

    // Create the WMA Playlist
    if( FAILED( m_pSoundBank->CreateWmaPlayList( m_dwSoundCueIndex,
                                                 XACT_FLAG_WMAPLAYLIST_PLAYBACK_LOOP |
                                                 ( GetRandom() ? XACT_FLAG_WMAPLAYLIST_PLAYBACK_RANDOM : 0 ),
                                                 &m_pWMAPlaylist ) ) )
    {
        return E_FAIL;
    }

    // Add all songs in the soundtrack to the playlist
    XACT_WMA_PLAYLIST_ADD xwpAddInfo = { 0 };

    if( m_vSoundtracks[dwSoundtrack].m_bGameSoundtrack )
    {
        xwpAddInfo.dwType           = eXACTWmaPlayListAdd_Directory;
        xwpAddInfo.pszFileName      = g_aGameSoundtracks[ m_vSoundtracks[ dwSoundtrack ].m_uSoundtrackIndex ].strDir;
    }
    else
    {
        xwpAddInfo.dwType           = eXACTWmaPlayListAdd_Soundtrack;
        xwpAddInfo.dwSoundtrackId   = m_vSoundtracks[ dwSoundtrack ].m_uSoundtrackID;
    }

    m_pWMAPlaylist->Add( &xwpAddInfo, NULL );

    // Initially, there is no 'active' song in the playlist.  We force
    // it to select an active song by calling Next() here, but an
    // active song would be chosen if we called Play() without calling Next()
    m_pWMAPlaylist->Next();

    // Update our current soundtrack & song indices
    // We know the 0th song is selected, since we looped through
    // the entire soundtrack above
    m_uCurrentSoundtrack = dwSoundtrack;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: NextSoundtrack()
// Desc: Switches to the next soundtrack.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::NextSoundtrack()
{
    if( m_vSoundtracks.size() > 1 )
    {
        if( FAILED( Stop() ) )
            return E_FAIL;

        UINT uSoundtrack;

        if( GetGlobal() )
            uSoundtrack = rand() % m_vSoundtracks.size();
        else
            uSoundtrack = ( m_uCurrentSoundtrack + 1 ) % m_vSoundtracks.size();

        if( FAILED( SelectSoundtrack( uSoundtrack ) ) )
            return E_FAIL;

        return Play();
    }

    return S_FALSE;
}




//-----------------------------------------------------------------------------
// Name: NextSong()
// Desc: Switches to the next song in the current soundtrack.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::NextSong()
{
    if( FAILED( m_pWMAPlaylist->Next() ) )
        return E_FAIL;

    m_mmState = MM_PLAYING;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: PrevSoundtrack()
// Desc: Switches to the prev soundtrack.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::PrevSoundtrack()
{
    if( m_vSoundtracks.size() > 1 )
    {
        if( FAILED( Stop() ) )
            return E_FAIL;

        UINT uSoundtrack;

        if( GetGlobal() )
            uSoundtrack = rand() % m_vSoundtracks.size();
        else
            uSoundtrack = ( m_uCurrentSoundtrack + m_vSoundtracks.size() - 1 ) % m_vSoundtracks.size();

        if( FAILED( SelectSoundtrack( uSoundtrack ) ) )
            return E_FAIL;

        return Play();
    }

    return S_FALSE;
}




//-----------------------------------------------------------------------------
// Name: PrevSong()
// Desc: Switches to the prev song in the current soundtrack.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::PrevSong()
{
    if( FAILED( m_pWMAPlaylist->Previous() ) )
        return E_FAIL;

    m_mmState = MM_PLAYING;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetCurrentInfo()
// Desc: Returns pointers to info.  Buffers should be appropriately sized, ie
//       MAX_SOUNDTRACK_NAME and MAX_SONG_NAME, respectively
//
// IMPORTANT NOTE: GetCurrentInfo() makes a call to
// IXACTWmaPlayList::GetCurrentSongInfo.
//
// This function has the potential to block for long periods of time.
// GetCurrentSongInfo() requires a WMA XMO to be open to be able to
// retrieve the song's information.  When a song starts playing,
// IXACTWmaPlayList will cache the song's information and a call to
// GetCurrentSongInfo() is very fast.
//
// However, functions that start new songs (Play(), Next(),
// Previous(), etc.) are asynchronous for performance reasons.
// If GetCurrentSongInfo() is called during the period that
// IXACTWmaPlayList is waiting for a new song to start,
// GetCurrentSongInfo() will block until the WMA XMO has been opened
// before it will return.
//
// Should you require the use of GetCurrentSongInfo in a performance
// sensitive environment, use start notifications and cache the song's
// information using GetCurrentInfo at the time the notification is received
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::GetCurrentInfo( WCHAR* strSoundtrack, WCHAR* strSong, DWORD* pdwLength )
{
    if( !m_pWMAPlaylist )
        return E_FAIL;

    if( strSoundtrack )
        m_vSoundtracks[ m_uCurrentSoundtrack ].GetSoundtrackName( strSoundtrack );

    m_pWMAPlaylist->GetCurrentSongInfo( pdwLength, strSong, MAX_SONG_NAME * sizeof( WCHAR ), NULL );

    // The song name may be unavailable - in that case, let's display
    // something more useful than a blank string
    if( strSong[0] == L'\0' )
    {
        // Song name was unavailable
        wcscpy( strSong, L"[Unavailable]" );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetRandom()
// Desc: Sets the playback mode for how to pick the next song.  If fRandom is
//       true, the next track is picked randomly, otherwise it's sequential.
//       If fGlobal is true, we'll move between soundtracks, otherwise we stay
//       within the current soundtrack
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetRandom( BOOL bRandom )
{
    m_bRandom = bRandom;

    if( m_bRandom )
        m_pWMAPlaylist->SetPlaybackBehavior( XACT_FLAG_WMAPLAYLIST_PLAYBACK_RANDOM |
                                             XACT_FLAG_WMAPLAYLIST_PLAYBACK_LOOP );
    else
        m_pWMAPlaylist->SetPlaybackBehavior( XACT_FLAG_WMAPLAYLIST_PLAYBACK_LOOP );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetGlobal()
// Desc: Sets the playback mode for how to pick the next song.  If fGlobal is 
//       true, we'll move between soundtracks, otherwise we stay  within the 
//       current soundtrack
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetGlobal( BOOL bGlobal )
{
    m_bGlobal = bGlobal;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetVolume()
// Desc: Sets the overall volume level for music playback.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetVolume( FLOAT fVolume )
{
    m_fVolume = fVolume;

    if( m_fVolume < DSBVOLUME_MIN )
        m_fVolume = DSBVOLUME_MIN;
    else if( m_fVolume > DSBVOLUME_MAX )
        m_fVolume = DSBVOLUME_MAX;

    // We use the SOUNDTRACK_CATEGORY to control only the cue
    // that is used for soundtrack playback.  Although we could
    // have controlled the cue directly by obtaining a pointer
    // from IXACTSoundBank::Play(), categories enable us to
    // control multiple cues simultaneously.
    if( FAILED( m_pXACT->SetMasterVolume( SOUNDTRACK_CATEGORY, (LONG)m_fVolume ) ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Play()
// Desc: Starts playing background music
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Play()
{
    // Return if already playing
    if( m_mmState == MM_PLAYING )
        return S_FALSE;

    // Play if stopped, unpause if paused
    if( m_mmState == MM_STOPPED )
    {
        if( FAILED( m_pSoundBank->Play( m_dwSoundCueIndex, NULL, XACT_FLAG_SOUNDCUE_AUTORELEASE, NULL ) ) )
            return E_FAIL;

    }
    else if( m_mmState == MM_PAUSED )
    {
        // We use SOUNDTRACK_CATEGORY to control only the cue
        // that is being used for soundtrack playback
        if( FAILED( m_pXACT->GlobalPause( SOUNDTRACK_CATEGORY, FALSE ) ) )
            return E_FAIL;
    }

    // Mark when the song started playing
    QueryPerformanceCounter( &m_liLastTime );

    m_mmState = MM_PLAYING;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Stop()
// Desc: Stops background music playback
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Stop()
{
    if( m_mmState != MM_STOPPED )
    {
        if( FAILED( m_pSoundBank->Stop( m_dwSoundCueIndex, 0, NULL ) ) )
            return E_FAIL;

        // Reset the amount of time the current track has played
        m_liPlayTime.QuadPart = 0;

        m_mmState = MM_STOPPED;

        return S_OK;
    }

    return S_FALSE;
}




//-----------------------------------------------------------------------------
// Name: Pause()
// Desc: Pauses background music playback
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Pause()
{
    if( m_mmState == MM_PLAYING )
    {
        // We use SOUNDTRACK_CATEGORY to control only the cue
        // that is being used for soundtrack playback
        if( FAILED( m_pXACT->GlobalPause( SOUNDTRACK_CATEGORY, TRUE ) ) )
            return E_FAIL;

        m_mmState = MM_PAUSED;

        return S_OK;
    }

    return S_FALSE;
}
