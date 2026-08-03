//-----------------------------------------------------------------------------
// File: MallocAlloc.h
//
// Desc: Custom STL allocator that uses malloc/free. Simplest example of a
//       custom allocator.
//
// Hist: 10.11.02 - New for November 2002 XDK release
//
// Example Usage 1:
//
//     // All allocations for v will use malloc
//     std::vector< int, MallocAlloc< int > > v;
//
// Example Usage 2:
//
//     // All allocations for v will use malloc
//     typedef MallocAlloc< int > MyIntAlloc;
//     typedef std::vector< int, MyIntAlloc > MyIntVector;
//     MyIntVector v;
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#if !defined(MALLOC_ALLOC_H)
#define MALLOC_ALLOC_H

#pragma warning( disable: 4100 )
#include <memory>
#include <malloc.h>


//-----------------------------------------------------------------------------
// Name: MallocAlloc()
// Desc: Allocator that uses CRT functions malloc/free
//-----------------------------------------------------------------------------
template< typename T >
class MallocAlloc
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
    MallocAlloc()
    {
    }

    MallocAlloc( const MallocAlloc< T >& )
    {
    }

    template< typename U >
    MallocAlloc( const MallocAlloc< U >& )
    {
    }

    ~MallocAlloc()
    {
    }

    //-------------------------------------------------------------------------
    // Boilerplate allocator functions
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef MallocAlloc< U > other;
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
    // Desc: Allocates memory using CRT function malloc
    //-------------------------------------------------------------------------
    pointer allocate( size_type nCount )
    {
        return allocate( nCount, NULL );
    }

    pointer allocate( size_type nCount, const void* /* pHint */ )
    {
        pointer p = (pointer)malloc( nCount * sizeof( T ) );
        
        // For C++ Standard compliance, throw bad_alloc on error.
        // If your code is expecting NULL in failure cases, remove these lines.
        if( p == NULL )
            throw std::bad_alloc();
            
        return p;
    }

    //-------------------------------------------------------------------------
    // Name: deallocate
    // Desc: Deallocate memory using CRT function free
    //-------------------------------------------------------------------------
    void deallocate( pointer p, size_type /* nCount */ )
    {
        free( p );
    }

private:

    // Unused
    MallocAlloc< T >& operator=( const MallocAlloc< T >& );

};


//-----------------------------------------------------------------------------
// MallocAlloc standard template operators
//-----------------------------------------------------------------------------
template< typename T, typename U >
inline bool operator==( const MallocAlloc< T >&, const MallocAlloc< U >& )
{
    return true;
}

template< typename T, typename U >
inline bool operator!=( const MallocAlloc< T >&, const MallocAlloc< U >& )
{
    return false;
}


//-----------------------------------------------------------------------------
// Specialize for void
//-----------------------------------------------------------------------------
template<>
class MallocAlloc< void >
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
    MallocAlloc()
    {
    }

    template< typename U >
    MallocAlloc( const MallocAlloc< U >& )
    {
    }

    //-------------------------------------------------------------------------
    // Boilerplate rebind
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef MallocAlloc< U > other;
    };
};


#endif // MALLOC_ALLOC_H
