//-----------------------------------------------------------------------------
// File: AudioEngine.h
//
// Desc: Declaration of the AudioEngine class.  This class controls
//       XACT to provide audio playback in the game
//
// Hist: 6.26.02 - New for August 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef TECHCERTGAME_AUDIOENGINE_H
#define TECHCERTGAME_AUDIOENGINE_H
#include "Common.h"
#include <dsstdfx.h>
#include <xact.h>
#include <vector>
#include "XACTSounds.h"

using namespace std;




//-----------------------------------------------------------------------------
// Name: class CSoundtrack
// Desc: Abstraction layer for soundtracks that help merge together game
//       soundtracks and user soundtracks stored on the Xbox hard drive
//-----------------------------------------------------------------------------
class CSoundtrack
{
public:
    CSoundtrack() {}

    VOID    GetSoundtrackName( WCHAR * strName ) const
        { wcscpy( strName, m_strName ); }
    UINT    GetSongCount() const
        { return m_uSongCount; }

    WCHAR       m_strName[MAX_SOUNDTRACK_NAME];
    UINT        m_uSongCount;
    BOOL        m_fGameSoundtrack;
    union {
        UINT    m_uSoundtrackID;
        UINT    m_uSoundtrackIndex;
    };
};



//-----------------------------------------------------------------------------
// Name: class AudioEngine
// Desc: Audio engine.  Basically controls XACT to do our bidding
//-----------------------------------------------------------------------------
class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    HRESULT Initialize();                                       // Initialize the audio engine
    HRESULT LoadFile( CHAR* strFilename, VOID** ppvData, DWORD* pdwSize );  // Loads a file into memory

    // Playback status
    HRESULT Play( DWORD xactCue );  // Begin playback
    HRESULT Stop( DWORD xactCue = XACT_SOUNDCUE_INDEX_UNUSED ); // Stop playback
    HRESULT Pause( WORD xactCategory = XACT_CATEGORY_INDEX_UNUSED );    // Pause playback
    HRESULT Unpause( WORD xactCategory = XACT_CATEGORY_INDEX_UNUSED );  // Unpause audio

    // Accessor methods
    HRESULT SetRandom( BOOL fRandom );                          // Sets random song selection
    BOOL    GetRandom() { return m_fRandom; }                   // Gets the random song selection status
    HRESULT SetVolume( XACT_CATEGORY xactCategory, FLOAT fVolume);      // Sets the volume of a category
    const CSoundtrack& GetSoundtrack( UINT iSoundtrackIndex ) const;    // Gets information on a particular soundtrack
    UINT    GetNumberOfSoundtracks() { return m_vSoundtracks.size(); }  // Gets the number of soundtracks
    HRESULT SelectSoundtrack( DWORD dwSoundtrack, BOOL fInitialize = FALSE ); // Switch to a soundtrack

    // Worker function
    VOID    DoWork() { return XACTEngineDoWork(); }             // Allows the audio engine to do work

private:
    HRESULT LoadSoundtracks();                                  // Loads soundtrack information

    // State variables
    BOOL                    m_fRandom;                          // TRUE to randomize among songs
    LONG                    m_lMusicVolume;                     // Music Volume level
    LONG                    m_lSoundEffectVolume;               // Sound effect Volume level

    // XACT Engine
    PXACTENGINE             m_pXACT;                            // XACT Engine
    PBYTE                   m_pbWaveBank;                       // Wave Bank data
    PXACTWAVEBANK           m_pWaveBank;                        // XACT Wave Bank
    PBYTE                   m_pbSoundBank;                      // Sound Bank data
    PXACTSOUNDBANK          m_pSoundBank;                       // XACT Sound Bank
    PXACTWMAPLAYLIST        m_pWMAPlaylist;                     // WMA Playlist for soundtracks

protected:
    // Music information
    vector<CSoundtrack>     m_vSoundtracks;                     // Vector of soundtracks
    UINT                    m_uCurrentSoundtrack;               // Currently selected soundtrack
};

#endif // TECHCERTGAME_AUDIOENGINE_H
