//-----------------------------------------------------------------------------
// File: WMAInMemory.h
//
// Desc: Streaming wave file playback.
//
// Hist: 03.15.01 - New for May XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef WMAINMEMORY_H
#define WMAINMEMORY_H


// Define the maximum amount of packets we will ever submit to the renderer
#define WMASTRM_PACKET_COUNT 8


// Size of an IO packed (128k)
#define IOPACKETSIZE ( 128*1024 )


// Define the source packet size:
// This value is hard-coded assuming a WMA file of stereo, 16bit resolution.
// If this value can by dynamically set based on the WMA format, keeping in
// mind that WMA needs enough buffer for a minimum of 2048 samples worth of
// PCM data
#define WMASTRM_SOURCE_PACKET_BYTES ( 2048*2*2 )




//-----------------------------------------------------------------------------
// Name: WMAStreamCallback()
// Desc: Callback function for the stream to request more data
//-----------------------------------------------------------------------------
DWORD CALLBACK WMAStreamCallback( VOID* pContext, DWORD offset,
                                  DWORD num_bytes, VOID** ppData );


//-----------------------------------------------------------------------------
// Name: WAVStreamThreadProc()
// Desc: Thread proc for sound processing worker thread
//-----------------------------------------------------------------------------
DWORD WINAPI WAVStreamThreadProc( VOID* pParameter );




//-----------------------------------------------------------------------------
// Name: class CWMAFileStream
// Desc: Wave file streaming object
//-----------------------------------------------------------------------------
class CWMAFileStream
{
protected:
    XMediaObject*       m_pSourceFilter;    // Source (wave file) filter
    IDirectSoundStream* m_pRenderFilter;    // Render(DirectSoundStream) filter
    LPVOID              m_pvSourceBuffer;   // Source filter data buffer
    LPVOID              m_pvRenderBuffer;   // Render filter data buffer
    DWORD               m_adwPacketStatus[WMASTRM_PACKET_COUNT];// Packet
                                                                // status array
    DWORD               m_dwFileLength;     // File duration, in bytes
    HANDLE              m_hFile;            // File handle
    HANDLE              m_hThread;          // Handle to our IO thread
    BOOL                m_bKillThread;      // Signal to kill the thread

    // Front and back read buffers
    BYTE*               m_pFileBuffer;          
    BYTE*               m_pBackBuffer;
    
    // Packet processing
    BOOL                FindFreePacket( DWORD* pdwPacketIndex );
    HRESULT             ProcessSource( DWORD dwPacketIndex );
    HRESULT             ProcessRenderer( DWORD dwPacketIndex );

    // Callback friend
    friend DWORD CALLBACK WMAStreamCallback( VOID* pContext, ULONG ulOffset,
                                             ULONG ulNumbytes, VOID** ppData );

    // IO proc friend
    friend DWORD WINAPI WAVStreamThreadProc( LPVOID lpParameter );

    ULONG               m_ulCurrentOffset;  // Current buffer offset
    VOID                SwitchBuffers();    // Switch packet buffers
    
    HRESULT             Process();          // Processing

public:
    // Initialization
    HRESULT Initialize( const CHAR* strFileName );

    // Play control
    VOID    Pause( DWORD dwPause );

    // Constructor/destructor
    CWMAFileStream();
    ~CWMAFileStream();
};




#endif // WMAINMEMORY_H
