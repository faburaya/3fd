#ifndef GC_MEMBLOCK_H // header guard
#define GC_MEMBLOCK_H

#include "utils.h"
#include "gc_common.h"
#include "gc_memaddress.h"

#include <functional>
#include <cstdint>
#include <cstdlib>

/*
	Comments about the implementation of the Vertex class:

	The convention here is

	+ Regular vertices are passed as 'Vertex *'
	+ Root vertices are passed as 'void *'
	+ Memory addresses in general are handled as 'void *'

	The class has an array of edges, that are addresses to other vertices. In order to
	distinguish starting and receiving edges, such addresses are marked setting their
	least significant bit. When such addresses come from the heap, the availability of
	the least significant bit must be guaranteed with allocation of memory aligned in a
	2 byte boundary. Root vertices are pointers living on the stack, hence their addresses
	are already aligned this way.

	There is no need to distinguish root and regular vertices, because they are searched
	individually by their addresses, which are always distinct.
*/
namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// Represents a vertex in a directed graph of pieces of memory.
		/// </summary>
		class Vertex : notcopiable
		{
		private:

			/// <summary>
			/// Holds pointers to all verticesconnected to this node.
			/// Vertices have the least significant bit set when they represent starting edges.
			/// </summary>
			MemAddress *m_array;

			uint32_t m_arraySize;

			uint32_t m_arrayCapacity;

			/// <summary>
			/// Counting of how many root vertices (from receiving edges) are in the array.
			/// </summary>
			uint32_t m_rootCount;

			void EvaluateShrinkCapacity();

			void CreateEdge(MemAddress vtx);

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

			void ReceiveEdge(void *vtxRoot);

			void ReceiveEdge(Vertex *vtxRegularFrom);

			void StartEdge(Vertex *vtxRegularTo);

			void RemoveReceivingEdge(void *vtxRoot);

			void RemoveReceivingEdge(Vertex *vtxRegularFrom);

			void RemoveStartingEdge(Vertex *vtxRegularTo);

			void RemoveAllEdges();

			void ForEachStartingEdge(const std::function<void (Vertex &)> &callback);

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
