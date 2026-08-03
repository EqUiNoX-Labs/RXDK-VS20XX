//-----------------------------------------------------------------------------
// File: CHeapAlloc.h
//
// Desc: Heap allocator class. Used to implement heaps for custom allocators.
//
// Hist: 09.05.02 - New for October 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef __CHEAPALLOC_H__
#define __CHEAPALLOC_H__


#include <xtl.h>
#include <xact.h>
#include <assert.h>


// Constant variable declarations
const DWORD PAGE_SIZE                = 4096;
const DWORD DEFAULT_NUMBER_BLOCKS    = 25;


//-----------------------------------------------------------------------------
// Name: class CHeapAlloc
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CHeapAlloc
{
public:
    // CTORs and DTORs
    CHeapAlloc();
    ~CHeapAlloc();

    // Access methods
    inline DWORD    GetObjectType();
    inline BOOL     IsInitialized()     { return ( m_pvHeap != NULL ); }
    inline BOOL     AddressInHeap( PVOID* ppvBuffer );

    // Heap operation methods
    HRESULT         CreateHeap( DWORD dwNumHeapBlocks, DWORD dwNumHeapBlocksize, DWORD dwAllocAttributes, DWORD flProtect );
    HRESULT         HeapAlloc( PVOID* ppvBuffer, DWORD dwZeroInitialize );
    HRESULT         HeapFree( PVOID* ppvBuffer );


// CLASS IMPLEMENTATION
private:
    // Heap objects
    DWORD           m_dwAllocAttributes;    // Allocation attributes
    DWORD           m_dwNumHeapBlocks;      // Number of blocks in the heap
    DWORD           m_dwHeapBlockSize;      // Heap block size

    PVOID           m_pvHeap;               // Heap memory
    DWORD           m_cbHeap;               // Heap size in bytes
    PVOID           m_pvHeapMin;            // Heap first free space
    PVOID           m_pvHeapMax;            // Heap max space

    // Space map objects
    BYTE*           m_pbSpaceMapArray;      // Space map array
    DWORD           m_cbSpaceMapArray;      // Space map array size in bytes
};


//-----------------------------------------------------------------------------
// CHeapAlloc inline methods
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name: CHeapAlloc::GetObjectType()
// Desc: Return the heap object type
//-----------------------------------------------------------------------------
inline DWORD CHeapAlloc::GetObjectType()
{ 
    PXALLOC_ATTRIBUTES pAttributes = ( PXALLOC_ATTRIBUTES ) &m_dwAllocAttributes;
    assert ( NULL != pAttributes );

    return pAttributes->dwObjectType;
}




//-----------------------------------------------------------------------------
// Name: CHeapAlloc::AddressInHeap()
// Desc: Returns true is address is in heap space
//-----------------------------------------------------------------------------
inline BOOL CHeapAlloc::AddressInHeap( PVOID* ppvBuffer )
{ 
    return ( ( ( *ppvBuffer ) >= m_pvHeap ) && ( ( *ppvBuffer ) <= m_pvHeapMax ) );
}


#endif // __CHEAPALLOC_H__
