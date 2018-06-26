#ifndef UTILS_LOCKFREEQUEUE_H // header guard
#define UTILS_LOCKFREEQUEUE_H

#include "preprocessing.h"

#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <queue>

#ifdef _WIN32
#   ifdef _3FD_PLATFORM_WINRT
#       include "Interlockedapi.h"
#   else
#       include "Windows.h"
#   endif
#endif

namespace _3fd
{
namespace utils
{
    /// <summary>
    /// Implements a lock-free queue for multiple writers
    /// but a single consumer.
    /// </summary>
    template<typename Type>
    class LockFreeQueue
    {
    private:

        struct Element
        {
            std::atomic<Type *> value;
            std::atomic<Element *> next;

            Element(Type *entry = nullptr)
                : next(nullptr), value(entry) {}
        };

        std::atomic<Element *> m_head;
        std::atomic<Element *> m_tail;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="LockFreeQueue{Type}"/> class.
        /// The initialization of this instance is NOT THREAD-SAFE.
        /// </summary>
        LockFreeQueue()
        {
            auto emptyElem = new Element();
            m_tail.store(emptyElem, std::memory_order_relaxed);
            m_head.store(emptyElem, std::memory_order_release);
        }

		LockFreeQueue(const LockFreeQueue &) = delete;

        /// <summary>
        /// Finalizes an instance of the <see cref="LockFreeQueue{Type}"/> class.
        /// The destruction of this instance is NOT THREADSAFE.
        /// </summary>
        ~LockFreeQueue()
        {
            // Clears all the elements from the queue:
            auto tail = m_tail.load(std::memory_order_relaxed);

            do
            {
                auto value = tail->value.load(std::memory_order_relaxed);
                delete value;

                auto next = tail->next.load(std::memory_order_acquire);
                delete tail;
                tail = next;
            }
            while (tail != nullptr);
        }

        /// <summary>
        /// Adds a new entry to the queue head.
        /// </summary>
        /// <param name="entry">The entry to insert.</param>
        void Add(Type *entry)
        {
            // Create a new element and without locks, replace the head of the queue:
            auto newElem = new Element(entry);
            auto headBefore = m_head.exchange(newElem, std::memory_order_acq_rel);
            headBefore->next.store(newElem, std::memory_order_release);
        }

        /// <summary>
        /// Removes an entry from the tail of the queue.
        /// </summary>
        /// <returns>
        /// The value removed from the tail, or a null pointer when none found.
        /// </returns>
        Type *Remove()
        {
            while (true)
            {
                // Get the tail and the next in line:
                auto tail = m_tail.load(std::memory_order_relaxed);
                auto next = tail->next.load(std::memory_order_acquire);

                /* If the tail has a next element, it is not the
                head, hence consume the value and move the tail: */
                if (next != nullptr)
                {
                    auto value = tail->value.load(std::memory_order_relaxed);
                    delete tail;
                    m_tail.store(next, std::memory_order_relaxed); // move tail

                    // this value can be null if already consumed before
                    if (value != nullptr)
                        return value;
                }
                /* Otherwise, if there is no next element, the tail
                is also the head. So keep it, but consume the value: */
                else
                {
                    // this can be null if already consumed before
                    return tail->value.exchange(nullptr, std::memory_order_relaxed);
                }
            }
        }

        /// <summary>
        /// Determines whether the queue is empty.
        /// </summary>
        /// <returns>
        /// <c>true</c> when the queue is empty, otherwise, <c>false</c>.
        /// </returns>
        bool IsEmpty() const
        {
            auto tail = m_tail.load(std::memory_order_relaxed);
            auto value = tail->value.load(std::memory_order_relaxed);
            auto head = m_head.load(std::memory_order_acquire);
            return tail == head && value == nullptr;
        }
    };

    /// <summary>
    /// Implements a locked queue in order to aid the testing of
    /// the lock-free implementation.
    /// </summary>
    template<typename Type>
    class LockedQueue
    {
    private:

        std::queue<Type *> m_queue;

        std::mutex m_writeAccessMutex;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="LockedQueue{Type}"/> class.
        /// </summary>
        LockedQueue() {}

		LockedQueue(const LockedQueue &) = delete;

        /// <summary>
        /// Finalizes an instance of the <see cref="LockedQueue{Type}"/> class.
        /// </summary>
        ~LockedQueue() {}

        /// <summary>
        /// Adds a new entry to the queue head.
        /// </summary>
        /// <param name="entry">The entry to insert.</param>
        void Add(Type *entry)
        {
            std::lock_guard<std::mutex> lock(m_writeAccessMutex);
            m_queue.push(entry);
        }

