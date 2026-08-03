//-----------------------------------------------------------------------------
// File: LevelLoader.h
//
// Desc: Asynchronously loads and caches level data from XPR (Xbox Packed
//       Resource) files using non-buffered DMA.  
//
// Hist: 03.12.02 - New for May XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef LEVEL_LOADER_H
#define LEVEL_LOADER_H

#include <xtl.h>
#include <xgraphics.h>
#include <cstdio>
#include <XBUtil.h>
#include <XBResource.h>



// NOTE: The terms "system memory" and "video memory" are used throughout the
//       source code.  "Video memory" is contiguous physically addressed
//       write-combining memory that is used for GPU resources. "System memory"
//       is virtually addressed cached memory that is used by the CPU.


//-----------------------------------------------------------------------------
// SIG_MAGIC
// Marker that specifies that a signature file exists and is valid.
// Overlapped IO may open and pre-size a file without writing to it and
// we want to detect this without signaling a corruption error.
//-----------------------------------------------------------------------------
#define SIG_MAGIC 0xF0F0F0F0ul


//-----------------------------------------------------------------------------
// XRPX_MAGIC_VALUE
// bundled resource magic value headers
//-----------------------------------------------------------------------------
#define XPR0_MAGIC_VALUE 0x30525058ul
#define XPR1_MAGIC_VALUE 0x31525058ul




//-----------------------------------------------------------------------------
// Name: struct LevelState
// Desc: Struct to hold state about a level
//-----------------------------------------------------------------------------
struct LevelState
{
    CHAR*       strName;
    DWORD       dwDVDFileSize;   // Size of level on DVD, used for error
                                 // detection
    // State of level cache
    BOOL        bIsPreCached;            
    BOOL        bWasPreCached;
    BOOL        bIsCacheCorrupted;
    BOOL        bWasCacheCorrupted;

    // Memory requirements for the level
    DWORD       dwSysMemSize;       
    DWORD       dwVidMemSize;

    // Level file handles
    HANDLE      hDVDFile;       // DVD file handle
    HANDLE      hHDFile;        // Utility region file handle
    HANDLE      hSigFile;       // Signature file handle

    BOOL        bIsOpen;        // Whether the handles opened
};




//-----------------------------------------------------------------------------
// Name: class CLevelLoader
// Desc: Cached level base class
//       Handles loading and caching of levels
//-----------------------------------------------------------------------------
class CLevelLoader 
{
protected:
    // IO status
    enum FileIOStatus
    {
        BadRead = 0,
        BadWrite,
        BadHeader,
        BadSig,
        SigMismatch,
        NoSigMagic,
        Finished,
        Pending
    };

    // Openfile status
    enum FileOpenStatus
    {
        BadOpen = 0,
        BadSigMagicWrite,
        FilesOpened
    };

     // Type of IO requested
    enum IOType
    {
        Read = 0,
        Write,
    };

    // Current step in loading the level resources
    enum IOState
    {
        Begin = 0,
        LoadSig,
        LoadSysMem,
        ParseHeader,
        CalcSig,
        LoadVidMem,
        RegisterResources,
        WriteSysMem,
        WriteVidMem,
        WriteSig,
        End,
        Idle
    };

    // Opens the current level
    HRESULT         OpenLevel( LevelState* pState, DWORD dwFlags );
    HRESULT         EndOpenLevel( LevelState* pLevel, FileOpenStatus Status );

    // Streams the current level
    HRESULT         StreamCurrentLevel();
    VOID            ResetStreaming();
    HRESULT         EndStreamLevel( FileIOStatus Status );

    // Registers the current level
    VOID            RegisterCurrentLevel();

    // Is the current cache valid
    inline BOOL     IsCurrentCacheGood();

    // Set the current IO file
    virtual VOID    SetCurrentFile( HANDLE hFile ) = 0;

    // Wrappers for read and write file
    virtual HRESULT DoIO( IOType Type, VOID* pBuffer, DWORD dwNumBytes ) = 0;

    // Has an IO op completed
    virtual HRESULT HasIOCompleted() = 0;

    // Allocated memory for resource headers etc.
    BYTE*           m_pSysMemData;    
    DWORD           m_dwSysMemSize;
    
    // Allocated memory for resource data, etc.
    DWORD           m_dwVidMemSize;
    BYTE*           m_pVidMemData;    

    // ASCII-referenced offsets for the resources
    DWORD           m_dwNumResourceTags;
    XBRESOURCE*     m_pResourceTags;

    // Level state array
    LevelState*     m_pLevels;
    DWORD           m_dwNumLevels;

    // Current level
    LevelState*     m_pCurrentLevel;

    // Stats data
    DOUBLE          m_dStartTime;
    DOUBLE          m_dLoadTime;        
    DOUBLE          m_dCacheTime;
    
    // Current IO state
    IOState         m_IOState;

    // Temp variables used during load
    BYTE*           m_pSysMemBuffer;
    BYTE*           m_pFileSig;
    HANDLE          m_hSignature;

public:
    // Constructor/destructor
                    CLevelLoader();
    virtual         ~CLevelLoader();

