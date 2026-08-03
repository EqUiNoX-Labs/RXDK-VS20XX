//-----------------------------------------------------------------------------
// File: CustomSTLAllocators.cpp
//
// Desc: Custom STL allocators allow games to use unique and specialized
//       allocators for particular STL containers. For instance, if a list of
//       matrices requires specific alignment to utilize SSE instructions, the
//       list/deque/vector can use an allocator that enforces a particular
//       memory alignment. Or, if the memory allocations for each node of
//       a set, list, or map are wasting too much memory (default overhead
//       is 16-31 bytes per allocation) or taking too long to allocate and
//       free, then the PoolAlloc custom STL allocator can conserve memory
//       and improve performance, at the cost of some flexibility.
//
//       The Xbox title libraries also allow developers to provide custom
//       allocators for the XTL. See CustomMemoryAllocator for an example.
//
// Hist: 10.11.02 - New for November 2002 XDK release
//       12.01.02 - Modified to add PoolAlloc for February 2003 XDK release
//       02.22.03 - Added new test cases; updated for 7.1 compiler
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

// STL container headers used for testing
#pragma warning( disable: 4702 ) // ignore unreachable code
#include <vector>
#include <deque>
#include <list>
#include <map>
#include <set>

// STL container adapters used for testing
#include <stack>
#include <queue>

// Strings support allocators
#include <string>

// Non-standard hash containers
#include <hash_map>
#include <hash_set>

// For testing
#include <complex>

// Custom allocators
#include "MallocAlloc.h"
#include "AlignedAlloc.h"
#include "InPlaceAlloc.h"
#include "DebugAlloc.h"
#include "CustomHeapAlloc.h"
#include "PoolAlloc.h"

const int MAX_ITEMS = 100;

using namespace std;


//-----------------------------------------------------------------------------
// Name: Bill
// Desc: Test structure for verifying allocators. Has a non-trival constructor,
//       copy ctor and destructor.
//-----------------------------------------------------------------------------
class Bill
{
public:
    
    Bill()
    :
        p( new char( 'x' ) )
    {
    }
    
    Bill( const Bill& s )
    :
        p( new char( *s.p ) )
    {
    }
    
    Bill& operator=( const Bill& w )
    {
        // See Exceptional C++ Item 13
        Bill temp( w );
        std::swap( p, temp.p );
        return *this;
    }

    ~Bill()
    {
        assert( *p == 'x' );
        delete p;
    }

    // Comparable so it can be used in sets and maps
    bool operator <( const Bill& s ) const
    { 
        return p < s.p;
    }
    
    // Hashable so it can be used in hash containers
    operator size_t() const
    { 
        return (size_t)p;
    }
    
private:
    char* p;

};


//-----------------------------------------------------------------------------
// Name: Ted
// Desc: Complex test structure for verifying allocators. Contains multiple
//       objects, with non-trival ctor, copy ctor and dtor.
//-----------------------------------------------------------------------------
class Ted
{
public:

    Ted( int j )
    :
        i( j ), f( 2.3f ), c( 4.5, 6.7 ), s( "TedTedTedTedTed" )
    {
    }
    
    Ted( const Ted& g )
    :        
        i( g.i ), f( g.f ), c( g.c ), s( g.s )
    {
    }

    Ted& operator=( const Ted& g )
    {
        // See Exceptional C++ Item 13
        Ted temp( g );
        std::swap( i, temp.i );
        std::swap( f, temp.f );
        std::swap( c, temp.c );
        std::swap( s, temp.s );
        return *this;
    }
    
    ~Ted()
    {
        assert( i == 1 ); // NOTE: requires all tests to construct w/ 1
        assert( f == 2.3f );
        assert( c.real() == 4.5 );
        assert( c.imag() == 6.7 );
        assert( s == "TedTedTedTedTed" );
    }
    
    // Comparable so it can be used in sets and maps
    bool operator<( const Ted& s ) const
    {
        return this->s.c_str() < s.s.c_str();
    }
    
    // Hashable so it can be used in hash containers
    operator size_t() const
    {
        return size_t( s.c_str() );
    }
    
private:

    // Default ctor disabled for example purposes
    Ted();    

private:    
    int i;
    float f;
    complex<double> c;
    string s;
};


