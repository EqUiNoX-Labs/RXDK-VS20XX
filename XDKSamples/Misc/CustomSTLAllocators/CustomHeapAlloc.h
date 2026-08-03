//-----------------------------------------------------------------------------
// File: CustomHeapAlloc.h
//
// Desc: Custom STL allocator that allows the caller to specify the desired
//       heap used for allocations. It is the caller's responsibility to
//       create and destroy the heap (see HeapCreate and friends).
//
// Hist: 10.11.02 - New for November 2002 XDK release
//
// Example Usage 1:
//
//     // All allocations for v will be from the specified heap
//     CustomHeapAlloc< int > a( hHeap );
//     std::vector< int, CustomHeapAlloc< int > > v( a );
//
// Example Usage 2:
//
//     // All allocations for v will be from the specified heap
//     hHeap = HeapCreate( ... );
//     typedef CustomHeapAlloc< int > MyIntAlloc;
//     MyIntAlloc a( hHeap );
//     typedef std::vector< int, MyIntAlloc > MyIntVector;
//     MyIntVector v( a );
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#if !defined(CUSTOMHEAP_ALLOC_H)
#define CUSTOMHEAP_ALLOC_H

#include <memory>
#include <cassert>
#include <xtl.h>


//-----------------------------------------------------------------------------
// Name: CustomHeapAlloc()
// Desc: Allocator that uses a specific heap
//-----------------------------------------------------------------------------
template< typename T >
class CustomHeapAlloc
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
    CustomHeapAlloc()
    :
        m_hHeap( GetProcessHeap() )
    {
    }

    explicit CustomHeapAlloc( HANDLE hHeap )
    :
        m_hHeap( hHeap )
    {
        assert( hHeap != NULL );
    }

    CustomHeapAlloc( const CustomHeapAlloc< T >& a )
    :
        m_hHeap( a.m_hHeap )
    {
    }

    template< typename U >
    CustomHeapAlloc( const CustomHeapAlloc< U >& a )
    :
        m_hHeap( a.m_hHeap )
    {
    }

    ~CustomHeapAlloc()
    {
    }

    //-------------------------------------------------------------------------
    // Boilerplate allocator functions
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef CustomHeapAlloc< U > other;
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
        assert( m_hHeap != NULL );
    
        DWORD dwBytes = nCount * sizeof( T );
        pointer p = (pointer)HeapAlloc( m_hHeap, 0, dwBytes );
        
        // For C++ Standard compliance, throw bad_alloc on error.
        // If your code is expecting NULL in failure cases, remove these lines.
        if( p == NULL )
            throw std::bad_alloc();
            
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

        HeapFree( m_hHeap, 0, p );
    }

    //-------------------------------------------------------------------------
    // Accessor function
    //-------------------------------------------------------------------------
    HANDLE GetHeap() const
    {
        return m_hHeap;
    }

private:

    HANDLE m_hHeap;

    // Grant all specializations access private data.
    // This is required for the template copy ctor.
    template <typename U> friend class CustomHeapAlloc;

    // Unused
    CustomHeapAlloc< T >& operator=( const CustomHeapAlloc< T >& );
    
};


//-----------------------------------------------------------------------------
// CustomHeapAlloc standard template operators
//-----------------------------------------------------------------------------
template< typename T, typename U >
inline bool operator==( const CustomHeapAlloc< T >& lhs, const CustomHeapAlloc< U >& rhs )
{
    return lhs.GetHeap() == rhs.GetHeap();
}

template< typename T, typename U >
inline bool operator!=( const CustomHeapAlloc< T >& lhs, const CustomHeapAlloc< U >& rhs )
{
    return lhs.GetHeap() != rhs.GetHeap();
}


//-----------------------------------------------------------------------------
// Specialize for void
//-----------------------------------------------------------------------------
template<>
class CustomHeapAlloc< void >
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
    CustomHeapAlloc()
    {
    }

    template< typename U >
    CustomHeapAlloc( const CustomHeapAlloc< U >& )
    {
    }

    //-------------------------------------------------------------------------
    // Boilerplate rebind
    //-------------------------------------------------------------------------
    template< typename U >
    struct rebind
    {
        typedef CustomHeapAlloc< U > other;
    };
};


#endif // CUSTOMHEAP_ALLOC_H
