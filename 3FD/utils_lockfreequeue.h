#ifndef UTILS_LOCKFREEQUEUE_H // header guard
#define UTILS_LOCKFREEQUEUE_H

#include <atomic>
#include <queue>

namespace _3fd
{
    namespace utils
    {
		/// <summary>
		/// Implements a lock free queue for multiple writers
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

				Element() {}

				Element(Type *entry)
					: value(entry) {}
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
			LockedQueue()
			{
			}

			/// <summary>
			/// Finalizes an instance of the <see cref="LockedQueue{Type}"/> class.
			/// </summary>
			~LockedQueue()
			{
			}

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

    }// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
