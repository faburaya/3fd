#include "stdafx.h"
#include "gc_memblock.h"

#include <algorithm>
#include <cassert>

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
				if (left->GetEncoded() > right->GetEncoded())
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

				if (what.GetEncoded() < middle->GetEncoded())
					right = middle;
				else if (what.GetEncoded() > middle->GetEncoded())
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
		/// <param name="vtx">
		/// The address of the vertex object. Root vertices must be marked.
		/// </param>
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
		/// Adds a receiving an edge from a regular vertex.
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
			vtx.SetBit0(true); // bit position 0 set means root vertex
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
				auto temp = (MemAddress *)realloc(m_arrayEdges, m_arrayCapacity * sizeof (MemAddress));

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
			vtx.SetBit0(true); // bit position 0 marked means root vertex
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
		/// Determines whether this instance is connected to a root vertex.
		/// </summary>
		/// <returns>
		/// <c>true</c> if connected (directly or not), otherwise, <c>false</c>.
		/// </returns>
		bool Vertex::IsReachable() const
		{
			if (m_rootCount > 0)
				return true;
			else if (m_arraySize > 0)
			{
				// Mark this vertex as visited
				const_cast<Vertex *> (this)->Mark(true);

				// Depth-first search:
				uint32_t idx(0);
				while (idx < m_arraySize)
				{
					auto edge = m_arrayEdges[idx++];
					_ASSERTE(!edge.GetBit0()); // bit 0 activated means "root vertex" and the cast below is invalid

					auto vertex = static_cast<Vertex *> (edge.Get());

					if (!vertex->IsMarked() && vertex->IsReachable())
					{
						// Unmark this vertex before leaving
						const_cast<Vertex *> (this)->Mark(false);
						return true;
					}
				}

				// Unmark this vertex before leaving
				const_cast<Vertex *> (this)->Mark(false);
			}
			
			return false;
		}

	}// end of namespace memory
}// end of namespace _3fd
