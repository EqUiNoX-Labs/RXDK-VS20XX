//-----------------------------------------------------------------------------
// File: LevelLoader.cpp
//
// Desc: Asynchronously loads and caches level data from XPR (Xbox Packed
//       Resource) files using non-buffered DMA.
//
// Hist: 03.12.02 - New for May XDK release
//       
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "LevelLoader.h"




//-----------------------------------------------------------------------------
// Name: Check_NonBuffered_Alignment()
// Desc: Checks the alignment of file offset, read/write size, and 
//       target/source memory for non-buffered IO
//-----------------------------------------------------------------------------
#ifdef _DEBUG
VOID Check_NonBuffered_Alignment( HANDLE hFile, VOID* pBuffer,
                                  DWORD dwOffset, DWORD dwNumBytesIO )
{
    // Assert proper target memory alignment
    assert( pBuffer );
    assert( (DWORD(pBuffer) & 0x00000003) == 0 );

     // Figure out whether this file is on the DVD or hard disk
    BY_HANDLE_FILE_INFORMATION Info;
    GetFileInformationByHandle( hFile, &Info );
    DWORD dwDVDSerialNumber;
    GetVolumeInformation( "D:\\", NULL, 0, &dwDVDSerialNumber,
                          NULL, NULL, NULL, 0 );

    // Assert proper file offset and read/write size
    if( Info.dwVolumeSerialNumber == dwDVDSerialNumber )
    {
        assert( dwOffset % XBOX_DVD_SECTOR_SIZE == 0 );         
        assert( dwNumBytesIO % XBOX_DVD_SECTOR_SIZE == 0); 
    }
    else
    {
        assert( dwOffset % XBOX_HD_SECTOR_SIZE == 0 );       
        assert( dwNumBytesIO % XBOX_HD_SECTOR_SIZE == 0 ); 
    }
}
#endif




//-----------------------------------------------------------------------------
// Name: CLevelLoader()
// Desc: Constructor
//-----------------------------------------------------------------------------
CLevelLoader::CLevelLoader()
{
    m_pSysMemData   = NULL;
    m_dwSysMemSize  = 0;
    m_dwVidMemSize  = 0;
    m_pVidMemData   = NULL;
    m_pLevels       = NULL;
    m_dwNumLevels   = 0;
    m_pCurrentLevel = NULL;
    m_IOState       = Idle;
    m_pSysMemBuffer = NULL;
    m_pFileSig      = NULL;
    m_hSignature    = INVALID_HANDLE_VALUE;
}




