//-----------------------------------------------------------------------------
// File: MemAlloc.c
//
// Desc: Custom memory allocation functions.
//
// Hist: 09.05.02 - New for October 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xact.h>
#include <assert.h>
#include "CHeapAlloc.h"


// Extern declarations
extern CHeapAlloc*  g_pSoundCueHeap;    // Heap allocator object


// Constant declarations
const DWORD DEFAULT_NUMBER_OF_CUES   = 25;
const DWORD PROTECT_TYPE_TO_FLAGS[4] = { PAGE_READONLY,
                                         PAGE_NOCACHE | PAGE_READWRITE,
                                         PAGE_READWRITE,
                                         PAGE_WRITECOMBINE | PAGE_READWRITE };


// Function prototypes
DWORD GetNumberOfObjects();


//-----------------------------------------------------------------------------
// Name: XMemSize()
//
// Desc: The XMemSize function returns current size of address passed in.
//
// Arguments:
//      VOID* pAddress - Memory to check size
//      DWORD dwAllocAttributes - allocation attributes
//
// Return Value:
//      Memory size
//-----------------------------------------------------------------------------
SIZE_T WINAPI XMemSize( IN VOID* pAddress, IN DWORD dwAllocAttributes )
{
    // Not using our heap, use the default XMemSize method, which
    // is thread safe. The title is still responsible for making
    // thread-safe their own part of the custom allocator.
    return XMemSizeDefault( pAddress, dwAllocAttributes );
}




//-----------------------------------------------------------------------------
// Name: XMemAlloc()
//
// Desc: The XMemAlloc function calls our helper routine Alloc() to allocate
//       the specified number of bytes. Statistics tracking is also done here.
//
// Arguments:
//      SIZE_T dwSize - Size of memory to allocate
//      DWORD  dwAllocAttributes - allocation attributes
//
// Return Value:
//      Allocated memory or NULL.
//-----------------------------------------------------------------------------
VOID* WINAPI XMemAlloc( SIZE_T dwSize, DWORD dwAllocAttributes )
{
    XALLOC_ATTRIBUTES* pAttributes = (XALLOC_ATTRIBUTES*)&dwAllocAttributes;
    DWORD              flProtect   = PROTECT_TYPE_TO_FLAGS[pAttributes->dwMemoryProtect];
    VOID*              pvBuffer    = NULL;

    // Validate physical allocations
    assert( dwAllocAttributes );
    assert( !( ( pAttributes->dwHeapTracksAttributes ) &&
               ( pAttributes->dwMemoryType == XALLOC_MEMTYPE_PHYSICAL ) ) );

    // If object type is allocated by our heap,
    // then use our heap to allocate the object.
    if( ( NULL != g_pSoundCueHeap ) &&
        ( eXALLOCAllocatorId_XACT == pAttributes->dwAllocatorId ) &&
        ( eXACTMemoryObject_SoundCueInstance ==  pAttributes->dwObjectType ) )
    {
        // Check heap initialized
        if( !g_pSoundCueHeap->IsInitialized() )
        {
            g_pSoundCueHeap->CreateHeap( GetNumberOfObjects(), dwSize,
                                         dwAllocAttributes, flProtect );
        }

        // Allocate the object
        if( SUCCEEDED( g_pSoundCueHeap->HeapAlloc( &pvBuffer, pAttributes->dwZeroInitialize ) ) )
        {
            return pvBuffer;
        }

        // Failed to allocate using our heap, attempt
        // to allocate using normal allocator routine.
    }

    // Not using our heap, use the default XMemAlloc method, which
    // is thread safe. The title is still responsible for making
    // thread-safe their own part of the custom allocator.
    pvBuffer = XMemAllocDefault( dwSize, dwAllocAttributes );

    // Add statistics tracking here, utilizing dwSize and dwAllocAttributes.
    // Add your own thread safey if more than one thread allocs/frees memory.
    // For the sample we are simple tracking allocations and deallocations.

    return pvBuffer;
}




//-----------------------------------------------------------------------------
// Name: XMemFree()
// Desc: The XMemFree function allocates the specified number of bytes using
//       dwAllocType to determine where and how to allocate memory.
//
// Arguments:
//      VOID* pBaseAddress - Memory to free
//      DWORD dwAllocAttributes - allocation attributes
//-----------------------------------------------------------------------------
VOID WINAPI XMemFree( PVOID pBaseAddress, DWORD dwAllocAttributes )
{
    // Validate the argument
    assert( NULL != pBaseAddress );
    assert( dwAllocAttributes );

    // If the object allocation was done in our heap space,
    // call our heap allocator to free the object.
    if( ( NULL != g_pSoundCueHeap ) &&
        ( TRUE == g_pSoundCueHeap->AddressInHeap( &pBaseAddress ) ) )
    {
        g_pSoundCueHeap->HeapFree( &pBaseAddress );
        return;
    }

    // Not using our heap, use the default XMemFree method, which
    // is thread safe. The title is still responsible for making
    // thread-safe their own part of the custom allocator.
    XMemFreeDefault( pBaseAddress, dwAllocAttributes );

    // Add statistics tracking here, utilizing dwSize and dwAllocAttributes.
    // Add your own thread safey if more than one thread allocs/frees memory.
    // For the sample we are simple tracking allocations and deallocations.

    return;
}




//-----------------------------------------------------------------------------
// Name: GetNumberOfObjects()
// Desc: Read the persisted number of objects allocated from the last
//       sample run, if the log file exists. Otherwise use the default.
//
// Return Value:
//      Number of objects to allocate
//-----------------------------------------------------------------------------
DWORD GetNumberOfObjects()
{
    DWORD dwNumberCues = DEFAULT_NUMBER_OF_CUES;

    // Check for a file containing the previous
    // sample run number of cue instances allocated.
    HANDLE hFile = CreateFile( "D:\\CueInstancesPlayed.log", GENERIC_READ,
                               FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL );

    if( hFile != INVALID_HANDLE_VALUE )
    {
        // Read in file size
        DWORD dwFileSize = GetFileSize(hFile, NULL);

        // Check for zero file size
        if( dwFileSize == 0 )
        {
            CloseHandle( hFile );
            goto Exit;
        }

        // Get number of cue instances from log file
        BYTE* pbyBuffer = new BYTE[ dwFileSize ];
        DWORD dwNumBytesRead;
        if( !ReadFile( hFile, pbyBuffer, dwFileSize, &dwNumBytesRead, NULL ) )
        {
            OutputDebugStringA( "Error: Unable to read cue instance count log file!\n" );
        }
        else
        {
            dwNumberCues = *pbyBuffer;
        }

        // If the files has bad data, reset the number
        // of cues to the default value.
        if( dwNumberCues < 1 )
            dwNumberCues = DEFAULT_NUMBER_OF_CUES;

        // Cleanup
        CloseHandle( hFile );
        delete[] pbyBuffer;
    }

Exit:
    return dwNumberCues;
}
