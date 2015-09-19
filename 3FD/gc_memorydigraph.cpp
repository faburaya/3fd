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
		/// Gets the vertex representing a given memory address.
		/// </summary>
		/// <param name="memAddr">The given memory address.</param>
		/// <returns>The vertex representing the given memory address.</returns>
		MemBlock * MemoryDigraph::GetVertex(void *memAddr) const
		{
			MemAddrContainer key(memAddr);
			auto iter = m_vertices.find(&key);

			if (m_vertices.end() != iter)
				return static_cast<MemBlock *> (*iter);
			else
				return nullptr;
		}

		/// <summary>
		/// Gets the regular vertex which represents a memory block containing a given address.
		/// </summary>
		/// <param name="addr">The address for which a container will looked.</param>
		/// <returns>
		/// The vertex representing the memory block which contains the given address,
		/// if existent. Otherwise, a <c>nullptr</c>.
		/// </returns>
		MemBlock * MemoryDigraph::GetContainerVertex(void *addr) const
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
		/// Adds a new vertex to the graph.
		/// </summary>
		/// <param name="memAddr">The memory address represented by the new vertex.</param>
		/// <param name="blockSize">Size of the represented memory block.</param>
		/// <param name="freeMemCallback">The callback that frees the memory block.</param>
		/// <returns>
		/// The <see cref="MemBlock" /> object representing the GC allocated piece of memory,
		/// which is the added vertex.
		/// </returns>
		MemBlock * MemoryDigraph::AddVertex(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback)
		{
			auto vtx = new MemBlock(memAddr, blockSize, freeMemCallback);
			auto insertSucceded = m_vertices.insert(vtx).second;
			_ASSERTE(insertSucceded); // insertion should always succeed because a vertex cannot be added twice
			return vtx;
		}

		/// <summary>
		/// Removes a given vertex from the graph.
		/// </summary>
		/// <param name="vtx">
		/// The vertex to remove, which represents a memory block.
		/// </param>
		/// <param name="allowDestruction">
		/// Whether the pointed object should have its destructor invoked just
		/// in case it is to be collected. The destruction must not be allowed
		/// when the object construction has failed due to a thrown exception.
		/// </param>
		/// <remarks>
		/// If the receiving vertex becomes unreachable as a result of this
		/// operation, the resources allocated to this vertex and its represented
		/// piece of memory will be released.
		/// </remarks>
		void MemoryDigraph::RemoveVertex(MemBlock *vtx, bool allowDestruction)
		{
			auto iter = m_vertices.find(vtx);
			_ASSERTE(m_vertices.end() != iter); // cannot handle removal of unexistent vertex
			m_vertices.erase(iter);

			// for each vertex hit by an edge started from the one being removed...
			vtx->ForEachStartingEdge(
				[vtx](Vertex &hitBy)
				{
					hitBy.RemoveReceivingEdge(vtx); // ... remove such edge
				}
			);

			// release resources:
			vtx->Free(allowDestruction);
			delete vtx;
		}

		/// <summary>
		/// Adds an edge that goes from a root vertex to a regular vertex.
		/// </summary>
		/// <param name="fromRoot">The address of the root vertex.</param>
		/// <param name="toAddr">The address to which the edge goes.</param>
		void MemoryDigraph::AddRootEdge(void *fromRoot, void *toAddr)
		{
			auto vtxRegularTo = GetVertex(toAddr);

			/* by the time the connection is to be set, the vertex representing
			the pointed memory block must already exist in the graph */
			_ASSERTE(vtxRegularTo != nullptr);

			vtxRegularTo->ReceiveEdge(fromRoot);
		}

		/// <summary>
		/// Adds an edge that goes from one regular vertex to another.
		/// </summary>
		/// <param name="fromAddr">The address from which the edge comes.</param>
		/// <param name="toAddr">The address to which the edge goes.</param>
		void MemoryDigraph::AddRegularEdge(void *fromAddr, void *toAddr)
		{
			auto receivingVtx = GetVertex(toAddr);

			/* The receiving vertex cannot be found if this edge closes a cycle
			pointing to a vertex which represents an already collected piece of
			memory. In such case, do not create this edge: */
			if (receivingVtx == nullptr)
				return;

			/* The origin is always a pointer. Find the vertex, which is a piece
			of memory containing the pointer, so as to set up a connection between
			the container and the pointed memory address: */
			auto originatorVtx = GetContainerVertex(fromAddr);
			
			/* There is a scenario when the container vertex (memory block)
			cannot be found for a given pointer. It can happen when the object
			containing such pointer has been collected (hence no longer existent
			in the memory graph), but its destructor (for some reason) tried to
			set the pointer (which is a data member of if) to some another object
			before finally destroying the same pointer. In this case, do not
			create the this edge: */
			if (originatorVtx == nullptr)
				return;
			
			// create the edge:
			originatorVtx->StartEdge(receivingVtx);
			receivingVtx->ReceiveEdge(originatorVtx);
		}

		/// <summary>
		/// Removes an edge that goes from a root vertex to a regular vertex.
		/// </summary>
		/// <param name="fromRoot">The address of the root vertex.</param>
		/// <param name="toAddr">The address to which the edge goes.</param>
		/// <param name="allowDestruction">
		/// Whether the pointed object should have its destructor invoked just
		/// in case it is to be collected. The destruction must not be allowed
		/// when the object construction has failed due to a thrown exception.
		/// </param>
		/// <remarks>
		/// If the receiving vertex becomes unreachable as a result of this
		/// operation, the resources allocated to this vertex and its represented
		/// piece of memory will be released.
		/// </remarks>
		void MemoryDigraph::RemoveRootEdge(void *fromRoot, void *toAddr, bool allowDestruction)
		{
			auto vtxRegularTo = GetVertex(toAddr);
			
			/* pointed piece of memory not present means it
			has been unappropriately collected too soon */
			_ASSERTE(vtxRegularTo != nullptr);
			
			vtxRegularTo->RemoveReceivingEdge(fromRoot);

			// If the memory block became unreachable, remove it from the graph:
			if (!vtxRegularTo->IsReachable())
				RemoveVertex(vtxRegularTo, allowDestruction);
		}

		/// <summary>
		/// Removes an edge that goes from one regular vertex to another.
		/// </summary>
		/// <param name="fromAddr">The address from which the edge comes.</param>
		/// <param name="toAddr">The address to which the edge goes.</param>
		/// <param name="allowDestruction">
		/// Whether the pointed object should have its destructor invoked just
		/// in case it is to be collected. The destruction must not be allowed
		/// when the object construction has failed due to a thrown exception.
		/// </param>
		/// <remarks>
		/// If the receiving vertex becomes unreachable as a result of this
		/// operation, the resourcesallocated to this vertex and its represented
		/// piece of memory will be released.
		/// </remarks>
		void MemoryDigraph::RemoveRegularEdge(void *fromAddr, void *toAddr, bool allowDestruction)
		{
			auto receivingVtx = GetVertex(toAddr);

			/* The receiving vertex cannot be found if this edge closes a cycle
			pointing to a vertex which represents an already collected piece of
			memory. In this case, there is nothing to do: */
			if (receivingVtx == nullptr)
				return;

			/* The origin is always a pointer. Find the vertex, which is a piece
			of memory containing the pointer, so as to unset the connection between
			the container and the pointed memory address: */
			auto originatorVtx = GetContainerVertex(fromAddr);

			/* There is a scenario when the container vertex (memory block)
			cannot be found for a given pointer. It can happen when the object
			containing such pointer has been already collected, hence no longer
			existent in the memory graph. In this case, there is no need to remove
			the edge, because the this implementation has already taken	care of
			erasing all edges connected to the container object by the time it
			was collected: */
			if (originatorVtx != nullptr)
				return;

			// remove the edge:
			originatorVtx->RemoveStartingEdge(receivingVtx);
			receivingVtx->RemoveReceivingEdge(originatorVtx);

			// If the memory block became unreachable, remove it from the graph:
			if (!receivingVtx->IsReachable())
				RemoveVertex(receivingVtx, allowDestruction);
		}

	}// end of namespace memory
}// end of namespace _3fd
