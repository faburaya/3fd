//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#ifndef UTILS_MEMORY_H // header guard
#define UTILS_MEMORY_H

#include <3fd/core/exceptions.h>

#include <array>
#include <cinttypes>
#include <map>
#include <memory>
#include <queue>
#include <stack>
#include <vector>

#ifdef _3FD_HAS_STLOPTIMALLOC
#   include <memory_resource>
#endif

namespace _3fd
{
namespace utils
{
    /////////////////////////////////
    // Memory Allocation Utilities
    /////////////////////////////////

#ifdef _3FD_HAS_STLOPTIMALLOC

    /* t_maxBlocksPerChunk: The maximum number of blocks that will be allocated at once from the
     * upstream std::pmr::memory_resource to replenish the pool. If the value is zero or is greater than
     * an implementation-defined limit, that limit is used instead. The STL implementation may choose to
     * use a smaller value than is specified in this field and may use different values for different pools.
     *
     * t_bkSizeBytesThresholdHint: The largest allocation size (in bytes) that is required to be fulfilled
     * using the pooling mechanism. Attempts to allocate a single block larger than this threshold will be
     * allocated directly from the upstream std::pmr::memory_resource. If largest_required_pool_block is zero
     * or is greater than an implementation-defined limit, that limit is used instead. The implementation may
     * choose a pass-through threshold larger than specified in this field.
     */

    /// <summary>
    /// Creates a unique STL memory pool (thread-safe) for each template instantiation.
    /// </summary>
    template <typename Type, size_t t_maxBlocksPerChunk, size_t t_bkSizeBytesThresholdHint>
    std::pmr::memory_resource &GetStlOptimizedSyncMemPool() noexcept
    {
        static std::pmr::synchronized_pool_resource memoryPool{
            std::pmr::pool_options{ t_maxBlocksPerChunk, t_bkSizeBytesThresholdHint }
        };

        return memoryPool;
    };

    /// <summary>
    /// Creates a unique STL memory pool (NOT thread-safe, faster) for each template instantiation.
    /// </summary>
    template <typename Type, size_t t_maxBlocksPerChunk, size_t t_bkSizeBytesThresholdHint>
    std::pmr::memory_resource &GetStlOptimizedUnsyncMemPool() noexcept
    {
        static std::pmr::unsynchronized_pool_resource memoryPool{
            std::pmr::pool_options{ t_maxBlocksPerChunk, t_bkSizeBytesThresholdHint }
        };

        return memoryPool;
    };

    /// <summary>
    /// Base for minimal STL allocator relying on C++17 STL memory pools.
    /// </summary>
    template <typename Type, bool t_threadSafe>
    class StlOptimizedAllocatorBase
    {
    private:

        // Guess a good size for pool memory chunk in number of blocks
        static constexpr size_t GuessNumMemBlocksPerChunk(size_t blockSizeBytes)
        {
            /* Use a chunk of 16 KB, which is 4x the amount of memory in a memory page
            fetched from physical memory. That should render 87.5% probability of getting
            a page entirely inside the chunk when fetching one of its objects, which is
            what I am looking for here: cache efficiency. */
            return 16424 / blockSizeBytes;
        }

        // Gets the memory pools that serves this allocator
        std::pmr::memory_resource &GetMemoryPool() const noexcept
        {
            constexpr auto numBlocksPerChunk = GuessNumMemBlocksPerChunk(sizeof(Type));
            constexpr auto blockSizeThresholdHint = sizeof(Type);

            if (!t_threadSafe)
            {
                return GetStlOptimizedUnsyncMemPool<Type, numBlocksPerChunk, blockSizeThresholdHint>();
            }
            else
            {
                return GetStlOptimizedSyncMemPool<Type, numBlocksPerChunk, blockSizeThresholdHint>();
            }
        }

    public:

        template<typename OtherType>
        bool IsEqualTo(const StlOptimizedAllocatorBase<OtherType, t_threadSafe> &ob) const noexcept
        {
            // instances might not be equivalent!
            return &(this->GetMemoryPool()) == &(ob.GetMemoryPool());
        }

        // allocates blocks of memory
        Type *allocate(const size_t numBlocks) const
        {
            if (numBlocks != 0)
            {
                return static_cast<Type *> (
                    GetMemoryPool().allocate(numBlocks * sizeof(Type))
                );
            }

            return nullptr;
        }

