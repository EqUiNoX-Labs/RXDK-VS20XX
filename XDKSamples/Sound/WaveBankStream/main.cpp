//-----------------------------------------------------------------------------
// File: main.cpp
//
// Desc: This sample demonstrates how to use the new wavebank format to 
//       efficiently stream data from the DVD or HDD. It requires no threads
//       since it issues all file requests asynchronously using the
//       asynchronous file XMO.  The wavebank format packages waves in sector 
//       aligned regions to facilitate this. Use the XACT tool to create 
//       these streamed wavebanks.
//
// Hist: 05.01.02 - New for May 2002 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBUtil.h>
#include <XBHelp.h>
#include "wavebankStream.h"
#include "dsstdfx.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Pause all" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Add stream" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Remove\nstream" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);




static const DWORD NUM_STREAMS = 7;

CHAR* g_strWaveBankFileName = "d:\\media\\sounds\\XactSounds_streaming.xwb";




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CWaveBankStream m_aStreams[NUM_STREAMS];    // Streamer class

    HANDLE          m_hWaveBank;                // Wavebank file handle
    WAVEBANKHEADER* m_pWaveBankHeader;          // Wavebank header      
    WAVEBANKDATA*   m_pWaveBankData;            // Wavebank section data
    WAVEBANKENTRY*  m_paWaveBankEntries;        // Array of wavebank entries

    CXBFont         m_Font;                     // A font to render text
    CXBHelp         m_Help;                     // Help object
    BOOL            m_bDrawHelp;                // Should we draw help?
    HRESULT         m_hrOpenResult;             // Error code from WMAStream::Initialize()
    HANDLE          m_hWorkerThread;            // Worker thread
    DWORD           m_dwPercentCompleted[NUM_STREAMS];   // Percentage of file processed
    BOOL            m_bPaused;                  // Paused?

    LPDIRECTSOUND8  m_pDSound;                  // DirectSound object
    DWORD           m_dwNumStreams;             // Max number of streams

    HRESULT LoadWaveBank( CHAR* strFilename );              // Loads and parses a wavebank creating streams for each entry
    DWORD   CalculateStreamPacketSize( LPWAVEFORMATEX pwfx ); //Calculate packet size based on alignment and format
    HRESULT ReadDiskData( HANDLE hFile, VOID* pvBuffer, DWORD dwSize, 
                          DWORD dwOffset, OVERLAPPED* pOverlapped ); // Helper routine for reading disk data

public:
    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();

    CXBoxSample();
    ~CXBoxSample();
};




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
    m_bDrawHelp         = FALSE;
    m_bPaused           = FALSE;
    m_hWaveBank         = INVALID_HANDLE_VALUE;
    m_pWaveBankHeader   = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CXBoxSample() (dtor)
// Desc: Destructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::~CXBoxSample()
{
    if( m_hWaveBank != INVALID_HANDLE_VALUE )
        CloseHandle( m_hWaveBank );

    if( m_pWaveBankHeader )
        delete[] m_pWaveBankHeader;
}



