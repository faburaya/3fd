#include "stdafx.h"
#include "gc_vertexstore.h"
#include "configuration.h"

namespace _3fd
{
	namespace memory
	{
		using core::AppConfig;

		/// <summary>
		/// Initializes a new instance of the <see cref="VertexStore"/> class.
		/// </summary>
		VertexStore::VertexStore() :
			m_memBlocksPool(
				AppConfig::GetSettings().framework.gc.memBlocksMemPool.initialSize,
				sizeof(Vertex),
				AppConfig::GetSettings().framework.gc.memBlocksMemPool.growingFactor
			)
		{
			Vertex::SetMemoryPool(m_memBlocksPool);
		}

		/// <summary>
		/// Shrinks the pool of <see cref="Vertex"/> objects.
		/// </summary>
		void VertexStore::ShrinkPool()
		{
			m_memBlocksPool.Shrink();
		}

		/// <summary>
		/// Gets the vertex representing a given memory address.
		/// </summary>
		/// <param name="memAddr">The given memory address.</param>
		/// <returns>The vertex representing the given memory address.</returns>
		Vertex * VertexStore::GetVertex(void *memAddr) const
		{
			MemAddrContainer key(memAddr);
			auto iter = m_vertices.find(&key);

			if (m_vertices.end() != iter)
				return static_cast<Vertex *> (*iter);
			else
				return nullptr;
		}

		/// <summary>
		/// Gets the vertex which represents a memory block containing a given address.
		/// </summary>
		/// <param name="addr">The address for which a container will be searched.</param>
		/// <returns>
		/// The vertex representing the memory block which contains the given address,
		/// if existent, otherwise, a <c>nullptr</c>.
		/// </returns>
		Vertex * VertexStore::GetContainerVertex(void *addr) const
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
		/// Adds a new vertex to the store.
		/// </summary>
		/// <param name="memAddr">The memory address represented by the new vertex.</param>
		/// <param name="blockSize">Size of the represented memory block.</param>
		/// <param name="freeMemCallback">The callback that frees the memory block.</param>
		void VertexStore::AddVertex(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback)
		{
			auto memBlock = new Vertex(memAddr, blockSize, freeMemCallback);
			auto insertSucceded = m_vertices.insert(memBlock).second;
			_ASSERTE(insertSucceded); // insertion should always succeed because a vertex cannot be added twice
		}

		/// <summary>
		/// Removes a given vertex from the store.
		/// </summary>
		/// <param name="memBlock">
		/// The vertex to remove, which represents a memory block.
		/// </param>
		void VertexStore::RemoveVertex(Vertex *memBlock)
		{
			auto iter = m_vertices.find(memBlock);
			_ASSERTE(m_vertices.end() != iter); // cannot handle removal of unexistent vertex
			m_vertices.erase(iter);
		}

	}// end of namespace memory
}// end of namespace _3fd
