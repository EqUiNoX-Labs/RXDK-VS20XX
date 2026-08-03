//-----------------------------------------------------------------------------
// File: MusicManager.cpp
//
// Desc: Implementation file for CMusicManager class.
//
// Hist: 08.20.01 - New for October 01 XDK
//       07.10.02 - Now uses the new WMA decoder
//       10.07.02 - Cleaned up for November 02 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "MusicManager.h"
#include "XbUtil.h"
#include <assert.h>
#include <stdio.h>
#include <tchar.h>



// Here is our game soundtrack including WMA files we ship
// with our game.  If needed, this concept could be extended
// to include several different game soundtracks

// Structure representing game soundtrack song
struct MM_SONG
{
    WCHAR*  strName;
    CHAR*   strFilename;
    DWORD   dwLength;
};

MM_SONG g_aGameSoundtrack[] =
{
    { L"Becky",       "D:\\Media\\Sounds\\Becky.wma", 165000 },
    { L"Becky remix", "D:\\Media\\Sounds\\Becky.wma", 165000 },
};

const DWORD NUM_GAME_SONGS = sizeof( g_aGameSoundtrack ) / sizeof( g_aGameSoundtrack[0] );

const DWORD WMA_LOOKAHEAD = 64 * 1024;




//-----------------------------------------------------------------------------
// Name: CMusicManager()
// Desc: Initializes member variables
//-----------------------------------------------------------------------------
CMusicManager::CMusicManager()
{
    m_mmState       = MM_STOPPED;
    m_bRandom       = FALSE;
    m_bGlobal       = TRUE;
    m_fVolume       = DSBVOLUME_MAX;
    m_dwStream      = 0;
    m_dwSong        = 0;
    m_strSong[0]    = 0;
    m_dwLength      = 0;
    m_dwSongID      = 0;

    m_aSoundtracks          = NULL;
    m_uSoundtrackCount      = 0;
    m_uCurrentSoundtrack    = 0;
    m_dwPacketsCompleted    = 0;

    m_pSampleData   = NULL;
    m_pDecoder      = NULL;
    m_pStream[0]    = NULL;
    m_pStream[1]    = NULL;
    m_hDecodingFile = INVALID_HANDLE_VALUE;
}




