//-----------------------------------------------------------------------------
// File: CHeapAlloc.cpp
//
// Desc: Heap allocator class. Used to implement heaps for custom allocators.
//
// Hist: 09.05.02 - New for October 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <stdlib.h>
#include "CHeapAlloc.h"




//-----------------------------------------------------------------------------
// Name: CHeapAlloc::CHeapAlloc()
// Desc: Constructor
//-----------------------------------------------------------------------------
CHeapAlloc::CHeapAlloc()
{
    // Heap objects
    m_dwAllocAttributes = 0;
    m_dwNumHeapBlocks   = DEFAULT_NUMBER_BLOCKS;
    m_dwHeapBlockSize   = 0;

    m_pvHeap            = NULL;
    m_cbHeap            = 0;
    m_pvHeapMin         = NULL;
    m_pvHeapMax         = NULL;

    // Space map array
    m_pbSpaceMapArray   = NULL;
    m_cbSpaceMapArray   = 0;
}




//-----------------------------------------------------------------------------
// Name: CHeapAlloc::~CHeapAlloc()
// Desc: Desctructor
//-----------------------------------------------------------------------------
CHeapAlloc::~CHeapAlloc()
{
    PXALLOC_ATTRIBUTES  pAttributes = ( PXALLOC_ATTRIBUTES ) &m_dwAllocAttributes;
    assert( NULL != pAttributes );

    // Delete the heap
    if( NULL != m_pvHeap )
    {
        if( XALLOC_MEMTYPE_PHYSICAL == pAttributes->dwMemoryType )
        {   
            XPhysicalFree( m_pvHeap );                
        }
        else
        {        
            LocalFree( m_pvHeap );
        }
        m_pvHeap = NULL;
    }

    // Clear heap objects
    m_pvHeapMin       = NULL;
    m_pvHeapMax       = NULL;

    // Delete the space map
    delete[] m_pbSpaceMapArray;
    m_pbSpaceMapArray = NULL;
}