//-----------------------------------------------------------------------------
// Name: BaseTest()
// Desc: Test base functionality of allocators
//-----------------------------------------------------------------------------
template< typename T, typename Alloc >
void BaseTest( const T& t, const Alloc& a )
{
    // Default construction is not tested, because some allocators
    // don't support default construction

    Alloc copy(a); // copy ctor
    
    // comparison operators
    bool bSame = a == copy;
    assert( bSame );
    bool bDiff = a != copy;
    assert( !bDiff );
    
    // rebind
    Alloc::rebind<char>::other b(a); // template copy ctor
    
    // rebind to void (all allocators should provide specialized version for void)
    Alloc::rebind<void>::other c(a);
    
    // comparison operators with different types
    bSame = a == b;
    assert( bSame );
    bDiff = a != b;
    assert( !bDiff );
    
    // address
    Alloc::const_pointer address = a.address( t );
    assert( address != NULL );
    (void)address;
    
    // allocate
    Alloc::pointer p = copy.allocate( 2, NULL );
    
    // construct
    copy.construct( p, t );
    copy.construct( p+1, t );
    
    // destroy
    copy.destroy( p );
    copy.destroy( p+1 );
    
    // deallocate
    copy.deallocate( p, 2 );
    
    // simple test
    p = copy.allocate( 1 );
    copy.deallocate( p, 1 );
    
    // max_size
    Alloc::size_type ms = a.max_size();
    (void)ms;
}


