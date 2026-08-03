//-----------------------------------------------------------------------------
// File: MusicManager.h
//
// Desc: Class definition for the CMusicManager class.  This is the real
//       playback engine.
//
// Hist: 08.20.01 - New for October XDK
//       10.07.02 - Cleaned up for November 02 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef MUSICMANAGER_H
#define MUSICMANAGER_H

#include <xtl.h>

// Packet size is set up to be 2048 samples
// Samples are 16-bit and stereo
const DWORD PACKET_SIZE     = 2048 * 2 * 2;
const FLOAT PACKET_TIME     = 2048.0f / 44100.0f;

const DWORD PACKET_COUNT    = 43;     // Base number of packets
const DWORD EXTRA_PACKETS   = 10;    // Extra packets to get through the transition
const FLOAT FADE_TIME       = PACKET_COUNT * PACKET_TIME;


enum MM_STATE
{
    MM_STOPPED,
    MM_PAUSED,
    MM_PLAYING,
};




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

    VOID    GetSongInfo( UINT uSongIndex, DWORD* pdwID, DWORD* pdwLength, WCHAR* strName );
    HANDLE  OpenSong( DWORD dwSongID );

    
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
// Name: class CMusicManager
// Desc: Background music engine class.  Spawns its own worker thread when
//       initialized.  Cost of calls from main rendering loop is negligible.
//       All public methods are safe to be called from main rendering loop, 
//       however, they do not all take effect immediately.
//-----------------------------------------------------------------------------
class CMusicManager
{
public:
    CMusicManager();
    ~CMusicManager();
    HRESULT Initialize();  // Initialize MusicManager

    HRESULT Play();                                 // Start playing
    HRESULT Stop();                                 // Stop playback
    HRESULT Pause();                                // Pause playback
    MM_STATE GetStatus() { return m_mmState; }      // Returns current playback status
    HRESULT SetRandom( BOOL bRandom );              // Change random mode
    BOOL    GetRandom() { return m_bRandom; }       // Get random mode
    HRESULT SetGlobal( BOOL bGlobal );              // Toggle global mode
    BOOL    GetGlobal() { return m_bGlobal; }       // Get global mode
    HRESULT SetVolume( FLOAT fVolume );             // Set volume level
    FLOAT   GetVolume() { return m_fVolume; }       // Get volume level

    // Returns info on currently playing song
    HRESULT GetCurrentInfo( WCHAR* strSoundtrack, WCHAR* strSong, DWORD* pdwLength );
    FLOAT   GetPlaybackPosition();                  // Returns position in current song
    HRESULT NextSoundtrack();                       // Switch to next soundtrack
    HRESULT NextSong();                             // Switch to next song
    HRESULT RandomSong( BOOL bGlobal = TRUE );      // Switch to a random song

    HRESULT Process();                              // Workhorse function - update state and manage packets

private:
    HRESULT LoadSoundtracks();                      // Fill our soundtrack cache
    HRESULT SelectSoundtrack( DWORD dwSoundtrack ); // Switch to a soundtrack
    HRESULT SelectSong( DWORD dwSong );             // Switch to a song
    HRESULT FindNextSong();                         // Determine next song to play

    HRESULT Prepare();          // Get ready to decode and playback current song
    HRESULT Cleanup();          // Cleanup from playing current song

    BOOL    FindFreePacket( DWORD *pdwPacket, DWORD dwStream );     // Looks for a free packet
    HRESULT ProcessSource( DWORD dwPacket, XMEDIAPACKET * pxmp );   // Fills packet from source
    HRESULT ProcessStream( DWORD dwPacket, XMEDIAPACKET * pxmp );   // Submit packet to stream

    // Stream callback routine for updating playback position
    friend static void CALLBACK StreamCallback( LPVOID pStreamContext, LPVOID pPacketContext, DWORD dwStatus );

    // State variables
    BOOL                    m_bGlobal;                          // TRUE to loop/randomize globally
    BOOL                    m_bRandom;                          // TRUE to move randomly
    FLOAT                   m_fVolume;                          // Volume level
    MM_STATE                m_mmState;                          // New state set from game thread
    DWORD                   m_dwPacketsCompleted;               // # of packets completed

    // Music information
    CSoundtrack*            m_aSoundtracks;                     // List of soundtracks
    UINT                    m_uSoundtrackCount;                 // Total number of soundtracks
    UINT                    m_uCurrentSoundtrack;               // Currently selected soundtrack
    WCHAR                   m_strSong[MAX_SONG_NAME];           // Current song name
    DWORD                   m_dwLength;                         // Length of current song
    DWORD                   m_dwSong;                           // Current song index
    DWORD                   m_dwSongID;                         // Current song ID
    HANDLE                  m_hDecodingFile;                    // Song file handle

    // Decode/playback members
    XWmaFileMediaObject*    m_pDecoder;                         // WMA decoder
    IDirectSoundStream*     m_pStream[2];                       // Output streams
    DWORD                   m_dwStream;                         // Current stream
    BYTE*                   m_pSampleData;                      // Audio sample data

    // Packet status values
    DWORD                   m_adwStatus[PACKET_COUNT+EXTRA_PACKETS];

#if _DEBUG
    HRESULT DebugVerify();  // Verify all game soundtracks are present
#endif
};




#endif // MUSICMANAGER_H
