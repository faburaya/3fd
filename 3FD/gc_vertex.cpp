#include "stdafx.h"
#include "gc_vertex.h"

namespace _3fd
{
	namespace memory
	{
		utils::DynamicMemPool * Vertex::dynMemPool(nullptr);

		/// <summary>
		/// Sets the object pool that provides all the <see cref="Vertex"/> instances.
		/// </summary>
		/// <param name="ob">The object pool to use.</param>
		void Vertex::SetMemoryPool(utils::DynamicMemPool &ob)
		{
			_ASSERTE(dynMemPool == nullptr); // can only set it once
			dynMemPool = &ob;
		}

		/// <summary>
		/// Implements the operator new, to construct a new <see cref="Vertex"/>
		/// instance from resources in the object pool.
		/// </summary>
		/// <param name="">Currently not used.</param>
		/// <returns>
		/// A new <see cref="Vertex"/> instance retrieved from the object pool.
		/// </returns>
		void * Vertex::operator new(size_t)
		{
			return dynMemPool->GetFreeBlock();
		}

		/// <summary>
		/// Implements the operator delete, to destroy a <see cref="Vertex"/>
		/// instance returning itself to the object pool.
		/// </summary>
		/// <param name="ptr">The address of the object to delete.</param>
		void Vertex::operator delete(void *ptr)
		{
			dynMemPool->ReturnBlock(ptr);
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="Vertex"/> class.
		/// </summary>
		/// <param name="memAddr">The address of the memory block.</param>
		/// <param name="blockSize">Size of the block.</param>
		/// <param name="freeMemCallback">
		/// The callback that frees the memory block this vertex represents.
		/// </param>
		Vertex::Vertex(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback) :
			MemAddrContainer(memAddr),
			m_freeMemCallback(freeMemCallback),
			m_blockSize(blockSize), 
			m_outEdgeCount(0)
		{
			_ASSERTE(!GetMemoryAddress().GetBit0()); // regular vertices must have bit 0 unset
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="Vertex"/> class.
		/// </summary>
		Vertex::~Vertex()
		{
			m_incomingEdges.Clear();
		}

		/// <summary>
		/// Increments the count of outgoing edges.
		/// </summary>
		void Vertex::IncrementOutgoingEdgeCount()
		{
			++m_outEdgeCount;
		}

		/// <summary>
		/// Decrements the count of outgoing edges.
		/// </summary>
		void Vertex::DecrementOutgoingEdgeCount()
		{
			--m_outEdgeCount;
			// counting of outgoing edges can never be negative
			_ASSERTE(m_outEdgeCount >= 0);
		}
		
		/// <summary>
		/// Determines whether this vertex has any edges
		/// (incoming and outgoing).
		/// </summary>
		/// <returns>
		/// <c>true</c> if this vertex is connected to any other vertex, otherwise, <c>false</c>.
		/// </returns>
		bool Vertex::HasAnyEdges() const
		{
			return m_incomingEdges.Size() + m_outEdgeCount > 0;
		}
		
		/// <summary>
		/// Determines whether the memory block represented by
		/// this vertex contains the specified memory address.
		/// </summary>
		/// <param name="someAddr">The memory address to test.</param>
		/// <returns>Whether this memory block contains the specified memory address</returns>
		bool Vertex::Contains(void *someAddr) const
		{
			return someAddr >= GetMemoryAddress().Get()
				&& someAddr < (void *)((uintptr_t)GetMemoryAddress().Get() + m_blockSize);
		}

		/// <summary>
		/// Marks or unmarks this vertex.
		/// </summary>
		/// <param name="on">
		/// if set to <c>true</c>, marks the instance, otherwise, unmark it.
		/// </param>
		void Vertex::Mark(bool on)
		{
			/* In order to save memory, use the vacant bits
			in the held memory address for marking */
			GetMemoryAddress().SetBit0(on);
		}

		/// <summary>
		/// Determines whether this vertex is marked.
		/// </summary>
		/// <returns><c>true</c> whether marked, otherwise, <c>false</c>.</returns>
		bool Vertex::IsMarked() const
		{
			return GetMemoryAddress().GetBit0();
		}

		/// <summary>
		/// Frees the resources allocated to
		/// the object represented by this vertex.
		/// </summary>
		/// <param name="destroy">
		/// If set to <c>true</c>, invokes the object destructor.
		/// </param>
		void Vertex::ReleaseReprObjResources(bool destroy)
		{
			_ASSERTE(GetMemoryAddress().Get() != nullptr); // resource already freed
			(*m_freeMemCallback)(GetMemoryAddress().Get(), destroy);
			SetMemoryAddress(nullptr);
		}

		/// <summary>
		/// Determines whether the resources of the object
		/// represent by this vertex are released already.
		/// </summary>
		/// <returns>
		/// <c>true</c>, if resources have already been relesed, otherwise, <c>false</c>.
		/// </returns>
		bool Vertex::AreReprObjResourcesReleased() const
		{
			return GetMemoryAddress().Get() == nullptr;
		}

	}// end of namespace memory
}// end of namespace _3fd
