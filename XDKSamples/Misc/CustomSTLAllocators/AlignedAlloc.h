//-----------------------------------------------------------------------------
// File: AlignedAlloc.h
//
// Desc: Custom STL allocator that guarantees object alignment. Useful for
//       objects that require a specific memory alignment on Xbox. Also shows
//       how to create an allocator with multiple template parameters.
//
// Hist: 10.11.02 - New for November 2002 XDK release
//
// Example Usage 1:
//
//     // All allocations for v will be 16-byte aligned
//     std::vector< MyMatrix, AlignedAlloc< MyMatrix, 16 > > v;
//
// Example Usage 2:
//
//     // All allocations for v will be 32-byte aligned
//     typedef AlignedAlloc< MyMatrix, 32 > MyAlignedMatrixAlloc;
//     typedef std::vector< MyMatrix, MyAlignedMatrixAlloc > MyAlignedMatrixVector;
//     MyAlignedMatrixVector v;
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#if !defined(ALIGNED_ALLOC_H)
#define ALIGNED_ALLOC_H

#include <memory>
#include <cassert>
#include <malloc.h>


//-----------------------------------------------------------------------------
// Name: AlignedAlloc()
// Desc: Allocator that guarantees the specified alignment of objects in memory.
//       Alignment is specified by the 2nd template parameter; must be a power
//       of 2.
//-----------------------------------------------------------------------------
template< typename T, int Alignment >
class AlignedAlloc
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
    AlignedAlloc()
    {
    }

    AlignedAlloc( const AlignedAlloc< T, Alignment >& )
    {
    }

    template< typename U, int Alignment >
    AlignedAlloc( const AlignedAlloc< U, Alignment >& )
    {
    }

    ~AlignedAlloc()
    {
    }

    //-------------------------------------------------------------------------
    // Boilerplate allocator functions
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef AlignedAlloc< U, Alignment > other;
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
        size_t nCount = (size_t)( -1 ) / sizeof( T );
        return( 0 < nCount ? nCount : 1 );
    }

    //-------------------------------------------------------------------------
    // Name: allocate
    // Desc: Allocates aligned memory using MS CRT function _aligned_malloc
    //-------------------------------------------------------------------------
    pointer allocate( size_type nCount )
    {
        return allocate( nCount, NULL );
    }

    pointer allocate( size_type nCount, const void* /* pHint */ )
    {
        pointer p = (pointer)_aligned_malloc( nCount * sizeof( T ), Alignment );
        assert( (size_t)p % Alignment == 0 ); // verify alignment
        
        // For C++ Standard compliance, throw bad_alloc on error.
        // If your code is expecting NULL in failure cases, remove these lines.
        if( p == NULL )
            throw std::bad_alloc();
        
        return p;
    }

    //-------------------------------------------------------------------------
    // Name: deallocate
    // Desc: Deallocate aligned memory using MS CRT function _aligned_free
    //-------------------------------------------------------------------------
    void deallocate( pointer p, size_type /* nCount */ )
    {
        assert( (size_t)p % Alignment == 0 ); // verify alignment
        _aligned_free( p );
    }

private:

    // Unused
    AlignedAlloc< T, Alignment >& operator=( const AlignedAlloc< T, Alignment >& );

};


//-----------------------------------------------------------------------------
// AlignedAlloc standard template operators
//-----------------------------------------------------------------------------
template< typename T, typename U, int Alignment >
inline bool operator==( const AlignedAlloc< T, Alignment >&, const AlignedAlloc< U, Alignment >& )
{
    return true;
}

template< typename T, typename U, int Alignment >
inline bool operator!=( const AlignedAlloc< T, Alignment >&, const AlignedAlloc< U, Alignment >& )
{
    return false;
}


//-----------------------------------------------------------------------------
// Specialize for void
//-----------------------------------------------------------------------------
template< int Alignment >
class AlignedAlloc< void, Alignment >
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
    AlignedAlloc()
    {
    }

    template< typename U, int Alignment >
    AlignedAlloc( const AlignedAlloc< U, Alignment >& )
    {
    }

    //-------------------------------------------------------------------------
    // Boilerplate rebind
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef AlignedAlloc< U, Alignment > other;
    };
};

#endif // ALIGNED_ALLOC_H