//-----------------------------------------------------------------------------
// Name: TestAlloc()
// Desc: Test containers' use of custom allocators. Minimum test is adding
//       an element from the container and then destroying the container, which
//       forces the container to free any memory it has allocated.
//       All containers are constructed using the input Alloc parameter.
//       This is only officially required for allocators can't be created 
//       using the default allocator constructor (like InPlaceAlloc).
//       The individual scope blocks ensure that the container and allocator 
//       are cleaned up properly before testing continues.
//-----------------------------------------------------------------------------
template< typename T, typename Alloc >
void TestAlloc( const T& t, const Alloc& a )
{
    // Test base level functionality
    BaseTest( t, a );

    // Sequence containers ----------------------------------------------------
    
    // Vector
    {
    vector< T, Alloc > v( a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        v.push_back( t );
    bool bSame = ( v.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
    
    // Deque
    {
    deque < T, Alloc > q( a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        q.push_back( t );
    bool bSame = ( q.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
    
    // List
    {
    list < T, Alloc > l( a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        l.push_back( t );
    bool bSame = ( l.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }

    // Associative containers -------------------------------------------------
    
    // Set
    {
    set< T, less<T>, Alloc > s( less<T>(), a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        s.insert( t );
    bool bSame = ( s.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
    
    // Multiset
    {    
    multiset< T, less<T>, Alloc > ms( less<T>(), a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        ms.insert( t );
    bool bSame = ( ms.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
    
    // Map
    {
    map< T, T, less<T>, Alloc > m( less<T>(), a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        m.insert( make_pair( t, t ) );
    bool bSame = ( m.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
    
    // Multimap
    {
    multimap< T, T, less<T>, Alloc > mm( less<T>(), a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        mm.insert( make_pair( t, t ) );
    bool bSame = ( mm.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
    
    // Container adapters -----------------------------------------------------
    
    // Stack
    {
    deque< T, Alloc > d( a );
    stack< T, deque< T, Alloc > > s1( d );
    for( int i = 0; i < MAX_ITEMS; ++i )
        s1.push( t );
    
    vector< T, Alloc > v( a );
    stack< T, vector< T, Alloc > > s2( v );
    for( int i = 0; i < MAX_ITEMS; ++i )
        s2.push( t );
    }
    
    // Queue
    {
    deque< T, Alloc > d( a );
    queue< T, deque< T, Alloc > > q1( d );
    for( int i = 0; i < MAX_ITEMS; ++i )
        q1.push( t );
    
    list< T, Alloc > l( a );
    queue< T, list< T, Alloc > > q2( l );
    for( int i = 0; i < MAX_ITEMS; ++i )
        q2.push( t );
    }
    
    // Priority Queue
    {
    deque< T, Alloc > d( a );
    priority_queue< T, deque< T, Alloc > > q1( less<T>(), d );
    for( int i = 0; i < MAX_ITEMS; ++i )
        q1.push( t );
    
    vector< T, Alloc > v( a );
    priority_queue< T, vector< T, Alloc > > q2( less<T>(), v );
    for( int i = 0; i < MAX_ITEMS; ++i )
        q2.push( t );
    }
    
    // Hash-based associative containers (non-standard)------------------------
    
    // Hash set
    {
    stdext::hash_set< T, stdext::hash_compare<T>, Alloc > 
        hs( stdext::hash_compare<T>(), a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        hs.insert( t );
    bool bSame = ( hs.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
    
    // Hash multiset
    {
    stdext::hash_multiset< T, stdext::hash_compare<T>, Alloc >
        hms( stdext::hash_compare<T>(), a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        hms.insert( t );
    bool bSame = ( hms.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
    
    // Hash map
    {
    stdext::hash_map< T, T, stdext::hash_compare<T>, Alloc >
        hm( stdext::hash_compare<T>(), a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        hm.insert( make_pair( t, t ) );
    bool bSame = ( hm.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
    
    // Hash multimap
    {
    stdext::hash_multimap< T, T, stdext::hash_compare<T>, Alloc >
        hmm( stdext::hash_compare<T>(), a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        hmm.insert( make_pair( t, t ) );
    bool bSame = ( hmm.get_allocator() == a );
    assert( bSame );
    (void)bSame;
    }
}


//-----------------------------------------------------------------------------
// Name: TestString()
// Desc: Test string allocators. Because a string can hold only a limited set
//       of types T (typically char and wchar_t), this code is separated out.
//-----------------------------------------------------------------------------
template< typename T, typename Alloc >
void TestString( const T& t, const Alloc& a )
{
    basic_string< T, char_traits<T>, Alloc > s1( a );
    for( int i = 0; i < MAX_ITEMS; ++i )
        s1.push_back( t );
}


//-----------------------------------------------------------------------------
// Name: TestDefaultAlloc()
// Desc: Test DefaultAlloc allocator
//-----------------------------------------------------------------------------
void TestDefaultAlloc()
{
    // Minimal test case
    vector< int, allocator< int > > v;
    v.push_back( 1 );
    v.clear();
    
    // Test with different objects
    TestAlloc( 1,        allocator< int >() );
    TestAlloc( (void*)1, allocator< void* >() );
    TestAlloc( Bill(),   allocator< Bill >() );
    TestAlloc( Ted(1),   allocator< Ted >() );

    // String testing    
    TestString( 'a',  allocator< char >() );
    TestString( L'a', allocator< wchar_t >() );
    TestString( 1,    allocator< int > () );
}

//-----------------------------------------------------------------------------
// Name: TestMallocAlloc()
// Desc: Test MallocAlloc allocator
//-----------------------------------------------------------------------------
void TestMallocAlloc()
{
    // Minimal test case
    vector< int, MallocAlloc< int > > v;
    v.push_back( 1 );
    v.clear();
    
    // Usage tests from MallocAlloc.h
    vector< int, MallocAlloc< int > > v1;
    typedef MallocAlloc< int > MyIntAlloc;
    typedef vector< int, MyIntAlloc > MyIntVector;
    MyIntVector v2;

    // Test with different objects
    TestAlloc( 1,        MallocAlloc< int >() );
    TestAlloc( (void*)1, MallocAlloc< void* >() );
    TestAlloc( Bill(),   MallocAlloc< Bill >() );
    TestAlloc( Ted(1),   MallocAlloc< Ted >() );
    
    // String testing    
    TestString( 'a',  MallocAlloc< char >() );
    TestString( L'a', MallocAlloc< wchar_t >() );
    TestString( 1,    MallocAlloc< int > () );
}


//-----------------------------------------------------------------------------
// Name: TestAlignedAlloc()
// Desc: Test AlignedAlloc allocator
//-----------------------------------------------------------------------------
void TestAlignedAlloc()
{
    // Minimal test case
    vector< int, AlignedAlloc< int, 32 > > v;
    v.push_back( 1 );
    v.clear();
    
    // Usage tests from AlignedAlloc.h
    typedef Bill MyMatrix;
    vector< MyMatrix, AlignedAlloc< MyMatrix, 16 > > v1;
    typedef AlignedAlloc< MyMatrix, 32 > MyAlignedMatrixAlloc;
    typedef vector< MyMatrix, MyAlignedMatrixAlloc > MyAlignedMatrixVector;
    MyAlignedMatrixVector v2;

    // Test with different objects (and alignments)
    TestAlloc( 1,        AlignedAlloc< int, 8 >() );
    TestAlloc( (void*)1, AlignedAlloc< void*, 8 >() );
    TestAlloc( Bill(),   AlignedAlloc< Bill, 16 >() );
    TestAlloc( Ted(1),   AlignedAlloc< Ted, 16 >() );

    // String testing    
    TestString( 'a',  AlignedAlloc< char, 8 >() );
    TestString( L'a', AlignedAlloc< wchar_t, 32 >() );
    TestString( 1,    AlignedAlloc< int, 16 > () );
}


//-----------------------------------------------------------------------------
// Name: TestInPlaceAlloc()
// Desc: Test InPlaceAlloc allocator
//-----------------------------------------------------------------------------
void TestInPlaceAlloc()
{
    // Minimal test case
    BYTE pStack[ 1024 ];
    InPlaceAlloc< int > a( pStack, 1024 );
    vector< int, InPlaceAlloc< int > > v( a );
    v.push_back( 1 );
    v.clear();

    // Usage tests from InPlaceAlloc.h
    BYTE pStack1[1024];
    InPlaceAlloc< int > alloc1( pStack1, 1024 );
    vector< int, InPlaceAlloc< int > > v1( alloc1 );
    typedef InPlaceAlloc< int > MyIntAlloc;
    typedef vector< int, MyIntAlloc > MyIntVector;
    BYTE pStack2[1024];
    MyIntAlloc alloc2( pStack2, 1024 );
    MyIntVector v2( alloc2 );
    BYTE* pHeap1 = (BYTE*)malloc( 1024 );
    MyIntAlloc alloc3( pHeap1, 1024 );
    MyIntVector v3( alloc3 );
    free( pHeap1 );

    // Create some allocators
    const size_t nStackSize = 100 * MAX_ITEMS * sizeof(int);
    BYTE pBigStack[ nStackSize ];
    InPlaceAlloc< int > intAlloc( pBigStack, nStackSize );

    const size_t nHeapSize = 100 * MAX_ITEMS * sizeof( Ted );
    BYTE* pHeap = (BYTE*)malloc( nHeapSize );
    InPlaceAlloc< void*> voidAlloc( pHeap, nHeapSize );
    InPlaceAlloc< Bill > billAlloc( pHeap, nHeapSize );
    InPlaceAlloc< Ted >  tedAlloc( pHeap, nHeapSize );

    // Test with different objects
    TestAlloc( 1,        intAlloc );
    TestAlloc( (void*)1, voidAlloc );
    TestAlloc( Bill(),   billAlloc );
    TestAlloc( Ted(1),   tedAlloc );
    
    InPlaceAlloc< char >    charAlloc( pHeap, nHeapSize );
    InPlaceAlloc< wchar_t > wcharAlloc( pHeap, nHeapSize );

    // String testing    
    TestString( 'a',  charAlloc );
    TestString( L'a', wcharAlloc );
    TestString( 1,    intAlloc );

    free( pHeap );
}


//-----------------------------------------------------------------------------
// Name: TestDebugAlloc()
// Desc: Test Debug allocator
//-----------------------------------------------------------------------------
void TestDebugAlloc()
{
    // Minimal test case
    vector< int, DebugAlloc< int > > v;
    v.push_back( 1 );
    v.clear();

    // Usage tests from DebugAlloc.h
    vector< int, DebugAlloc< int > > v1;
    typedef DebugAlloc< int > MyIntAlloc;
    typedef vector< int, MyIntAlloc > MyIntVector;
    MyIntVector v2;
    typedef DebugAlloc< int > MyIntAlloc;
    typedef vector< int, MyIntAlloc > MyIntVector;
    MyIntVector v3;
    DWORD dwBytesAllocated = v3.get_allocator().GetBytesAllocated();
    v3.push_back( 1 );
    dwBytesAllocated = v3.get_allocator().GetBytesAllocated();
    DWORD dwAllocations = v3.get_allocator().GetAllocationCount();
    (VOID)dwAllocations; // avoid compiler warning

    // Test with different objects
    TestAlloc( 1,        DebugAlloc< int >() );
    TestAlloc( (void*)1, DebugAlloc< void* >() );
    TestAlloc( Bill(),   DebugAlloc< Bill >() );
    TestAlloc( Ted(1),   DebugAlloc< Ted >() );

    // String testing    
    TestString( 'a',  DebugAlloc< char >() );
    TestString( L'a', DebugAlloc< wchar_t >() );
    TestString( 1,    DebugAlloc< int > () );
}


//-----------------------------------------------------------------------------
// Name: TestCustomHeapAlloc()
// Desc: Test CustomHeap allocator
//-----------------------------------------------------------------------------
void TestCustomHeapAlloc()
{
    // Minimal test case
    vector< int, CustomHeapAlloc< int > > v;
    v.push_back( 1 );
    v.clear();

    // Usage tests from CustomHeapAlloc.h
    HANDLE hHeap = HeapCreate( HEAP_NO_SERIALIZE, 1024, 0 );
    {
    CustomHeapAlloc< int > a1( hHeap );
    vector< int, CustomHeapAlloc< int > > v1( a1 );
    typedef CustomHeapAlloc< int > MyIntAlloc;
    MyIntAlloc a2( hHeap );
    typedef vector< int, MyIntAlloc > MyIntVector;
    MyIntVector v2( a2 );
    }

    // Test with different objects
    TestAlloc( 1,        CustomHeapAlloc< int >() );
    TestAlloc( (void*)1, CustomHeapAlloc< void* >() );
    TestAlloc( Bill(),   CustomHeapAlloc< Bill >() );
    TestAlloc( Ted(1),   CustomHeapAlloc< Ted >() );

    // String testing    
    TestString( 'a',  CustomHeapAlloc< char >() );
    TestString( L'a', CustomHeapAlloc< wchar_t >() );
    TestString( 1,    CustomHeapAlloc< int > () );
    
    // Test with custom heap
    const size_t nMaxSize = 100 * MAX_ITEMS * sizeof( Ted );
    HANDLE hCustom = HeapCreate( HEAP_NO_SERIALIZE, 1024 * 10, nMaxSize );
    TestAlloc( 1,        CustomHeapAlloc< int >( hCustom ) );
    TestAlloc( (void*)1, CustomHeapAlloc< void* >( hCustom ) );
    TestAlloc( Bill(),   CustomHeapAlloc< Bill >( hCustom ) );
    TestAlloc( Ted(1),   CustomHeapAlloc< Ted >( hCustom ) );
    
    HeapDestroy( hHeap );
    HeapDestroy( hCustom );
}


//-----------------------------------------------------------------------------
// Name: TestPoolAlloc()
// Desc: Test PoolAlloc allocator
//-----------------------------------------------------------------------------
void TestPoolAlloc()
{
    // Minimal test case
    vector< int, PoolAlloc< int > > v;
    v.push_back( 1 );
    v.clear();
    
    // Usage tests from PoolAlloc.h
    std::vector< int, PoolAlloc< int > > v1;
    typedef PoolAlloc< int > MyIntAlloc;
    typedef std::vector< int, MyIntAlloc > MyIntVector;
    MyIntVector v2;

    // Test with different objects
    TestAlloc( 1,        PoolAlloc< int >() );
    TestAlloc( (void*)1, PoolAlloc< void* >() );
    TestAlloc( Bill(),   PoolAlloc< Bill >() );
    TestAlloc( Ted(1),   PoolAlloc< Ted >() );

    // String testing    
    TestString( 'a',  PoolAlloc< char >() );
    TestString( L'a', PoolAlloc< wchar_t >() );
    TestString( 1,    PoolAlloc< int > () );

#ifdef  _DEBUG
    GetTinyObjAllocator().DumpStats();
#endif
}


//-----------------------------------------------------------------------------
// Name: main()
// Desc: Test the various allocators
//-----------------------------------------------------------------------------
void __cdecl main()
{
    OutputDebugStringA( "Custom STL Allocator tests started\n" );

    TestDefaultAlloc();    
    TestMallocAlloc();
    TestAlignedAlloc();
    TestInPlaceAlloc();
    TestDebugAlloc();
    TestCustomHeapAlloc();
    TestPoolAlloc();

    OutputDebugStringA( "Custom STL Allocator tests complete\n" );
    for(;;);
}