    // Initializes the class with a list of levels and memory
    HRESULT         Initialize( CHAR** astrLevels, DWORD dwNumlevels,
                                BYTE* pSysMemData, DWORD dwSysMemSize,
                                BYTE* pVidMemData, DWORD dwVidMemSize );

    // Load the specified level asynchronously
    virtual VOID    AsyncStreamLevel( DWORD dwlevel );

    // Updates load/cache state
    virtual VOID    Update() = 0;

    // Refreshes the opened and loads states of all levels
    virtual HRESULT RefreshLevelStates();

    // Function to retrieve state of current level
    inline DWORD   GetCurrentDVDFileSize() const;
    inline BOOL    IsCurrentPrecached() const;        
    inline BOOL    WasCurrentPrecached() const;       
    inline BOOL    IsCurrentCacheCorrupted() const;  
    inline BOOL    WasCurrentCacheCorrupted() const;  
    inline BOOL    IsCurrentOpen() const;
    inline BOOL    IsCurrentLoaded() const;
    
    // Functions to retrieve state of loader
    inline BOOL    IsCacheing() const;
    inline BOOL    IsLoading() const;
    inline BOOL    IsIdle() const;
    inline DOUBLE  GetLoadTime() const;   
    inline DOUBLE  GetCacheTime() const;  
    
    inline DWORD   GetNumResourceTags()            { return m_dwNumResourceTags; }
    inline DWORD   GetResourceTagOffset( DWORD i ) { return m_pResourceTags[i].dwOffset; }

    // Functions to retrieve resource by their offset
    inline VOID*                    GetData( DWORD dwOffset ) const;
    inline LPDIRECT3DRESOURCE8      GetResource( DWORD dwOffset ) const;
    inline LPDIRECT3DTEXTURE8       GetTexture( DWORD dwOffset ) const;
    inline LPDIRECT3DCUBETEXTURE8   GetCubemap( DWORD dwOffset ) const;
    inline LPDIRECT3DVOLUMETEXTURE8 GetVolumeTexture( DWORD dwOffset ) const;
    inline LPDIRECT3DVERTEXBUFFER8  GetVertexBuffer( DWORD dwOffset ) const;
    
    // Functions to retrieve resources by their name
    VOID*                           GetData( const CHAR* strName ) const;
    inline LPDIRECT3DRESOURCE8      GetResource( const CHAR* strName ) const;
    inline LPDIRECT3DTEXTURE8       GetTexture( const CHAR* strName ) const;
    inline LPDIRECT3DCUBETEXTURE8   GetCubemap( const CHAR* strName ) const;
    inline LPDIRECT3DVOLUMETEXTURE8 GetVolumeTexture( const CHAR* strN ) const;
    inline LPDIRECT3DVERTEXBUFFER8  GetVertexBuffer( const CHAR* strN ) const;
};




