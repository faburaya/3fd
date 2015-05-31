#include "stdafx.h"
#include "utils.h"

#include <cassert>
#include <sstream>

namespace _3fd
{
	namespace utils
	{
		////////////////////////////////////
		// DynamicMemPool Class
		////////////////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="DynamicMemPool"/> class.
		/// </summary>
		/// <param name="initialSize">The initial size.</param>
		/// <param name="blockSize">Size of the block.</param>
		/// <param name="increasingFactor">The factor for increasing size of the pool.</param>
		DynamicMemPool::DynamicMemPool(size_t initialSize, size_t blockSize, float increasingFactor) :
			m_initialSize(initialSize),
			m_blockSize(blockSize),
			m_increasingFactor(increasingFactor)
		{
			_ASSERTE(initialSize * blockSize > 0); // The object pool cannot start zero-sized
			_ASSERTE(increasingFactor > 0); // The increasing factor must be a positive number
		}

		/// <summary>
		/// Gets the free block.
		/// </summary>
		/// <returns></returns>
		void * DynamicMemPool::GetFreeBlock()
		{
			if (m_availableMemPools.empty() == false) // there are memory pools with available memory:
			{
				auto memPool = m_availableMemPools.front();
				auto addr = memPool->GetFreeBlock();

				if (addr != nullptr)
					return addr;
				else
				{
					m_availableMemPools.pop();
					return GetFreeBlock(); // tail recursion
				}
			}
			else // there are no memory available in the existent pools, so create a new one:
			{
				MemoryPool memPool(
					m_memPools.empty() ? m_initialSize : m_initialSize * m_increasingFactor,
					m_blockSize
				);

				auto addr = memPool.GetFreeBlock();

				auto &movedMemPool = m_memPools.insert(
					std::make_pair(addr, std::move(memPool))
					).first->second;

				m_availableMemPools.push(&movedMemPool); // make the new memory pool available

				return addr;
			}
		}

		/// <summary>
		/// Returns a block of memory.
		/// </summary>
		/// <param name="object">The address of the object to return.</param>
		void DynamicMemPool::ReturnBlock(void *object)
		{
			// Finds the corresponding memory pool
			auto iter = m_memPools.upper_bound(object);

			_ASSERTE(iter != m_memPools.begin()); // Cannot return a memory block which does not belong to the pool

			auto &memPool = (--iter)->second;

			_ASSERTE(memPool.Contains(object)); // Cannot return a memory block which does not belong to the pool

			// If the corresponding memory pool was empty:
			if (memPool.IsEmpty())
			{
				m_availableMemPools.push(&memPool); // mark the pool as 'available' (not empty / with available memory)

				// The empty memory pool might have already been in the queue (front element):
				if (m_availableMemPools.front() == &memPool)
					m_availableMemPools.pop();
			}

			memPool.ReturnBlock(object); // returns the memory to the pool
		}

		/// <summary>
		/// Shrinks the set of memory pools releasing the resources of the pools which are full.
		/// </summary>
		void DynamicMemPool::Shrink()
		{
			std::vector<MemoryPool *> toBeDeleted;

			toBeDeleted.reserve(m_availableMemPools.size());

			// Scan the queue and hold the pointers to the pool objects that are full:
			for (auto count = m_availableMemPools.size(); count > 0; --count)
			{
				auto memPool = m_availableMemPools.front();

				if (memPool->IsFull())
				{
					m_availableMemPools.pop();
					toBeDeleted.push_back(memPool);
				}
				else
				{
					m_availableMemPools.pop();
					m_availableMemPools.push(memPool);
				}
			}

			// Delete the pool objects that were found full:
			for (auto memPool : toBeDeleted)
			{
				auto iter = m_memPools.find(memPool->GetBaseAddress());
				m_memPools.erase(iter);
			}
		}

	} // end of namespace utils
} // end of namespace _3fd
