//-----------------------------------------------------------------------------
// File: SoundbitDB.h
//
// SoundbitDB just lets us record and store bot voices. 
// The code here isn't commented because it really isn't
// part of the demonstration of voice partitioning
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

#define SDB_STATE_READY         0x0
#define SDB_STATE_RECORDING     0x1
#define SDB_STATE_PLAYING       0x2

struct PlayInstance
{
    INT  iVoice;  
    INT  iSample;   
    INT  iPacket;                           // next packet to submit
    
    INT  iMode;                             // play to audio = 0, play to voice mgr = 1
    XUID xuid;                              // xuid we are playing as
    WORD luid;                              // luid we are playing as

    __int64             SampleTime;         // time last packet played
    _XHV_CODEC_HEADER   PacketNumber;       // sample sequence number we're on    
};

class SoundbitDB
{
public:
    SoundbitDB();    

    VOID ReadFromFile( char *pszFilename );
    VOID WriteToFile( char *pszFilename );

    // Standard interface
    VOID Update();                                             // call every frame - keeps track of time since last call    
    VOID PlaySoundbit( INT iVoice, INT iSample, XUID xuid );   // play voice and sample as xuid XUID
    VOID PlaySoundbitToVoiceMgr( INT iVoice, INT iSample, WORD luid );
    VOID StopSoundbit( XUID xuid );                            // stop playing soundbit with xuid XUID
    VOID StopSoundbit( WORD luid );                            // stop playing soundbit 
    BOOL IsPlaying( XUID xuid );                               // returns whether the given xuid is playing still
    BOOL IsPlaying( WORD luid );                               // returns whether the given luid is playing still

    // edit mode interface
    VOID EditRecordSoundbit( INT iVoice, INT iSample );             // coorespond to rows/cols
    VOID EditSubmitSpeechPacketsToSoundbit( BYTE *pBytes, INT nPackets );
    VOID EditPlaySoundbit( INT iVoice, INT iSample );
    VOID EditStop();        
    
    // Accessors 
    WORD State();    
    INT  CurVoice();
    INT  CurSample();
    WORD NumPackets( INT iVoice, INT iSample );

private:        
    // big easy flat array with lots of wasted space
    BYTE m_byPackets[ SOUNDBIT_ROWS * SOUNDBIT_COLS * COMPRESSED_VOICE_SIZE * MAX_PACKETS_PER_SOUNDBIT];  
    
    // number of packets used in each 
    WORD m_wNumPackets[ SOUNDBIT_ROWS * SOUNDBIT_COLS ];
  
    // playing, recording, ready-- SDB_STATE_ defines above
    WORD m_State;

    // Data on the sample we are playing or recording
    INT  m_iRow;        // for recording 
    INT  m_iCol;        // for recording
    INT  m_iPacket;     // for recording
    
    // Data on instances currently being played
    std::vector< PlayInstance > m_PlayList;
};

extern SoundbitDB g_SoundbitDB;