//-----------------------------------------------------------------------------
// Inlined access functions
//-----------------------------------------------------------------------------
inline DWORD CLevelLoader::GetCurrentDVDFileSize() const
{
    return m_pCurrentLevel ? m_pCurrentLevel->dwDVDFileSize: 0;
}
inline BOOL CLevelLoader::IsCurrentPrecached() const        
{
    return m_pCurrentLevel ? m_pCurrentLevel->bIsPreCached : FALSE;
}
inline BOOL CLevelLoader::WasCurrentPrecached() const       
{
    return m_pCurrentLevel ? m_pCurrentLevel->bWasPreCached : FALSE;
}
inline BOOL CLevelLoader::IsCurrentCacheCorrupted() const   
{
    return m_pCurrentLevel ? m_pCurrentLevel->bIsCacheCorrupted : FALSE;
}
inline BOOL CLevelLoader::WasCurrentCacheCorrupted() const  
{
    return m_pCurrentLevel ? m_pCurrentLevel->bWasCacheCorrupted : FALSE;
}
inline BOOL CLevelLoader::IsCurrentOpen() const           
{
    return m_pCurrentLevel ? m_pCurrentLevel->bIsOpen : FALSE;
}
inline BOOL CLevelLoader::IsCacheing() const
{
    return m_pCurrentLevel && m_IOState > RegisterResources && m_IOState < Idle;
}
inline BOOL CLevelLoader::IsCurrentLoaded() const
{
    return m_pCurrentLevel && m_IOState > RegisterResources;
}
inline BOOL CLevelLoader::IsLoading() const
{
    return m_pCurrentLevel && m_IOState < WriteSysMem;
}
inline DOUBLE CLevelLoader::GetLoadTime() const
{
    return m_dLoadTime;
}
inline DOUBLE CLevelLoader::GetCacheTime() const
{
    return m_dCacheTime;
}
inline BOOL CLevelLoader::IsIdle() const
{
    return m_IOState == Idle;
}
inline BOOL CLevelLoader::IsCurrentCacheGood()
{
    return m_pCurrentLevel ? m_pCurrentLevel->bIsPreCached && !m_pCurrentLevel->bIsCacheCorrupted : FALSE;
}
inline VOID* CLevelLoader::GetData( DWORD dwOffset ) const
{
    return m_pCurrentLevel ? &m_pSysMemData[dwOffset] : NULL;
}
inline LPDIRECT3DRESOURCE8 CLevelLoader::GetResource( DWORD dwOffset ) const
{
    return (LPDIRECT3DRESOURCE8)GetData(dwOffset);
}
inline LPDIRECT3DTEXTURE8 CLevelLoader::GetTexture( DWORD dwOffset ) const
{
    return (LPDIRECT3DTEXTURE8)GetData( dwOffset );
}
inline LPDIRECT3DCUBETEXTURE8 CLevelLoader::GetCubemap( DWORD dwOffset ) const
{
    return (LPDIRECT3DCUBETEXTURE8)GetData( dwOffset );
}
inline LPDIRECT3DVOLUMETEXTURE8 CLevelLoader::GetVolumeTexture( DWORD dwOffset ) const
{
    return (LPDIRECT3DVOLUMETEXTURE8)GetData( dwOffset );
}
inline LPDIRECT3DVERTEXBUFFER8 CLevelLoader::GetVertexBuffer( DWORD dwOffset ) const
{
    return (LPDIRECT3DVERTEXBUFFER8)GetData( dwOffset );
}
inline LPDIRECT3DRESOURCE8 CLevelLoader::GetResource( const CHAR* strName ) const
{
    return (LPDIRECT3DRESOURCE8)GetData( strName );
}
inline LPDIRECT3DTEXTURE8 CLevelLoader::GetTexture( const CHAR* strName ) const
{
    return (LPDIRECT3DTEXTURE8)GetResource( strName );
}
inline LPDIRECT3DCUBETEXTURE8 CLevelLoader::GetCubemap( const CHAR* strName ) const
{
    return (LPDIRECT3DCUBETEXTURE8)GetResource( strName );
}
inline LPDIRECT3DVOLUMETEXTURE8 CLevelLoader::GetVolumeTexture( const CHAR* strName ) const
{
    return (LPDIRECT3DVOLUMETEXTURE8)GetResource( strName );
}
inline LPDIRECT3DVERTEXBUFFER8 CLevelLoader::GetVertexBuffer( const CHAR* strName ) const
{
    return (LPDIRECT3DVERTEXBUFFER8)GetResource( strName );
}




//-----------------------------------------------------------------------------
// Name: IOProc()
// Desc: Thread proc for IO
//-----------------------------------------------------------------------------
DWORD WINAPI IOProc( VOID* pParameter );




//-----------------------------------------------------------------------------
// Name: class CThreadedLevelLoader
// Desc: Threaded Cached resource.  Handles loading and caching of packed
//       resources, using a thread
//-----------------------------------------------------------------------------
class CThreadedLevelLoader : public CLevelLoader
{
protected:
    // Current file
    HANDLE          m_hFile;
    
    // Thread handle and wake event
    HANDLE          m_hEvent;
    HANDLE          m_hThread;

    // Signal to kill the thread (only done when the class is destroyed)
    BOOL            m_bKillThread;
    
    // The IO proc is our friend
    friend          DWORD WINAPI IOProc( LPVOID lpParameter );

    virtual VOID    SetCurrentFile( HANDLE hFile );
    virtual HRESULT DoIO( IOType Type, VOID* pBuffer, DWORD dwNumBytes );
    virtual HRESULT HasIOCompleted() { return S_OK; }; // IO is synchronous
    
public:
    // Constructor/destructor
                    CThreadedLevelLoader();
    virtual         ~CThreadedLevelLoader();

    virtual VOID    AsyncStreamLevel( DWORD dwlevel );
    virtual VOID    Update() {} // Does nothing for the threaded version
};




//-----------------------------------------------------------------------------
// Name: class COverlappedLevelLoader
// Desc: Overlapped Cached resource.  Handles loading and caching of packed
//       resources, using Overlapped IO
//-----------------------------------------------------------------------------
class COverlappedLevelLoader : public CLevelLoader
{
protected:
    // Context of overlapped IO
    struct OverlappedContext
    {
        OVERLAPPED      Overlapped;
        DWORD           dwPointer;  // Store this since overlapped IO doesn't
                                    // move the file pointer
        DWORD           dwNumBytesExpected;
        HANDLE          hFile;
    };

    // Current IO context
    OverlappedContext m_Context;

    virtual VOID    SetCurrentFile( HANDLE hFile );
    virtual HRESULT DoIO( IOType Type, VOID* pBuffer, DWORD dwNumBytes );
    virtual HRESULT HasIOCompleted();
    
public:
    // Constructor/destructor
                    COverlappedLevelLoader();
    virtual         ~COverlappedLevelLoader();
    
    virtual VOID    Update();
    virtual HRESULT RefreshLevelStates();
    
};




#endif //LEVEL_LOADER_H