        /// <summary>
        /// Removes an entry from the tail of the queue.
        /// </summary>
        /// <returns>
        /// The value removed from the tail, or a null pointer when none found.
        /// </returns>
        Type *Remove()
        {
            std::lock_guard<std::mutex> lock(m_writeAccessMutex);

            if (!m_queue.empty())
            {
                auto value = m_queue.front();
                m_queue.pop();
                return value;
            }
                
            return nullptr;
        }
    };

#   ifdef _WIN32
    namespace Win32ApiWrappers
    {
        /// <summary>
        /// Implementation of lock-free queue that uses Win32 API.
        /// This is an alternative to the lock-free queue from Boost,
        /// which appears not to work in ARM architecture (Windows Phone hardware).
        /// </summary>
        template <typename ItemType> class LockFreeQueue
        {
        private:

            struct QueueItem
            {
                SLIST_ENTRY itemEntry;
                ItemType data;

                QueueItem(const ItemType &p_data)
                    : data(p_data) {}

                QueueItem(ItemType &&p_data)
                    : data(std::move(p_data)) {}
            };

            PSLIST_HEADER m_front;

            /// <summary>
            /// The count of items in the queue.
            /// This is not very reliable, because it does not get updated atomically
            /// along with the queue insertion, but it serves to provide a value very close
            /// to the real one, but understimating it.
            /// </summary>
            std::atomic<size_t> m_itemsCount;

            // Iterates the singly-linked list backwards in order to get its elements in chronological order
            static size_t IterateRecursiveForEach(QueueItem *front,
                                                  const std::function<void(ItemType &)> &callback)
            {
                if (front == nullptr)
                    return 0;

                auto nextFront = reinterpret_cast<QueueItem *> (front->itemEntry.Next);
                auto sizeRecursiveCalc = 1 + IterateRecursiveForEach(nextFront, callback);

                auto item = reinterpret_cast<QueueItem *> (front);
                callback(item->data);

                item->QueueItem::~QueueItem();
                _aligned_free(item);

                return sizeRecursiveCalc;
            }

        public:

            /// <summary>
            /// Initializes a new instance of the <see cref="LockFreeQueue"/> class.
            /// </summary>
            LockFreeQueue()
                : m_itemsCount(0)
            {
                m_front = (PSLIST_HEADER)_aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT);

                if (m_front == nullptr)
                    throw std::bad_alloc();

                InitializeSListHead(m_front);
            }

			// copy operation is forbidden
			LockFreeQueue(const LockFreeQueue &) = delete;

            /// <summary>
            /// Finalizes an instance of the <see cref="LockFreeQueue"/> class.
            /// </summary>
            ~LockFreeQueue()
            {
                // Upon destruction remove all items:
                ForEach([](const ItemType &) {});
                _aligned_free(m_front);
            }

            /// <summary>
            /// Adds the specified item to the front of the queue.
            /// </summary>
            /// <param name="item">The item to add (using copy semantics).</param>
            void Push(const ItemType &item)
            {
                auto ptr = _aligned_malloc(sizeof(QueueItem), MEMORY_ALLOCATION_ALIGNMENT);
                    
                if (ptr == nullptr)
                    throw std::bad_alloc();
                        
                auto newQueueItem = new (ptr) QueueItem(item);
                InterlockedPushEntrySList(m_front, &(newQueueItem->itemEntry));
                m_itemsCount.fetch_add(1ULL, std::memory_order_acq_rel);
            }

            /// <summary>
            /// Adds the specified item to the front of the queue.
            /// </summary>
            /// <param name="item">The item to add (using move semantics).</param>
            void Push(ItemType &&item)
            {
                auto ptr = _aligned_malloc(sizeof(QueueItem), MEMORY_ALLOCATION_ALIGNMENT);

                if (ptr == nullptr)
                    throw std::bad_alloc();

                auto newQueueItem = new (ptr) QueueItem(std::move(item));
                InterlockedPushEntrySList(m_front, &(newQueueItem->itemEntry));
                m_itemsCount.fetch_add(1ULL, std::memory_order_acq_rel);
            }

            /// <summary>
            /// Flushes the queue, then iterates over all the elements.
            /// </summary>
            /// <param name="callback">The callback to execute for each item.</param>
            /// <returns>How many items were flushed from the queue.</returns>
            size_t ForEach(const std::function<void(ItemType &)> &callback)
            {
                auto front = InterlockedFlushSList(m_front);
                size_t size = m_itemsCount.exchange(0ULL, std::memory_order_acq_rel);

                const size_t sizeThreshold(128);

                /* Use recursive iteration if the queue is short enough
                to guarantee that a stack overflow is not going to happen: */
                if(size < sizeThreshold)
                    return IterateRecursiveForEach(reinterpret_cast<QueueItem *> (front), callback);

                /* Use a vector as a stack, in order to take advantage
                of speed boost of accessing contiguous memory. */
                std::vector<QueueItem *> stack;
                stack.reserve(2 * sizeThreshold);

                // Revert the queue by reading it backwards into a stack:
                do
                {
                    auto item = reinterpret_cast<QueueItem *> (front);
                    stack.push_back(item);
                    front = item->itemEntry.Next;
                }
                while (front != nullptr);
                    
                // Now process the items in the correct order:
                std::for_each(stack.rbegin(), stack.rend(), [&callback](QueueItem *item)
                {
                    callback(item->data);
                    item->QueueItem::~QueueItem();
                    _aligned_free(item);
                });

                return stack.size();
            }
        };

    }// end of namespace Win32ApiWrappers
#   endif
}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
