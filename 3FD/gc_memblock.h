#ifndef GC_MEMORYBLOCK_H // header guard
#define GC_MEMORYBLOCK_H

#include "utils.h"
#include "gc_common.h"
#include "gc_memaddress.h"

#include <functional>
#include <cstdint>
#include <cstdlib>

namespace _3fd
{
	namespace memory
	{
		/*
			The convention here will be:
			+ Regular vertices are passed as 'Vertex *'
			+ Root vertices are passed as 'void *'
			+ Memory addresses in general are handled as 'void *'
		*/

		/// <summary>
		/// Represents a vertex in a directed graph of pieces of memory.
		/// </summary>
		class Vertex : notcopiable
		{
		private:

			/// <summary>
			/// Holds pointers to all vertices which have an edge directed to this node.
			/// Root vertices are marked.
			/// </summary>
			MemAddress *m_arrayEdges;

			uint32_t m_arraySize;

			uint32_t m_arrayCapacity;

			/// <summary>
			/// Counting of how many root vertices are in the array.
			/// </summary>
			uint32_t m_rootCount;

			void EvaluateShrinkCapacity();

			void ReceiveEdgeImpl(MemAddress vtx);

			void RemoveEdgeImpl(MemAddress vtx);

		public:

			Vertex();

			~Vertex();

			void ReceiveEdge(Vertex *vtxRegular);

			void ReceiveEdge(void *vtxRoot);

			void RemoveEdge(Vertex *vtxRegular);

			void RemoveEdge(void *vtxRoot);

			void RemoveAllEdges();

			bool HasRootEdge() const;

			bool ForEachRegularEdgeWhile(const std::function<bool(const Vertex &)> &callback) const;
		};

		/// <summary>
		/// Represents a memory block region managed by the GC.
		/// </summary>
		class MemBlock : notcopiable, public Vertex, public MemAddrContainer
		{
		private:

			FreeMemProc		m_freeMemCallback;
			uint32_t		m_blockSize;

			static utils::DynamicMemPool *dynMemPool;

		public:

			static void SetMemoryPool(utils::DynamicMemPool &ob);

			void *operator new(size_t);

			void operator delete(void *ptr);

			MemBlock(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback);

			~MemBlock();

			bool Contains(void *someAddr) const;

			bool IsReachable() const;

			void Free(bool destroy);
		};

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
