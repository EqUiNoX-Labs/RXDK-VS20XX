//-----------------------------------------------------------------------------
// File: WMAInMemory.cpp
//
// Desc: Class for streaming wave file playback using in-memory WMA codec XMO.
//
// Hist: 3.08.01 - New for May XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <cassert>
#include <algorithm>
#include <xtl.h>
#include "WMAInMemory.h"
#include "xbutil.h"




//-----------------------------------------------------------------------------
// Name: CWMAFileStream()
// Desc: Object constructor.
//-----------------------------------------------------------------------------
CWMAFileStream::CWMAFileStream()
{
    m_pSourceFilter    = NULL;
    m_pRenderFilter    = NULL;
    m_pvSourceBuffer   = NULL;
    m_pvRenderBuffer   = NULL;
    
    // init packets
    for( DWORD i = 0; i < WMASTRM_PACKET_COUNT; i++ )
        m_adwPacketStatus[i] = XMEDIAPACKET_STATUS_SUCCESS;

    m_dwFileLength   = 0;
    m_pFileBuffer = NULL;
    m_pBackBuffer = NULL;
    m_hFile = INVALID_HANDLE_VALUE;
    m_hThread = NULL;   // Error return for CreateThread is NULL
    m_bKillThread = FALSE;
}




//-----------------------------------------------------------------------------
// Name: ~CWMAFileStream()
// Desc: Object destructor.
//-----------------------------------------------------------------------------
CWMAFileStream::~CWMAFileStream()
{
    SAFE_RELEASE( m_pSourceFilter );
    SAFE_RELEASE( m_pRenderFilter );
    delete[] m_pvSourceBuffer;
    delete[] m_pFileBuffer;
    delete[] m_pFileBuffer;
    CloseHandle( m_hFile );

    // Kill the thread 

    // First, resume the thread in case it was paused
    ResumeThread( m_hThread );
    
    // Signal thread 
    m_bKillThread = TRUE;
    
    // Wait for thread to exit
    WaitForSingleObject( m_hThread, INFINITE );

    CloseHandle( m_hThread );
    m_hThread = NULL;   // Error return for CreateThread is NULL
}





//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the wave file streaming subsystem.
//-----------------------------------------------------------------------------
HRESULT CWMAFileStream::Initialize( const CHAR* strFileName )
{
    HRESULT hr;
    
    // Before we create the in memory decoder, we must read the WMA file
    // and have it in memory. The WmaCreateInMemoryDecoder function
    // will start calling our callback immediately for data...

    m_hFile = CreateFile( strFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING, 
                          FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN,
                          NULL );

    if( m_hFile == INVALID_HANDLE_VALUE )
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        return hr;
    }

    m_dwFileLength = GetFileSize( m_hFile, NULL );

    assert( m_dwFileLength >= 2*IOPACKETSIZE );

    m_pFileBuffer = new BYTE[IOPACKETSIZE];
    m_pBackBuffer = new BYTE[IOPACKETSIZE];

    // Do some checks here
    DWORD dwNumBytesRead;
    if( !ReadFile( m_hFile, m_pFileBuffer, IOPACKETSIZE, &dwNumBytesRead, NULL ) ||
                   dwNumBytesRead != IOPACKETSIZE )
        return E_UNEXPECTED;

    if( !ReadFile( m_hFile, m_pBackBuffer, IOPACKETSIZE, &dwNumBytesRead, NULL ) ||
                   dwNumBytesRead != IOPACKETSIZE )
        return E_UNEXPECTED;

    // Initialize current offset;
    m_ulCurrentOffset = 0;
   
    // Create the source (WMA file) filter
    WAVEFORMATEX   wfxSourceFormat;
    hr = WmaCreateInMemoryDecoder( WMAStreamCallback, this, 0, // don't yield
                                   &wfxSourceFormat, &m_pSourceFilter );
    if( FAILED(hr) )
        return hr;

    // Create the render (DirectSoundStream) filter
    DSSTREAMDESC dssd;
    ZeroMemory( &dssd, sizeof(dssd) );
    dssd.dwMaxAttachedPackets = WMASTRM_PACKET_COUNT;
    dssd.lpwfxFormat          = &wfxSourceFormat;

    hr = DirectSoundCreateStream( &dssd, &m_pRenderFilter );
    if( FAILED(hr) )
        return hr;

#ifdef _DEBUG
    // We expect the source filter to be synchronous and read-only, the
    // transform filter to be synchronous and read/write and the render
    // filter to be asynchronous write-only.  Assert that all of this 
    // is true and check the packet sizes for compatibility.
    XMEDIAINFO xmi;
    hr = m_pSourceFilter->GetInfo( &xmi );
    if( FAILED(hr) )
        return hr;
    assert( xmi.dwFlags & XMO_STREAMF_FIXED_SAMPLE_SIZE );

    assert( !xmi.dwMaxLookahead );
    assert( xmi.dwOutputSize );
    assert( !(WMASTRM_SOURCE_PACKET_BYTES % xmi.dwOutputSize) );

    hr = m_pRenderFilter->GetInfo( &xmi );
    if( FAILED(hr) )
        return hr;
    assert( xmi.dwFlags ==
            (XMO_STREAMF_FIXED_SAMPLE_SIZE | XMO_STREAMF_INPUT_ASYNC) );
    assert( WMASTRM_SOURCE_PACKET_BYTES * WMASTRM_PACKET_COUNT >=
            xmi.dwMaxLookahead );
    assert( !(WMASTRM_SOURCE_PACKET_BYTES % xmi.dwInputSize) );
    assert( !xmi.dwOutputSize );
