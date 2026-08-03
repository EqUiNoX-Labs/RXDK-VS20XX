//-----------------------------------------------------------------------------
// Name: CustomSTLAllocators
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample includes multiple custom STL allocators. Each allocator is designed
   to show a particular allocation feature. All allocators use the boilerplate code
   found in the <xmemory> header file.
   
Required files and media
========================
   This sample requires no media

Programming Notes
=================

   MallocAlloc is the simplest allocator. It uses malloc and free for memory
   management. If you're writing your own custom allocator, pattern off of
   MallocAlloc.
   
   AlignedAlloc uses _aligned_malloc for memory allocation. It guarantees that
   memory is aligned to the particular size specified. For maximum runtime speed,
   the alignment is specified as a template parameter rather than an 
   allocator constructor parameter.
   
   DebugAlloc tracks memory usage. At runtime, containers using this allocator
   can be queried to determine how much memory is being used by the container.
   
   InPlaceAlloc allows the caller to pass in their own chunk of memory for
   use by the container. InPlaceAlloc never frees memory; that's the responsibility
   of the caller. That means that InPlaceAlloc can be extremely fast, because
   each allocation only involves simple pointer arithmetic rather than
   searches of free lists.
   
   CustomHeapAlloc allows the caller to specify their own heap handle from
   HeapCreate. This allows the caller to use custom heaps for specific containers,
   or use heaps that don't do thread synchronization in the case where the
   container is only used in a single thread.

   PoolAlloc uses the TinyObjAllocator to allocate small blocks of memory very
   quickly. TinyObjAllocator has an array of fixed size allocators that it
   can allocate and free from with just a few instructions. This allocator is
   less flexible since it grabs memory at startup and never releases it, but
   it can be substantially faster. The TinyObjAllocator can also be used as
   a class allocator.
