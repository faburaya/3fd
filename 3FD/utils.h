#ifndef UTILS_H // header guard
#define UTILS_H

#include "base.h"
#include "exceptions.h"

#include <map>
#include <stack>
#include <queue>
#include <vector>
#include <memory>
#include <mutex>
#include <future>
#include <condition_variable>
#include <functional>
#include <cinttypes>

#ifdef _3FD_PLATFORM_WINRT
#   include "utils_lockfreequeue.h"
#endif

#ifdef _MSC_VER
#   include <allocators>
#endif

namespace _3fd
{
namespace utils
{
    /////////////////////////////////
    // Memory Allocation Utilities
    /////////////////////////////////

#ifdef _MSC_VER
    _ALLOCATOR_DECL(CACHE_CHUNKLIST, stdext::allocators::sync_none, unsafe_pool_allocator);
#endif

    /// <summary>
    /// Provides uninitialized and contiguous memory.
    /// There is a limit 4 GB, which is far more than enough.
    /// The pool was designed for single-thread access.
    /// </summary>
    class MemoryPool : notcopiable
    {
    private:

        void    *m_baseAddr,
                *m_nextAddr,
                *m_end;

        const uint32_t m_blockSize;

        /// <summary>
        /// Keeps available memory blocks as offset integers to the base address.
        /// Because the offset is a 32 bit integer, this imposes the practical limit of 4 GB to the pool.
        /// </summary>
        std::stack<uint32_t> m_availableAddrsAsOffset;

    public:

        MemoryPool(uint32_t numBlocks, uint32_t blockSize);

        MemoryPool(MemoryPool &&ob);

        ~MemoryPool();

        size_t GetNumBlocks() const NOEXCEPT;

        void *GetBaseAddress() const NOEXCEPT;

        bool Contains(void *addr) const NOEXCEPT;

        bool IsFull() const NOEXCEPT;

        bool IsEmpty() const NOEXCEPT;

        void *GetFreeBlock() NOEXCEPT;

        void ReturnBlock(void *addr);
    };

    /// <summary>
    /// A template class for a memory pool that expands dynamically.
    /// The pool was designed for single-thread access.
    /// </summary>
    class DynamicMemPool : notcopiable
    {
    private:

        const float        m_increasingFactor;
        const size_t    m_initialSize,
                        m_blockSize;

        std::map<void *, MemoryPool>    m_memPools;
        std::queue<MemoryPool *>        m_availableMemPools;

    public:

        DynamicMemPool(size_t initialSize,
            size_t blockSize,
            float increasingFactor);

        void *GetFreeBlock();

        void ReturnBlock(void *object);

        void Shrink();
    };

    ////////////////////////////////////////////////
    // Multi-thread and Synchronization Utilities
    ////////////////////////////////////////////////

    /// <summary>
    /// Implements an event for thread synchronization making 
    /// use of a lightweight mutex and a condition variable.
    /// </summary>
    class Event
    {
    private:

        std::mutex m_mutex;
        std::condition_variable    m_condition;
        bool m_flag;

    public:

        Event();

        void Signalize();

        void Wait(const std::function<bool()> &predicate);

        bool WaitFor(unsigned long millisecs);
    };

    /// <summary>
    /// Provides helpers for asynchronous callbacks.
    /// </summary>
    class Asynchronous
    {
    public:

        static void InvokeAndLeave(const std::function<void()> &callback);
    };

#   ifdef _WIN32

    /// <summary>
    /// Alternative implementation for std::shared_mutex from C++14 Std library.
    /// </summary>
    class shared_mutex : notcopiable
    {
    private:

        enum LockType : short { None, Shared, Exclusive };

        SRWLOCK m_srwLockHandle;
        LockType m_curLockType;

    public:

        shared_mutex();
        ~shared_mutex();

        void lock_shared();
        void unlock_shared();

        void lock();
        void unlock();
    };

#   endif

}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
