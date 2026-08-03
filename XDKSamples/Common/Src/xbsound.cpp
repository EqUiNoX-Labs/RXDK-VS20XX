//-----------------------------------------------------------------------------
// File: XBSound.cpp
//
// Desc: Helper class for reading a .wav file and playing it in a DirectSound
//       buffer.
//
// Hist: 12.15.00 - New for December XDK release
//
// Copyright (c) 2000-2001 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <stdio.h>
#include "XBSound.h"
#include "XBUtil.h"




//-----------------------------------------------------------------------------
// FourCC definitions
//-----------------------------------------------------------------------------
const DWORD FOURCC_RIFF   = 'FFIR';
const DWORD FOURCC_WAVE   = 'EVAW';
const DWORD FOURCC_FORMAT = ' tmf';
const DWORD FOURCC_DATA   = 'atad';
const DWORD FOURCC_WSMP   = 'pmsw';
const DWORD FOURCC_SMPL   = 'lpms';



//-----------------------------------------------------------------------------
// Name: struct WAVESAMPE
// Desc: RIFF chunk type that contains loop point information
//-----------------------------------------------------------------------------
struct WAVESAMPLE
{
    ULONG   cbSize;
    USHORT  usUnityNote;
    SHORT   sFineTune;
    LONG    lGain;
    ULONG   ulOptions;
    ULONG   cSampleLoops;
};

//-----------------------------------------------------------------------------
// Name: struct WAVESAMPLE_LOOP
// Desc: Loop point (contained in WSMP chunk)
//-----------------------------------------------------------------------------
struct WAVESAMPLE_LOOP
{
    ULONG cbSize;
    ULONG ulLoopType;
    ULONG ulLoopStart;
    ULONG ulLoopLength;
};


//-----------------------------------------------------------------------------
// Name: struct SAMPLER
// Desc: RIFF chunk that may contain loop point information
//-----------------------------------------------------------------------------
struct SAMPLER
{
    DWORD dwManufacturer;
    DWORD dwProduct;
    DWORD dwSamplePeriod;
    DWORD dwMIDIUnityNote;
    DWORD dwMIDIPitchFraction;
    DWORD dwSMPTEFormat;
    DWORD dwSMPTEOffset;
    DWORD dwNumSampleLoops;
    DWORD dwSamplerData;
};

//-----------------------------------------------------------------------------
// Name: struct SAMPLER_LOOP
// Desc: Loop point contained in SMPL chunk
//-----------------------------------------------------------------------------
struct SAMPLER_LOOP
{
    DWORD dwCuePointID;
    DWORD dwType;
    DWORD dwStart;
    DWORD dwEnd;
    DWORD dwFraction;
    DWORD dwPlayCount;
};



