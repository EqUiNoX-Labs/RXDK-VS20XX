//-----------------------------------------------------------------------------
// File: DebugAlloc.h
//
// Desc: Custom STL allocator that adds checking for heap corruption and
//       supports heap statistics. Actual allocations are done with 
//       HeapAlloc on the default XBE heap. Every time an allocation or
//       deallocation is done the heap is validated.
//
// Hist: 10.11.02 - New for November 2002 XDK release
//
// Example Usage 1:
//
//     // All allocations for v will be logged
//     std::vector< int, DebugAlloc< int > > v;
//
// Example Usage 2:
//
//     // All allocations for v will be logged
//     typedef DebugAlloc< int > MyIntAlloc;
//     typedef std::vector< int, MyIntAlloc > MyIntVector;
//     MyIntVector v;
//
// Example Usage 3:
//
//     // Track statistics
//     typedef DebugAlloc< int > MyIntAlloc;
//     typedef std::vector< int, MyIntAlloc > MyIntVector;
//     MyIntVector v;
//     v.push_back( 1 );
//     DWORD dwBytesAllocated = v.get_allocator().GetBytesAllocated();
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#if !defined(DEBUG_ALLOC_H)
#define DEBUG_ALLOC_H

#include <memory>
#include <cassert>
#include <xtl.h>


//-----------------------------------------------------------------------------
// Name: DebugAlloc()
// Desc: Allocator that validates the heap and tracks allocations
//-----------------------------------------------------------------------------
template< typename T >
class DebugAlloc
{
public:

    //-------------------------------------------------------------------------
    // Boilerplate allocator typedefs
    //-------------------------------------------------------------------------
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
    typedef T*        pointer;
    typedef const T*  const_pointer;
    typedef T&        reference;
    typedef const T&  const_reference;
    typedef T         value_type;

    //-------------------------------------------------------------------------
    // Constructors/Destructor
    //-------------------------------------------------------------------------
    DebugAlloc()
    :
        m_dwAllocCount( 0 ),
        m_dwBytesAllocated( 0 )
    {
    }

    DebugAlloc( const DebugAlloc< T >& a )
    :
        m_dwAllocCount( a.GetAllocationCount() ),
        m_dwBytesAllocated( a.GetBytesAllocated() )
    {
    }

    template< typename U >
    DebugAlloc( const DebugAlloc< U >& a )
    :
        m_dwAllocCount( a.GetAllocationCount() ),
        m_dwBytesAllocated( a.GetBytesAllocated() )
    {
    }

    ~DebugAlloc()
    {
        // Validate the heap
        #ifdef _DEBUG
        if( !HeapValidate( GetProcessHeap(), 0, NULL ) )
        {
            OutputDebugStringA( "Heap corrupted" );
            DebugBreak();
        }
        #endif
    }

    //-------------------------------------------------------------------------
    // Boilerplate allocator functions
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef DebugAlloc< U > other;
    };

    pointer address( reference x ) const
    {   
        return &x;
    }

    const_pointer address( const_reference x ) const
    {
        return &x;
    }

    void construct( pointer p, const T& val )
    {
        new ((void *)p) T(val); // placement new
    }

    void destroy( pointer p )
    {
        (p)->~T(); // in-place destruction
    }

    size_t max_size() const // maximum array size
    {
        size_t nCount = (size_t)( -1 ) / sizeof ( T );
        return( 0 < nCount ? nCount : 1 );
    }

    //-------------------------------------------------------------------------
    // Name: allocate
    // Desc: Allocates memory using HeapAlloc
    //-------------------------------------------------------------------------
    pointer allocate( size_type nCount )
    {
        return allocate( nCount, NULL );
    }

    pointer allocate( size_type nCount, const void* /* pHint */ )
    {
        DWORD dwBytes = nCount * sizeof( T );
        pointer p = (pointer)HeapAlloc( GetProcessHeap(), 0, dwBytes );
        
        // For C++ Standard compliance, throw bad_alloc on error.
        // If your code is expecting NULL in failure cases, remove these lines.
        if( p == NULL )
        {
            DebugBreak();
            throw std::bad_alloc();
        }
            
        // Track statistics
        ++m_dwAllocCount;
        m_dwBytesAllocated += dwBytes;
        
        // Validate the heap
        #ifdef _DEBUG
        if( !HeapValidate( GetProcessHeap(), 0, NULL ) )
        {
            DebugBreak();
            throw std::bad_alloc();
        }
        #endif
            
        return p;
    }

    //-------------------------------------------------------------------------
    // Name: deallocate
    // Desc: Deallocate memory using HeapFree
    //-------------------------------------------------------------------------
    void deallocate( pointer p, size_type /* nCount */ )
    {
        if( p == NULL )
            return;

        // Find out the size of the allocation
        DWORD dwBytes = HeapSize( GetProcessHeap(), 0, p );

        assert( m_dwBytesAllocated >= dwBytes );
        m_dwBytesAllocated -= dwBytes;

        // Track statistics
        assert( m_dwAllocCount > 0 );
        --m_dwAllocCount;
        
        // Free the memory
        HeapFree( GetProcessHeap(), 0, p );
    }

    //-------------------------------------------------------------------------
    // Accessor functions
    //-------------------------------------------------------------------------
    DWORD GetAllocationCount() const
    {
        return m_dwAllocCount;
    }
    
    DWORD GetBytesAllocated() const
    {
        return m_dwBytesAllocated;
    }
    
private:

    DWORD m_dwAllocCount;       // Number of outstanding allocations
    DWORD m_dwBytesAllocated;   // Number of bytes allocated
    
    DebugAlloc< T >& operator=( const DebugAlloc< T >& );
    
};


//-----------------------------------------------------------------------------
// DebugAlloc standard template operators
//-----------------------------------------------------------------------------
template< typename T, typename U >
inline bool operator==( const DebugAlloc< T >&, const DebugAlloc< U >& )
{
    return true;
}

template< typename T, typename U >
inline bool operator!=( const DebugAlloc< T >&, const DebugAlloc< U >& )
{
    return false;
}


//-----------------------------------------------------------------------------
// Specialize for void
//-----------------------------------------------------------------------------
template<>
class DebugAlloc< void >
{
public:

    //-------------------------------------------------------------------------
    // Boilerplate allocator typedefs; no references to void possible
    //-------------------------------------------------------------------------
    typedef void*       pointer;
    typedef const void* const_pointer;
    typedef void        value_type;

    //-------------------------------------------------------------------------
    // Constructors
    //-------------------------------------------------------------------------
    DebugAlloc()
    {
    }

    template< typename U >
    DebugAlloc( const DebugAlloc< U >& )
    {
    }

    //-------------------------------------------------------------------------
    // Boilerplate rebind
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef DebugAlloc< U > other;
    };
};


#endif // DEBUG_ALLOC_H
