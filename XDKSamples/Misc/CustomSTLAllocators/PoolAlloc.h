//-----------------------------------------------------------------------------
// File: PoolAlloc.h
//
// Desc: STL allocator based on MallocAlloc - uses TinyObj.* to allocate
//       memory faster than new/delete. Most appropriate for list, set, and map,
//       which make thousands of small allocations, all the same size.
//
// Example Usage 1:
//
//     // All allocations for v will use GetTinyObjAllocator()
//     std::vector< int, PoolAlloc< int > > v1;
//
// Example Usage 2:
//
//     // All allocations for v will use GetTinyObjAllocator()
//     typedef PoolAlloc< int > MyIntAlloc;
//     typedef std::vector< int, MyIntAlloc > MyIntVector;
//     MyIntVector v2;
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#if !defined(POOL_ALLOC_H)
#define POOL_ALLOC_H

#pragma warning( disable: 4100 )
#include <memory>
#include <malloc.h>
#include "Tinyobj.h"


//-----------------------------------------------------------------------------
// Name: PoolAlloc()
// Desc: Allocator that uses TinyObjAllocator
//-----------------------------------------------------------------------------
template< typename T >
class PoolAlloc
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
    PoolAlloc()
    :
        m_SmallObjAllocator( &GetTinyObjAllocator() )
    {
    }

    template< typename U >
    PoolAlloc( const PoolAlloc< U >& )
    :
        m_SmallObjAllocator( &GetTinyObjAllocator() )
    {
    }

    ~PoolAlloc()
    {
    }

    //-------------------------------------------------------------------------
    // Boilerplate allocator functions
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef PoolAlloc< U > other;
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
    // Desc: Allocates memory using TinyObjAllocator
    //-------------------------------------------------------------------------
    pointer allocate( size_type nCount )
    {
        return allocate( nCount, NULL );
    }

    pointer allocate( size_type nCount, const void* /* pHint */ )
    {
        pointer p = (pointer)m_SmallObjAllocator->Allocate( nCount * sizeof( T ) );
        
        // For C++ Standard compliance, throw bad_alloc on error.
        // If your code is expecting NULL in failure cases, remove these lines.
        //if( p == NULL )
        //    throw std::bad_alloc();

        return p;
    }

    //-------------------------------------------------------------------------
    // Name: deallocate
    //-------------------------------------------------------------------------
    void deallocate( pointer p, size_type nCount )
    {
        m_SmallObjAllocator->Deallocate(p, nCount * sizeof( T ) );
    }
private:
    // Cache this for better performance - the GetSmallObjAllocator() function
    // has to check each time to see if the object has been constructed, which
    // is wasteful. Caching the allocator wastes some memory, but usually not
    // enough to matter.
    TinyObjAllocator* m_SmallObjAllocator;
};


//-----------------------------------------------------------------------------
// PoolAlloc standard template operators
//-----------------------------------------------------------------------------
template< typename T, typename U >
inline bool operator==( const PoolAlloc< T >&, const PoolAlloc< U >& )
{
    return true;
}

template< typename T, typename U >
inline bool operator!=( const PoolAlloc< T >&, const PoolAlloc< U >& )
{
    return false;
}


//-----------------------------------------------------------------------------
// Specialize for void
//-----------------------------------------------------------------------------
template<>
class PoolAlloc< void >
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
    PoolAlloc()
    {
    }

    template< typename U >
    PoolAlloc( const PoolAlloc< U >& )
    {
    }

    //-------------------------------------------------------------------------
    // Boilerplate rebind
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef PoolAlloc< U > other;
    };
};


#endif
