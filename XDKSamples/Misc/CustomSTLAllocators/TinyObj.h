////////////////////////////////////////////////////////////////////////////////
// TinyObj.h
//
// Desc: small object allocator customized. Allocates small blocks faster
// than new/delete. Requires customization to reserve the appropriate
// amount of memory.
//
// Example Usage:
//
//     TinyObjAllocator& allocator = GetTinyObjAllocator();
//     void* ptr = allocator.Allocate(10);
//
//     Usually used via the PoolAlloc STL allocator or by a class overload
// of operator new/delete.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef TINYOBJ_H
#define TINYOBJ_H

class TinyObjAllocator
{
public:
    // Customize the constructor using results from DumpStats() in order
    // to allocate precisely the right amount of memory.
    TinyObjAllocator();
#ifdef  _DEBUG
    // Dump statistics on how many objects of each size were
    // allocated - essential for tuning how much space should
    // be reserved.
    void DumpStats() const;
#endif
    ~TinyObjAllocator();

    // Try to allocate a block of the requested size. If the size is larger
    // than this allocator can handle, fall back to operator new.
    void* Allocate(size_t size);
    // Free the block. Note that the size is required for this to
    // function properly, so that it knows which allocator to use -
    // possibly including operator delete.
    void Deallocate(void* ptr, size_t size);

private:
    // Private and unimplemented - no copying allowed.
    TinyObjAllocator(const TinyObjAllocator& rhs);
    TinyObjAllocator& operator=(const TinyObjAllocator& rhs);

    // Fixed Block Allocator is an allocator that can allocate
    // blocks of exactly one size. It maintains a linked list of
    // free blocks and doles them out on demand. If it runs out
    // of memory it will return zero, and will crash on subsequent
    // allocation requests - be careful!
    class FixedBlockAllocator
    {
    public:
        FixedBlockAllocator();
        ~FixedBlockAllocator();
        void Init(size_t blockSize, size_t numBlocks);
        void* Allocate();
        void Deallocate(void* ptr);

    private:
        // Private and unimplemented - no copying allowed.
        FixedBlockAllocator(const FixedBlockAllocator& rhs);
        FixedBlockAllocator& operator=(const FixedBlockAllocator& rhs);

        // Pointed to linked list of free blocks.
        char*   m_nodes;
#ifdef  _DEBUG
        int     m_allocated;
        int     m_maximumAllocated;
        // It's handy to track these for easier debugging.
        size_t  m_blockSize;
        size_t  m_numBlocks;
        friend TinyObjAllocator;
#endif
    };

    // Ultimately this code should handle having granularity set to 1, 2, or 4. Right
    // now it only handles 4 because the links are that size.
    static const int kGranularity = 4;
    static const int kNumBlockSizes = 20;
    // Actual allocators, each used for several sizes.
    FixedBlockAllocator m_allocators[kNumBlockSizes];
    // Pointers to allocators, used for faster finding of the correct
    // allocator. Just index this array with the block size.
    FixedBlockAllocator* m_pAllocators[kNumBlockSizes * kGranularity + 1];
};

// Function to get the global tiny object allocator. The object must be stored
// in a function to guarantee order of initialization. Cache the result for
// best performance.
TinyObjAllocator& GetTinyObjAllocator();

#endif