//-----------------------------------------------------------------------------
// Name: ~CLevelLoader()
// Desc: Destructor
//-----------------------------------------------------------------------------
CLevelLoader::~CLevelLoader()
{
    // Assert that we are not being deleted in the middle of an IO op
    assert( IsIdle() );

    // These should be already be cleaned up
    assert( m_hSignature == INVALID_HANDLE_VALUE );
    assert( m_pSysMemBuffer == NULL );
    assert( m_pFileSig == NULL );

    // Close all handles
    for( DWORD i = 0; i < m_dwNumLevels; i++ )
    {
        CloseHandle( m_pLevels[i].hDVDFile );
        CloseHandle( m_pLevels[i].hHDFile );
        CloseHandle( m_pLevels[i].hSigFile );
    }

    // We don't own this memory, so don't delete it
    m_pSysMemData = NULL;
    m_pVidMemData = NULL;

    // Free the level state array
    SAFE_DELETE_ARRAY( m_pLevels );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize loader
//-----------------------------------------------------------------------------
HRESULT CLevelLoader::Initialize( CHAR** astrLevels, DWORD dwNumLevels,
                                  BYTE* pSysMemData, DWORD dwSysMemSize,
                                  BYTE* pVidMemData, DWORD dwVidMemSize )
{
    // Initialize data
    assert( pSysMemData );
    m_dwSysMemSize = dwSysMemSize;
    m_pSysMemData  = pSysMemData;

    // Move pointer up by header size
    // System memory data comes right after the header, and we load the
    // entire region into memory at once.  We don't want to add the size
    // of the resource header to the system memory pointer each time we
    // want a resource so we add it once here
    m_pSysMemData += sizeof(XPR_HEADER);

    // Create initial video buffer
    assert( pVidMemData );
    m_dwVidMemSize = dwVidMemSize;
    m_pVidMemData  = pVidMemData;

    // Create level states
    m_dwNumLevels = dwNumLevels;
    m_pLevels = new LevelState[m_dwNumLevels];
    
    // Init level states
    for( DWORD i = 0; i < m_dwNumLevels; i++ )
    {
        // Copy  level desc
        m_pLevels[i].strName = astrLevels[i];

        // Clear file handle
        m_pLevels[i].hDVDFile = INVALID_HANDLE_VALUE;
        m_pLevels[i].hHDFile  = INVALID_HANDLE_VALUE;
        m_pLevels[i].hSigFile = INVALID_HANDLE_VALUE;
        
        m_pLevels[i].bWasPreCached = FALSE;
        m_pLevels[i].bWasCacheCorrupted = FALSE;

        m_pLevels[i].dwDVDFileSize = 0;
    }

    return RefreshLevelStates();
}




//-----------------------------------------------------------------------------
// Name: RefreshLevelStates()
// Desc: Refreshes the level loader's level states
//-----------------------------------------------------------------------------
HRESULT CLevelLoader::RefreshLevelStates()
{
    assert( IsIdle() );

    // Init states
    for( DWORD i = 0; i < m_dwNumLevels; i++ )
    {   
        m_pLevels[i].bIsPreCached = FALSE;
        m_pLevels[i].bIsCacheCorrupted = FALSE;

        m_pLevels[i].bIsOpen = FALSE;
        m_pLevels[i].dwSysMemSize = 0;
        m_pLevels[i].dwVidMemSize = 0;

        // Close any opened handles
        CloseHandle( m_pLevels[i].hDVDFile );
        CloseHandle( m_pLevels[i].hSigFile );
        CloseHandle( m_pLevels[i].hHDFile );

        m_pLevels[i].hDVDFile = INVALID_HANDLE_VALUE;
        m_pLevels[i].hHDFile  = INVALID_HANDLE_VALUE;
        m_pLevels[i].hSigFile = INVALID_HANDLE_VALUE;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OpenLevel()
// Desc: Opens a level
//-----------------------------------------------------------------------------
HRESULT CLevelLoader::OpenLevel( LevelState* pLevel, DWORD dwFlags )
{
    // If the level has never been opened before, open it
    if( !pLevel->bIsOpen )
    {
        // Assert that the level file handles are not open
        assert( pLevel->hDVDFile == INVALID_HANDLE_VALUE );
        assert( pLevel->hHDFile == INVALID_HANDLE_VALUE );
        assert( pLevel->hSigFile == INVALID_HANDLE_VALUE );

        char strBuffer[MAX_PATH];
        
        // DVD File
        sprintf( strBuffer, "D:\\media\\%s.xpr", pLevel->strName );

        // Open DVD file for reading
        pLevel->hDVDFile = CreateFile( strBuffer, GENERIC_READ, FILE_SHARE_READ,
                                       NULL, OPEN_EXISTING, dwFlags, NULL );
        if( pLevel->hDVDFile == INVALID_HANDLE_VALUE )
            return EndOpenLevel( pLevel, BadOpen );

        // Get size of file on DVD
        // NOTE: This size is used to help detect corrupt cached levels.
        //       if a cached level is not the same size as its DVD 
        //       counterpart, the cached level is considered corrupt
        pLevel->dwDVDFileSize = ::GetFileSize( pLevel->hDVDFile, NULL );


        // NOTE: Both files stored on the utility drive (signature file and
        //       level) are pre-sized.  This prevents the files from
        //       becoming fragmented in the FATX.  Also, Overlapped
        //       reads/writes to the hard disk are synchronous if the file
        //       system has to hit the FATX to determine local->physical
        //       cluster mapping

        // SIG File
        sprintf( strBuffer, "Z:\\media\\%s.sig", pLevel->strName );

        // See if we have a cached sig
        pLevel->bIsPreCached = ( GetFileAttributes( strBuffer ) != DWORD(-1) );
                
        // Open sig file for reading and writing 
        pLevel->hSigFile = CreateFile( strBuffer,
                                       GENERIC_READ | GENERIC_WRITE,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                       NULL, OPEN_ALWAYS, dwFlags, NULL );
        if( pLevel->hSigFile == INVALID_HANDLE_VALUE )
            return EndOpenLevel( pLevel, BadOpen );
        
        // Cache is corrupted if sig file is not the right size
        // NOTE: The sig file is XBOX_HD_SECTOR_SIZE in size.  The actual
        //       signature calculated by XCalculateSignature* is much
        //       smaller that XBOX_HD_SECTOR_SIZE, but we must write at least
        //       XBOX_HD_SECTOR_SIZE to use non-buffered IO on the hard disk.
        pLevel->bIsCacheCorrupted = pLevel->bIsPreCached &&
            ::GetFileSize( pLevel->hSigFile, NULL ) != XBOX_HD_SECTOR_SIZE;
        
        // If the sig file is corrupted or not saved, resize it
        if( !pLevel->bIsPreCached || pLevel->bIsCacheCorrupted )
        {
            // Set file size for faster write
            SetFilePointer( pLevel->hSigFile,
                            XBOX_HD_SECTOR_SIZE, NULL, FILE_BEGIN );
            SetEndOfFile( pLevel->hSigFile );
            
            // Clear Sig magic
            BYTE apyBuffer[XBOX_HD_SECTOR_SIZE];
            *(DWORD*)(apyBuffer) = ~SIG_MAGIC;
            SetCurrentFile( pLevel->hSigFile );
            if( FAILED( DoIO( Write, apyBuffer, XBOX_HD_SECTOR_SIZE ) ) )
                return EndOpenLevel( pLevel, BadSigMagicWrite );

            // Wait for IO completion for sig magic writes
            while( HasIOCompleted() != S_OK );
        }
        
        // CACHED file
        sprintf( strBuffer, "Z:\\media\\%s.xpr", pLevel->strName );

        // See if we have a cached file
        pLevel->bIsPreCached = pLevel->bIsPreCached &&
            ( GetFileAttributes( strBuffer ) != DWORD(-1) ); 
        
        // Open cached file for reading and writing 
        pLevel->hHDFile = CreateFile( strBuffer, GENERIC_WRITE | GENERIC_READ, 
                                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                      NULL, OPEN_ALWAYS, dwFlags, NULL );
        if( pLevel->hHDFile == INVALID_HANDLE_VALUE )
            return EndOpenLevel( pLevel, BadOpen );
        
        // Cache is corrupted if cache file is not the right size
        pLevel->bIsCacheCorrupted = pLevel->bIsPreCached &&
            ( (::GetFileSize( pLevel->hHDFile, NULL ) !=
            pLevel->dwDVDFileSize) || pLevel->bIsCacheCorrupted );

        // If the cache file is corrupted or not saved, resize it
        if( !pLevel->bIsPreCached || pLevel->bIsCacheCorrupted )
        {
            // Assert correct file size
            // NOTE: We cannot set the file pointer to a non sector
            //       aligned position when the file is opened for
            //       non-buffered IO
            assert( pLevel->dwDVDFileSize % XBOX_HD_SECTOR_SIZE == 0 );

            // Set file size for faster write
            SetFilePointer( pLevel->hHDFile,
                            pLevel->dwDVDFileSize, NULL, FILE_BEGIN );
            SetEndOfFile( pLevel->hHDFile );
        }

        return EndOpenLevel( pLevel, FilesOpened );

        
    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: AsyncStreamLevel()
// Desc: Loads all the resources from the given XPR asynchronously using 
//       non buffered IO
//-----------------------------------------------------------------------------
VOID CLevelLoader::AsyncStreamLevel( DWORD dwLevel )
{
    assert( IsIdle() );
    assert( dwLevel < m_dwNumLevels );

    // We are no longer idle
    m_IOState = Begin;

    // Set current level
    m_pCurrentLevel = &m_pLevels[dwLevel];

    // Start timer
    m_dStartTime = GetTimeInSeconds();
}




//-----------------------------------------------------------------------------
// Name: StreamCurrentBundle()
// Desc: Updates the streaming state, returning S_OK when finished
//-----------------------------------------------------------------------------
HRESULT CLevelLoader::StreamCurrentLevel()
{
    HRESULT hr;
  
    switch( m_IOState )
    {
        case Begin:
        {
            // Reset IO
            ResetStreaming();

            // If we are reading from the cache, get the signature
            if( IsCurrentCacheGood() )
            {
                SetCurrentFile( m_pCurrentLevel->hSigFile );
                if( FAILED( hr = DoIO( Read, m_pFileSig, XBOX_HD_SECTOR_SIZE ) ) )
                    return EndStreamLevel( BadRead );
            }
            m_IOState = LoadSig;
        }

        case LoadSig:
        {
            // Wait for previous IO to complete
            if( FAILED( hr = HasIOCompleted() ) )
                return EndStreamLevel( hr == E_PENDING ? Pending : BadRead );
            
            // Reading cached file, so a signature exists
            if( IsCurrentCacheGood() )
            {
                // look for sig magic
                DWORD dwSig = *(DWORD*)(m_pFileSig);
                if( dwSig != SIG_MAGIC )
                {
                    if( dwSig == ~SIG_MAGIC )
                        return EndStreamLevel( NoSigMagic );
                    else
                        return EndStreamLevel( BadSig );
                }
            }
            else
            {
                // Set sig magic so it is written out
                *(DWORD*)(m_pFileSig) = SIG_MAGIC;
            }

            m_IOState = LoadSysMem;
        }

        case LoadSysMem:
        {
            // Wait for previous IO to complete
            if( FAILED( hr = HasIOCompleted() ) )
                return EndStreamLevel( hr == E_PENDING ? Pending : BadRead );
            
            // Read initial system memory buffer
            if( IsCurrentCacheGood() )
                SetCurrentFile( m_pCurrentLevel->hHDFile );
            else
                SetCurrentFile( m_pCurrentLevel->hDVDFile );
            if( FAILED( DoIO( Read, m_pSysMemData - sizeof(XPR_HEADER),
                              m_dwSysMemSize ) ) )
                return EndStreamLevel( BadRead );

            m_IOState = ParseHeader;
        }

        case ParseHeader:
        {
            // Wait for previous IO to complete
            if( FAILED( hr = HasIOCompleted() ) )
                return EndStreamLevel( hr == E_PENDING ? Pending : BadRead );
        
            // Get header
            XPR_HEADER* pHeader = (XPR_HEADER*)(m_pSysMemData - sizeof(XPR_HEADER));

            // check for magic values
            if( pHeader->dwMagic != XPR0_MAGIC_VALUE &&
                pHeader->dwMagic != XPR1_MAGIC_VALUE )
            {
                // If the magic number is not in the header, the cache or DVD
                // is corrupted.
                return EndStreamLevel( BadHeader );
            }
    
            // Set sizes
            m_pCurrentLevel->dwSysMemSize = pHeader->dwHeaderSize;
            m_pCurrentLevel->dwVidMemSize = pHeader->dwTotalSize - pHeader->dwHeaderSize;

            // Make sure our static buffers are large enough
            assert( m_pCurrentLevel->dwSysMemSize == m_dwSysMemSize );
            assert( m_pCurrentLevel->dwVidMemSize  <= m_dwVidMemSize );
        
            m_IOState = CalcSig;
        }

        case CalcSig:
        {
            // Wait for previous IO to complete
            if( FAILED( hr = HasIOCompleted() ) )
                return EndStreamLevel( hr == E_PENDING ? Pending : BadRead );
            
            // Calculate signature
            // NOTE: Calling XCalculateSignatueUpdate on huge amounts of
            //       (non-cached, write-combining) video memory would be
            //       slow, so we only use the system memory portion of the 
            //       level data to calculate the signature
            XCalculateSignatureUpdate( m_hSignature, m_pSysMemData - sizeof(XPR_HEADER),
                                       m_pCurrentLevel->dwSysMemSize );

            // Copy system memory buffer if reading from DVD
            // NOTE: Registering the system memory data affects buffer, so
            //       we must copy the buffer before we register the resources
            //       since we are going to save it out
            if( !IsCurrentCacheGood() )
            {
                assert( m_pSysMemBuffer );
                memcpy( m_pSysMemBuffer, m_pSysMemData - sizeof(XPR_HEADER),
                        m_pCurrentLevel->dwSysMemSize );
            }

            // finish signature
            XCALCSIG_SIGNATURE CalcSig;
            XCalculateSignatureEnd( m_hSignature, &CalcSig );
            m_hSignature = INVALID_HANDLE_VALUE;
    
            // Compare signature
            if( IsCurrentCacheGood() )
            {   
                // if the signatures don't match, reload the DVD version
                if( memcmp( m_pFileSig + sizeof(SIG_MAGIC), CalcSig.Signature,
                            XCALCSIG_SIGNATURE_SIZE ) != 0 )
                    return EndStreamLevel( SigMismatch );
            }
            // DVD read, copy signature
            else
                memcpy( m_pFileSig + sizeof(SIG_MAGIC), CalcSig.Signature,
                        XCALCSIG_SIGNATURE_SIZE );

            m_IOState = LoadVidMem;
        }

        case LoadVidMem:
        {
            // Load video memory
            if( FAILED( DoIO( Read, m_pVidMemData, m_pCurrentLevel->dwVidMemSize ) ) )
                return EndStreamLevel( BadRead );

            m_IOState = RegisterResources;
        }

        case RegisterResources:
        {
            // Wait for previous IO to complete
            if( FAILED( hr = HasIOCompleted() ) )
                return EndStreamLevel( hr == E_PENDING ? Pending : BadRead );

            // Register the current level's resources
            RegisterCurrentLevel();

            // We are now loaded
            m_dLoadTime = GetTimeInSeconds() - m_dStartTime;
            
            // If we are loading from cache, we are finished.  Else, cache
            // the file
            if( IsCurrentCacheGood() )
            {
                m_pCurrentLevel->bIsPreCached = TRUE;
                m_pCurrentLevel->bIsCacheCorrupted = FALSE;
                return EndStreamLevel( Finished );
            }

            // Reset start time.
            // This value will never be reset since we only try and
            // write once
            m_dStartTime = GetTimeInSeconds();

            m_IOState = WriteSysMem;
        }

        case WriteSysMem:
        {
            // Write system memory
            SetCurrentFile( m_pCurrentLevel->hHDFile );
            if( FAILED( DoIO( Write, m_pSysMemBuffer, m_pCurrentLevel->dwSysMemSize) ) )
                return EndStreamLevel( BadWrite );
        
            m_IOState = WriteVidMem;
        }

        case WriteVidMem:
        {
            // Wait for previous IO to complete
            if( FAILED( hr = HasIOCompleted() ) )
                return EndStreamLevel( hr == E_PENDING ? Pending : BadRead );

             // Clear system memory buffer
            SAFE_DELETE_ARRAY( m_pSysMemBuffer );

            // Write video memory
            // NOTE: This memory in write combining (non-caching) memory,
            //       and reading from it can be slow, but we are using non
            //       buffered IO and the CPU does not touch the memory buffer
            if( FAILED( hr = DoIO( Write, m_pVidMemData, m_pCurrentLevel->dwVidMemSize ) ) )
                return EndStreamLevel( BadWrite );
        
            m_IOState = WriteSig;
        }

        case WriteSig:
        {
            // Wait for previous IO to complete
            if( FAILED( hr = HasIOCompleted() ) )
                return EndStreamLevel( hr == E_PENDING ? Pending : BadRead );
    
            // Write sig
            // NOTE: We write the sig file last.  If the box is turned off
            //       during a cache operation, the sig file will be missing.
            //       If the box is turned off during the write of the sig 
            //       file, it won't have a header or will not match the level
            //       data.  In either case, the level will be re-cached.
            SetCurrentFile( m_pCurrentLevel->hSigFile );
            if( FAILED( hr = DoIO( Write, m_pFileSig, XBOX_HD_SECTOR_SIZE) ) )
                return EndStreamLevel( BadWrite );

            m_IOState = End;
        }

        case End:
        {
            // Wait for previous IO to complete
            if( FAILED( hr = HasIOCompleted() ) )
                return EndStreamLevel( hr == E_PENDING ? Pending : BadRead );
    
            // Record cache time
            m_dCacheTime = GetTimeInSeconds() -  m_dStartTime;

            // We are now cached
            m_pCurrentLevel->bIsPreCached = TRUE;
            m_pCurrentLevel->bIsCacheCorrupted = FALSE;

            return EndStreamLevel( Finished );
        }
    }

    // Should never reach here
    assert( FALSE );
    return E_FAIL;
}




//-----------------------------------------------------------------------------
// Name: EndOpenLevel()
// Desc: Called when IO has completed or there is an error
//-----------------------------------------------------------------------------
HRESULT CLevelLoader::EndOpenLevel( LevelState* pLevel, FileOpenStatus Status )
{
    assert( pLevel );

    if( Status == BadOpen || Status == BadSigMagicWrite )
    {
        // Close all file handles
        CloseHandle( pLevel->hDVDFile );
        CloseHandle( pLevel->hHDFile );
        CloseHandle( pLevel->hSigFile );
        pLevel->hDVDFile = INVALID_HANDLE_VALUE;
        pLevel->hHDFile = INVALID_HANDLE_VALUE;
        pLevel->hSigFile = INVALID_HANDLE_VALUE;

        return E_FAIL;
    }

    // Level is now opened
    pLevel->bIsOpen = TRUE;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: EndStreamLevel()
// Desc: Called when IO has completed or there is an error
//-----------------------------------------------------------------------------
HRESULT CLevelLoader::EndStreamLevel( FileIOStatus Status )
{
    // if IO is not pending, we are finished
    if( Status != Pending )
    {
        // close the signature if needed
        if( m_hSignature != INVALID_HANDLE_VALUE )
        {
            XCalculateSignatureEnd( m_hSignature, NULL );
            m_hSignature = INVALID_HANDLE_VALUE;
        }

        // free buffers if needed
        SAFE_DELETE_ARRAY( m_pSysMemBuffer );
        SAFE_DELETE_ARRAY( m_pFileSig );
    }

    // Determine new state
    switch( Status )
    {
        case BadRead:
        case BadHeader:
        case BadSig:
        case SigMismatch:
            m_pCurrentLevel->bIsCacheCorrupted = TRUE;
            m_IOState = Begin; // Retry streaming the level
            return E_FAIL;
        case NoSigMagic:
            m_pCurrentLevel->bIsPreCached = FALSE;
            m_IOState = Begin; // Retry streaming the level
            return E_FAIL;
        case BadWrite:  // If we have a bad write, finish anyways since
                        // the cache will be corrupt next time around
            m_pCurrentLevel->bIsCacheCorrupted = TRUE;
        case Finished:
            m_IOState = Idle;  // Done with IO, so we are now idle
            return S_OK;
        case Pending:
            return E_PENDING;
    }

    // Should never reach here
    assert( FALSE );
    return E_FAIL;
}




//-----------------------------------------------------------------------------
// Name: CRestIO()
// Desc: Records last state for current level and get ready for IO
//-----------------------------------------------------------------------------
VOID CLevelLoader::ResetStreaming()
{
    // Record last state
    m_pCurrentLevel->bWasPreCached = m_pCurrentLevel->bIsPreCached;
    m_pCurrentLevel->bWasCacheCorrupted = m_pCurrentLevel->bIsCacheCorrupted;

    // Begin signature
    assert( m_hSignature == INVALID_HANDLE_VALUE );
    m_hSignature = XCalculateSignatureBegin( XCALCSIG_FLAG_NON_ROAMABLE );
    assert( m_hSignature != INVALID_HANDLE_VALUE );
    
    // Create system memory buffer (we need this so that we have a clear version
    // to save that has not been registered
    assert( m_pSysMemBuffer == NULL );
    m_pSysMemBuffer = new BYTE[m_dwSysMemSize];

    // Create file sig buffer
    assert( m_pFileSig == NULL );
    assert( XCALCSIG_SIGNATURE_SIZE + sizeof(SIG_MAGIC) <= XBOX_HD_SECTOR_SIZE );
    m_pFileSig = new BYTE[XBOX_HD_SECTOR_SIZE];
}




//-----------------------------------------------------------------------------
// Name: RegisterCurrentLevel()
// Desc: Registers the current level's resources
//-----------------------------------------------------------------------------
VOID CLevelLoader::RegisterCurrentLevel()
{
    // Get header
    XPR_HEADER* pHeader = (XPR_HEADER*)(m_pSysMemData - sizeof(XPR_HEADER));

    if( pHeader->dwMagic == XPR0_MAGIC_VALUE )
    {
        m_dwNumResourceTags = 0L;
        m_pResourceTags     = NULL;
    }
    else if( pHeader->dwMagic == XPR1_MAGIC_VALUE )
    {
        m_dwNumResourceTags = *(DWORD*)(m_pSysMemData+0);
        m_pResourceTags     = (XBRESOURCE*)(m_pSysMemData+4);

        // Patch up the resource strings
        for( DWORD i=0; i<m_dwNumResourceTags; i++ )
            m_pResourceTags[i].strName = (CHAR*)( m_pSysMemData + (DWORD)m_pResourceTags[i].strName );
    }
            
    // Loop over resources, calling Register()
    for( DWORD i = 0; i < m_dwNumResourceTags; i++ )
    {
        D3DResource* pResource = (D3DResource*)&m_pSysMemData[m_pResourceTags[i].dwOffset];

        // Check for user data (defined by the 0x8000000 tag in system memory)
        if( *((DWORD*)pResource) & 0x80000000 )
        {
            // Do nothing
        }
        else
        {
            // Register the resource
            pResource->Register( m_pVidMemData );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: GetData()
// Desc: Looks for level data given a string identifier
//-----------------------------------------------------------------------------
VOID* CLevelLoader::GetData( const CHAR* strName ) const
{
    if( NULL == m_pCurrentLevel || 
        NULL == m_pResourceTags ||
        NULL == strName )
        return NULL;

    for( DWORD i=0; i < m_dwNumResourceTags; i++ )
    {
        if( !_stricmp( strName, m_pResourceTags[i].strName ) )
            return &m_pSysMemData[m_pResourceTags[i].dwOffset];
    }

    return NULL;
}




//-----------------------------------------------------------------------------
// Name: IOProc()
// Desc: Thread proc for IO
//-----------------------------------------------------------------------------
DWORD WINAPI IOProc( VOID* pParameter )
{
    CThreadedLevelLoader* pLoader = (CThreadedLevelLoader*)pParameter;
    assert( pLoader );
   
    for(;;)
    {
        // Wait for the IO event to be signaled
        WaitForSingleObject( pLoader->m_hEvent, INFINITE );

        // If the loaded has closed the thread we are finished
        if( pLoader->m_bKillThread )
            ExitThread( 0 );
        
        // NOTE: In the threaded version of the loader, we are able to open
        //       files and set file sizes mid-game without noticing a severe
        //       frame "glitch."  If you use overlapped IO, you must remember
        //       that OpenFile, SetEndOfFile, etc. are blocking

        // Open the current level
        // If we can't open files, we are in trouble, so in the release
        // build, we just keep trying
        while( FAILED( pLoader->OpenLevel( pLoader->m_pCurrentLevel,
                                           FILE_FLAG_NO_BUFFERING |
                                           FILE_FLAG_SEQUENTIAL_SCAN ) ) )
        {
            // Kreak in the debug build to find out what the problem is
            assert( FALSE );
        }
    
        // Keep trying to load and cache the level until we are successful
        while( FAILED( pLoader->StreamCurrentLevel() ) );
    }
}




//-----------------------------------------------------------------------------
// Name: CThreadedLevelLoader()
// Desc: Constructor
//-----------------------------------------------------------------------------
CThreadedLevelLoader::CThreadedLevelLoader()
{
    m_bKillThread = FALSE;

    // Create event that is used to block the thread when it is not being used
    // for IO
    m_hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );

    // Create IO thread
    m_hThread =  CreateThread( NULL, 0, IOProc, this, 0, NULL);     

    SetCurrentFile( INVALID_HANDLE_VALUE );
}




//-----------------------------------------------------------------------------
// Name: ~CThreadedLevelLoader()
// Desc: Destructor
//-----------------------------------------------------------------------------
CThreadedLevelLoader::~CThreadedLevelLoader()
{
    assert( IsIdle() );

    // Kill the thread by signalling it and waiting for it to exit

    // Signal thread 
    m_bKillThread = TRUE;
    SetEvent( m_hEvent );
    
    // Wait for thread to exit
    WaitForSingleObject( m_hThread, INFINITE );

    // Close handle to thread and event
    CloseHandle( m_hThread );
    CloseHandle( m_hEvent );
}




//-----------------------------------------------------------------------------
// Name: DoIO()
// Desc: wrapper for ReadFile() and WriteFile() that checks parameters
//-----------------------------------------------------------------------------
HRESULT CThreadedLevelLoader::DoIO( IOType Type, VOID* pBuffer,
                                    DWORD dwNumBytes )
{
    // Check Non buffered alignment
#ifdef _DEBUG
    Check_NonBuffered_Alignment( m_hFile, pBuffer, 
                                 SetFilePointer( m_hFile, 0, NULL, FILE_CURRENT ),
                                 dwNumBytes );
#endif

    // Do IO
    DWORD dwNumBytesIO;
    BOOL  bSuccess;

    if( Type == Read )
        bSuccess = ::ReadFile( m_hFile, pBuffer, dwNumBytes, &dwNumBytesIO, NULL );
    else
        bSuccess = ::WriteFile( m_hFile, pBuffer, dwNumBytes, &dwNumBytesIO, NULL );


    // If IO is successful and we have the number of expected
    // bytes, return success
    // NOTE: If the file size is X*SECTORSIZE + Y where Y is less than
    //       SECTORSIZE, we still request SECTORSIZE bytes when reading Y
    //       in order to get non bufferd IO.  The file system will return
    //       success, and the number of bytes read is Y.
    // NOTE: You must specify sector size writes
    if( !bSuccess )
        return E_FAIL;
        
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetCurrentFile()
// Desc: Sets the current file to be used during IO
//-----------------------------------------------------------------------------
VOID CThreadedLevelLoader::SetCurrentFile( HANDLE hFile )
{
    if( hFile != INVALID_HANDLE_VALUE )
    {
        // Reset file pointer to beginning
        SetFilePointer( hFile, 0, NULL, FILE_BEGIN );
    }

    m_hFile = hFile;
}




//-----------------------------------------------------------------------------
// Name: AsyncStreamLevel()
// Desc: Begins streaming of the level. Returns immediately
//-----------------------------------------------------------------------------
VOID CThreadedLevelLoader::AsyncStreamLevel( DWORD dwLevel )
{
    CLevelLoader::AsyncStreamLevel( dwLevel );

    // Signal thread to start IO
    SetEvent( m_hEvent );
}




//-----------------------------------------------------------------------------
// Name: COverlappedLevelLoader()
// Desc: Constructor
//-----------------------------------------------------------------------------
COverlappedLevelLoader::COverlappedLevelLoader()
{   
    SetCurrentFile( INVALID_HANDLE_VALUE );
}




//-----------------------------------------------------------------------------
// Name: ~COverlappedLevelLoader()
// Desc: Destructor
//-----------------------------------------------------------------------------
COverlappedLevelLoader::~COverlappedLevelLoader()
{
    assert( IsIdle() );
}




//-----------------------------------------------------------------------------
// Name: DoIO()
// Desc: wrapper for ReadFile() and WriteFile() that checks parameters
//-----------------------------------------------------------------------------
HRESULT COverlappedLevelLoader::DoIO( IOType Type, VOID* pBuffer,
                                      DWORD dwNumBytes )
{
    // Update context
    m_Context.dwNumBytesExpected = dwNumBytes;
    m_Context.Overlapped.Offset  = m_Context.dwPointer;

    // Check NonBuffered alignment
#ifdef _DEBUG
    Check_NonBuffered_Alignment( m_Context.hFile, pBuffer,
                                 m_Context.Overlapped.Offset, dwNumBytes );
#endif

    // DoIO
    BOOL bSuccess;

    if( Type == Read )
    {
        // NOTE: ReadFile() blocks even during an "asynchronous" overlapped IO
        //       op if it has to map physical clusters to hard disk logical
        //       clusters and the mapping is not in the file cache (it has
        //       to seek to the file table). the DVD stores files on
        //       contiguous sectors ( it does not use a file table), so
        //       overlapped reads from the DVD will not block.
        bSuccess = ::ReadFile( m_Context.hFile, pBuffer,
                               dwNumBytes, NULL, &m_Context.Overlapped );
    }
    else
    {
        // NOTE: WriteFile() blocks even during an "asynchronous" overlapped IO
        //       op if it has to map physical clusters to hard disk logical
        //       clusters and the mapping is not in the file cache or it has
        //       to fetch more physical clusters to make room for the file
        //       (it has to seek to the file table). This is another reason
        //       why we pre-size our files before we write to them.
        bSuccess = ::WriteFile( m_Context.hFile, pBuffer,
                                dwNumBytes, NULL, &m_Context.Overlapped );
    }
    
    if( !bSuccess && GetLastError() != ERROR_IO_PENDING ) 
        return E_FAIL;
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: HasIOCompleted()
// Desc: has an IO op completed
//-----------------------------------------------------------------------------
HRESULT COverlappedLevelLoader::HasIOCompleted()
{
    // If we don't have a file specified for IO or are not expecting any bytes 
    // written/read, we don't have an outstanding IO op, so return S_OK so no
    // one waits
    if( m_Context.hFile == INVALID_HANDLE_VALUE ||
        m_Context.dwNumBytesExpected == 0)
        return S_OK;

    DWORD dwNumBytesIO;
    if( GetOverlappedResult( m_Context.hFile, &m_Context.Overlapped,
                             &dwNumBytesIO, FALSE ) )
    {
        m_Context.dwPointer += dwNumBytesIO;

        // If GetOverlappedResult() is successful return success
        // NOTE: If the file size is X*SECTORSIZE + Y where Y is less than
        //       SECTORSIZE, we still request SECTORSIZE bytes when reading Y
        //       in order to get non-buffered IO.  The file system will return
        //       success, and the number of bytes read is Y.
        // NOTE: You must specify sector size writes
        m_Context.dwNumBytesExpected = 0;

        return S_OK;
    }

    DWORD dwError = GetLastError();

    // Still pending
    if( dwError == ERROR_IO_PENDING || dwError == ERROR_IO_INCOMPLETE )
        return E_PENDING;

    return E_FAIL;
}




//-----------------------------------------------------------------------------
// Name: SetCurrentFile()
// Desc: Sets the current file
//-----------------------------------------------------------------------------
VOID COverlappedLevelLoader::SetCurrentFile( HANDLE hFile )
{
    // Clear overlapped structure
    ZeroMemory( &m_Context.Overlapped, sizeof(OVERLAPPED) );
    
    // Clear context info
    m_Context.dwNumBytesExpected = 0;
    m_Context.dwPointer = 0;

    m_Context.hFile = hFile;
}




//-----------------------------------------------------------------------------
// Name: RefreshLevelStates()
// Desc: AsyncStreamLevel
//-----------------------------------------------------------------------------
HRESULT COverlappedLevelLoader::RefreshLevelStates()
{
    assert( IsIdle() );

    // Base class update
    if( FAILED( CLevelLoader::RefreshLevelStates() ) )
        return E_FAIL;

    // Open all levels
    // NOTE: Overlapped IO must open all files in the beginning to be
    //       asynchronous since OpenFile may block if the FATX must be read
    //       and SetEndOfFile will block since it has to update the FATX.
    for( DWORD i = 0; i < m_dwNumLevels; i++ )
    {
        // Open the current level
        // If we can't open files, we are in trouble so in the release
        // build, we just keep trying 
        while( FAILED( OpenLevel( &m_pLevels[i],
                                  FILE_FLAG_NO_BUFFERING |
                                  FILE_FLAG_SEQUENTIAL_SCAN |
                                  FILE_FLAG_OVERLAPPED ) ) )
        {
            // In the debug build  break so we can investigate
            assert( FALSE );
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Updates the streaming state
//-----------------------------------------------------------------------------
VOID COverlappedLevelLoader::Update()
{
    // If we are idle, nothing to update
    if( IsIdle() )
        return;

    //Stream the level
    StreamCurrentLevel();
}

