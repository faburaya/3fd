#ifndef	GC_MEMORYDIGRAPH_H // header guard
#define GC_MEMORYDIGRAPH_H

/*
	Here is implemented a directed graph tuned for reachability analysis used by the garbage collector.
	In this graph, each vertex is either a safe pointer or a piece of garbage collected memmory (which
	might contain a safe pointer inside it). The vertices are distinguished as regular or root. A root
	vertex is a safe pointer allocated in memory not managed by the garbage collector. That means client
	code uses such pointers to access the remaining of the graph (garbage collected memory pieces),	that
	are the regular vertices.

	This distinction is central for the GC to perform reachability analysis. When a given piece of memory
	is not connected to any root vertex, that means it became out of reach and has to be collected. This
	approach is not vulnerable to cyclic references, unlike reference counting.
*/

#include "gc_vertexstore.h"
#include "gc_addresseshashtable.h"

namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// Directed graph representing the connections made by safe pointers
		/// between pieces of memory managed by the GC.
		/// </summary>
		class MemoryDigraph : notcopiable
		{
		private:

			/// <summary>
			/// An unsorted map of elements representing <see cref="sptr"/> objects.
			/// </summary>
			/// <remarks>
			/// A hash table here might improve performance and
			/// can be used because it does not have to be sorted.
			/// </remarks>
			AddressesHashTable m_sptrObjects;

			VertexStore m_vertices;

			void MakeReference(AddressesHashTable::Element &sptrObjHashTableElem, Vertex *pointedMemBlock);

			void MakeReference(AddressesHashTable::Element &sptrObjHashTableElem, void *pointedAddr);

			void UnmakeReference(AddressesHashTable::Element &sptrObjHashTableElem, bool allowDestruction);

		public:

			void ShrinkVertexPool();

			void AddRegularVertex(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback);

			void AddPointer(void *pointerAddr, void *pointedAddr);

			void AddPointerOnCopy(void *leftPointerAddr, void *rightPointerAddr);

			void ResetPointer(void *pointerAddr, void *newPointedAddr, bool allowDtion);

			void ResetPointer(void *pointerAddr, void *otherPointerAddr);

			void ReleasePointer(void *pointerAddr);

			void RemovePointer(void *pointerAddr);
		};

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
