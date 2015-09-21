#ifndef GC_ARRAYOFEDGES_H // header guard
#define GC_ARRAYOFEDGES_H

#include "base.h"
#include <cinttypes>
#include <functional>

/*
	The convention here is

	+ Regular vertices are passed as 'Vertex *'
	+ Root vertices are passed as 'void *'
	+ Memory addresses in general are handled as 'void *'
*/
namespace _3fd
{
	namespace memory
	{
		class Vertex;

		/// <summary>
		/// A dinamically resizable array of edges,
		/// for implementation of directed graphs.
		/// </summary>
		class ArrayOfEdges : notcopiable
		{
		private:

			/// <summary>
			/// Holds pointers to all vertices, that represent receiving edges.
			/// </summary>
			void **m_array;

			uint32_t m_arraySize;

			uint32_t m_arrayCapacity;

			/// <summary>
			/// Counting of how many root vertices are in the array.
			/// </summary>
			uint32_t m_rootCount;

			void CreateEdgeImpl(void *vtx);

			void RemoveEdgeImpl(void *vtx);

			void EvaluateShrinkCapacity();

		public:

			ArrayOfEdges();

			~ArrayOfEdges();

			void AddEdge(void *vtxRoot);

			void AddEdge(Vertex *vtxRegular);

			void RemoveEdge(void *vtxRoot);

			void RemoveEdge(Vertex *vtxRegular);

			void Clear();

			uint32_t Size() const;

			bool HasRootEdges() const;

			void ForEachRegular(const std::function<bool(Vertex *)> &callback);
		};

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
