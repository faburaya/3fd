#ifndef GC_MEMBLOCK_H // header guard
#define GC_MEMBLOCK_H

#include "utils.h"
#include "gc_common.h"
#include "gc_memaddress.h"

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

		protected:

			/// <summary>
			/// Marks or unmarks the instance.
			/// </summary>
			/// <param name="on">
			/// if set to <c>true</c>, marks the instance, otherwise, unmark it.
			/// </param>
			virtual void Mark(bool on) = 0;

			/// <summary>
			/// Determines whether this instance is marked.
			/// </summary>
			/// <returns><c>true</c> whether marked, otherwise, <c>false</c>.</returns>
			virtual bool IsMarked() const = 0;

		public:

			Vertex();

			virtual ~Vertex();

			void ReceiveEdge(Vertex *vtxRegular);

			void ReceiveEdge(void *vtxRoot);

			void RemoveEdge(Vertex *vtxRegular);

			void RemoveEdge(void *vtxRoot);

			void RemoveAllEdges();

			bool IsReachable() const;
		};

		/// <summary>
		/// Represents a memory block region managed by the GC.
		/// </summary>
		class MemBlock : notcopiable, public MemAddrContainer, public Vertex
		{
		private:

			FreeMemProc		m_freeMemCallback;
			uint32_t		m_blockSize;

			virtual void Mark(bool on) override;

			virtual bool IsMarked() const override;

			static utils::DynamicMemPool *dynMemPool;

		public:

			static void SetMemoryPool(utils::DynamicMemPool &ob);

			void *operator new(size_t);

			void operator delete(void *ptr);

			MemBlock(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback);

			virtual ~MemBlock();

			bool Contains(void *someAddr) const;

			void Free(bool destroy);			
		};

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