//-----------------------------------------------------------------------------
// Name: ~CMusicManager()
// Desc: Releases any resources allocated by the object
//-----------------------------------------------------------------------------
CMusicManager::~CMusicManager()
{
    // This takes care of the decoder and file handle
    Cleanup();

    if( m_pStream[0] )
        m_pStream[0]->Release();
    m_pStream[0] = NULL;

    if( m_pStream[1] )
        m_pStream[1]->Release();
    m_pStream[1] = NULL;

    if( m_pSampleData )
        delete[] m_pSampleData;

    free( m_aSoundtracks );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Sets up the object to run
//-----------------------------------------------------------------------------
HRESULT CMusicManager::Initialize()
{
    HRESULT hr;

    // Load up soundtrack information
    hr = LoadSoundtracks();
    if( FAILED( hr ) )
        return hr;
    SelectSong( 0 );

    srand( GetTickCount() );

    // Allocate sample data buffer
    m_pSampleData = new BYTE[ ( PACKET_COUNT + EXTRA_PACKETS ) * PACKET_SIZE ];
    if( !m_pSampleData )
        return E_OUTOFMEMORY;

    // All user-created soundtracks are created with the same standard
    // wave format: 44.1KHz, 16-bit, stereo PCM.
    // Title-supplied soundtracks should be the same format.  If the
    // title needs to use soundtracks with a different format, they
    // can use the SetFormat() method to change the stream format.
    WAVEFORMATEX wfx    = {0};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = 2;
    wfx.nSamplesPerSec  = 44100;
    wfx.wBitsPerSample  = 16;
    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;

    // Set up the stream descriptor so we can create our streams
    DSSTREAMDESC dssd           = {0};
    dssd.dwMaxAttachedPackets   = PACKET_COUNT;
    dssd.lpwfxFormat            = &wfx;
    dssd.lpfnCallback           = StreamCallback;
    dssd.lpvContext             = this;

    // Create the streams
    hr = DirectSoundCreateStream( &dssd, &m_pStream[0] );
    if( FAILED( hr ) )
        return hr;
    hr = DirectSoundCreateStream( &dssd, &m_pStream[1] );
    if( FAILED( hr ) )
        return hr;

    // Set up amplitude envelopes to handle the fade-in/fade-out
    DSENVELOPEDESC dsed = {0};
    dsed.dwEG           = DSEG_AMPLITUDE;
    dsed.dwMode         = DSEG_MODE_ATTACK;
    dsed.dwAttack       = DWORD( 48000 * FADE_TIME / 512 );
    dsed.dwRelease      = DWORD( 48000 * FADE_TIME / 512 );
    dsed.dwSustain      = 255;
    m_pStream[0]->SetEG( &dsed );
    m_pStream[1]->SetEG( &dsed );

#if _DEBUG
    if( FAILED( DebugVerify() ) )
        return E_FAIL;
#endif // _DEBUG

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Play()
// Desc: Starts playing background music
//-----------------------------------------------------------------------------
HRESULT CMusicManager::Play()
{
    // Make sure the streams are unpaused
    if( m_mmState == MM_STOPPED )
        Prepare();
    else if( m_mmState == MM_PAUSED )
    {
        if( m_pStream[0] )
            m_pStream[0]->Pause( DSSTREAMPAUSE_RESUME );
        if( m_pStream[1] )
            m_pStream[1]->Pause( DSSTREAMPAUSE_RESUME );
    }

    m_mmState = MM_PLAYING;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Stop()
// Desc: Stops background music playback
//-----------------------------------------------------------------------------
HRESULT CMusicManager::Stop()
{
    // Can always transition to stopped
    if( m_mmState != MM_STOPPED )
    {
        if( m_pStream[0] )
            m_pStream[0]->FlushEx( 0, DSSTREAMFLUSHEX_ASYNC );
        if( m_pStream[1] )
            m_pStream[1]->FlushEx( 0, DSSTREAMFLUSHEX_ASYNC );
        Cleanup();
    }

    m_mmState = MM_STOPPED;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Pause()
// Desc: Pauses background music playback
//-----------------------------------------------------------------------------
HRESULT CMusicManager::Pause()
{
    if( m_mmState != MM_PLAYING )
        return S_FALSE;

    // Can only transition to paused from playing.
    if( m_mmState == MM_PLAYING )
    {
        if( m_pStream[0] )
            m_pStream[0]->Pause( DSSTREAMPAUSE_PAUSE );
        if( m_pStream[1] )
            m_pStream[1]->Pause( DSSTREAMPAUSE_PAUSE );
    }

    m_mmState = MM_PAUSED;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetRandom()
// Desc: Sets the playback mode for how to pick the next song.  If fRandom is
//       true, the next track is picked randomly, otherwise it's sequential.
//       If fGlobal is true, we'll move between soundtracks, otherwise we stay
//       within the current soundtrack
//-----------------------------------------------------------------------------
HRESULT CMusicManager::SetRandom( BOOL bRandom )
{
    m_bRandom = bRandom;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SetGlobal()
// Desc: Sets the playback mode for how to pick the next song.  If fGlobal is 
//       true, we'll move between soundtracks, otherwise we stay  within the 
//       current soundtrack
//-----------------------------------------------------------------------------
HRESULT CMusicManager::SetGlobal( BOOL bGlobal )
{
    m_bGlobal = bGlobal;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SetVolume()
// Desc: Sets the overall volume level for music playback.  
//-----------------------------------------------------------------------------
HRESULT CMusicManager::SetVolume( FLOAT fVolume )
{
    m_fVolume = fVolume;

    if( m_pStream[0] )
        m_pStream[0]->SetVolume( (LONG)fVolume );
    if( m_pStream[1] )
        m_pStream[1]->SetVolume( (LONG)fVolume );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetCurrentInfo()
// Desc: Returns pointers to info.  Buffers should be appropriately sized, ie
//       MAX_SOUNDTRACK_NAME and MAX_SONG_NAME, respectively
//-----------------------------------------------------------------------------
HRESULT CMusicManager::GetCurrentInfo( WCHAR* strSoundtrack, WCHAR* strSong,
                                       DWORD* pdwLength )
{
    if( strSoundtrack )
        m_aSoundtracks[m_uCurrentSoundtrack].GetSoundtrackName( strSoundtrack );
    if( strSong )
        wcscpy( strSong, m_strSong );
    if( pdwLength )
        (*pdwLength) = m_dwLength;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetPlaybackPosition()
// Desc: Returns the current playback position, in seconds
//-----------------------------------------------------------------------------
FLOAT CMusicManager::GetPlaybackPosition()
{
    return PACKET_TIME * m_dwPacketsCompleted;
}




//-----------------------------------------------------------------------------
// Name: NextSoundtrack()
// Desc: Switches to the next soundtrack.  This is only allowed if playback
//       is stopped.
//-----------------------------------------------------------------------------
HRESULT CMusicManager::NextSoundtrack()
{
    if( m_mmState == MM_STOPPED )
    {
        return SelectSoundtrack( ( m_uCurrentSoundtrack + 1 ) % m_uSoundtrackCount );
    }

    return S_FALSE;
}




//-----------------------------------------------------------------------------
// Name: NextSong()
// Desc: Switches to the next song in the current soundtrack.  This is only 
//       allowed if playback is stopped
//-----------------------------------------------------------------------------
HRESULT CMusicManager::NextSong()
{
    if( m_mmState == MM_STOPPED )
    {
        return SelectSong( ( m_dwSong + 1 ) % m_aSoundtracks[m_uCurrentSoundtrack].GetSongCount() );
    }

    return S_FALSE;
}




//-----------------------------------------------------------------------------
// Name: RandomSong()
// Desc: Switches to a random song, either in this soundtrack if fGlobal is
//       FALSE or globally random if fGlobal is TRUE.  This is only allowed
//       if playback is stopped.
//-----------------------------------------------------------------------------
HRESULT CMusicManager::RandomSong( BOOL bGlobal )
{
    if( m_mmState == MM_STOPPED )
    {
        if( bGlobal )
        {
            SelectSoundtrack( rand() % m_uSoundtrackCount );
        }
        return SelectSong( rand() % m_aSoundtracks[m_uCurrentSoundtrack].GetSongCount() );
    }
    else
        return S_FALSE;
}




//-----------------------------------------------------------------------------
// Name: LoadSoundtracks()
// Desc: Loads soundtrack info for user soundtracks stored on HD
//-----------------------------------------------------------------------------
HRESULT CMusicManager::LoadSoundtracks()
{
    HANDLE hSoundtrack;
    XSOUNDTRACK_DATA stData;
    UINT uAllocatedSoundtracks;

    m_aSoundtracks = (CSoundtrack *)malloc( sizeof( CSoundtrack ) );
    if( !m_aSoundtracks )
        return E_OUTOFMEMORY;
    uAllocatedSoundtracks = 1;

    // Set up our game soundtrack as soundtrack 0.
    // If we had more than 1 game soundtrack, we could
    // set them all up here
    m_aSoundtracks[0].m_bGameSoundtrack     = TRUE;
    m_aSoundtracks[0].m_uSoundtrackIndex    = 0;
    m_aSoundtracks[0].m_uSongCount          = NUM_GAME_SONGS;
    wcscpy( m_aSoundtracks[0].m_strName, L"Game Soundtrack" );
    m_uSoundtrackCount = 1;

    // Start scanning the soundtrack DB
    hSoundtrack = XFindFirstSoundtrack( &stData );
    if( INVALID_HANDLE_VALUE != hSoundtrack )
    {
        do
        {
            // Double our buffer if we need more space
            if( m_uSoundtrackCount + 1 > uAllocatedSoundtracks )
            {
                void * pNewAlloc = realloc( m_aSoundtracks, ( uAllocatedSoundtracks * 2 ) * sizeof( CSoundtrack ) );
                if( !pNewAlloc )
                {
                    // We couldn't expand our buffer, so clean up
                    // and bail out
                    free( m_aSoundtracks );
                    m_aSoundtracks = NULL;
                    XFindClose( hSoundtrack );
                    return E_OUTOFMEMORY;
                }
                m_aSoundtracks = (CSoundtrack *)pNewAlloc;
                uAllocatedSoundtracks *= 2;
            }

            // Ignore empty soundtracks
            if( stData.uSongCount > 0 )
            {
                // Copy the data over
                m_aSoundtracks[m_uSoundtrackCount].m_bGameSoundtrack = FALSE;
                m_aSoundtracks[m_uSoundtrackCount].m_uSoundtrackID   = stData.uSoundtrackId;
                m_aSoundtracks[m_uSoundtrackCount].m_uSongCount      = stData.uSongCount;
                wcscpy( m_aSoundtracks[m_uSoundtrackCount].m_strName, stData.szName );

                m_uSoundtrackCount++;
            }

        } while( XFindNextSoundtrack( hSoundtrack, &stData ) );

        XFindClose( hSoundtrack );
    }

    // Shrink our allocation down to what's actually needed, since
    // we can't add soundtracks on the fly.
    VOID* pRealloc = realloc( m_aSoundtracks, m_uSoundtrackCount * sizeof( CSoundtrack ) );
    if( !pRealloc )
    {
        free( m_aSoundtracks );
        m_aSoundtracks = NULL;
        return E_OUTOFMEMORY;
    }

    m_aSoundtracks = (CSoundtrack *)pRealloc;

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: SelectSoundtrack()
// Desc: Changes to the specified soundtrack
//-----------------------------------------------------------------------------
HRESULT CMusicManager::SelectSoundtrack( DWORD dwSoundtrack )
{
    m_uCurrentSoundtrack = dwSoundtrack;
    SelectSong( 0 );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SelectSong()
// Desc: Switches to the specified song and caches song info
//-----------------------------------------------------------------------------
HRESULT CMusicManager::SelectSong( DWORD dwSong )
{
    m_dwSong = dwSong;
    m_aSoundtracks[m_uCurrentSoundtrack].GetSongInfo( m_dwSong, &m_dwSongID,
                                                      &m_dwLength, m_strSong );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FindNextSong()
// Desc: Simple helper function to switch to the next song, based on the
//       global repeat flag.
//-----------------------------------------------------------------------------
HRESULT CMusicManager::FindNextSong()
{
    if( m_bRandom )
    {
        if( m_bGlobal )
        {
            SelectSoundtrack( rand() % m_uSoundtrackCount );
        }
        SelectSong( rand() % m_aSoundtracks[m_uCurrentSoundtrack].GetSongCount() );
    }
    else
    {
        if( m_bGlobal && 
            m_dwSong == m_aSoundtracks[m_uCurrentSoundtrack].GetSongCount() - 1 )
        {
            SelectSoundtrack( ( m_uCurrentSoundtrack + 1 ) % m_uSoundtrackCount );
        }
        else
            SelectSong( ( m_dwSong + 1 ) % m_aSoundtracks[m_uCurrentSoundtrack].GetSongCount() );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Prepare()
// Desc: Prepares to begin playback of the currently selected.  To avoid
//          blocking, we just signal our worker thread and have it create
//          the file for us.  ProcessSource() won't do anything until the
//          decoder has been created.
//-----------------------------------------------------------------------------
HRESULT CMusicManager::Prepare()
{
    // Open the song
    m_hDecodingFile = m_aSoundtracks[m_uCurrentSoundtrack].OpenSong( m_dwSongID );

    // Create the new decoder
    WMAXMODECODERPARAMETERS Params = {0};
    Params.hFile                    = m_hDecodingFile;
    Params.dwLookaheadBufferSize    = WMA_LOOKAHEAD;

    HRESULT hr = XWmaDecoderCreateMediaObject( (LPCWMAXMODECODERPARAMETERS)&Params, &m_pDecoder );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Cleans up decoding resources
//-----------------------------------------------------------------------------
HRESULT CMusicManager::Cleanup()
{
    // Free up the decoder
    if( m_pDecoder )
    {
        m_pDecoder->Release();
        m_pDecoder = NULL;
    }

    // Close the open file
    if( m_hDecodingFile != INVALID_HANDLE_VALUE )
    {
        CloseHandle( m_hDecodingFile );
        m_hDecodingFile = INVALID_HANDLE_VALUE;
    }

    // Zero packets completed
    m_dwPacketsCompleted = 0;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Process()
// Desc: Manages audio packets, filling them from the source XMO and 
//       dispatching them to the appropriate stream.
//-----------------------------------------------------------------------------
HRESULT CMusicManager::Process()
{
    HRESULT hr;
    DWORD   dwPacket;

    // If we're currently playing, then process packets.  Note that
    // we could be playing, and not yet have a decoder if the worker
    // thread is still trying to open the file.
    if( m_pDecoder )
    {
        // Pump the decoder so it can handle its asynchronous file I/O
        m_pDecoder->DoWork();

        // Process packets
        while( FindFreePacket( &dwPacket, m_dwStream ) )
        {
            XMEDIAPACKET xmp;

            hr = ProcessSource( dwPacket, &xmp );
            if( FAILED( hr ) )
                return hr;

            // If we got a packet of size zero, that means that we either
            // 1) Hit the end of the current track, or 
            // 2) The decoder is not yet ready to produce output.  
            // Either way, we'll skip out, let the decoder do some work,
            // and try again next time.
            if( xmp.dwMaxSize > 0 )
            {
                hr = ProcessStream( dwPacket, &xmp );
                if( FAILED( hr ) )
                    return hr;
            }
            else
                break;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FindFreePacket()
// Desc: Looks for a free audio packet.  Returns TRUE if one was found and
//       returns the index
//-----------------------------------------------------------------------------
BOOL CMusicManager::FindFreePacket( DWORD* pdwPacket, DWORD dwStream )
{
    for( DWORD dwIndex = dwStream; dwIndex < PACKET_COUNT; dwIndex += 2 )
    {
        // The first EXTRA_PACKETS * 2 packets are reserved - odd packets
        // for stream 1, even packets for stream 2.  This is to ensure
        // that there are packets available during the crossfade
        if( XMEDIAPACKET_STATUS_PENDING != m_adwStatus[dwIndex] &&
            ( dwIndex > EXTRA_PACKETS * 2 || dwIndex % 2 == dwStream ) )
        {
            (*pdwPacket) = dwIndex;
            return TRUE;
        }
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ProcessSource()
// Desc: Fills audio packets from the decoder XMO
//-----------------------------------------------------------------------------
HRESULT CMusicManager::ProcessSource( DWORD dwPacket, XMEDIAPACKET* pxmp )
{
    HRESULT      hr;
    DWORD        dwBytesDecoded;

    ZeroMemory( pxmp, sizeof( XMEDIAPACKET ) );

    // See if the decoder is ready to provide output yet
    DWORD dwStatus;
    m_pDecoder->GetStatus( &dwStatus );
    if( ( dwStatus & XMO_STATUSF_ACCEPT_OUTPUT_DATA ) == 0 )
        return S_FALSE;

    // Set up the XMEDIAPACKET structure
    pxmp->pvBuffer          = m_pSampleData + dwPacket * PACKET_SIZE;
    pxmp->dwMaxSize         = PACKET_SIZE;
    pxmp->pdwCompletedSize  = &dwBytesDecoded;

    hr = m_pDecoder->Process( NULL, pxmp );
    if( FAILED( hr ) )
        return hr;

    if( 0 == dwBytesDecoded )
    {
        // We hit the end of the current track.  Key the fade-out
        // Note that we queue an asynchronous flush to be done, and
        // then (in Cleanup), we release the stream.  The flush should
        // still occur on the next call to DirectSoundDoWork().
        m_pStream[m_dwStream]->Discontinuity();
        m_pStream[m_dwStream]->FlushEx( 0, DSSTREAMFLUSHEX_ASYNC | DSSTREAMFLUSHEX_ENVELOPE );

        // Clean up resources from this song, and get
        // ready for the next one:
        Cleanup();
        FindNextSong();
        m_dwStream = ( m_dwStream + 1 ) % 2;
        Prepare();
    }

    pxmp->dwMaxSize = dwBytesDecoded;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ProcessStream()
// Desc: Submits audio packets to the appropriate stream
//-----------------------------------------------------------------------------
HRESULT CMusicManager::ProcessStream( DWORD dwPacket, XMEDIAPACKET* pxmp )
{
    HRESULT      hr;

    // The XMEDIAPACKET should already have been filled out
    // by the call to ProcessSource().  In addition, ProcessSource()
    // should have properly set the dwMaxSize member to reflect
    // how much data was decoded.
    pxmp->pdwStatus         = &m_adwStatus[dwPacket];
    pxmp->pdwCompletedSize  = NULL;
    pxmp->pContext          = (LPVOID)m_dwStream;

    hr = m_pStream[m_dwStream]->Process( pxmp, NULL );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: StreamCallback()
// Desc: Called back on completion of stream packets.  The stream context
//       contains a pointer to the CMusicManager object, and the packet
//       context contains the stream number
//-----------------------------------------------------------------------------
void CALLBACK StreamCallback( VOID* pStreamContext, VOID* pPacketContext, DWORD dwStatus )
{
    CMusicManager* pMusicManager = (CMusicManager*)pStreamContext;

    if( DWORD(pPacketContext) == pMusicManager->m_dwStream &&
        dwStatus == XMEDIAPACKET_STATUS_SUCCESS )
    {
        ++pMusicManager->m_dwPacketsCompleted;
    }
}




#if _DEBUG
//-----------------------------------------------------------------------------
// Name: DebugVerify()
// Desc: Debug routine to verify that everything is properly set up:
//       * Must have at least 1 game soundtrack, since a game can't depend
//         on there being user soundtracks on the Xbox hard drive
//       * Check that we can open all soundtrack songs
//-----------------------------------------------------------------------------
HRESULT CMusicManager::DebugVerify()
{
    // Make sure we have at least 1 game soundtrack
    if( !m_aSoundtracks[0].m_bGameSoundtrack )
    {
        OUTPUT_DEBUG_STRING( "Must have at least 1 game soundtrack.\n" );
        return E_FAIL;
    }

    // Verify we can open all soundtrack files.  This could take a while.
    for( UINT uSoundtrack = 0; uSoundtrack < m_uSoundtrackCount; uSoundtrack++ )
    {
        for( UINT uSong = 0; uSong < m_aSoundtracks[uSoundtrack].GetSongCount(); uSong++ )
        {
            DWORD dwSongID;
            DWORD dwSongLength;
            WCHAR strSongName[MAX_SONG_NAME];

            m_aSoundtracks[uSoundtrack].GetSongInfo( uSong, &dwSongID, &dwSongLength, strSongName );
            HANDLE h = m_aSoundtracks[uSoundtrack].OpenSong( dwSongID );
            if( INVALID_HANDLE_VALUE == h )
            {
                OUTPUT_DEBUG_STRING( "Failed to open a soundtrack file.\n" );
                return E_FAIL;
            }
            CloseHandle( h );
        }
    }

    return S_OK;
}
#endif // _DEBUG




//-----------------------------------------------------------------------------
// Name: GetSongInfo()
// Desc: Returns information about the given song
//-----------------------------------------------------------------------------
void CSoundtrack::GetSongInfo( UINT uSongIndex, DWORD* pdwID, DWORD* pdwLength,
                               WCHAR strName[MAX_SONG_NAME] )
{
    if( m_bGameSoundtrack )
    {
        (*pdwID) = uSongIndex;
        (*pdwLength) = g_aGameSoundtrack[uSongIndex].dwLength;
        wcscpy( strName, g_aGameSoundtrack[uSongIndex].strName );
    }
    else
    {
        XGetSoundtrackSongInfo( m_uSoundtrackID, uSongIndex, pdwID, pdwLength, 
                                strName, MAX_SONG_NAME );
    }
}




//-----------------------------------------------------------------------------
// Name: OpenSong()
// Desc: Opens the song with the given ID and returns a handle to the file
//-----------------------------------------------------------------------------
HANDLE CSoundtrack::OpenSong( DWORD dwSongID )
{
    if( m_bGameSoundtrack )
        return CreateFile( g_aGameSoundtrack[dwSongID].strFilename, GENERIC_READ,
                           FILE_SHARE_READ, NULL, OPEN_EXISTING, 
                           FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL );
    else
        return XOpenSoundtrackSong( dwSongID, TRUE );
}
