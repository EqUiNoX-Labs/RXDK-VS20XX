//-----------------------------------------------------------------------------
// File: AudioEngine.cpp
//
// Desc: Implementation file for AudioEngine class.
//
// Hist: 6.26.02 - New for August 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "AudioEngine.h"

// Structure representing a game provided soundtrack
// All game provided soundtracks must consist of
// WMA file(s) in directories.  Soundtrack directories
// should be devoid of other filetypes, since XACT
// enumerates all files
typedef struct {
    WCHAR*  strName;
    CHAR*   strDir;
    UINT    uNumSongs;
} MM_GAMESOUNDTRACK;

// In this sample, we only care about one game soundtrack
// We also know we have 1 song in this soundtrack.
// If need be, the number of songs can be obtained by using
// XFindFirstFile() and XFindNextFile() at runtime
MM_GAMESOUNDTRACK   g_aGameSoundtracks[] =
{
    { L"Game Soundtrack 1", "d:\\media\\sounds\\soundtrack", 1 },
};

#define NUM_GAME_SOUNDTRACKS    ( sizeof( g_aGameSoundtracks ) / sizeof( g_aGameSoundtracks[0] ) )

const LONG  VOLUME_MIN = -4000;     // Minimum volume = -40dB
const LONG  VOLUME_RANGE = 4000;    // Range of volume = 40dB


//-----------------------------------------------------------------------------
// Name: AudioEngine (constructor)
// Desc: Constructor for AudioEngine class
//-----------------------------------------------------------------------------
AudioEngine::AudioEngine() :
    m_fRandom           ( FALSE ),
    m_lMusicVolume      ( DSBVOLUME_MAX ),
    m_lSoundEffectVolume( DSBVOLUME_MAX ),
    m_vSoundtracks      (),
    m_uCurrentSoundtrack( (UINT)-1 ),
    m_pXACT             ( NULL ),
    m_pbWaveBank        ( NULL ),
    m_pWaveBank         ( NULL ),
    m_pbSoundBank       ( NULL ),
    m_pSoundBank        ( NULL ),
    m_pWMAPlaylist      ( NULL )
{
}

//-----------------------------------------------------------------------------
// Name: ~AudioEngine (destructor)
// Desc: Destructor for the AudioEngine class
//-----------------------------------------------------------------------------
AudioEngine::~AudioEngine()
{
    if( m_pWMAPlaylist )
        m_pWMAPlaylist->Release();

    if( m_pSoundBank )
        m_pSoundBank->Release();

    if( m_pXACT )
        m_pXACT->Release();
}