//-----------------------------------------------------------------------------
// Name: CHeapAlloc::CreateHeap()
// Desc: Create the heap
//-----------------------------------------------------------------------------
HRESULT CHeapAlloc::CreateHeap( DWORD dwNumHeapBlocks, DWORD dwHeapBlockSize,
                                DWORD dwAllocAttributes, DWORD flProtect )
{
    HRESULT             hr = NO_ERROR;
    PXALLOC_ATTRIBUTES  pAttributes = ( PXALLOC_ATTRIBUTES ) &dwAllocAttributes;

    // Validate arguments
    assert( dwNumHeapBlocks );
    assert( dwHeapBlockSize );
    assert( dwAllocAttributes );

    // Save heap allocation attributes
    m_dwAllocAttributes = dwAllocAttributes;

    // Set heap block size and number of blocks
    m_dwNumHeapBlocks   = dwNumHeapBlocks;
    m_dwHeapBlockSize   = dwHeapBlockSize;

    // Set the heap size
    m_cbHeap = m_dwNumHeapBlocks * m_dwHeapBlockSize;

    // Create the heap using appropriate allocator
    if( XALLOC_MEMTYPE_PHYSICAL == pAttributes->dwMemoryType )
    {
        m_pvHeap = XPhysicalAlloc( m_cbHeap, MAXULONG_PTR,
                                   pAttributes->dwAlignment * PAGE_SIZE, flProtect );

        // Initialize memory
        if( ( NULL != m_pvHeap ) && ( pAttributes->dwZeroInitialize ) )
        {
            ZeroMemory( m_pvHeap, m_cbHeap );
        }
    }
    else
    {        
        // Allocate block of memory
        m_pvHeap = LocalAlloc( LMEM_FIXED | ( pAttributes->dwZeroInitialize ? LMEM_ZEROINIT : 0), m_cbHeap );
    }

    // Check memory
    if( NULL == m_pvHeap )
        return E_OUTOFMEMORY;

    // Create heap bit array to track allocations and frees
    m_cbSpaceMapArray = ( m_dwNumHeapBlocks / 8 ) + 1;
    m_pbSpaceMapArray = new BYTE[ m_cbSpaceMapArray ];   
    if( NULL == m_pbSpaceMapArray )
        return E_OUTOFMEMORY;

    // Initialize heap bit array
    ZeroMemory( m_pbSpaceMapArray, m_cbSpaceMapArray );

    // Set the min and max pointers to track next free allocation
    m_pvHeapMin = m_pvHeap;
    m_pvHeapMax = ( (BYTE*)m_pvHeap + m_cbHeap );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: CHeapAlloc::HeapAlloc()
// Desc: Allocate an object using the custom heap allocator
//-----------------------------------------------------------------------------
HRESULT CHeapAlloc::HeapAlloc( VOID** ppvBuffer, DWORD dwZeroInitialize )
{
    // Validate heap state
    if( ( NULL == m_pvHeap ) || ( m_pvHeapMin == m_pvHeapMax ) )
        return E_FAIL;

    // Set the return pointer to the next available
    // memory in the heap. 
    (*ppvBuffer) = m_pvHeapMin;

    // Get the corresponding byte and bit from the free array
    DWORD dwCurrentByte = ( ( (BYTE*)m_pvHeapMin - (BYTE*)m_pvHeap ) / m_dwHeapBlockSize ) / 8;
    DWORD dwCurrentBit  = ( ( (BYTE*)m_pvHeapMin - (BYTE*)m_pvHeap ) / m_dwHeapBlockSize ) % 8;

    // Set the "used" bit in our heap free array
    m_pbSpaceMapArray[ dwCurrentByte ] |= BYTE( 1 << dwCurrentBit );

    // Find the next free space in our heap free
    // array if one exists.
    BOOL bBitFound = FALSE;
    for( dwCurrentByte; dwCurrentByte < m_cbSpaceMapArray; dwCurrentByte++ )
    {
        for( dwCurrentBit = 0; dwCurrentBit < 8; dwCurrentBit++)
        {
            if( !( m_pbSpaceMapArray[ dwCurrentByte ] & ( 1 << dwCurrentBit ) ) )
            {
                // Set our min heap pointer to the next free space
                m_pvHeapMin = (BYTE*)m_pvHeap + ( ( dwCurrentByte * ( m_dwHeapBlockSize * 8 ) ) + ( dwCurrentBit * m_dwHeapBlockSize ) );
                bBitFound = TRUE;
                break;
            }
        }

        // Free space found, break out of loop
        if( bBitFound )
            break;
    }

    // Initialize this block of memory if the caller requested
    if( ( NULL != *ppvBuffer ) && ( dwZeroInitialize ) )
    {
        ZeroMemory( *ppvBuffer, m_dwHeapBlockSize );
    }

    return NO_ERROR;
}




//-----------------------------------------------------------------------------
// Name: CHeapAlloc::HeapFree()
// Desc: Free an object from the custom heap allocator
//-----------------------------------------------------------------------------
HRESULT CHeapAlloc::HeapFree( VOID** ppvBuffer )
{
    // Validate arguments
    if( NULL == ppvBuffer )
        return NO_ERROR;

    // Validate address and heap state
    if( ( NULL == m_pvHeap ) || ( FALSE == AddressInHeap( ppvBuffer ) ) )
        return E_FAIL;

    // Get the corresponding byte and bit from the free array
    DWORD dwCurrentByte = ( ( (BYTE*)(*ppvBuffer) - (BYTE*)m_pvHeap ) / m_dwHeapBlockSize ) / 8;
    DWORD dwCurrentBit  = ( ( (BYTE*)(*ppvBuffer) - (BYTE*)m_pvHeap ) / m_dwHeapBlockSize ) % 8;

    // Set the "un-used" bit in our heap free array 
    // for this memory location.
    m_pbSpaceMapArray[dwCurrentByte] &= ~( 1 << dwCurrentBit );

    // If this address is less than our current min,
    // then set our min heap pointer to this space.
    if( (*ppvBuffer) < m_pvHeapMin )
        m_pvHeapMin = (*ppvBuffer);

    return NO_ERROR;
}
