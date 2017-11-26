#include "stdafx.h"
#include "utils.h"
#include "exceptions.h"

#include <sstream>
#include <cstring>
#include <cassert>

namespace _3fd
{
	namespace utils
	{
		////////////////////////////////
		// MemoryPool Class
		////////////////////////////////

		/// <summary>
		/// Perform allocation of aligned memory for an array, initialized to zero.
		/// </summary>
		/// <param name="alignment">The alignment.</param>
		/// <param name="numBlocks">How many blocks to allocate.</param>
		/// <param name="blockSize">The amount of bytes to allocate per block.</param>
		/// <returns>
		/// A pointer to the allocated memory.
		/// </returns>
		/// <remarks>
		/// The product 'blockSize * numBlocks' has to be a multiple of the alignment.
		/// </remarks>
		static void *aligned_calloc(size_t alignment, size_t numBlocks, size_t blockSize)
		{
			auto nBytes = numBlocks * blockSize;
#	ifdef _WIN32
			auto ptr = _aligned_malloc(nBytes, alignment);
#	else
			auto ptr = aligned_alloc(alignment, nBytes);
#	endif
			if (ptr != nullptr)
			{
				memset(ptr, 0x0, nBytes);
				return ptr;
			}
			else
				throw core::AppException<std::runtime_error>("Failed to allocate memory for memory pool");
		}

		/// <summary>
		/// Memories the pool.
		/// </summary>
		/// <param name="numBlocks">The number blocks.</param>
		/// <param name="blockSize">Size of the block.</param>
		MemoryPool::MemoryPool(uint32_t numBlocks, uint32_t blockSize)
		try :
			m_baseAddr(nullptr),
			m_nextAddr(nullptr),
			m_end(nullptr),
			m_blockSize(blockSize),
			m_availableAddrsAsOffset()
		{
			_ASSERTE(numBlocks * blockSize > 0); // Cannot handle a null value as the amount of memory

			/* Allocation aligned in 4 bytes guarantees the addresses will always have
			the 2 least significant bit unused. This is explored in the GC implementation. */
			m_baseAddr = aligned_calloc(4, numBlocks, blockSize);
			m_end = reinterpret_cast<void *> (reinterpret_cast<size_t> (m_baseAddr) +numBlocks * blockSize);
			m_nextAddr = m_baseAddr;
		}
		catch (core::IAppException &)
		{
			throw; // just forward errors known to have been previously handled
		}
		catch (std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when creating memory pool: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="MemoryPool"/> class.
		/// </summary>
		/// <param name="ob">The object whose resource will be stolen.</param>
		MemoryPool::MemoryPool(MemoryPool &&ob) :
			m_baseAddr(ob.m_baseAddr),
			m_nextAddr(ob.m_nextAddr),
			m_end(ob.m_end),
			m_blockSize(ob.m_blockSize),
			m_availableAddrsAsOffset(std::move(ob.m_availableAddrsAsOffset))
		{
			ob.m_baseAddr = ob.m_nextAddr = ob.m_end = nullptr;
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="MemoryPool"/> class.
		/// </summary>
		MemoryPool::~MemoryPool()
		{
			// Memory pool destruction was reached not having all its memory returned
			_ASSERTE(m_availableAddrsAsOffset.size() ==
				(reinterpret_cast<uintptr_t> (m_nextAddr) - reinterpret_cast<uintptr_t> (m_baseAddr)) / m_blockSize);

			if (m_baseAddr != nullptr)
#	ifdef _WIN32
				_aligned_free(m_baseAddr);
#	else
				free(m_baseAddr);
#	endif
		}

		/// <summary>
		/// Gets the number of memory blocks in the pool.
		/// </summary>
		/// <returns>How many memory blocks, with the size set in pool construction, are there in the pool.</returns>
		size_t MemoryPool::GetNumBlocks() const NOEXCEPT
		{
			return (reinterpret_cast<uintptr_t> (m_end) - reinterpret_cast<uintptr_t> (m_baseAddr)) / m_blockSize;
		}

		/// <summary>
		/// Gets the base memory address of the pool.
		/// </summary>
		/// <returns>The memory address of the chunk allocated by this pool.</returns>
		void * MemoryPool::GetBaseAddress() const NOEXCEPT
		{
			return m_baseAddr;
		}

		/// <summary>
		/// Assess whether the memory pool contains the given memory address.
		/// </summary>
		/// <param name="addr">The memory address to test.</param>
		/// <returns>
		/// <c>true</c> if the given address belongs to the memory chunk in the pool, otherwise, <c>false</c>.
		/// </returns>
		bool MemoryPool::Contains(void *addr) const NOEXCEPT
		{
			return addr >= m_baseAddr && addr < m_end;
		}

		/// <summary>
		/// Determines whether the pool is full.
		/// </summary>
		/// <returns><c>true</c> if all the memory is available, otherwise, <c>false</c>.</returns>
		bool MemoryPool::IsFull() const NOEXCEPT
		{
			return m_availableAddrsAsOffset.size() == GetNumBlocks() || m_nextAddr == m_baseAddr;
		}

		/// <summary>
		/// Determines whether the pool is empty.
		/// </summary>
		/// <returns><c>true</c> if the pool has no memory available, otherwise, <c>false</c>.</returns>
		bool MemoryPool::IsEmpty() const NOEXCEPT
		{
			return m_nextAddr == m_end && m_availableAddrsAsOffset.empty();
		}

		/// <summary>
		/// Gets a free block of memory.
		/// </summary>
		/// <returns></returns>
		void * MemoryPool::GetFreeBlock() NOEXCEPT
		{
			if (!m_availableAddrsAsOffset.empty())
			{
				auto addr = reinterpret_cast<void *> (
					reinterpret_cast<uintptr_t> (m_baseAddr) + m_availableAddrsAsOffset.top()
				);
				m_availableAddrsAsOffset.pop();
				return addr;
			}
			else if (m_nextAddr < m_end)
			{
				auto addr = m_nextAddr;
				m_nextAddr = reinterpret_cast<void *> (reinterpret_cast<size_t> (m_nextAddr) + m_blockSize);
				return addr;
			}
			else
				return nullptr;
		}

		/// <summary>
		/// Returns a block of memory to the pool.
		/// </summary>
		/// <param name="addr">The address of the block to return.</param>
		void MemoryPool::ReturnBlock(void *addr)
		{
			_ASSERTE(Contains(addr)); // Cannot return a memory block which does not belongs to the memory pool
			m_availableAddrsAsOffset.push(
				reinterpret_cast<uintptr_t> (addr) - reinterpret_cast<uintptr_t> (m_baseAddr)
			);
		}

	} // end of namespace utils
} // end of namespace _3fd
