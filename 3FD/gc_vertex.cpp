#include "stdafx.h"
#include "gc_memblock.h"

#include <algorithm>
#include <cassert>

/*
	Regular vertices are kept in the second partition whereas root vertices stay in the first. That is 
	because the handling of regular vertices is expected to happen more often, hence their insertion must 
	be simple (faster). They happen more often when the graph has long and complex paths, which is the 
	scenario where garbage collection makes sense.
*/
namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// Does insertion sort.
		/// It expects a array interval that was sorted until the last insertion to the right.
		/// The algorithm is fast (linear complexity) for such cases.
		/// </summary>
		/// <param name="begin">An iterator to the first position in the array interval to sort.</param>
		/// <param name="end">An iterator to one past the last position in the array interval to sort.</param>
		static void InsertionSort(MemAddress *begin, MemAddress *end)
		{
			auto &right = end;
			while (begin < --right)
			{
				auto left = right - 1;
				if (left->Get() > right->Get())
				{
					auto temp = *right;
					*right = *left;
					*left = temp;
				}
				else
					return;
			}
		}

		/// <summary>
		/// Searches for a value in a sorted array interval.
		/// </summary>
		/// <param name="left">An iterator to the first position in the array interval to search.</param>
		/// <param name="right">An iterator to one past the last position in the array interval to search.</param>
		/// <param name="what">The address value to look for.</param>
		/// <returns>An iterator to the position where it was found, otherwise, <c>nullptr</c>.</returns>
		static MemAddress *Search(MemAddress *left, MemAddress *right, MemAddress what)
		{
			// Use scan if the vector is small enough:
			auto size = right - left;
			if (size <= 7)
				return std::find(left, right, what);

			// Otherwise, do binary search:
			do
			{
				auto middle = left + size / 2;

				if (what.Get() < middle->Get())
					right = middle;
				else if (what.Get() > middle->Get())
					left = middle + 1;
				else
					return middle;

				size = right - left;
			}
			while (size > 0);

			return nullptr;
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="Vertex"/> class.
		/// </summary>
		Vertex::Vertex() : 
			m_arrayEdges(nullptr),
			m_arraySize(0),
			m_arrayCapacity(0),
			m_rootCount(0)
		{}

		/// <summary>
		/// Finalizes an instance of the <see cref="Vertex"/> class.
		/// </summary>
		Vertex::~Vertex()
		{
			if (m_arrayEdges != nullptr)
				free(m_arrayEdges);
		}

		/// <summary>
		/// Implementation for receiving a new edge.
		/// </summary>
		/// <param name="vtx">The address of the vertex object. Root vertices must be marked.</param>
		void Vertex::ReceiveEdgeImpl(MemAddress vtx)
		{
			if (m_arrayCapacity > m_arraySize) // there is room in the array
			{
				m_arrayEdges[m_arraySize++] = vtx;
			}
			else if (m_arrayCapacity > 0) // it must expand the array so as to make room
			{
				m_arrayCapacity *= 2;
				auto temp = (MemAddress *)realloc(m_arrayEdges, m_arrayCapacity * sizeof(MemAddress));

				if (temp != nullptr)
					m_arrayEdges = temp;
				else
					throw std::bad_alloc();

				m_arrayEdges[m_arraySize++] = vtx;
			}
			else // the array is not yet allocated
			{
				m_arrayEdges = (MemAddress *)malloc(sizeof(MemAddress));

				if (m_arrayEdges != nullptr)
					m_arrayEdges[0] = vtx;
				else
					throw std::bad_alloc();

				m_arrayCapacity = m_arraySize = 1;
				return;
			}

			// keep the array sorted
			InsertionSort(m_arrayEdges, m_arrayEdges + m_arraySize);
		}

		/// <summary>
		/// Adds a receiving an edge from a regultar vertex.
		/// </summary>
		/// <param name="vtxRegular">The regular vertex.</param>
		void Vertex::ReceiveEdge(Vertex *vtxRegular)
		{
			ReceiveEdgeImpl(MemAddress(vtxRegular));
		}

		/// <summary>
		/// Adds a receiving edge from a root vertex.
		/// </summary>
		/// <param name="vtxRoot">The root vertex.</param>
		void Vertex::ReceiveEdge(void *vtxRoot)
		{
			MemAddress vtx(vtxRoot);
			vtx.Mark(true);
			ReceiveEdgeImpl(vtx);
			++m_rootCount;
		}

		/// <summary>
		/// Evaluates whether capacity should shrink, then execute it.
		/// </summary>
		void Vertex::EvaluateShrinkCapacity()
		{
			if (m_arraySize < m_arrayCapacity / 4)
			{
				m_arrayCapacity /= 2;
				auto temp = (MemAddress *)realloc(m_arrayEdges, m_arrayCapacity * sizeof(MemAddress));

				if (temp != nullptr)
					m_arrayEdges = temp;
				else
					throw std::bad_alloc();
			}
		}

		/// <summary>
		/// Implementation for removing an edge.
		/// </summary>
		/// <param name="vtx">The vertex to remove. Root vertices must be marked.</param>
		void Vertex::RemoveEdgeImpl(MemAddress vtx)
		{
			auto where = Search(m_arrayEdges, m_arrayEdges + m_arraySize--, vtx);
			_ASSERTE(where != nullptr); // cannot handle removal of unexistent edge

			for (auto idx = where - m_arrayEdges; idx < m_arraySize; ++idx)
				m_arrayEdges[idx] = m_arrayEdges[idx + 1];

			EvaluateShrinkCapacity();
		}

		/// <summary>
		/// Removes an edge coming from a regular vertex.
		/// </summary>
		/// <param name="vtxRegular">The regular vertex.</param>
		void Vertex::RemoveEdge(Vertex *vtxRegular)
		{
			RemoveEdgeImpl(MemAddress(vtxRegular));
		}

		/// <summary>
		/// Removes an edge coming from a root vertex.
		/// </summary>
		/// <param name="vtxRoot">The root vertex.</param>
		void Vertex::RemoveEdge(void *vtxRoot)
		{
			MemAddress vtx(vtxRoot);
			vtx.Mark(true);
			RemoveEdgeImpl(vtx);
			--m_rootCount;
		}

		/// <summary>
		/// Removes all receiving edges from this vertex.
		/// </summary>
		void Vertex::RemoveAllEdges()
		{
			m_arraySize = m_rootCount = 0;
			EvaluateShrinkCapacity();
		}

		/// <summary>
		/// Determines whether this vertex has any receiving edge from a root vertex.
		/// </summary>
		/// <returns>Whether it has a receiving edge from a root vertex.</returns>
		bool Vertex::HasRootEdge() const
		{
			return m_rootCount > 0;
		}

		/// <summary>
		/// Iterates over regular edges while the callback keeps asking for continuation.
		/// </summary>
		/// <param name="callback">The callback that receives the vertices as parameter. Iteration continues while it returns 'true'.</param>
		/// <returns><c>false</c> if the callback has returned <c>false</c> for any edge from regular vertex.</returns>
		bool Vertex::ForEachRegularEdgeWhile(const std::function<bool(const Vertex &)> &callback) const
		{
			if (m_arraySize > 0)
			{
				uint32_t idx(0);
				bool goAhead(true);
				while (idx < m_arraySize && goAhead)
				{
					auto edge = m_arrayEdges[idx++];
					if (!edge.isMarked())
						goAhead = callback(*static_cast<Vertex *> (edge.Get()));
				}

				return idx == m_arraySize && goAhead;
			}
			else
				return true;
		}

	}// end of namespace memory
}// end of namespace _3fd