#endif

    // Allocate data buffers.  Since the source filter is synchronous, we only
    // have to allocate enough data to process a single packet.  The render
    // filter, however, is asynchronous, so we'll have to allocate enough
    // space to hold all the packets that could be submitted at any given time.
    m_pvSourceBuffer = new BYTE[ WMASTRM_SOURCE_PACKET_BYTES * WMASTRM_PACKET_COUNT ];
    if( NULL == m_pvSourceBuffer )
        return E_OUTOFMEMORY;

     // Create worker thread to process audio
    m_hThread = CreateThread( NULL, 0, WAVStreamThreadProc, this, 0, NULL );
    if( m_hThread == NULL )   // Error return for CreateThread is NULL
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: WMAStreamCallback()
// Desc: The WMA decoder calls this function to retrieve raw (compressed)
//       file data.
//-----------------------------------------------------------------------------
DWORD CALLBACK WMAStreamCallback( VOID* pContext, ULONG ulOffset,
                                  ULONG ulNumbytes, VOID** ppData )
{
    CWMAFileStream *pThis = (CWMAFileStream*)pContext;
    assert( pThis );

    // If the loaded has closed the thread we are finished
    if( pThis->m_bKillThread )
        ExitThread( 0 );

    // Patch buffer
    static BYTE pBuffer[128];
    assert( ulNumbytes <= 128 );

    // File has looped
    if( ulOffset < pThis->m_ulCurrentOffset )
    {
        pThis->SwitchBuffers();
        assert( pThis->m_ulCurrentOffset == 0 );
        assert( ulOffset == 0 );
    }

    // We are in a buffer region
    if( ulOffset < (pThis->m_ulCurrentOffset + IOPACKETSIZE) )
    {
        ULONG ulBufferOffset = ulOffset - pThis->m_ulCurrentOffset;

        (*ppData) = pThis->m_pFileBuffer + ulBufferOffset;

        // request is across buffers,  patch the buffers
        if( (ulBufferOffset + ulNumbytes) > IOPACKETSIZE )
        {
            ULONG ulFirstHalf = IOPACKETSIZE - ulBufferOffset;
            memcpy( pBuffer, *ppData, ulFirstHalf );
            pThis->SwitchBuffers();
            (*ppData) = pThis->m_pFileBuffer;

            ULONG ulSecondHalf = ulNumbytes - (IOPACKETSIZE - ulBufferOffset);
        
            memcpy( pBuffer + IOPACKETSIZE - ulBufferOffset, (*ppData), ulSecondHalf );
            (*ppData) = pBuffer;

            assert( ulFirstHalf + ulSecondHalf == ulNumbytes );
        }
    }
    else  // Right on buffer boundary,  just switch
    {
        pThis->SwitchBuffers();
        ULONG ulBufferOffset = ulOffset - pThis->m_ulCurrentOffset;
        (*ppData) = pThis->m_pFileBuffer + ulBufferOffset;
    }

    return ulNumbytes;
}