//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: Initializes the XACT engine
//-----------------------------------------------------------------------------
HRESULT AudioEngine::Initialize()
{
    // Initialize the XACT Engine

    // Create the XACT runtime engine
    // Note: We only really need one concurrent stream here, but
    // concurrent streams are also used for auditioning, so we'll
    // just leave a few extra.
    // The dwMaxConcurrentStreams parameter may be removed in a future
    // release of the XACT audio library, but it's here for now
    XACT_RUNTIME_PARAMETERS xrParams;
    xrParams.dwMax2DHwVoices        = 128;
    xrParams.dwMax3DHwVoices        = 32;
    xrParams.dwMaxConcurrentStreams = 16;
    xrParams.dwMaxNotifications     = 0;
    if( FAILED( XACTEngineCreate( &xrParams, &m_pXACT ) ) )
        return E_FAIL;

    // Register our soundbank with XACT
    DWORD dwFileSize;
    if( FAILED( LoadFile( "d:\\media\\sounds\\XactSounds.xsb", (VOID **)&m_pbSoundBank, &dwFileSize ) ) )
        return E_FAIL;
    if( FAILED( m_pXACT->CreateSoundBank( m_pbSoundBank, dwFileSize, &m_pSoundBank ) ) )
        return E_FAIL;

    // Register our in-memory wavebank with XACT
    if( FAILED( LoadFile( "d:\\media\\sounds\\XactSounds_memory.xwb", (VOID **)&m_pbWaveBank, &dwFileSize ) ) )
        return E_FAIL;
    if( FAILED( m_pXACT->RegisterWaveBank( m_pbWaveBank, dwFileSize, &m_pWaveBank ) ) )
        return E_FAIL;

    // Load our DSP image and register it with XACT
    VOID* pDSPImage = NULL;
    if( FAILED( LoadFile( "d:\\media\\dsstdfx.bin", &pDSPImage, &dwFileSize ) ) )
        return E_FAIL;

    DSEFFECTIMAGELOC dseil;
    dseil.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    dseil.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( m_pXACT->DownloadEffectsImage( pDSPImage, dwFileSize, &dseil, NULL ) ) )
        return E_FAIL;

    // Free our copy of the DSP image
    free( pDSPImage );

    // Load up soundtrack information
    if( FAILED( LoadSoundtracks() ) )
        return E_FAIL;

    // Initialize the random number generator
    srand( GetTickCount() );

    // Select the first soundtrack
    SelectSoundtrack( 0, TRUE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: LoadFile
// Desc: Loads a file from disk using aysnchronous, unbuffered I/O.  This
//          routine allocates a buffer using malloc, which it is then the
//          caller's responsibility to free.  Since unbuffered I/O requires
//          sector-multiple reads, the buffer may contain some extra slop
//          space at the end.  The pdwSize parameter returned reflects the
//          amount of VALID data in the buffer, not the size of the allocation
//-----------------------------------------------------------------------------
HRESULT AudioEngine::LoadFile( CHAR* strFilename, VOID** ppvData, DWORD* pdwSize )
{
    // Open up the file for unbuffered, asynchronous IO
    HANDLE hFile = CreateFile( strFilename,
                               GENERIC_READ,
                               FILE_SHARE_READ,
                               NULL,
                               OPEN_EXISTING,
                               FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING,
                               NULL );
    if( INVALID_HANDLE_VALUE == hFile )
    {
        return E_FAIL;
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
    ReadFile( hFile, *ppvData, dwFileSize, NULL, &overlapped );

    // Wait for the result to complete
    DWORD dwBytesRead;
    GetOverlappedResult( hFile, &overlapped, &dwBytesRead, TRUE );
    assert( dwBytesRead == *pdwSize );

    // Close the file
    CloseHandle( hFile );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: LoadSoundtracks
// Desc: Loads soundtrack info - both game provided soundtracks and user
//       created soundtracks are enumerated
//-----------------------------------------------------------------------------
HRESULT AudioEngine::LoadSoundtracks()
{
    UINT i;

    // Add each game provided soundtrack to the soundtrack vector
    for(i = 0; i < NUM_GAME_SOUNDTRACKS; ++i)
    {
        CSoundtrack sndtrk;

        sndtrk.m_fGameSoundtrack    = TRUE;
        sndtrk.m_uSoundtrackIndex   = i;
        sndtrk.m_uSongCount         = g_aGameSoundtracks[i].uNumSongs;
        wcscpy( sndtrk.m_strName, g_aGameSoundtracks[i].strName );

        m_vSoundtracks.push_back( sndtrk );
    }

#if !defined(XDEMO)
    // TCR Hard Disk Usage (demos)
    // Add each user-provided soundtrack to the soundtrack vector
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

                sndtrk.m_fGameSoundtrack    = FALSE;
                sndtrk.m_uSoundtrackID      = stData.uSoundtrackId;
                sndtrk.m_uSongCount         = stData.uSongCount;
                wcscpy( sndtrk.m_strName, stData.szName );

                m_vSoundtracks.push_back( sndtrk );
            }
        } while( XFindNextSoundtrack( hSoundtrack, &stData ) );
    }
    XFindClose( hSoundtrack );
#endif    

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SelectSoundtrack
// Desc: Changes to the specified soundtrack and creates a WMA Playlist from it
//-----------------------------------------------------------------------------
HRESULT AudioEngine::SelectSoundtrack( DWORD dwSoundtrack, BOOL fInitialize )
{
    // If we're not being called from Initialize(), check to make
    // sure we're not selecting the same soundtrack again
    if( dwSoundtrack == m_uCurrentSoundtrack )
        return S_FALSE;

    // Clear out any previous playlist
    if( m_pWMAPlaylist )
    {
        m_pWMAPlaylist->Release();
        m_pWMAPlaylist = NULL;
    }

    // Create the WMA Playlist
    if( FAILED( m_pSoundBank->CreateWmaPlayList( XACT_SOUNDBANK_GAME_MUSIC,
                                                 XACT_FLAG_WMAPLAYLIST_PLAYBACK_LOOP |
                                                 ( GetRandom() ? XACT_FLAG_WMAPLAYLIST_PLAYBACK_RANDOM : 0 ),
                                                 &m_pWMAPlaylist ) ) )
    {
        return E_FAIL;
    }

    // Add all songs in the soundtrack to the playlist
    XACT_WMA_PLAYLIST_ADD xwpAddInfo = { 0 };

    if( m_vSoundtracks[ dwSoundtrack ].m_fGameSoundtrack )
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

    // Update our current soundtrack & song indices
    // We know the 0th song is selected, since we looped through
    // the entire soundtrack above
    m_uCurrentSoundtrack = dwSoundtrack;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SetRandom
// Desc: Sets the playback mode for how to pick the next song.  If fRandom is
//       true, the next track is picked randomly, otherwise it's sequential.
//-----------------------------------------------------------------------------
HRESULT AudioEngine::SetRandom( BOOL fRandom )
{
    m_fRandom = fRandom;

    if( m_fRandom )
        m_pWMAPlaylist->SetPlaybackBehavior( XACT_FLAG_WMAPLAYLIST_PLAYBACK_RANDOM );
    else
        m_pWMAPlaylist->SetPlaybackBehavior( XACT_FLAG_WMAPLAYLIST_PLAYBACK_LOOP );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetVolume
// Desc: Sets the volume of a particular category
//-----------------------------------------------------------------------------
HRESULT AudioEngine::SetVolume( XACT_CATEGORY xactCategory, FLOAT fVolume )
{
    LONG* pVolume = NULL;

    // Make sure we are changing the volume of a proper category
    if( xactCategory == XACT_CATEGORY_BGMUSIC )
        pVolume = &m_lMusicVolume;
    else if( xactCategory == XACT_CATEGORY_SFX )
        pVolume = &m_lSoundEffectVolume;
    else
        return E_FAIL;

    // Use fVolume as a percentage
    // and make sure it is in range
    *pVolume = VOLUME_MIN + LONG( fVolume * VOLUME_RANGE / 100.0f );

    if( *pVolume < DSBVOLUME_MIN )
        *pVolume = DSBVOLUME_MIN;
    else if( *pVolume > DSBVOLUME_MAX )
        *pVolume = DSBVOLUME_MAX;

    // Set the proper volume
    if( FAILED( m_pXACT->SetMasterVolume( ( WORD )xactCategory, *pVolume ) ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetSoundtrack
// Desc: Returns a reference to the soundtrack by index
//-----------------------------------------------------------------------------
const CSoundtrack& AudioEngine::GetSoundtrack( UINT uSoundtrack ) const 
{
    return m_vSoundtracks[ uSoundtrack ];
}



//-----------------------------------------------------------------------------
// Playback control
//-----------------------------------------------------------------------------




//-----------------------------------------------------------------------------
// Name: Play
// Desc: Plays audio
//-----------------------------------------------------------------------------
HRESULT AudioEngine::Play( DWORD xactCue )
{
    // Reject XACT_SOUNDCUE_INDEX_UNUSED
    if( xactCue == XACT_SOUNDCUE_INDEX_UNUSED )
        return S_FALSE;

    if( FAILED( m_pSoundBank->Play( xactCue, NULL, XACT_FLAG_SOUNDCUE_AUTORELEASE, NULL ) ) )
        return E_FAIL;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Stop
// Desc: Stops audio
//-----------------------------------------------------------------------------
HRESULT AudioEngine::Stop( DWORD xactCue )
{
    if( xactCue == XACT_SOUNDCUE_INDEX_UNUSED )
    {
        // Stop all cues
        for( DWORD i = 0; i < XACT_SOUNDBANK_GAME_CUE_COUNT; ++i )
        {
            if( FAILED( m_pSoundBank->Stop( i, 0, NULL ) ) )
                return E_FAIL;
        }
    }
    else
    {
        if( FAILED( m_pSoundBank->Stop( xactCue, 0, NULL ) ) )
            return E_FAIL;
    }

    return S_FALSE;
}


//-----------------------------------------------------------------------------
// Name: Pause
// Desc: Pauses audio
//-----------------------------------------------------------------------------
HRESULT AudioEngine::Pause( WORD xactCategory )
{
    if( xactCategory == XACT_CATEGORY_INDEX_UNUSED )
    {
        for( WORD i = 0; i < XACT_CATEGORY_COUNT; ++i )
        {
            if( FAILED( m_pXACT->GlobalPause( i, TRUE ) ) )
                return E_FAIL;
        }
    }
    else
    {

        if( FAILED( m_pXACT->GlobalPause( xactCategory, TRUE ) ) )
            return E_FAIL;
    }

    return S_FALSE;
}


//-----------------------------------------------------------------------------
// Name: Unpause
// Desc: Unpauses audio
//-----------------------------------------------------------------------------
HRESULT AudioEngine::Unpause( WORD xactCategory )
{
    if( xactCategory == XACT_CATEGORY_INDEX_UNUSED )
    {
        for( WORD i = 0; i < XACT_CATEGORY_COUNT; ++i )
        {
            if( FAILED( m_pXACT->GlobalPause( i, FALSE ) ) )
                return E_FAIL;
        }
    }
    else
    {

        if( FAILED( m_pXACT->GlobalPause( xactCategory, FALSE ) ) )
            return E_FAIL;
    }

    return S_FALSE;
}