//-----------------------------------------------------------------------------
// Name: CRiffChunk()
// Desc: Object constructor.
//-----------------------------------------------------------------------------
CRiffChunk::CRiffChunk()
{
    // Initialize defaults
    m_fccChunkId   = 0;
    m_pParentChunk = NULL;
    m_hFile        = INVALID_HANDLE_VALUE;
    m_dwDataOffset = 0;
    m_dwDataSize   = 0;
    m_dwFlags      = 0;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the object
//-----------------------------------------------------------------------------
VOID CRiffChunk::Initialize( FOURCC fccChunkId, const CRiffChunk* pParentChunk, 
                             HANDLE hFile )
{
    m_fccChunkId   = fccChunkId;
    m_pParentChunk = pParentChunk;
    m_hFile        = hFile;
}




//-----------------------------------------------------------------------------
// Name: Open()
// Desc: Opens an existing chunk.
//-----------------------------------------------------------------------------
HRESULT CRiffChunk::Open()
{
    RIFFHEADER rhRiffHeader;
    LONG       lOffset = 0;

    // Seek to the first byte of the parent chunk's data section
    if( m_pParentChunk )
    {
        lOffset = m_pParentChunk->m_dwDataOffset;

        // Special case the RIFF chunk
        if( FOURCC_RIFF == m_pParentChunk->m_fccChunkId )
            lOffset += sizeof(FOURCC);
    }
    
    // Read each child chunk header until we find the one we're looking for
    for( ;; )
    {
        if( INVALID_SET_FILE_POINTER == SetFilePointer( m_hFile, lOffset, NULL, FILE_BEGIN ) )
            return HRESULT_FROM_WIN32( GetLastError() );

        DWORD dwRead;
        if( 0 == ReadFile( m_hFile, &rhRiffHeader, sizeof(rhRiffHeader), &dwRead, NULL ) )
            return HRESULT_FROM_WIN32( GetLastError() );

        // Hit EOF without finding it
        if( 0 == dwRead )
            return E_FAIL;

        // Check if we found the one we're looking for
        if( m_fccChunkId == rhRiffHeader.fccChunkId )
        {
            // Save the chunk size and data offset
            m_dwDataOffset = lOffset + sizeof(rhRiffHeader);
            m_dwDataSize   = rhRiffHeader.dwDataSize;

            // Success
            m_dwFlags |= RIFFCHUNK_FLAGS_VALID;

            return S_OK;
        }

        lOffset += sizeof(rhRiffHeader) + rhRiffHeader.dwDataSize;
    }
}




//-----------------------------------------------------------------------------
// Name: Read()
// Desc: Reads from the file
//-----------------------------------------------------------------------------
HRESULT CRiffChunk::ReadData( LONG lOffset, VOID* pData, DWORD dwDataSize )
{
    // Seek to the offset
    DWORD dwPosition = SetFilePointer( m_hFile, m_dwDataOffset+lOffset, NULL, FILE_BEGIN );
    if( INVALID_SET_FILE_POINTER == dwPosition )
        return HRESULT_FROM_WIN32( GetLastError() );

    // Read from the file
    DWORD dwRead;
    if( 0 == ReadFile( m_hFile, pData, dwDataSize, &dwRead, NULL ) )
        return HRESULT_FROM_WIN32( GetLastError() );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CWaveFile()
// Desc: Object constructor.
//-----------------------------------------------------------------------------
CWaveFile::CWaveFile()
{
    m_hFile = INVALID_HANDLE_VALUE;
}




//-----------------------------------------------------------------------------
// Name: ~CWaveFile()
// Desc: Object destructor.
//-----------------------------------------------------------------------------
CWaveFile::~CWaveFile()
{
    Close();
}




//-----------------------------------------------------------------------------
// Name: Open()
// Desc: Initializes the object.
//-----------------------------------------------------------------------------
HRESULT CWaveFile::Open( const CHAR* strFileName )
{
    // If we're already open, close
    Close();
    
    // Open the file
    m_hFile = CreateFile( strFileName, GENERIC_READ, FILE_SHARE_READ, NULL, 
                          OPEN_EXISTING, 0L, NULL );
    if( INVALID_HANDLE_VALUE == m_hFile )
        return HRESULT_FROM_WIN32( GetLastError() );

    // Initialize the chunk objects
    m_RiffChunk.Initialize( FOURCC_RIFF, NULL, m_hFile );
    m_FormatChunk.Initialize( FOURCC_FORMAT, &m_RiffChunk, m_hFile );
    m_DataChunk.Initialize( FOURCC_DATA, &m_RiffChunk, m_hFile );
    m_WaveSampleChunk.Initialize( FOURCC_WSMP, &m_RiffChunk, m_hFile );
    m_SamplerChunk.Initialize( FOURCC_SMPL, &m_RiffChunk, m_hFile );

    HRESULT hr = m_RiffChunk.Open();
    if( FAILED(hr) )
        return hr;

    hr = m_FormatChunk.Open();
    if( FAILED(hr) )
        return hr;

    hr = m_DataChunk.Open();
    if( FAILED(hr) )
        return hr;

    // Wave Sample and Sampler chunks are not required
    m_WaveSampleChunk.Open();
    m_SamplerChunk.Open();

    // Validate the file type
    FOURCC fccType;
    hr = m_RiffChunk.ReadData( 0, &fccType, sizeof(fccType) );
    if( FAILED(hr) )
        return hr;

    if( FOURCC_WAVE != fccType )
        return HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetFormat()
// Desc: Gets the wave file format.  Since Xbox only supports WAVE_FORMAT_PCM,
//          WAVE_FORMAT_XBOX_ADPCM, and WAVE_FORMAT_EXTENSIBLE, we know any
//          valid format will fit into a WAVEFORMATEXTENSIBLE struct
//-----------------------------------------------------------------------------
HRESULT CWaveFile::GetFormat( WAVEFORMATEXTENSIBLE* pwfxFormat )
{
    HRESULT hr = S_OK;
    DWORD dwValidSize = m_FormatChunk.GetDataSize();

    // Make sure the caller passed in a valid struct for us to fill
    if( NULL == pwfxFormat )
        return E_INVALIDARG;

    // Anything larger than WAVEFORMATEXTENSIBLE is not a valid Xbox WAV file
    if( dwValidSize > sizeof( WAVEFORMATEXTENSIBLE ) )
        return E_FAIL;

    // Read the format chunk into the buffer
    hr = m_FormatChunk.ReadData( 0, pwfxFormat, dwValidSize );
    if( FAILED(hr) )
        return hr;

    // Zero out remaining bytes, in case enough bytes were not read
    if( dwValidSize < sizeof( WAVEFORMATEXTENSIBLE ) )
        ZeroMemory( (BYTE*)pwfxFormat + dwValidSize, sizeof( WAVEFORMATEXTENSIBLE ) - dwValidSize );
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ReadSample()
// Desc: Reads data from the audio file.
//-----------------------------------------------------------------------------
HRESULT CWaveFile::ReadSample( DWORD dwPosition, VOID* pBuffer, 
                               DWORD dwBufferSize, DWORD* pdwRead )
{                                   
    // Don't read past the end of the data chunk
    DWORD dwDuration;
    GetDuration( &dwDuration );

    if( dwPosition + dwBufferSize > dwDuration )
        dwBufferSize = dwDuration - dwPosition;

    HRESULT hr = S_OK;
    if( dwBufferSize )
        hr = m_DataChunk.ReadData( (LONG)dwPosition, pBuffer, dwBufferSize );

    if( pdwRead )
        *pdwRead = dwBufferSize;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetLoopRegion
// Desc: Gets the loop region, in terms of samples
//-----------------------------------------------------------------------------
HRESULT CWaveFile::GetLoopRegion( DWORD *pdwStart, DWORD *pdwLength )
{
    HRESULT hr = S_OK;

    // Check arguments
    if( NULL == pdwStart || NULL == pdwLength )
        return E_INVALIDARG;

    *pdwStart  = 0;
    *pdwLength = 0;

    // First, look for a MIDI-style SMPL chunk, then for a DLS-style
    // WSMP chunk.
    BOOL bGotLoopRegion = FALSE;
    if( !bGotLoopRegion &&
        m_SamplerChunk.IsValid() )
    {
        SAMPLER         smpl;
        SAMPLER_LOOP    smpl_loop;

        // Read the SAMPLER struct from the chunk
        hr = m_SamplerChunk.ReadData( 0, &smpl, sizeof( SAMPLER ) );
        if( FAILED( hr ) )
            return hr;

        // Check if the chunk contains any loop regions
        if( smpl.dwNumSampleLoops > 0 )
        {
            hr = m_SamplerChunk.ReadData( sizeof( SAMPLER ), &smpl_loop, sizeof( SAMPLER_LOOP ) );
            if( FAILED( hr ) )
                return E_FAIL;

            // Documentation on the SMPL chunk indicates that dwStart and
            // dwEnd are stored as byte-offsets, rather than sample counts,
            // but SoundForge stores sample counts, so we'll go with that.
            *pdwStart = smpl_loop.dwStart;
            *pdwLength = smpl_loop.dwEnd - smpl_loop.dwStart + 1;
            bGotLoopRegion = TRUE;
        }
    }

    if( !bGotLoopRegion &&
        m_WaveSampleChunk.IsValid() )
    {
        WAVESAMPLE ws;
        WAVESAMPLE_LOOP wsl;

        // Read the WAVESAMPLE struct from the chunk
        hr = m_WaveSampleChunk.ReadData( 0, &ws, sizeof( WAVESAMPLE ) );
        if( FAILED( hr ) )
            return hr;

        // Check if the chunk contains any loop regions
        if( ws.cSampleLoops > 0 )
        {
            // Read the loop region
            hr = m_WaveSampleChunk.ReadData( ws.cbSize, &wsl, sizeof( WAVESAMPLE_LOOP ) );
            if( FAILED( hr ) )
                return hr;
            
            // Fill output vars with the loop region
            *pdwStart = wsl.ulLoopStart;
            *pdwLength = wsl.ulLoopLength;
            bGotLoopRegion = TRUE;
        }
    }

    // Couldn't find either chunk, or they didn't contain loop points
    if( !bGotLoopRegion )
        return E_FAIL;
    
    return S_OK;
}
    



//-----------------------------------------------------------------------------
// Name: GetLoopRegionBytes
// Desc: Gets the loop region, in terms of bytes
//-----------------------------------------------------------------------------
HRESULT CWaveFile::GetLoopRegionBytes( DWORD *pdwStart, DWORD *pdwLength )
{
    // Check arguments
    if( NULL == pdwStart || NULL == pdwLength )
        return E_INVALIDARG;

    // If no loop region is explicitly specified, loop the entire file
    *pdwStart  = 0;
    GetDuration( pdwLength );

    // We'll need the wave format to convert from samples to bytes
    WAVEFORMATEXTENSIBLE wfx;
    if( FAILED( GetFormat( &wfx ) ) )
        return E_FAIL;

    // See if the file contains an embedded loop region
    DWORD dwLoopStartSamples;
    DWORD dwLoopLengthSamples;
    if( FAILED( GetLoopRegion( &dwLoopStartSamples, &dwLoopLengthSamples ) ) )
        return S_FALSE;

    // We need to convert the loop points from sample counts to
    // byte offsets, but it's slightly different between PCM and ADPCM
    if( wfx.Format.wFormatTag == WAVE_FORMAT_XBOX_ADPCM ||
        ( wfx.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && 
          wfx.SubFormat == KSDATAFORMAT_SUBTYPE_XBOX_ADPCM ) )
    {
        // For ADPCM, calculate # of blocks and multiply that by bytes per 
        // block.  Xbox ADPCM is always 64 samples per block.
        *pdwStart  = dwLoopStartSamples  / 64 * wfx.Format.nBlockAlign;
        *pdwLength = dwLoopLengthSamples / 64 * wfx.Format.nBlockAlign;
    }
    else
    {
        // For PCM, multiply by bytes per sample
        DWORD cbBytesPerSample = wfx.Format.nChannels * wfx.Format.wBitsPerSample / 8;
        *pdwStart  = dwLoopStartSamples  * cbBytesPerSample;
        *pdwLength = dwLoopLengthSamples * cbBytesPerSample;
    }

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Close()
// Desc: Closes the object
//-----------------------------------------------------------------------------
VOID CWaveFile::Close()
{
    if( m_hFile != INVALID_HANDLE_VALUE )
    {
        CloseHandle( m_hFile );
        m_hFile = INVALID_HANDLE_VALUE;
    }
}




//-----------------------------------------------------------------------------
// Name: CXBSound()
// Desc: 
//-----------------------------------------------------------------------------
CXBSound::CXBSound()
{
    m_pDSoundBuffer = NULL;
    m_dwBufferSize  = 0L;
}




//-----------------------------------------------------------------------------
// Name: ~CXBSound()
// Desc: 
//-----------------------------------------------------------------------------
CXBSound::~CXBSound()
{
    Destroy();
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Creates the sound. Sound is buffered to memory allocated internally
//       by DirectSound.
//-----------------------------------------------------------------------------
HRESULT CXBSound::Create( const CHAR* strFileName, DWORD dwFlags )
{
    // Find the media file
    CHAR strWavePath[512];
    HRESULT   hr;
    if( FAILED( hr = XBUtil_FindMediaFile( strWavePath, strFileName ) ) )
        return hr;

    // Open the .wav file
    CWaveFile waveFile;
    hr = waveFile.Open( strWavePath );
    if( FAILED(hr) )
        return hr;

    // Get the WAVEFORMAT structure for the .wav file
    hr = waveFile.GetFormat( &m_WaveFormat );
    if( FAILED(hr) )
        return hr;

    // Get the size of the .wav file
    waveFile.GetDuration( &m_dwBufferSize );

    // Create the sound buffer
    hr = Create( &m_WaveFormat, dwFlags, NULL, m_dwBufferSize );
    if( FAILED(hr) )
        return hr;

    // Lock the buffer so it can be filled
    VOID* pLock1 = NULL;
    VOID* pLock2 = NULL;
    DWORD dwLockSize1 = 0L;
    DWORD dwLockSize2 = 0L;
    hr = m_pDSoundBuffer->Lock( 0L, m_dsbd.dwBufferBytes, &pLock1, &dwLockSize1, 
                                &pLock2, &dwLockSize2, 0L );
    if( FAILED(hr) )
        return hr;

    // Read the wave file data into the buffer
    hr = waveFile.ReadSample( 0L, pLock1, dwLockSize1, NULL );
    if( FAILED(hr) )
        return hr;

    // Unlock the buffer
    hr = m_pDSoundBuffer->Unlock( &pLock1, dwLockSize1, &pLock2, dwLockSize2 );
    if( FAILED(hr) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Creates the sound and tells DirectSound where the sound data will be
//       stored. If pBuffer is NULL, DirectSound handles buffer creation.
//-----------------------------------------------------------------------------
HRESULT CXBSound::Create( const WAVEFORMATEXTENSIBLE* pwfxFormat, DWORD dwFlags,
                          const VOID* pBuffer, DWORD dwBytes )
{
    // Setup the sound buffer description
    ZeroMemory( &m_dsbd, sizeof(DSBUFFERDESC) );
    m_dsbd.dwSize      = sizeof(DSBUFFERDESC);
    m_dsbd.dwFlags     = dwFlags;
    m_dsbd.lpwfxFormat = (LPWAVEFORMATEX)pwfxFormat;

    // If pBuffer is non-NULL, dwBufferBytes will be zero, which informs
    // DirectSoundCreateBuffer that we will presently be using SetBufferData().
    // Otherwise, we set dwBufferBytes to the size of the WAV data, potentially
    // including alignment bytes.
    if( pBuffer == NULL )
    {
        m_dsbd.dwBufferBytes = ( 0 == m_WaveFormat.Format.nBlockAlign ) ? dwBytes : 
                                 dwBytes - ( dwBytes % m_WaveFormat.Format.nBlockAlign );
    }

    HRESULT hr = DirectSoundCreateBuffer(&m_dsbd, &m_pDSoundBuffer);
    if( FAILED(hr) )
        return hr;

    // If buffer specified, tell DirectSound to use it
    if( pBuffer != NULL )
    {
        hr = m_pDSoundBuffer->SetBufferData( (LPVOID)pBuffer, dwBytes );
        if( FAILED(hr) )
            return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Destroy()
// Desc: Destroys the resources used by the sound
//-----------------------------------------------------------------------------
VOID CXBSound::Destroy()
{
    SAFE_RELEASE( m_pDSoundBuffer );
}




//-----------------------------------------------------------------------------
// Name: Play()
// Desc: Plays the sound
//-----------------------------------------------------------------------------
HRESULT CXBSound::Play( DWORD dwFlags ) const
{
    if( NULL == m_pDSoundBuffer )
        return E_INVALIDARG;
        
    return m_pDSoundBuffer->Play( 0, 0, dwFlags );
}




//-----------------------------------------------------------------------------
// Name: Stop()
// Desc: Stops the sound
//-----------------------------------------------------------------------------
HRESULT CXBSound::Stop() const
{
    if( NULL == m_pDSoundBuffer )
        return E_INVALIDARG;
        
    return m_pDSoundBuffer->Stop();
}




//-----------------------------------------------------------------------------
// Name: SetPosition()
// Desc: Positions the sound
//-----------------------------------------------------------------------------
HRESULT CXBSound::SetPosition( const D3DXVECTOR3& v ) const
{
    if( NULL == m_pDSoundBuffer )
        return E_INVALIDARG;
        
    return m_pDSoundBuffer->SetPosition( v.x, v.y, v.z, DS3D_IMMEDIATE );
}




//-----------------------------------------------------------------------------
// Name: SetVelocity()
// Desc: Sets the sound's velocity
//-----------------------------------------------------------------------------
HRESULT CXBSound::SetVelocity( const D3DXVECTOR3& v ) const
{
    if( NULL == m_pDSoundBuffer )
        return E_INVALIDARG;
        
    return m_pDSoundBuffer->SetVelocity( v.x, v.y, v.z, DS3D_IMMEDIATE );
}




//-----------------------------------------------------------------------------
// Name: XBSound_DrawLevelMeters()
// Desc: Draws a quick-and-dirty set of sound level meters in the specified
//       rectangle.
//-----------------------------------------------------------------------------
HRESULT XBSound_DrawLevelMeters( DSOUTPUTLEVELS* pDSLevels, 
                                 FLOAT fLeft, FLOAT fTop,
                                 FLOAT fWidth, FLOAT fHeight )
{
    DWORD adwLevels[] = 
    {
        pDSLevels->dwDigitalFrontLeftPeak,
        pDSLevels->dwDigitalFrontRightPeak,
        pDSLevels->dwDigitalFrontCenterPeak,
        pDSLevels->dwDigitalLowFrequencyPeak,
        pDSLevels->dwDigitalBackLeftPeak,
        pDSLevels->dwDigitalBackRightPeak,
        0, // Spacer
        0, // Spacer
        pDSLevels->dwAnalogLeftTotalPeak,
        pDSLevels->dwAnalogRightTotalPeak,
    };
    const DWORD dwNumChannels = sizeof(adwLevels) / sizeof(adwLevels[0]);

    // Set up parameters for looping
    FLOAT fX = fLeft;
    FLOAT fY = fTop + fHeight;
    FLOAT fBarSpace = fWidth / dwNumChannels;
    FLOAT fBarWidth = fBarSpace * 0.7f;

    // Set rendering state
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    D3DDevice::SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    D3DDevice::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    D3DDevice::SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
    D3DDevice::Begin( D3DPT_QUADLIST );
    
    for( DWORD i = 0; i < dwNumChannels; i++ )
    {
        FLOAT fDB = -60.0f;

        // Calculate
        FLOAT fLevel = fabsf( (FLOAT)adwLevels[i] ) / 0x7FFFFF;
        if( fLevel != 0.0f )
            fDB = 20.0f * log10f( fLevel );

        // The minimum output from the EP is actually -60dB
        FLOAT fPercent = ( 60.0f + fDB ) / 60.0f;

        // Render the quad
        D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, 0x8000ff00 | ( DWORD( fPercent * 0xff ) << 16 ) );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX, fY - fPercent * fHeight, 0.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX + fBarWidth, fY - fPercent * fHeight, 0.0f, 1.0f );
        D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, 0x8000ff00  );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX + fBarWidth, fY, 0.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, fX, fY, 0.0f, 1.0f );

        fX += fBarSpace;
    }

    // End redering calls
    D3DDevice::End();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: XBSound_DrawLevelMeters()
// Desc: Draws a quick-and-dirty set of level meters in the specified
//       rectangle.  It uses the results of IDirectSound::GetOutputLevels,
//       specifically the peak levels, for the given frame.  Note that 
//       since we only draw once per frame, and the audio chip's encode
//       processor cycles every 5ms, we're really only getting 1 out of
//       every 3 EP frames.
//-----------------------------------------------------------------------------
HRESULT XBSound_DrawLevelMeters( LPDIRECTSOUND8 pDSound, 
                                 FLOAT fLeft, FLOAT fTop,
                                 FLOAT fWidth, FLOAT fHeight )
{
    DSOUTPUTLEVELS dsLevels;
    pDSound->GetOutputLevels( &dsLevels, FALSE );

    return XBSound_DrawLevelMeters( &dsLevels, fLeft, fTop, fWidth, fHeight );
}



