#include "stdafx.h"
#include "gc_memorydigraph.h"
#include "gc_memblock.h"
#include "configuration.h"

#include <cassert>

namespace _3fd
{
	namespace memory
	{
		using core::AppConfig;

		/// <summary>
		/// Initializes a new instance of the <see cref="MemoryDigraph"/> class.
		/// </summary>
		MemoryDigraph::MemoryDigraph() :
			m_memBlocksPool(AppConfig::GetSettings().framework.gc.memBlocksMemPool.initialSize,
							sizeof(MemBlock),
							AppConfig::GetSettings().framework.gc.memBlocksMemPool.growingFactor)
		{
			MemBlock::SetMemoryPool(m_memBlocksPool);
		}

		/// <summary>
		/// Shrinks the pool of <see cref="MemBlock"/> objects.
		/// </summary>
		void MemoryDigraph::ShrinkObjectPool()
		{
			m_memBlocksPool.Shrink();
		}

		/// <summary>
		/// Adds a new vertex to the graph.
		/// </summary>
		/// <param name="memAddr">The memory address represented by the new vertex.</param>
		/// <param name="blockSize">Size of the represented memory block.</param>
		/// <param name="freeMemCallback">The callback that frees the memory block.</param>
		MemAddrContainer *MemoryDigraph::AddVertex(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback)
		{
			auto vtx = new MemBlock(memAddr, blockSize, freeMemCallback);
			m_vertices.insert(vtx);
			return vtx;
		}

		/// <summary>
		/// Removes a given vertex.
		/// </summary>
		/// <param name="vtx">The vertex to remove.</param>
		/// <param name="allowDestruction">Whether the pointed object should have its destructor invoked.
		/// The destruction must not be allowed when the object construction has failed due to a thrown exception.</param>
		/// <remarks>
		/// The resources allocated to the vertex and its represented memory block will be released.
		/// </remarks>
		void MemoryDigraph::RemoveVertex(MemAddrContainer *vtx, bool allowDestruction)
		{
			auto iter = m_vertices.find(vtx);
			_ASSERTE(m_vertices.end() != iter); // cannot handle removal of unexistent vertex
			m_vertices.erase(iter);

			auto memBlock = static_cast<MemBlock *> (vtx);
			memBlock->Free(allowDestruction);
			delete memBlock; // ATTENTION: destructor is not virtual, so the type must be right!
		}

		/// <summary>
		/// Gets the vertex representing a given memory address.
		/// </summary>
		/// <param name="blockMemAddr">The given memory address.</param>
		/// <returns>The vertex representing the given memory address.</returns>
		MemAddrContainer * MemoryDigraph::GetVertex(void *blockMemAddr) const
		{
			MemAddrContainer key(blockMemAddr);
			auto iter = m_vertices.find(&key);
			if (m_vertices.end() != iter)
				return *iter;
			else
				return nullptr;
		}

		/// <summary>
		/// Gets the regular vertex which represents a memory block containing a given address.
		/// </summary>
		/// <param name="addr">The address for which a container will looked.</param>
		/// <returns>The vertex representing the memory block which contains the given address, if existent. Otherwise, a <c>nullptr</c>.</returns>
		MemAddrContainer * MemoryDigraph::GetContainerVertex(void *addr) const
		{
			if (!m_vertices.empty())
			{
				MemAddrContainer key(addr);
				auto iter = m_vertices.lower_bound(&key);

				if (m_vertices.end() != iter)
				{
					auto vtx = static_cast<MemBlock *> (*iter);
					if (vtx->Contains(addr))
						return vtx;
					
					if (m_vertices.begin() != iter)
						--iter;
				}
				else
					--iter;

				auto vtx = static_cast<MemBlock *> (*iter);
				if (vtx->Contains(addr))
					return vtx;
			}

			return nullptr;
		}


		/// <summary>
		/// Adds an edge that goes from a root vertex to a regular vertex.
		/// </summary>
		/// <param name="vtxRootFrom">The address of the root vertex.</param>
		/// <param name="vtxRegularTo">The address of the memory block represented by the regular vertex.</param>
		void MemoryDigraph::AddEdge(void *vtxRootFrom, MemAddrContainer *vtxRegularTo)
		{
			_ASSERTE(vtxRegularTo != nullptr);
			auto receivingVtx = static_cast<MemBlock *> (vtxRegularTo);
			receivingVtx->ReceiveEdge(vtxRootFrom);
		}

		/// <summary>
		/// Adds an edge that goes from one regular vertex to another.
		/// </summary>
		/// <param name="originatorVtx">The vertex from which the edge comes.</param>
		/// <param name="receivingVtx">The vertex to which the edge goes.</param>
		void MemoryDigraph::AddEdge(MemAddrContainer *originatorVtx, MemAddrContainer *receivingVtx)
		{
			_ASSERTE(receivingVtx != nullptr);
			static_cast<MemBlock *> (receivingVtx)->ReceiveEdge(
				static_cast<MemBlock *> (originatorVtx)
			);
		}

		/// <summary>
		/// Removes an edge that goes from a root vertex to a regular vertex.
		/// </summary>
		/// <param name="vtxRootFrom">The address of the root vertex.</param>
		/// <param name="vtxRegularTo">The regular vertex.</param>
		/// <param name="allowDestruction">Whether the pointed object should have its destructor invoked just in case it is to be collected.
		/// The destruction must not be allowed when the object construction has failed due to a thrown exception.</param>
		/// <remarks>
		/// If the receiving vertex becomes unreachable as a result of this operation, the resources allocated to 
		/// this vertex and its represented memory block will be released.
		/// </remarks>
		void MemoryDigraph::RemoveEdge(void *vtxRootFrom, MemAddrContainer *vtxRegularTo, bool allowDestruction)
		{
			_ASSERTE(vtxRegularTo != nullptr);
			auto receivingVtx = static_cast<MemBlock *> (vtxRegularTo);
			receivingVtx->RemoveEdge(vtxRootFrom);

			// If the memory block became unreachable, free its resources and return the MemBlock object to the pool:
			if (!receivingVtx->IsReachable())
				RemoveVertex(receivingVtx, allowDestruction);
		}

		/// <summary>
		/// Removes an edge that goes from one regular vertex to another.
		/// </summary>
		/// <param name="originatorVtx">The vertex from which the edge comes.</param>
		/// <param name="receivingVtx">The vertex to which the edge goes.</param>
		/// <param name="allowDestruction">Whether the pointed object should have its destructor invoked just in case it is to be collected.
		/// The destruction must not be allowed when the object construction has failed due to a thrown exception.</param>
		/// <remarks>
		/// If the receiving vertex becomes unreachable as a result of this operation, the resources allocated to 
		/// this vertex and its represented memory block will be released.
		/// </remarks>
		void MemoryDigraph::RemoveEdge(MemAddrContainer *originatorVtx, MemAddrContainer *receivingVtx, bool allowDestruction)
		{
			auto memBlock = static_cast<MemBlock *> (receivingVtx);
			memBlock->RemoveEdge(static_cast<MemBlock *> (originatorVtx));

			// If the memory block became unreachable, free its resources and return the MemBlock object to the pool:
			if (!memBlock->IsReachable())
				RemoveVertex(memBlock, allowDestruction);
		}

	}// end of namespace memory
}// end of namespace _3fd
