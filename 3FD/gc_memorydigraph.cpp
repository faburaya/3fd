#include "stdafx.h"
#include "gc_memorydigraph.h"
#include "gc_vertex.h"
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
							sizeof(Vertex),
							AppConfig::GetSettings().framework.gc.memBlocksMemPool.growingFactor)
		{
			Vertex::SetMemoryPool(m_memBlocksPool);
		}

		/// <summary>
		/// Shrinks the pool of <see cref="Vertex"/> objects.
		/// </summary>
		void MemoryDigraph::ShrinkVertexPool()
		{
			m_memBlocksPool.Shrink();
		}

		/// <summary>
		/// Gets the vertex representing a given memory address.
		/// </summary>
		/// <param name="memAddr">The given memory address.</param>
		/// <returns>The vertex representing the given memory address.</returns>
		Vertex * MemoryDigraph::GetVertex(void *memAddr) const
		{
			MemAddrContainer key(memAddr);
			auto iter = m_vertices.find(&key);

			if (m_vertices.end() != iter)
				return static_cast<Vertex *> (*iter);
			else
				return nullptr;
		}

		/// <summary>
		/// Gets the regular vertex which represents a memory block containing a given address.
		/// </summary>
		/// <param name="addr">The address for which a container will be searched.</param>
		/// <returns>
		/// The vertex representing the memory block which contains the given address,
		/// if existent, otherwise, a <c>nullptr</c>.
		/// </returns>
		Vertex * MemoryDigraph::GetContainerVertex(void *addr) const
		{
			if (!m_vertices.empty())
			{
				MemAddrContainer key(addr);
				auto iter = m_vertices.lower_bound(&key);

				if (m_vertices.end() != iter)
				{
					auto memBlock = static_cast<Vertex *> (*iter);
					if (memBlock->Contains(addr))
						return memBlock;

					if (m_vertices.begin() != iter)
						--iter;
				}
				else
					--iter;

				auto memBlock = static_cast<Vertex *> (*iter);
				if (memBlock->Contains(addr))
					return memBlock;
			}

			return nullptr;
		}

		/// <summary>
		/// Sets the connection between a pointer and its referred memory address,
		/// creating an edge in the graph.
		/// </summary>
		/// <param name="sptrObjHashTableElem">
		/// A hashtable element which represents the pointer.
		/// </param>
		/// <param name="pointedAddr">The referred memory address.</param>
		void MemoryDigraph::MakeReference(AddressesHashTable::Element &sptrObjHashTableElem, void *pointedAddr)
		{
			if (pointedAddr == nullptr)
				return;

			/* By the time the connection is to be set, the vertex representing
			the pointed memory block must already exist in the graph. If not present,
			that means it has been unappropriately collected too soon, or the vertex
			removed from graph too early... */
			auto pointedMemBlock = GetVertex(pointedAddr);
			_ASSERTE(pointedMemBlock != nullptr);

			sptrObjHashTableElem.SetPointedMemBlock(pointedMemBlock);

			if (sptrObjHashTableElem.IsRoot())
				pointedMemBlock->ReceiveEdgeFrom(sptrObjHashTableElem.GetSptrObjectAddr());
			else
			{
				auto originatorVtx = sptrObjHashTableElem.GetContainerMemBlock();
				originatorVtx->IncrementOutgoingEdgeCount();
				pointedMemBlock->ReceiveEdgeFrom(originatorVtx);
			}
		}

		/// <summary>
		/// Determines whether this vertex is reachable by any root vertex
		/// using depth-first search algorithm.
		/// </summary>
		/// <returns>
		/// <c>true</c> if there is a path through with a root vertex
		/// can reach this vertex, otherwise, <c>false</c>.
		/// </returns>
		bool IsReachable(Vertex *memBlock)
		{
			if (memBlock->HasRootEdges())
				return true;

			// mark this vertex before going deeper
			memBlock->Mark(true);

			// iterate over the vertices of receiving edges:
			bool rootFound(false);
			memBlock->ForEachRegularReceivingVertex(
				[&rootFound](Vertex *recvEdgeVtx)
				{
					return rootFound =
						!recvEdgeVtx->IsMarked() // prevent infinite recursion
						&& !recvEdgeVtx->AreReprObjResourcesReleased() // 
						&& !IsReachable(recvEdgeVtx); // recursive search
				}
			);

			// unmark this vertex before leaving
			memBlock->Mark(false);

			return rootFound;
		}

		/// <summary>
		/// Unsets the connection between a pointer and its referred memory address,
		/// changing the graph edges and vertices accordingly.
		/// </summary>
		/// <param name="sptrObjHashTableElem">
		/// A hashtable element which represents the pointer.
		/// </param>
		/// <param name="allowDestruction">
		/// Whether the pointed object should have its destructor invoked just
		/// in case it is to be collected. The destruction must not be allowed
		/// when the object construction has failed due to a thrown exception.
		/// </param>
		/// <remarks>
		/// If the previously pointed memory block becomes unreachable as a result
		/// of this operation, the resources allocated to it will be released.
		/// </remarks>
		void MemoryDigraph::UnmakeReference(AddressesHashTable::Element &sptrObjHashTableElem, bool allowDestruction)
		{
			// the receiving vertex is the pointed memory block, and has been cached
			auto receivingVtx = sptrObjHashTableElem.GetPointedMemBlock();

			if (receivingVtx == nullptr)
				return;

			if (sptrObjHashTableElem.IsRoot())
				receivingVtx->RemoveEdgeFrom(sptrObjHashTableElem.GetSptrObjectAddr());
			else
			{
				auto originatorVtx = sptrObjHashTableElem.GetContainerMemBlock();
				originatorVtx->DecrementOutgoingEdgeCount();

				/* if no longer starts or receives any edge, then
				this vertex became isolated in the graph (no other
				vertex can reach it) and can be safely returned to
				the object pool... */
				if (!originatorVtx->HasAnyEdges())
				{
					/* ... but the represented object resources have
					to be released before this vertex disappears */
					_ASSERTE(originatorVtx->AreReprObjResourcesReleased());
					delete this;
				}

				receivingVtx->RemoveEdgeFrom(originatorVtx);
			}

			/* If the memory block became unreachable, release its
			resources and remove it from the graph: */
			if (!IsReachable(receivingVtx))
			{
				receivingVtx->ReleaseReprObjResources(allowDestruction);
				RemoveVertex(receivingVtx);
			}
		}

		/// <summary>
		/// Adds a new vertex to the graph.
		/// </summary>
		/// <param name="memAddr">The memory address represented by the new vertex.</param>
		/// <param name="blockSize">Size of the represented memory block.</param>
		/// <param name="freeMemCallback">The callback that frees the memory block.</param>
		void MemoryDigraph::AddRegularVertex(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback)
		{
			auto memBlock = new Vertex(memAddr, blockSize, freeMemCallback);
			auto insertSucceded = m_vertices.insert(memBlock).second;
			_ASSERTE(insertSucceded); // insertion should always succeed because a vertex cannot be added twice
		}

		/// <summary>
		/// Removes a given vertex from the graph.
		/// </summary>
		/// <param name="memBlock">
		/// The vertex to remove, which represents a memory block.
		/// </param>
		void MemoryDigraph::RemoveVertex(Vertex *memBlock)
		{
			auto iter = m_vertices.find(memBlock);
			_ASSERTE(m_vertices.end() != iter); // cannot handle removal of unexistent vertex
			m_vertices.erase(iter);
		}

		/// <summary>
		/// Adds a new pointer to the graph.
		/// </summary>
		/// <param name="pointerAddr">The address of the pointer object.</param>
		/// <param name="pointedAddr">The memory address pointed by the object.</param>
		void MemoryDigraph::AddPointer(void *pointerAddr, void *pointedAddr)
		{
			/* The edge origin is always a pointer. If the pointer is not a root
			vertex, find the container vertex, which is a piece of memory containing
			the pointer, so the connection to the pointed memory address will be set
			using the container rather than the pointer: */
			auto containerMemBlock = GetContainerVertex(pointerAddr); // null if root
			auto &sptrObjHashTableElem = m_sptrObjects.Insert(pointerAddr, nullptr, containerMemBlock);
			MakeReference(sptrObjHashTableElem, pointedAddr);
		}

		/// <summary>
		/// Resets a given pointer to another memory address.
		/// </summary>
		/// <param name="pointerAddr">The address of the pointer object.</param>
		/// <param name="newPointedAddr">The new pointed memory address.</param>
		/// <param name="allowDestruction">
		/// Whether the pointed object should have its destructor invoked just
		/// in case it is to be collected. The destruction must not be allowed
		/// when the object construction has failed due to a thrown exception.
		/// </param>
		void MemoryDigraph::ResetPointer(void *pointerAddr, void *newPointedAddr, bool allowDtion)
		{
			auto &sptrObjHashTableElem = m_sptrObjects.Lookup(pointerAddr);
			UnmakeReference(sptrObjHashTableElem, allowDtion);
			MakeReference(sptrObjHashTableElem, newPointedAddr);
		}

		/// <summary>
		/// Removes a given pointer from the graph.
		/// </summary>
		/// <param name="pointerAddr">The address of the pointer object.</param>
		void MemoryDigraph::RemovePointer(void *pointerAddr)
		{
			auto &sptrObjHashTableElem = m_sptrObjects.Lookup(pointerAddr);
			UnmakeReference(sptrObjHashTableElem, true);
			m_sptrObjects.Remove(pointerAddr);
		}

	}// end of namespace memory
}// end of namespace _3fd