        // deallocates blocks of memory
        void deallocate(Type * const ptr, size_t numBlocks) const noexcept
        {
            GetMemoryPool().deallocate(ptr, numBlocks * sizeof(Type));
        }
    };

    /// <summary>
    /// Implements a minimal STL allocator (NOT thread-safe) relying on C++17 STL memory pools.
    /// </summary>
    template <typename Type>
    class StlOptimizedUnsafeAllocator : public StlOptimizedAllocatorBase<Type, false>
    {
    public:

        typedef Type value_type;

        // ctor not required by STL
        StlOptimizedUnsafeAllocator() noexcept {}

        // converting copy constructor (no-op because nothing is copied)
        template<typename OtherType>
        StlOptimizedUnsafeAllocator(const StlOptimizedUnsafeAllocator<OtherType> &) noexcept {}

        template<typename OtherType>
        bool operator==(const StlOptimizedUnsafeAllocator<OtherType> &that) const noexcept
        {
            return this->IsEqualTo(that);
        }

        template<typename OtherType>
        bool operator!=(const StlOptimizedUnsafeAllocator<OtherType> &that) const noexcept
        {
            return !(*this == that);
        }
    };

    /// <summary>
    /// Implements a minimal STL allocator (thread-safe) relying on C++17 STL memory pools.
    /// </summary>
    template <typename Type>
    class StlOptimizedAllocator : public StlOptimizedAllocatorBase<Type, true>
    {
    public:

        typedef Type value_type;

        // ctor not required by STL
        StlOptimizedAllocator() noexcept {}

        // converting copy constructor (no-op because nothing is copied)
        template<typename OtherType>
        StlOptimizedAllocator(const StlOptimizedAllocator<OtherType> &) noexcept {}

        template<typename OtherType>
        bool operator==(const StlOptimizedAllocator<OtherType> &that) const noexcept
        {
            return this->IsEqualTo(that);
        }

        template<typename OtherType>
        bool operator!=(const StlOptimizedAllocator<OtherType> &that) const noexcept
        {
            return !(*this == that);
        }
    };
#endif // _3FD_HAS_STLOPTIMALLOC

    /// <summary>
    /// Provides uninitialized and contiguous memory.
    /// There is a limit with magnitude of megabytes, which is enough if you take into consideration
    /// that <see cref="DynamicMemPool"> will use several instances of this class when it needs more memory.
    /// The pool was designed for single-thread access.
    /// </summary>
    class MemoryPool
    {
    private:

        void *m_baseAddr,
             *m_nextAddr,
             *m_end;

        const uint16_t m_blockSize;

        /// <summary>
        /// Keeps available memory addresses stored as distance in number of blocks
        /// from the base address. Because the offset is a 16 bit unsigned integer,
        /// this imposes a practical limit of aproximately 64k blocks to the pool.
        /// </summary>
        std::stack<uint16_t> m_availAddrsAsBlockIndex;

    public:

        MemoryPool(uint16_t numBlocks, uint16_t blockSize);

        MemoryPool(const MemoryPool &) = delete;

        MemoryPool(MemoryPool &&ob) noexcept;

        ~MemoryPool();

        size_t GetNumBlocks() const noexcept;

        void *GetBaseAddress() const noexcept;

        bool Contains(void *addr) const noexcept;

        bool IsFull() const noexcept;

        bool IsEmpty() const noexcept;

        void *GetFreeBlock() noexcept;

        void ReturnBlock(void *addr);
    };

    /// <summary>
    /// A template class for a memory pool that expands dynamically.
    /// The pool was designed for SINGLE-THREAD access.
    /// </summary>
    class DynamicMemPool
    {
    private:

        const float m_growingFactor;
        const uint16_t m_blockSize;
        const uint16_t m_initialSize;

#ifdef _3FD_HAS_STL_OPTIMALLOC
        typedef std::map<void *, MemoryPool, std::less<void *>,
            StlOptimizedUnsafeAllocator<std::map<void *, MemoryPool>::value_type>> MapOfMemoryPools;
#else
        typedef std::map<void *, MemoryPool> MapOfMemoryPools;
#endif
        MapOfMemoryPools m_memPools;
        std::queue<MemoryPool *> m_availableMemPools;

    public:

        DynamicMemPool(uint16_t initialSize,
                       uint16_t blockSize,
                       float growingFactor);

		DynamicMemPool(const DynamicMemPool &) = delete;

        void *GetFreeBlock();

        void ReturnBlock(void *object);

        void Shrink();
    };

}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
