//-----------------------------------------------------------------------------
// File: wavebankStream.cpp
//
// Desc: Class for streaming wave date from a wavebank.
//
// Hist: 5.01.02 - New for May 2002 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <assert.h>
#include <XBUtil.h>
#include "wavebankStream.h"




//-----------------------------------------------------------------------------
// Name: CWaveBankStream()
// Desc: Object constructor.
//-----------------------------------------------------------------------------
CWaveBankStream::CWaveBankStream()
{
    m_pSourceXMO        = NULL;
    m_pRenderXMO        = NULL;
    m_pvSourceBuffer    = NULL;

    m_dwFileLength      = 0;
    m_dwFileProgress    = 0;

    m_pszFriendlyName   = NULL;

    m_dwStartOffset     = 0;
    m_dwLastPacketIndex = -1;

    m_bPaused           = FALSE;
}




//-----------------------------------------------------------------------------
// Name: ~CWaveBankStream()
// Desc: Object destructor.
//-----------------------------------------------------------------------------
CWaveBankStream::~CWaveBankStream()
{
    if( m_pSourceXMO )
        m_pSourceXMO->Release();

    if( m_pRenderXMO )
        m_pRenderXMO->Release();

    if( m_pvSourceBuffer )
        XPhysicalFree( m_pvSourceBuffer );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the wave bank streaming subsystem.
//-----------------------------------------------------------------------------
HRESULT CWaveBankStream::Initialize( HANDLE          hFile, 
                                     WAVEBANKHEADER* pWaveBankHeader, 
                                     WAVEBANKENTRY*  pWaveBankEntry, 
                                     VOID*           pvLoopCache, 
                                     CHAR*           pszFriendlyName, 
                                     DWORD           dwPacketSize, 
                                     DWORD*          pdwPercentCompleted )
{
    HRESULT hr;
    WAVEBANKUNIWAVEFORMAT waveFormat;
    
    m_pdwPercentCompleted = pdwPercentCompleted;
    m_dwPacketSize        = dwPacketSize;
    m_pvLoopCachePacket   = pvLoopCache;
    m_pszFriendlyName     = pszFriendlyName;
    m_pWaveBankEntry      = pWaveBankEntry;
    m_pWaveBankHeader     = pWaveBankHeader;
    m_pWaveBankData       = (WAVEBANKDATA*)((BYTE*)m_pWaveBankHeader + 
                            m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_BANKDATA].dwOffset);

    if( m_pWaveBankEntry->LoopRegion.dwLength <= m_dwPacketSize )
    {
        OUTPUT_DEBUG_STRING( "Loop region must be more than 1 full packet\n" );
        return E_FAIL;
    }

    // Expand the wavebank format to a WAVEFORMATEX
    if( pWaveBankEntry->Format.wFormatTag == WAVEBANKMINIFORMAT_TAG_PCM ) 
    {        
        XAudioCreatePcmFormat( pWaveBankEntry->Format.nChannels,
                               pWaveBankEntry->Format.nSamplesPerSec,
                               (pWaveBankEntry->Format.wBitsPerSample == WAVEBANKMINIFORMAT_BITDEPTH_8 ) ? 8 : 16, 
                               &waveFormat.WaveFormatEx );
        
    } 
    else 
    {        
        XAudioCreateAdpcmFormat( pWaveBankEntry->Format.nChannels,
                                 pWaveBankEntry->Format.nSamplesPerSec,
                                 &waveFormat.AdpcmWaveFormat );
    }

    // Create the DirectSound Stream, remembering to account for the
    // extra packet we used for the cached start of the loop region.
    // The reason for setting dwMaxAttachedPackets = 2 * packet_count + 1
    // is that we could conceivably submit LoopCachePacket, Packet 0, 
    // LoopCachePacket, Packet 1, LoopCachePacket all together.  So we
    // end up potentially with 2 * packet_count + 1 packets submitted at
    // one time.
    DSSTREAMDESC dssd = {0};
    dssd.dwMaxAttachedPackets = 2 * WAVEBNKSTRM_PACKET_COUNT + 1;
    dssd.lpwfxFormat          = (LPWAVEFORMATEX)&waveFormat.WaveFormatEx;

    hr = DirectSoundCreateStream( &dssd, &m_pRenderXMO );
    if( FAILED( hr ) )
        return hr;

    // Create the source XMO. Note that an asynchronous wma XMO could can be 
    // used here to stream WMA entries embedded in a wavebank using nearly 
    // identical code to streaming PCM or adpcm
    hr = XFileCreateMediaObjectAsync( hFile, WAVEBNKSTRM_PACKET_COUNT, &m_pSourceXMO );
    if( FAILED( hr ) )
        return hr;

    // save absolute play region file offset
    m_dwStartOffset = pWaveBankEntry->PlayRegion.dwOffset + 
                      pWaveBankHeader->Segments[WAVEBANK_SEGIDX_ENTRYWAVEDATA].dwOffset;
    
    // Calculate the amount of bytes we need to stream on the first pass.
    // If the wave entry has a loop region, only calculate to the end of the loop region.
    // If it has no loop region, bytesRemaining will equal the size of the play region
    m_dwStreamBytesRemaining = pWaveBankEntry->LoopRegion.dwOffset + 
                               pWaveBankEntry->LoopRegion.dwLength;
    m_dwFileLength = m_dwStreamBytesRemaining;

    // Allocate data buffers. If the wavebank entry we are streaming is ADPCM 
    // data we need to supply physically contiguous memory. Here we always 
    // use XPhysicalAlloc for simplicity. For PCM or WMA entries, a regular 
    // heap allocation would suffice.
    m_pvSourceBuffer = XPhysicalAlloc( m_dwPacketSize * WAVEBNKSTRM_PACKET_COUNT,
                                       MAXULONG_PTR, 0, PAGE_READWRITE | PAGE_NOCACHE );
    if( NULL == m_pvSourceBuffer )
        return E_OUTOFMEMORY;

    // Seek to the start of the play region
    m_pSourceXMO->Seek( m_dwStartOffset, FILE_BEGIN, NULL);

    // Initialize packet contexts used in streaming. We use just two packets
    // and ping-pong between them, ensuring that at least one packet is always
    // pending at the destination.  We can get away with this because the time
    // it takes to fill a packet from the source is much less than the time
    // it takes the destination to consume a packet
    for( DWORD i = 0; i < WAVEBNKSTRM_PACKET_COUNT; i++ ) 
    {
        m_aContexts[i].dwPacketSize = m_dwPacketSize;
        m_aContexts[i].dwPacketStatus = XMEDIAPACKET_STATUS_SUCCESS;

        // Set the owner to destination XMO so it looks like the packet
        // was completed by the stream and is ready to be submitted to
        // the source
        m_aContexts[i].poPacketOwner = PACKET_OWNER_DEST;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Process()
// Desc: Performs any work necessary to keep the stream playing.
//-----------------------------------------------------------------------------
HRESULT CWaveBankStream::Process()
{
    DWORD   dwPacketIndex;
    HRESULT hr;

    if( m_bPaused )
        return S_OK;

    // call dowork on the source XMO
    m_pSourceXMO->DoWork();

    // If the decoder isn't accepting output data,
    // meaning, "isn't ready yet", return
    DWORD dwStatus;
    if( FAILED( m_pSourceXMO->GetStatus( &dwStatus ) ) )
        return E_FAIL;

    if( !( dwStatus & XMO_STATUSF_ACCEPT_OUTPUT_DATA ) )
        return S_OK;

    // If there are any free packets, send them to the appropriate XMO
    while( FindFreePacket( &dwPacketIndex ) )
    {
        if( m_aContexts[dwPacketIndex].poPacketOwner == PACKET_OWNER_DEST )
        {
            // destination XMO just completed a packet, attach to source (File XMO)
            hr = ProcessSource( dwPacketIndex );
            if( FAILED( hr ) )
                return hr;
        }
        else
        {
            // source XMO completed a packet, attach to destination (DSound Stream)
            hr = ProcessRenderer( dwPacketIndex );
            if( FAILED( hr ) )
                return hr;
        }
    }

    // Calculate the completion percentage based on the total amount of
    // data we've read from the source.
    if( m_pdwPercentCompleted )
        (*m_pdwPercentCompleted) = m_dwFileProgress * 100 / m_pWaveBankEntry->PlayRegion.dwLength;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FindFreePacket()
// Desc: Finds a render packet available for processing.
//-----------------------------------------------------------------------------
BOOL CWaveBankStream::FindFreePacket( DWORD* pdwPacketIndex )
{
    // If the dwLastPacketIndex is set, check that packet first so we submit
    // the loop cache packet to the renderer. We do this to cover the case
    // of really small streams that cause the general purpose logic to fail
    if( m_dwLastPacketIndex != -1 &&
        XMEDIAPACKET_STATUS_PENDING != m_aContexts[m_dwLastPacketIndex].dwPacketStatus )
    {
        (*pdwPacketIndex) = m_dwLastPacketIndex;
        return TRUE;
    }
    
    for( DWORD dwPacketIndex = 0; dwPacketIndex < WAVEBNKSTRM_PACKET_COUNT; dwPacketIndex++ )
    {
        if( XMEDIAPACKET_STATUS_PENDING != m_aContexts[dwPacketIndex].dwPacketStatus )
        {
           (*pdwPacketIndex) = dwPacketIndex;
           return TRUE;
        }
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ProcessSource()
// Desc: Reads data from the source filter.
//-----------------------------------------------------------------------------
HRESULT CWaveBankStream::ProcessSource( DWORD dwPacketIndex )
{
    HRESULT hr;
    
    // We're going to try to read a full packet's worth of data into the 
    // buffer.  If we hit the end of the file, then we'll just submit whatever
    // we got to the stream.  You'd think you could just seek back to the
    // beginning of the wave file and fill up the rest of the packet, but you
    // don't have the right alignment of space left (ie, can't read a multiple
    // of the sector size)
    XMEDIAPACKET xmp = {0};
    xmp.pvBuffer  = (BYTE *)m_pvSourceBuffer + dwPacketIndex * m_dwPacketSize;
    xmp.dwMaxSize = m_dwPacketSize;
        
    // When we reach the end of the wave region, we trim down to read just the
    // minimum number of sectors to read the rest of the region - there's no
    // point in wasting bandwidth
    if( m_dwStreamBytesRemaining < xmp.dwMaxSize )
    {
        xmp.dwMaxSize = m_dwStreamBytesRemaining;
        m_aContexts[dwPacketIndex].dwPacketSize = xmp.dwMaxSize;

        // Round the read size up to the next sector multiple
        xmp.dwMaxSize += m_pWaveBankData->dwAlignment - 1;
        xmp.dwMaxSize /= m_pWaveBankData->dwAlignment;
        xmp.dwMaxSize *= m_pWaveBankData->dwAlignment;

        // We can't submit zero-length reads to the source XMO.  This would
        // only happen if the entire loop region were contained in the loop
        // cache packet, which we don't allow.
        assert( xmp.dwMaxSize > 0 );
    }

    xmp.pdwStatus = &m_aContexts[dwPacketIndex].dwPacketStatus;

    // mark this packet as owner by the source
    m_aContexts[dwPacketIndex].poPacketOwner = PACKET_OWNER_SOURCE;

    // Read from the source
    hr = m_pSourceXMO->Process( NULL, &xmp );

    // Update the file progress
    assert( m_dwFileProgress + m_aContexts[dwPacketIndex].dwPacketSize <= m_dwFileLength );
    m_dwFileProgress += m_aContexts[dwPacketIndex].dwPacketSize; 

    // Update amount of data remaining
    assert( m_dwStreamBytesRemaining >= m_aContexts[dwPacketIndex].dwPacketSize );
    m_dwStreamBytesRemaining -= m_aContexts[dwPacketIndex].dwPacketSize;    

    if( m_dwStreamBytesRemaining == 0 )
    {
        m_dwFileProgress = m_pWaveBankEntry->LoopRegion.dwOffset;

        // We hit the end of the loop region, so we need to set up to start
        // reading back from the beginning of the loop region.
        // You could optionally keep track of a loop count here, breaking
        // out of the loop by setting
        // m_dwStreamBytesRemaining = m_pWaveBankEntry->PlayRegion.dwLength - m_pWaveBankEntry->LoopRegion.dwOffset
        // instead of
        // m_dwStreamBytesRemaining = m_pWaveBankEntry->LoopRegion.dwLength
        m_dwStreamBytesRemaining = m_pWaveBankEntry->LoopRegion.dwLength;

        // We need to start at the sector boundary just before the beginning
        // of the loop region
        m_dwStreamBytesRemaining += m_pWaveBankEntry->LoopRegion.dwOffset % m_pWaveBankData->dwAlignment;

        // Reset file offset to start of loop region aligned to the nearest
        // previous sector boundary. This will not affect playback, since it 
        // will occur at the authored loop offset when the renderer plays 
        // back the loop cached packet
        m_dwStartOffset = m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_ENTRYWAVEDATA].dwOffset +
                          m_pWaveBankEntry->PlayRegion.dwOffset +
                          m_pWaveBankEntry->LoopRegion.dwOffset;
        m_dwStartOffset /= m_pWaveBankData->dwAlignment;
        m_dwStartOffset *= m_pWaveBankData->dwAlignment;

        // Since we are going to use the loop cache packet after the last 
        // packet of the stream is played, adjust the file offset and 
        // remaining bytes.  If we allowed loop regions of less than 1
        // full packet, we'd have to be more careful here to clamp 
        // m_dwStartOffset and m_dwStreamBytesRemaining.
        m_dwStartOffset          += m_dwPacketSize;
        m_dwStreamBytesRemaining -= m_dwPacketSize;           

        // Mark this packet so that we know to submit
        // the loop cache packet after we submit this one
        m_dwLastPacketIndex       = dwPacketIndex;       

        m_pSourceXMO->Seek( m_dwStartOffset, FILE_BEGIN, NULL );
    }

    if( FAILED( hr ) && hr != E_PENDING )
    {
        // If we failed to fill the packet for some reason, we don't want to submit
        // bogus data to the stream for playback.  By marking the owner as DEST, it
        // will look like the packet was completed by the stream, and will get 
        // resubmitted back to the source.
        m_aContexts[dwPacketIndex].poPacketOwner  = PACKET_OWNER_DEST;
        m_aContexts[dwPacketIndex].dwPacketStatus = XMEDIAPACKET_STATUS_SUCCESS;
        m_aContexts[dwPacketIndex].dwPacketSize   = m_dwPacketSize;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ProcessRenderer()
// Desc: Sends data to the renderer.
//-----------------------------------------------------------------------------
HRESULT CWaveBankStream::ProcessRenderer( DWORD dwPacketIndex )
{
    HRESULT hr;

    // There's a full packet's worth of data ready for us to send to the
    // renderer.  We want to track the status of this packet since the
    // render filter is asynchronous and we need to know when the packet is
    // completed.
    XMEDIAPACKET xmp = {0};
    xmp.pvBuffer  = (BYTE*)m_pvSourceBuffer + (dwPacketIndex * m_dwPacketSize);
    xmp.dwMaxSize = m_aContexts[dwPacketIndex].dwPacketSize;
    xmp.pdwStatus = &m_aContexts[dwPacketIndex].dwPacketStatus;

    // Mark this packet as owned by the destination
    m_aContexts[dwPacketIndex].poPacketOwner = PACKET_OWNER_DEST;

    // Submit it to the stream
    hr = m_pRenderXMO->Process( &xmp, NULL );

    // If this was a partial packet read from the end of the wave,
    // we need to reset the packet size to be a full packet now
    m_aContexts[dwPacketIndex].dwPacketSize = m_dwPacketSize;

    // If we have reached the end of the stream loop region
    // submit the loop cache packet. The source XMO will skip the first
    // packet worth of data of the loop region since we adjusted the offset 
    // accordingly in ProcessSource().
    if( dwPacketIndex == m_dwLastPacketIndex )
    {
        m_dwLastPacketIndex = -1;

        // To provide sample accurate looping, adjust the packet size and 
        // buffer offset to point to the actual loop start offset, not the 
        // sector aligned data.
        // If we allowed loop regions of less than a packet, we'd have to cut
        // down xmp.dwMaxSize to stop at the end of the loop region, as well
        // as making sure it starts at the beginning
        DWORD dwLoopStartOffset = m_pWaveBankEntry->LoopRegion.dwOffset % 
                                  m_pWaveBankData->dwAlignment;
        xmp.pvBuffer  = (BYTE*)m_pvLoopCachePacket + dwLoopStartOffset;
        xmp.dwMaxSize = m_aContexts[dwPacketIndex].dwPacketSize - dwLoopStartOffset;
        xmp.pdwStatus = NULL;

        hr = m_pRenderXMO->Process( &xmp, NULL );
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: Pause()
// Desc: Pauses or resumes stream playback
//-----------------------------------------------------------------------------
VOID CWaveBankStream::Pause( DWORD dwPause )
{
    m_bPaused = ( dwPause == DSSTREAMPAUSE_PAUSE );
    m_pRenderXMO->Pause( dwPause );
}