//-----------------------------------------------------------------------------
// Name: ReadDiskData()
// Desc: Reads data from a non buffered file on the disk
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::ReadDiskData( HANDLE hFile, VOID* pvBuffer, DWORD dwSize, 
                                   DWORD dwOffset, OVERLAPPED* pOverlapped )
{
    HRESULT hr = S_OK;
    OVERLAPPED overlapped;
    BOOL bAsync = TRUE;

    ZeroMemory( &overlapped, sizeof(OVERLAPPED) );
    
    // If no overlapped struct was passed in, we'll still do an asynchronous
    // read, but we'll wait for it to finish before returning
    if( pOverlapped == NULL )
    {
        bAsync = FALSE;
        pOverlapped = &overlapped;
        pOverlapped->Offset = dwOffset;
    }

    // Kick off the read
    if( !ReadFile( hFile, pvBuffer, dwSize, NULL, pOverlapped ) )
    {
        int err = GetLastError();
        if( err == ERROR_IO_PENDING )
            hr = S_OK;
        else
            hr = HRESULT_FROM_WIN32(err);
    }

    // If we're doing a synchronous read, wait for the operation to
    // finish
    if( SUCCEEDED(hr) && !bAsync ) 
    {
        DWORD dwRead;
        if( !GetOverlappedResult( hFile, pOverlapped, &dwRead, TRUE ) )
        {
            hr = HRESULT_FROM_WIN32(GetLastError());                
        }             
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: CalculateStreamPacket()
// Desc: Calculates a sector and block aligned packet
//-----------------------------------------------------------------------------
DWORD CXBoxSample::CalculateStreamPacketSize( LPWAVEFORMATEX pwfx )
{
    // When doing asynchronous, un-buffered I/O, we have to make sure that
    // each read is a multiple of the media's sector size (either DVD or HDD).
    // Additionally, each packet that we submit to the stream has to be a 
    // multiple of the block size for that particular wave format.
    // Therefore, the packet size must be a multiple of the Least Common
    // Multiple of both the media's sector size and the block size for the
    // wave format.
    // Conveniently, all wave formats except for 6-channel PCM and mono/stereo
    // ADPCM are divisors of the HDD and DVD sector alignment.  We're starting
    // with the DVD alignment, but if we know we're loading from the HDD, we
    // could start with that instead.
    DWORD dwMinPacket = WAVEBANK_ALIGNMENT_DVD;
    if( pwfx->nChannels == 6 )
        dwMinPacket *= 3;
    else if( pwfx->wFormatTag == WAVE_FORMAT_XBOX_ADPCM )
        dwMinPacket *= 9;

    // This is the requested size based off the packet time
    DWORD cbSize = pwfx->nAvgBytesPerSec * WAVEBNKSTRM_PACKET_SIZE_IN_MS / 1000;

    // Round that up to a multiple of MinPacket calculated above
    cbSize += dwMinPacket - 1;
    cbSize /= dwMinPacket;
    cbSize *= dwMinPacket;    

    // Verify that we did our math correctly
    assert( cbSize % pwfx->nBlockAlign == 0 );
    assert( cbSize % WAVEBANK_ALIGNMENT_DVD == 0 );

    return cbSize;
}




//-----------------------------------------------------------------------------
// Name: LoadWaveBank()
// Desc: Loads and parses streamed wave bank
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::LoadWaveBank( CHAR* strFilename )
{
    HRESULT         hr = S_OK;

    // NOTE: We're using the more conservative DVD alignment.  If we knew the
    // file was using hard drive alignment and would only be loaded from the 
    // hard drive, we could use that instead.
    DWORD           dwSize              = WAVEBANK_ALIGNMENT_DVD;
    LPWAVEBANKENTRY pWaveBankEntry;
    DWORD           dwIndex             = 0;
    DWORD           dwOffset            = 0;

    if( SUCCEEDED(hr) )
    {
        // The file is opened for asynchronous, un-buffered I/O. This requires
        // all read requests to be sector aligned for the underlying media. In 
        // return, it provides efficient asynchronous I/O, removing the need to
        // create worker threads for file I/O.  Note that the CreateFile call
        // itself is NOT asynchronous, and will block while finding the file
        m_hWaveBank = CreateFile( strFilename,  GENERIC_READ, FILE_SHARE_READ, NULL, 
                                OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL );
        if( m_hWaveBank == INVALID_HANDLE_VALUE )
        {
            OUTPUT_DEBUG_STRING( "Sound bank not found!\n" );
            hr = XBAPPERR_MEDIANOTFOUND;
        }
    }

    if( SUCCEEDED(hr) ) 
    {
        // Allocate one sector worth of space to read the header in
        m_pWaveBankHeader = (WAVEBANKHEADER*)new BYTE[dwSize];
        if( m_pWaveBankHeader == NULL )
            hr = E_OUTOFMEMORY;
    }

    if( SUCCEEDED(hr) ) 
    {
        // Read a sectors worth of data
        hr = ReadDiskData( m_hWaveBank, m_pWaveBankHeader,
                           dwSize, 0, NULL );
    }

    if( SUCCEEDED(hr) ) 
    {
        m_pWaveBankData = (WAVEBANKDATA*)( (BYTE*)m_pWaveBankHeader + 
            m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_BANKDATA].dwOffset );

        // We assume that the bankdata segment fit entirely in the first
        // sector that we just read.
        assert( m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_BANKDATA].dwOffset + sizeof(WAVEBANKDATA) < dwSize );

    }
        
    if( SUCCEEDED(hr) ) 
    {
        // Validate the header
        // Note that we don't handle the new compact wave banks just yet
        if( m_pWaveBankHeader->dwSignature  != WAVEBANK_HEADER_SIGNATURE ||
            m_pWaveBankHeader->dwVersion    != WAVEBANK_HEADER_VERSION ||
            m_pWaveBankData->dwEntryCount   == 0 ||
            !(m_pWaveBankData->dwFlags & WAVEBANK_TYPE_STREAMING) ||
            m_pWaveBankData->dwFlags & WAVEBANK_FLAGS_COMPACT )
        {
            hr = E_FAIL;
        }
    }

    if( SUCCEEDED(hr) ) 
    {
        // Now that we've got the header, we need to make sure we've got the 
        // wave bank entry meta-data.  That may not all have fit in the one 
        // sector that we've just read, so we may need to read more data.  
        // In that case, re-allocate our buffer to be big enough.
        DWORD dwNewSize = m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwLength +
            m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwOffset;

        // Round up to our wave bank alignment
        dwNewSize += m_pWaveBankData->dwAlignment - 1;
        dwNewSize /= m_pWaveBankData->dwAlignment;
        dwNewSize *= m_pWaveBankData->dwAlignment;

        // If this is more than the amount we've already allocated and read,
        // we'll need to re-allocate and re-read
        if( dwNewSize > dwSize )
        {
            // Free the old buffer and re-allocate
            delete[] m_pWaveBankHeader;
            m_pWaveBankHeader = (WAVEBANKHEADER *)new BYTE[dwNewSize];
            if( m_pWaveBankHeader == NULL )
                hr = E_OUTOFMEMORY;

            if( SUCCEEDED(hr) ) 
            {
                // read in the header region plus all the wave bank entry
                // meta-data
                hr = ReadDiskData( m_hWaveBank,
                                   m_pWaveBankHeader,
                                   dwNewSize,
                                   0,
                                   NULL );

                // Adjust m_pWaveBankData to point to the newly 
                // re-allocated buffer
                m_pWaveBankData = (WAVEBANKDATA*)( (BYTE*)m_pWaveBankHeader + 
                    m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_BANKDATA].dwOffset );
            }
        }

        // Set up m_paWaveBankEntries to point to the beginning of the wave
        // bank meta-data entries
        m_paWaveBankEntries = (WAVEBANKENTRY *)((PUCHAR)m_pWaveBankHeader + 
            m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_ENTRYMETADATA].dwOffset);
    }
    

    if( SUCCEEDED(hr) )
    {
        WAVEBANKUNIWAVEFORMAT waveFormat;

        // Create a stream for each wavebank entry
        m_dwNumStreams = NUM_STREAMS;

        for( DWORD i = 0; i < m_dwNumStreams; i++ )
        {
            VOID* pvLoopCacheBuffer = NULL;

            dwIndex = i % m_pWaveBankData->dwEntryCount;
            pWaveBankEntry = (LPWAVEBANKENTRY)( (BYTE*)m_paWaveBankEntries + dwIndex * m_pWaveBankData->dwEntryMetaDataElementSize );

            // expand the wavebank format to a WAVEFORMATEX
            if (pWaveBankEntry->Format.wFormatTag == WAVEBANKMINIFORMAT_TAG_PCM) 
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

            dwSize =  CalculateStreamPacketSize( &waveFormat.WaveFormatEx );

            // Use one packet buffer to cache the start of the loop region.
            // This will guarantee seamless, sample accurate looping with only
            // one extra packet used between ALL streams for the same wave 
            // entry.  In this sample we spawn only one stream per wave entry 
            // but in a game you can have multiple concurrent streams of the 
            // same wave region.
            pvLoopCacheBuffer = XPhysicalAlloc( dwSize,
                                                MAXULONG_PTR,
                                                0,
                                                PAGE_READWRITE | PAGE_NOCACHE );

            if( pvLoopCacheBuffer == NULL )
            {
                hr = E_OUTOFMEMORY;
            }

            if( SUCCEEDED(hr) )
            {                
                if( pWaveBankEntry->LoopRegion.dwLength == 0 )
                {
                    // If there is no loop region specified, set the loop 
                    // region to be the entire the play region
                    pWaveBankEntry->LoopRegion.dwLength = pWaveBankEntry->PlayRegion.dwLength;
                }

                // We want to start reading from the beginning of the sector
                // that contains the start of the loop region
                dwOffset = pWaveBankEntry->LoopRegion.dwOffset +
                           pWaveBankEntry->PlayRegion.dwOffset +
                           m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_ENTRYWAVEDATA].dwOffset;
                dwOffset /= m_pWaveBankData->dwAlignment;
                dwOffset *= m_pWaveBankData->dwAlignment;
                
                // Read the first packet of the loop region. This packet is 
                // shared among all instances of the same streaming wave
                hr = ReadDiskData( m_hWaveBank,
                                   pvLoopCacheBuffer,
                                   dwSize,
                                   dwOffset,
                                   NULL );
            }

            if( SUCCEEDED(hr) ) 
            {
                // Calculate the size offset into the friendly name strings block,
                // in the wave bank header. Use the size to index into the friendly
                // name block offset and get a pointer to the friendly name string.
                DWORD dwEntrySize = m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_ENTRYNAMES].dwLength / m_pWaveBankData->dwEntryCount;
                CHAR* pszFriendlyName = (CHAR *)( (PUCHAR)m_pWaveBankHeader + 
                                                  ( ( m_pWaveBankHeader->Segments[WAVEBANK_SEGIDX_ENTRYNAMES].dwOffset) ) + ( dwEntrySize * i ) );
                hr = m_aStreams[i].Initialize( m_hWaveBank, m_pWaveBankHeader, pWaveBankEntry,                     
                                               pvLoopCacheBuffer, pszFriendlyName,
                                               dwSize, &m_dwPercentCompleted[i] );
            }

            if( FAILED(hr) && pvLoopCacheBuffer )
            {
                XPhysicalFree( pvLoopCacheBuffer );
            }
        }
    }
    
    ZeroMemory( m_dwPercentCompleted, sizeof(m_dwPercentCompleted) );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs all initialization needed to run the sample
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create the DirectSound object
    if( FAILED( DirectSoundCreate( NULL, &m_pDSound, NULL ) ) )
        return E_FAIL;

    // Load up the streaming wave bank
    if( FAILED( LoadWaveBank( g_strWaveBankFileName ) ) )
        return E_FAIL;

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    LPDSEFFECTIMAGEDESC pDesc;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\Media\\dsstdfx.bin", &EffectLoc,
                                            XAUDIO_DOWNLOADFX_EXTERNFILE, &pDesc ) ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame to update state
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Toggle global pause
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        m_bPaused = !m_bPaused;

        // Pause/resume each individual stream
        for( DWORD i = 0 ; i < m_dwNumStreams; i++ )
        {
            m_aStreams[i].Pause( m_bPaused ? DSSTREAMPAUSE_PAUSE : DSSTREAMPAUSE_RESUME );
        }
    }

    // Remove a stream from the list of those currently playing.  Note 
    // that we also pause the stream, so that it will stop playing
    // immediately.  If we just stopped submitting data to it, it might
    // take a little while before it really stopped.
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_BLACK ] )
    {
        if( m_dwNumStreams )
        {
            m_dwNumStreams--;            

            // Pause the stream so that output stops immediately
            m_aStreams[ m_dwNumStreams ].Pause( DSSTREAMPAUSE_PAUSE );
        }        
    }

    // Add a stream to the list of those currently playing.  If we're not
    // globally paused, we should resume the stream's playback as well.
    // If we resumed the stream's playback while we were globally paused,
    // it would play whatever data was still buffered up, and then starve
    // because we're not submitting any new data into it.
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_WHITE] ) 
    {
        if( m_dwNumStreams < NUM_STREAMS )
        {
            // If we're not globally paused, unpause the stream
            if( !m_bPaused )
                m_aStreams[ m_dwNumStreams ].Pause( DSSTREAMPAUSE_RESUME );

            m_dwNumStreams = m_dwNumStreams + 1;
        }        
    }

    // Call the stream pump so we can continue streaming data.
    // This call is non blocking and requires minimal CPU usage. This 
    // demonstrates that streaming can be done from the main render loop, 
    // without the need for extra threads.
    // It's important to remember that asynchronous reads from the HDD still
    // have to do FAT lookups synchronously.  If the necessary FAT entry isn't
    // in the cache, ReadFile has to do a synchronous read from disk to
    // get it.  See the "Fast Asynchronous Non Buffered DMA File IO on 
    // the XBox" whitepaper on Xbox Central for more details.
    if( !m_bPaused )
    {
        for( DWORD i = 0; i < m_dwNumStreams; i++ )
        {
            m_aStreams[i].Process();
        }
    }

    DirectSoundDoWork();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Render the scene (which is just the progress bar)
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0xff0000ff, 1.0f, 0L );

    // Draw the text
    WCHAR strBuffer[100];
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    swprintf( strBuffer, L"Streaming Wave Bank: %S", m_pWaveBankData->szBankName );
    m_Font.DrawText( 48, 46, 0xffffffff, strBuffer );
    m_Font.SetScaleFactors( 1.0f, 1.0f );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        swprintf( strBuffer, L"%d", m_dwNumStreams );
        m_Font.DrawText( 64, 85, 0xffffffff, L"Number of Streams: " );
        m_Font.DrawText( 0xffffff00, strBuffer );
    
        for( DWORD i = 0; i < m_dwNumStreams; i++ )
        {
            swprintf( strBuffer, L"%S", m_aStreams[i].GetFriendlyName() );
            m_Font.DrawText( 64, (135+(i*35.0f)), 0xFFFFFFFF, strBuffer );

            // Render a simple progress bar to show the percent 
            // completed for each stream
            struct BACKGROUNDVERTEX { D3DXVECTOR4 p; D3DCOLOR color; };
            BACKGROUNDVERTEX v[8];
            FLOAT x1 =  64, x2 = x1 + (512*m_dwPercentCompleted[i])/100, x3 = 64+512;
            FLOAT y1 = 130+(FLOAT)(i*35), y2 = y1 + 5;
            v[0].p = D3DXVECTOR4( x1-0.5f, y1-0.5f, 1.0f, 1.0f );  v[0].color = 0xffffff00;
            v[1].p = D3DXVECTOR4( x2-0.5f, y1-0.5f, 1.0f, 1.0f );  v[1].color = 0xffffff00;
            v[2].p = D3DXVECTOR4( x2-0.5f, y2-0.5f, 1.0f, 1.0f );  v[2].color = 0xffffff00;
            v[3].p = D3DXVECTOR4( x1-0.5f, y2-0.5f, 1.0f, 1.0f );  v[3].color = 0xffffff00;
            v[4].p = D3DXVECTOR4( x2-0.5f, y1-0.5f, 1.0f, 1.0f );  v[4].color = 0xff8080ff;
            v[5].p = D3DXVECTOR4( x3-0.5f, y1-0.5f, 1.0f, 1.0f );  v[5].color = 0xff8080ff;
            v[6].p = D3DXVECTOR4( x3-0.5f, y2-0.5f, 1.0f, 1.0f );  v[6].color = 0xff8080ff;
            v[7].p = D3DXVECTOR4( x2-0.5f, y2-0.5f, 1.0f, 1.0f );  v[7].color = 0xff8080ff;
        
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
            m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE );
            m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 2, v, sizeof(v[0]) );
        }
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
