//-----------------------------------------------------------------------------
// File: SoundbitDB.cpp
//
// SoundbitDB just lets us record and store bot voices. 
// The code here isn't commented because it really isn't
// part of the demonstration of voice partitioning
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "commoninclude.h"

SoundbitDB g_SoundbitDB;

//------------------------------------------------------------------------------
// Name: SoundbitDB::SoundbitDB()
// Desc: construct the database of voice for the bots
//------------------------------------------------------------------------------
SoundbitDB::SoundbitDB()
{
    int i, j;
    m_State = SDB_STATE_READY;
    for ( i = 0; i < SOUNDBIT_ROWS; i++ ) 
        for ( j = 0; j < SOUNDBIT_COLS; j++ )
        {
            m_wNumPackets[ j + i * SOUNDBIT_COLS ] = 0;
        }       
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::State()
// Desc: Returns the current state of the database
//------------------------------------------------------------------------------
WORD SoundbitDB::State()
{
    return m_State;
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::NumPackets()
// Desc: Returns the number of packets in the iSample sample of the iVoice voice
//------------------------------------------------------------------------------
WORD  SoundbitDB::NumPackets( INT iVoice, INT iSample )
{
    return m_wNumPackets[ iVoice * SOUNDBIT_COLS + iSample ];
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::CurVoice()
// Desc: Returns the currently selected voice
//------------------------------------------------------------------------------
INT  SoundbitDB::CurVoice()
{
    return m_iRow;
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::CurSample()
// Desc: Returns the currently selected sample
//------------------------------------------------------------------------------
INT  SoundbitDB::CurSample()
{
    return m_iCol;
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::EditStop()
// Desc: Stop recording or playing if we currently are (in edit mode)
//------------------------------------------------------------------------------
VOID SoundbitDB::EditStop()
{
    if ( m_State == SDB_STATE_RECORDING )
        m_wNumPackets[ m_iRow * SOUNDBIT_COLS + m_iCol ] = m_iPacket;
   
    StopSoundbit( g_AudioMgr.SimulationXUID() ); // if we're playing something, stop it

    m_State = SDB_STATE_READY;
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::EditPlaySoundbit()
// Desc: Play a selected soundbit while in edit mode
//------------------------------------------------------------------------------
VOID SoundbitDB::EditPlaySoundbit( INT iVoice, INT iSample )
{
    EditStop();

    // for UI
    m_iRow = iVoice;
    m_iCol = iSample;
    
    PlaySoundbit( iVoice, iSample, g_AudioMgr.SimulationXUID() );
    m_State = SDB_STATE_PLAYING;  
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::PlaySoundbit()
// Desc: Play a soundbit while in a game session (locally)
//------------------------------------------------------------------------------
VOID SoundbitDB::PlaySoundbit( INT iVoice, INT iSample, XUID xuid )
{
    PlayInstance pi;
    DWORD basePos;
    
    pi.iPacket = 0;
    pi.iSample = iSample;
    pi.iVoice = iVoice;
    pi.iMode = 0;
    pi.luid = ID_UNPROCESSED;
   
    basePos = ( iVoice * SOUNDBIT_COLS + iSample ) * MAX_PACKETS_PER_SOUNDBIT;    
    pi.PacketNumber =  *((_XHV_CODEC_HEADER *)( m_byPackets + basePos * COMPRESSED_VOICE_SIZE ));
    pi.SampleTime = GetMachineTime() - SOUNDBIT_VOICE_BUFFER;
    pi.xuid = xuid;    

    m_PlayList.push_back( pi );
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::PlaySoundbitToVoiceMgr()
// Desc: Play a soundbit into a network packet
//------------------------------------------------------------------------------
VOID SoundbitDB::PlaySoundbitToVoiceMgr( INT iVoice, INT iSample, WORD luid )
{
    PlayInstance pi;
    DWORD basePos;

    pi.iPacket = 0;
    pi.iSample = iSample;
    pi.iVoice = iVoice;
    pi.iMode = 1;
   
    basePos = ( iVoice * SOUNDBIT_COLS + iSample ) * MAX_PACKETS_PER_SOUNDBIT;    
    pi.PacketNumber =  *((_XHV_CODEC_HEADER *)( m_byPackets + basePos * COMPRESSED_VOICE_SIZE ));
    pi.SampleTime = GetMachineTime() - SOUNDBIT_VOICE_BUFFER;
    pi.luid = luid;
    pi.xuid = g_PlayerMgr.PlayerFromLUID( luid )->GetXUID();    

    m_PlayList.push_back( pi );
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::Update()
// Desc: Update the soundbit editor, or just playing soundbits if we are in 
//       a game session
//------------------------------------------------------------------------------
VOID SoundbitDB::Update()    
{
    std::vector<PlayInstance>::iterator i, j;
    DWORD basePos;
    INT nTotalPackets;
    INT nElapsedPackets;  
    VoicePacketWrapper vpw;
    _XHV_CODEC_HEADER   *pNextPacket;     // next possible sequence number
    
    BYTE *FixupVoicePacket = vpw.bVoiceData;
    _XHV_CODEC_HEADER *FixupVoicePacketHeader = (_XHV_CODEC_HEADER *)FixupVoicePacket;
   
    for ( i = m_PlayList.begin(); i != m_PlayList.end(); i = j )
    {
        j = i + 1;

        nTotalPackets = NumPackets( i->iVoice, i->iSample );
        
        nElapsedPackets = (INT) ( ( GetMachineTime() - i->SampleTime ) / VOICE_PACKET_TIME_IN_CPU_CYCLES );
        i->SampleTime += (__int64)VOICE_PACKET_TIME_IN_CPU_CYCLES * nElapsedPackets;
    
        basePos = ( i->iVoice * SOUNDBIT_COLS + i->iSample ) * MAX_PACKETS_PER_SOUNDBIT + i->iPacket;
        pNextPacket = ((_XHV_CODEC_HEADER *)( m_byPackets + basePos * COMPRESSED_VOICE_SIZE )); 
    
        while ( nElapsedPackets && ( i->iPacket != nTotalPackets ) )
        {

            // forge packet headers so it looks like we sent them recently, even though
            // we recorded them with completely different sequence numbers

            // because we are doing this based on absolute time, and XHV is based on 
            // the millisecond interrupt timer, voice quality may degrade a little
            // over time before XHV figures it out and resets the packet #s

            if ( pNextPacket->wSeqNo == i->PacketNumber.wSeqNo )
            {      
                memcpy( FixupVoicePacket, m_byPackets + basePos * COMPRESSED_VOICE_SIZE, COMPRESSED_VOICE_SIZE );
                
                FixupVoicePacketHeader->bMsgNo = pNextPacket->bMsgNo;
                FixupVoicePacketHeader->wSeqNo = (WORD) ( GetMachineTime() / VOICE_PACKET_TIME_IN_CPU_CYCLES ) - nElapsedPackets; 
                
                if (i->iMode == 0 )
                    g_AudioMgr.SubmitVoicePacket( i->xuid, FixupVoicePacket );        
                else
                {
                    vpw.nPackets = 1;
                    vpw.byVoiceMode = VMM_NORMAL;      
                    vpw.wVoiceFromID = i->luid;
                    vpw.wVoiceToID = ID_NORMAL_VOICE; 
                    g_VoiceMgr.ProcessVoiceData( vpw );
                }

                i->iPacket++;
                basePos++;

                pNextPacket = ((_XHV_CODEC_HEADER *)( m_byPackets + basePos * COMPRESSED_VOICE_SIZE )); 
            }
             
             i->PacketNumber.wSeqNo++;
             nElapsedPackets--;                          
        }

        if ( i->iPacket == nTotalPackets ) // we're done playing
        {
            j = m_PlayList.erase( i );
        }
    }      
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::IsPlaying()
// Desc: Returns whether we are playing a sound for the given XUID bot
//------------------------------------------------------------------------------
BOOL SoundbitDB::IsPlaying( XUID xuid )
{
    std::vector<PlayInstance>::iterator i;
    
    for ( i = m_PlayList.begin(); i != m_PlayList.end(); i++ )
    {        
        if ( XOnlineAreUsersIdentical( &(i->xuid), &xuid ) )
            return TRUE;
    }

    return FALSE;
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::IsPlaying()
// Desc: Returns whether we are playing a sound for the given LUID player
//------------------------------------------------------------------------------
BOOL SoundbitDB::IsPlaying( WORD luid )
{
    std::vector<PlayInstance>::iterator i;
    
    for ( i = m_PlayList.begin(); i != m_PlayList.end(); i++ )
    {        
        if ( i->luid == luid )
            return TRUE;
    }

    return FALSE;
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::StopSoundbit()
// Desc: Stop the given xuid from playing
//------------------------------------------------------------------------------
VOID SoundbitDB::StopSoundbit( XUID xuid )
{
   std::vector<PlayInstance>::iterator i;
    
   for ( i = m_PlayList.begin(); i != m_PlayList.end(); i++ )
   {
       if ( XOnlineAreUsersIdentical( &(i->xuid), &xuid ) )
       {
            m_PlayList.erase( i );
            return;
       }
   }
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::StopSoundbit()
// Desc: Stop the given luid from playing
//------------------------------------------------------------------------------
VOID SoundbitDB::StopSoundbit( WORD luid )
{
   std::vector<PlayInstance>::iterator i;
    
   for ( i = m_PlayList.begin(); i != m_PlayList.end(); i++ )
   {
       if ( i->luid == luid )
       {
            m_PlayList.erase( i );
            return;
       }
   }
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::EditRecordSoundbit()
// Desc: Records a soundbit in the editor
//------------------------------------------------------------------------------
VOID SoundbitDB::EditRecordSoundbit( INT iVoice, INT iSample )
{
    EditStop();
    m_iRow = iVoice;
    m_iCol = iSample;
    m_iPacket = 0;
    m_State = SDB_STATE_RECORDING;
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::EditSubmitSpeechPacketsToSoundbit()
// Desc: Submit microphone packets to the currently recording soundbit
//------------------------------------------------------------------------------
VOID SoundbitDB::EditSubmitSpeechPacketsToSoundbit( BYTE *pBytes, INT nPackets )
{
    DWORD basePos;

    if (nPackets == 0) return;

    if ( (m_iPacket + nPackets) > MAX_PACKETS_PER_SOUNDBIT )    
        nPackets = MAX_PACKETS_PER_SOUNDBIT - m_iPacket;
    
    basePos = ( ( m_iRow * SOUNDBIT_COLS + m_iCol ) * MAX_PACKETS_PER_SOUNDBIT + m_iPacket );
    
    memcpy( m_byPackets + basePos * COMPRESSED_VOICE_SIZE, pBytes, nPackets * COMPRESSED_VOICE_SIZE );
       
    m_iPacket += nPackets;    
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::ReadFromFile()
// Desc: Reads the database from a .sdb file
//------------------------------------------------------------------------------
VOID SoundbitDB::ReadFromFile( char *pszFilename )
{
    DWORD dwBytesRead;
    HANDLE hFile = CreateFile(pszFilename,
                      GENERIC_READ,                 // open for reading 
                      0,                            // do not share 
                      NULL,                         // no security 
                      OPEN_EXISTING,                // overwrite existing 
                      FILE_ATTRIBUTE_NORMAL |       // normal file 
                      FILE_FLAG_SEQUENTIAL_SCAN,    
                      NULL);                        // no attr. template 

    if ( hFile == INVALID_HANDLE_VALUE ) return;  // couldn't open file

    ReadFile( hFile, m_byPackets, SOUNDBIT_ROWS * SOUNDBIT_COLS * MAX_PACKETS_PER_SOUNDBIT * COMPRESSED_VOICE_SIZE, &dwBytesRead, NULL );
    ReadFile( hFile, m_wNumPackets, SOUNDBIT_ROWS * SOUNDBIT_COLS * sizeof ( WORD ), &dwBytesRead, NULL );
    
    CloseHandle( hFile );   
}




//------------------------------------------------------------------------------
// Name: SoundbitDB::WriteToFile()
// Desc: Write the database to a .sdb file
//------------------------------------------------------------------------------
VOID SoundbitDB::WriteToFile( char *pszFilename )
{
    DWORD dwBytesRead;
    HANDLE hFile = CreateFile(pszFilename,
                      GENERIC_WRITE,                // open for writing 
                      0,                            // do not share 
                      NULL,                         // no security 
                      CREATE_ALWAYS,                // overwrite existing 
                      FILE_ATTRIBUTE_NORMAL |       // normal file 
                      FILE_FLAG_SEQUENTIAL_SCAN,    
                      NULL);                        // no attr. template 

    if ( hFile == INVALID_HANDLE_VALUE ) return;  // couldn't open file

    WriteFile( hFile, m_byPackets, SOUNDBIT_ROWS * SOUNDBIT_COLS * MAX_PACKETS_PER_SOUNDBIT * COMPRESSED_VOICE_SIZE, &dwBytesRead, NULL );
    WriteFile( hFile, m_wNumPackets, SOUNDBIT_ROWS * SOUNDBIT_COLS * sizeof ( WORD ), &dwBytesRead, NULL );
   
    CloseHandle( hFile );   
}
    