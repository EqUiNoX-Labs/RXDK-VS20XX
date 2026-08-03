//-----------------------------------------------------------------------------
// File: XBSound.h
//
// Desc: Helper class for reading a .wav file and playing it in a DirectSound
//       buffer.
//
// Hist: 12.15.00 - New for December XDK release
//       02.15.01 - Updated for March XDK release
//
// Copyright (c) 2000-2001 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef XBSOUND_H
#define XBSOUND_H
#include <xtl.h>




//-----------------------------------------------------------------------------
// Name: RIFFHEADER
// Desc: For parsing WAV files
//-----------------------------------------------------------------------------
struct RIFFHEADER
{
    FOURCC  fccChunkId;
    DWORD   dwDataSize;
};

#define RIFFCHUNK_FLAGS_VALID   0x00000001




//-----------------------------------------------------------------------------
// Name: class CRiffChunk
// Desc: RIFF chunk utility class
//-----------------------------------------------------------------------------
class CRiffChunk
{
    FOURCC            m_fccChunkId;       // Chunk identifier
    const CRiffChunk* m_pParentChunk;     // Parent chunk
    HANDLE            m_hFile;
    DWORD             m_dwDataOffset;     // Chunk data offset
    DWORD             m_dwDataSize;       // Chunk data size
    DWORD             m_dwFlags;          // Chunk flags

public:
    CRiffChunk();

    // Initialization
    VOID    Initialize( FOURCC fccChunkId, const CRiffChunk* pParentChunk,
                        HANDLE hFile );
    HRESULT Open();
    BOOL    IsValid()     { return !!(m_dwFlags & RIFFCHUNK_FLAGS_VALID); }

    // Data
    HRESULT ReadData( LONG lOffset, VOID* pData, DWORD dwDataSize );

    // Chunk information
    FOURCC  GetChunkId()  { return m_fccChunkId; }
    DWORD   GetDataSize() { return m_dwDataSize; }
};




//-----------------------------------------------------------------------------
// Name: class CWaveFile
// Desc: Wave file utility class
//-----------------------------------------------------------------------------
class CWaveFile
{
    HANDLE      m_hFile;            // File handle
    CRiffChunk  m_RiffChunk;        // RIFF chunk
    CRiffChunk  m_FormatChunk;      // Format chunk
    CRiffChunk  m_DataChunk;        // Data chunk
    CRiffChunk  m_WaveSampleChunk;  // Wave Sample chunk
    CRiffChunk  m_SamplerChunk;     // Sampler chunk
    
public:
    CWaveFile();
    ~CWaveFile();

    // Initialization
    HRESULT Open( const CHAR* strFileName );
    VOID    Close();

    // File format
    HRESULT GetFormat( WAVEFORMATEXTENSIBLE* pwfxFormat );

    // File data
    HRESULT ReadSample( DWORD dwPosition, VOID* pBuffer, DWORD dwBufferSize, 
                        DWORD* pdwRead );

    // Loop region
    HRESULT GetLoopRegion( DWORD* pdwStart, DWORD* pdwLength );
    HRESULT GetLoopRegionBytes( DWORD *pdwStart, DWORD* pdwLength );

    // File properties
    VOID    GetDuration( DWORD* pdwDuration ) { *pdwDuration = m_DataChunk.GetDataSize(); }
};




//-----------------------------------------------------------------------------
// Name: class CSound
// Desc: Encapsulates functionality of a DirectSound buffer.
//-----------------------------------------------------------------------------
class CXBSound
{
protected:
    LPDIRECTSOUNDBUFFER  m_pDSoundBuffer;
    WAVEFORMATEXTENSIBLE m_WaveFormat;
    DSBUFFERDESC         m_dsbd;
    DWORD                m_dwBufferSize;

public:
    HRESULT Create( const CHAR* strFileName, DWORD dwFlags = 0L );
    HRESULT Create( const WAVEFORMATEXTENSIBLE* pwfxFormat, DWORD dwFlags,
                    const VOID* pBuffer, DWORD dwBytes );
    VOID    Destroy();

    HRESULT Play( DWORD dwFlags = 0L ) const;
    HRESULT Stop() const;
    HRESULT SetPosition( const D3DXVECTOR3& vPosition ) const;
    HRESULT SetVelocity( const D3DXVECTOR3& vVelocity ) const;

    CXBSound();
    ~CXBSound();
};




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
                                 FLOAT fWidth, FLOAT fHeight );




//-----------------------------------------------------------------------------
// Name: XBSound_DrawLevelMeters()
// Desc: Draws a quick-and-dirty set of sound level meters in the specified
//       rectangle.
//-----------------------------------------------------------------------------
HRESULT XBSound_DrawLevelMeters( DSOUTPUTLEVELS* pDSLevels, 
                                 FLOAT fLeft, FLOAT fTop,
                                 FLOAT fWidth, FLOAT fHeight );




#endif // XBSOUND_H
