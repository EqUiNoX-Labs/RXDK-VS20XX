//-----------------------------------------------------------------------------
// File: FileStream.h
//
// Desc: Streaming wave file playback.
//
// Hist: 12.15.00 - New for December XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef WAVEBNKSTREAM_H
#define WAVEBNKSTREAM_H

#include <xactwb.h>

// Convenient wave format union
typedef union _WAVEBANKUNIWAVEFORMAT
{
    WAVEFORMATEX        WaveFormatEx;
    XBOXADPCMWAVEFORMAT AdpcmWaveFormat;
} WAVEBANKUNIWAVEFORMAT, *LPWAVEBANKUNIWAVEFORMAT;

// Enum for tracking which XMO owns a packet
enum PACKET_OWNER
{
    PACKET_OWNER_SOURCE,
    PACKET_OWNER_DEST,
};

// Define the maximum amount of packets we will ever submit to the renderer
#define WAVEBNKSTRM_PACKET_COUNT 2
#define WAVEBNKSTRM_PACKET_SIZE_IN_MS   100

struct PACKET_CONTEXT
{
    DWORD           dwPacketStatus; // Status of packet
    PACKET_OWNER    poPacketOwner;  // Owner XMO
    DWORD           dwPacketSize;   // Valid data size in packet
};

//-----------------------------------------------------------------------------
// Name: class CWaveFileStream
// Desc: Wave file streaming object
//-----------------------------------------------------------------------------
class CWaveBankStream
{
protected:
    XFileMediaObject*   m_pSourceXMO;                             // Source (wave file) filter
    IDirectSoundStream* m_pRenderXMO;                             // Render (DirectSoundStream) filter
    WAVEBANKHEADER*     m_pWaveBankHeader;                        // Wavebank header struct
    WAVEBANKDATA*       m_pWaveBankData;                          // Wavebank data struct
    WAVEBANKENTRY*      m_pWaveBankEntry;                         // Wavebank entry used by this streamer
    LPVOID              m_pvSourceBuffer;                         // PCM data buffer
    PACKET_CONTEXT      m_aContexts[WAVEBNKSTRM_PACKET_COUNT];    // Contexts for tracking packets
    LPVOID              m_pvLoopCachePacket;

    DWORD               m_dwFileLength;                           // File duration, in bytes
    DWORD               m_dwFileProgress;                         // File progress, in bytes

    CHAR*               m_pszFriendlyName;                        // Stream friendly name

    DWORD               m_dwStartOffset;                          // Starting file offset for this wave
    DWORD               m_dwStreamBytesRemaining;                 // Remaining bytes in the loop play/loop region
    DWORD               m_dwLastPacketIndex;                      // When != -1, it tells the renderer to use the loop cache
    DWORD               m_dwPacketSize;                           // Size of packet to use
    DWORD*              m_pdwPercentCompleted;                    // Pointer to percentage completed

    BOOL                m_bPaused;                                // Is stream paused?

    // Packet processing
    BOOL    FindFreePacket(DWORD* pdwPacketIndex );
    HRESULT ProcessSource( DWORD dwPacketIndex );
    HRESULT ProcessRenderer( DWORD dwPacketIndex );    

public:
    CWaveBankStream();
    ~CWaveBankStream();

    // Accessor Methods
    CHAR* GetFriendlyName()   { return m_pszFriendlyName; }

    // Processing
    HRESULT Process();

    // Initialization
    HRESULT Initialize( HANDLE          hFile, 
                        WAVEBANKHEADER* pWaveBankHeader, 
                        WAVEBANKENTRY*  pEntry, 
                        VOID*           pvLoopCache, 
                        CHAR*           pszFriendlyName, 
                        DWORD           dwPacketSize, 
                        DWORD*          pdwPercent );

    // Play control
    VOID Pause( DWORD dwPause );
};




#endif // WAVEBNKSTREAMM_H
