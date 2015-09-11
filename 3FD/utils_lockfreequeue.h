#ifndef UTILS_LOCKFREEQUEUE_H // header guard
#define UTILS_LOCKFREEQUEUE_H

#include <atomic>

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
                std::atomic<const Type *> value;
                std::atomic<Element *> next;
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
                auto emptyElem = new Element{};
                m_head.store(emptyElem, std::memory_order_relaxed);
                m_tail.store(emptyElem, std::memory_order_relaxed);
            }

			/// <summary>
			/// Finalizes an instance of the <see cref="LockFreeQueue{Type}"/> class.
			/// The destruction of this instance is NOT THREADSAFE.
			/// </summary>
			~LockFreeQueue()
            {
				// Clears all the elements from the queue:
				auto tail = m_tail.load(std::memory_order_acquire);

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
			void Add(const Type *entry)
            {
                // Create a new element:
				auto newElem = new Element{ 0 };
                newElem->value.store(entry, std::memory_order_relaxed);

                // Without locks, replace the head of the queue:
                auto headBefore = m_head.exchange(newElem, std::memory_order_acq_rel);
                headBefore->next.store(newElem, std::memory_order_release);
            }

			/// <summary>
			/// Removes an entry from the tail of the queue.
			/// </summary>
			/// <returns>The value found in the tail, or a null pointer if none.</returns>
			const Type *Remove()
            {
                // Get the tail and the next in line:
                auto tail = m_tail.load(std::memory_order_consume);
                auto next = tail->next.load(std::memory_order_relaxed);

				/* If the tail has a next element, it is not the 
				head, hence consume the value and move the tail: */
				if (next != nullptr)
				{
					m_tail.store(next, std::memory_order_relaxed);
					return tail->value.load(std::memory_order_relaxed);
				}
                /* Otherwise, if there is no next element, the tail
				is also the head. So keep it, but consume the value: */
				else
                {
					auto value = tail->value.exchange(nullptr, std::memory_order_relaxed);
					return value; // this can be null if already consumed before
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
				// Get the head and its value:
				auto head = m_head.load(std::memory_order_consume);
				auto value = tail->value.load(std::memory_order_relaxed);

				// The only scenario where the head has no value is when the queue is empty:
				return value == nullptr;
			}
        };

    }// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
