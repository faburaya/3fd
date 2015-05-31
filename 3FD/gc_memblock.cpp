#include "stdafx.h"
#include "gc_memblock.h"

namespace _3fd
{
	namespace memory
	{
		utils::DynamicMemPool * MemBlock::dynMemPool(nullptr);

		/// <summary>
		/// Sets the object pool that provides all the <see cref="MemoryBlock"/> instances.
		/// </summary>
		/// <param name="ob">The object pool to use.</param>
		void MemBlock::SetMemoryPool(utils::DynamicMemPool &ob)
		{
			dynMemPool = &ob;
		}

		/// <summary>
		/// Implements the operator new, to construct a new <see cref="MemoryBlock"/> instance from resources in the object pool.
		/// </summary>
		/// <param name="">Currently not used.</param>
		/// <returns>
		/// A new <see cref="MemoryBlock"/> instance retrieved from the object pool.
		/// </returns>
		void * MemBlock::operator new(size_t)
		{
			return dynMemPool->GetFreeBlock();
		}

		/// <summary>
		/// Implements the operator delete, to destroy a <see cref="MemoryBlock"/> instance returning itself to the object pool.
		/// </summary>
		/// <param name="ptr">The address of the object to delete.</param>
		void MemBlock::operator delete(void *ptr)
		{
			dynMemPool->ReturnBlock(ptr);
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="MemBlock"/> class.
		/// </summary>
		/// <param name="memAddr">The address of the memory block.</param>
		/// <param name="blockSize">Size of the block.</param>
		/// <param name="freeMemCallback">The callback that frees the memory block.</param>
		MemBlock::MemBlock(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback) :
			MemAddrContainer(memAddr), 
			m_blockSize(blockSize), 
			m_freeMemCallback(freeMemCallback)
		{}

		/// <summary>
		/// Finalizes an instance of the <see cref="MemBlock"/> class.
		/// </summary>
		MemBlock::~MemBlock()
		{
			RemoveAllEdges();
		}

		/// <summary>
		/// Determines whether this memory block contains the specified memory address.
		/// </summary>
		/// <param name="someAddr">The memory address to test.</param>
		/// <returns>Whether this memory block contains the specified memory address</returns>
		bool MemBlock::Contains(void *someAddr) const
		{
			return someAddr >= memoryAddress().Get()
				&& someAddr < (void *)((uintptr_t)memoryAddress().Get() + m_blockSize);
		}

		/// <summary>
		/// Determines whether this instance is reachable.
		/// </summary>
		/// <returns></returns>
		bool MemBlock::IsReachable() const
		{
			if (HasRootEdge())
				return true;
			
			// Avoid cyclic paths by marking vertices
			memoryAddress().Mark(true);

			// Check if any receiving edge is a path coming from a reaching root vertex:
			bool isUnreachableViaReceivingEdges = 
				ForEachRegularEdgeWhile([this](const Vertex &vtx)
				{
					auto &memBlock = static_cast<const MemBlock &> (vtx);

					if (!memBlock.memoryAddress().isMarked())
						return !memBlock.IsReachable();
					else
						return true;
				});

			// unmark this vertex so as to leave the graph inaltered at the end of the algorithm
			memoryAddress().Mark(false);

			return !isUnreachableViaReceivingEdges;
		}

		/// <summary>
		/// Frees the specified destroy.
		/// </summary>
		/// <param name="destroy">if set to <c>true</c> [destroy].</param>
		void MemBlock::Free(bool destroy)
		{
			(*m_freeMemCallback)(memoryAddress().Get(), destroy);
		}

	}// end of namespace memory
}// end of namespace _3fd