//-----------------------------------------------------------------------------
// Name: Process()
// Desc: Performs any work necessary to keep the stream playing.
//-----------------------------------------------------------------------------
HRESULT CWMAFileStream::Process()
{
    HRESULT hr;

    // Find a free packet.  If there's none free, we don't have anything to do
    DWORD dwPacketIndex;
    while( FindFreePacket( &dwPacketIndex ) )
    {
         // Read from the source filter
         hr = ProcessSource( dwPacketIndex );
         if( FAILED(hr) )
             return hr;
         
         // Send the data to the renderer
         hr = ProcessRenderer( dwPacketIndex );
         if( FAILED(hr) )
             return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FindFreePacket()
// Desc: Finds a render packet available for processing.
//-----------------------------------------------------------------------------
BOOL CWMAFileStream::FindFreePacket( DWORD* pdwPacketIndex )
{
    for( DWORD dwIndex = 0; dwIndex < WMASTRM_PACKET_COUNT; dwIndex++ )
    {
        if( XMEDIAPACKET_STATUS_PENDING != m_adwPacketStatus[dwIndex] )
        {
            if( pdwPacketIndex )
                (*pdwPacketIndex) = dwIndex;

            return TRUE;
        }
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ProcessSource()
// Desc: Reads data from the source filter.
//-----------------------------------------------------------------------------
HRESULT CWMAFileStream::ProcessSource( DWORD dwPacketIndex )
{
    // We're going to read a full packet's worth of data into the source
    // buffer.  Since we're playing in an infinite loop, we'll just spin
    // until we've read enough data, even if that means wrapping around the
    // end of the file.

    XMEDIAPACKET xmp;
    DWORD        dwTotalSourceUsed   = 0;
    DWORD        dwSourceUsed = 0;
    ZeroMemory( &xmp, sizeof(xmp) );
    xmp.pvBuffer         = (BYTE*)m_pvSourceBuffer + (dwPacketIndex * WMASTRM_SOURCE_PACKET_BYTES);
    xmp.dwMaxSize        = WMASTRM_SOURCE_PACKET_BYTES;
    xmp.pdwCompletedSize = &dwSourceUsed;

    while( dwTotalSourceUsed < WMASTRM_SOURCE_PACKET_BYTES )
    {
        // Read from the source
        HRESULT hr = m_pSourceFilter->Process( NULL, &xmp );
        if( FAILED( hr ) )
            return hr;

        // Add the amount read to the total
        dwTotalSourceUsed += dwSourceUsed;

        // If we read less than the amount requested, it's because we hit
        // the end of the file.  Seek back to the start and keep going.
        if( dwSourceUsed < xmp.dwMaxSize )
        {
            xmp.pvBuffer  = (BYTE*)xmp.pvBuffer + dwSourceUsed;
            xmp.dwMaxSize = xmp.dwMaxSize - dwSourceUsed;
            
            hr = m_pSourceFilter->Flush();
            if( FAILED( hr ) )
                return hr;
        };
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ProcessRenderer()
// Desc: Sends data to the renderer.
//-----------------------------------------------------------------------------
HRESULT CWMAFileStream::ProcessRenderer( DWORD dwPacketIndex )
{
    // There's a full packet's worth of data ready for us to send to the
    // renderer.  We want to track the status of this packet since the
    // render filter is asynchronous and we need to know when the packet is
    // completed.
    XMEDIAPACKET xmp;
    ZeroMemory( &xmp, sizeof(xmp) );
    xmp.pvBuffer  = (BYTE*)m_pvSourceBuffer + (dwPacketIndex * WMASTRM_SOURCE_PACKET_BYTES);
    xmp.dwMaxSize = WMASTRM_SOURCE_PACKET_BYTES;
    xmp.pdwStatus = &m_adwPacketStatus[dwPacketIndex];

    HRESULT hr = m_pRenderFilter->Process( &xmp, NULL );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SwitchBuffers()
// Desc: switches the playback buffers
//-----------------------------------------------------------------------------
VOID CWMAFileStream::SwitchBuffers()
{
    // Swap file and back buffer
    std::swap( m_pFileBuffer, m_pBackBuffer );
    
    // Update buffer offset
    m_ulCurrentOffset += IOPACKETSIZE;
    if( m_ulCurrentOffset >= m_dwFileLength )
        m_ulCurrentOffset = 0;
    
    // Update read offset
    ULONG ulReadOffset = m_ulCurrentOffset + IOPACKETSIZE;
    if( ulReadOffset >= m_dwFileLength )
        SetFilePointer( m_hFile, 0, NULL, FILE_BEGIN );
    
    // Read in buffer
    DWORD dwNumBytesRead;
    ReadFile( m_hFile, m_pBackBuffer, IOPACKETSIZE, &dwNumBytesRead, NULL );
#ifdef DEBUG
    assert( bSuccess );
    // We should either read an entire packet or reach the end of the file
    assert( dwNumBytesRead == IOPACKETSIZE ||
            ulReadOffset + dwNumBytesRead == m_dwFileLength );
#endif
}




//-----------------------------------------------------------------------------
// Name: Pause()
// Desc: Pauses and resumes stream playback
//-----------------------------------------------------------------------------
VOID CWMAFileStream::Pause( DWORD dwPause )
{
    m_pRenderFilter->Pause( dwPause );
    if( dwPause )
        SuspendThread( m_hThread );
    else
        ResumeThread( m_hThread );
}




//-----------------------------------------------------------------------------
// Name: WAVStreamThreadProc()
// Desc: Thread proc for sound processing worker thread
//-----------------------------------------------------------------------------
DWORD WINAPI WAVStreamThreadProc( VOID* pParameter )
{
    // To process approximately once per frame, we can sleep
    // for 1000 ms / 60 FPS between calls to Process.
    DWORD dwQuantum = 1000 / 60;

    // Alternately, the minimum time between processing is
    // determined by how much data we're sending to the stream
    // at once:
    // 2048 * 16 samples per packet * 16 packets = 524288 samples
    // 524288 samples / 44100 samples per second = 11.8 seconds
    //
    // DWORD dwQuantum = 11000;

    CWMAFileStream* pThis = (CWMAFileStream*)pParameter;
    assert( pThis );

    for(;;)
    {
        // Kill thread if not needed 
        if( pThis->m_bKillThread )
            ExitThread( 0 );

        pThis->Process();
        Sleep( dwQuantum );
    }
}
