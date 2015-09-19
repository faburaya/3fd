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
		/// <param name="what">
		/// The memory address value to look for.
		/// Addresses of starting edges must have the less significant bit set.
		/// </param>
		/// <returns>An iterator to the position where it was found, otherwise, <c>nullptr</c>.</returns>
		static MemAddress *Search(MemAddress *left, MemAddress *right, MemAddress what)
		{
			// Use scan if the vector is small enough:
			auto size = right - left;
			if (size <= 7)
				return std::find(left, right, what);

			const auto key = what.GetEncoded();

			// Otherwise, do binary search:
			do
			{
				auto middle = left + size / 2;

				if (middle->GetEncoded() > key)
					right = middle;
				else if (middle->GetEncoded() < key)
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
			m_array(nullptr),
			m_arraySize(0),
			m_arrayCapacity(0),
			m_rootCount(0)
		{}

		/// <summary>
		/// Finalizes an instance of the <see cref="Vertex"/> class.
		/// </summary>
		Vertex::~Vertex()
		{
			if (m_array != nullptr)
				free(m_array);
		}

		/// <summary>
		/// Implementation for creating a new edge.
		/// </summary>
		/// <param name="vtx">
		/// The address of the vertex to connect. Vertices receiving an edge
		/// from this instance must be the less significant bit set.
		/// </param>
		void Vertex::CreateEdge(MemAddress vtx)
		{
			if (m_arrayCapacity > m_arraySize) // there is room in the array
			{
				m_array[m_arraySize++] = vtx;
			}
			else if (m_arrayCapacity > 0) // it must expand the array so as to make room
			{
				m_arrayCapacity *= 2;
				auto temp = (MemAddress *)realloc(m_array, m_arrayCapacity * sizeof(MemAddress));

				if (temp != nullptr)
					m_array = temp;
				else
					throw std::bad_alloc();

				m_array[m_arraySize++] = vtx;
			}
			else // the array is not yet allocated
			{
				m_array = (MemAddress *)malloc(sizeof(MemAddress));

				if (m_array != nullptr)
					m_array[0] = vtx;
				else
					throw std::bad_alloc();

				m_arrayCapacity = m_arraySize = 1;
				return;
			}

			// keep the array sorted
			InsertionSort(m_array, m_array + m_arraySize);
		}

		/// <summary>
		/// Adds a receiving edge from a root vertex.
		/// </summary>
		/// <param name="vtxRoot">The root vertex.</param>
		void Vertex::ReceiveEdge(void *vtxRoot)
		{
			MemAddress vtx(vtxRoot);
			_ASSERTE(!vtx.GetBit0()); // receiving edges must have the bit 0 unset
			CreateEdge(vtx);
			++m_rootCount;
		}

		/// <summary>
		/// Adds a receiving an edge from a regular vertex.
		/// </summary>
		/// <param name="vtxRegularFrom">
		/// The regular vertex starting the edge.
		/// </param>
		void Vertex::ReceiveEdge(Vertex *vtxRegularFrom)
		{
			MemAddress vtx(vtxRegularFrom);
			_ASSERTE(!vtx.GetBit0()); // receiving edges must have bit 0 unset
			CreateEdge(vtx);
		}

		/// <summary>
		/// Adds a starting edge, which is always to a regular vertex,
		/// because root vertices represent addresses of pointers living
		/// on the stack and never in the heap.
		/// </summary>
		/// <param name="vtxRegularTo">
		/// The regular vertex receiving the edge.
		/// </param>
		void Vertex::StartEdge(Vertex *vtxRegularTo)
		{
			MemAddress vtx(vtxRegularTo);
			_ASSERTE(!vtx.GetBit0()); // address must have bit 0 available for marking
			vtx.SetBit0(true);
			CreateEdge(vtx);
		}

		/// <summary>
		/// Evaluates whether capacity should shrink, then execute it.
		/// </summary>
		void Vertex::EvaluateShrinkCapacity()
		{
			if (m_arraySize < m_arrayCapacity / 4)
			{
				m_arrayCapacity /= 2;
				auto temp = (MemAddress *)realloc(m_array, m_arrayCapacity * sizeof (MemAddress));

				if (temp != nullptr)
					m_array = temp;
				else
					throw std::bad_alloc();
			}
		}

		/// <summary>
		/// Implementation for removing an edge.
		/// </summary>
		/// <param name="vtx">
		/// The vertex to remove. Vertices receiving an edge
		/// from this instance must have the less significant bit set.
		/// </param>
		void Vertex::RemoveEdgeImpl(MemAddress vtx)
		{
			auto where = Search(m_array, m_array + m_arraySize--, vtx);
			_ASSERTE(where != nullptr); // cannot handle removal of unexistent edge

			for (auto idx = where - m_array; idx < m_arraySize; ++idx)
				m_array[idx] = m_array[idx + 1];

			EvaluateShrinkCapacity();
		}

		/// <summary>
		/// Removes an edge coming from a root vertex.
		/// </summary>
		/// <param name="vtxRoot">The root vertex.</param>
		void Vertex::RemoveReceivingEdge(void *vtxRoot)
		{
			RemoveEdgeImpl(MemAddress(vtxRoot));
			--m_rootCount;
		}

		/// <summary>
		/// Removes an edge coming from a regular vertex.
		/// </summary>
		/// <param name="vtxRegularFrom">The regular vertex.</param>
		void Vertex::RemoveReceivingEdge(Vertex *vtxRegularFrom)
		{
			RemoveEdgeImpl(MemAddress(vtxRegularFrom));
		}

		/// <summary>
		/// Removes an edge going to a regular vertex.
		/// </summary>
		/// <param name="vtxRegularTo">The regular vertex.</param>
		void Vertex::RemoveStartingEdge(Vertex *vtxRegularTo)
		{
			MemAddress vtx(vtxRegularTo);
			vtx.SetBit0(true);
			RemoveEdgeImpl(vtx);
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
		/// Iterates over each edge started by this vertex.
		/// </summary>
		/// <param name="callback">
		/// The callback to invoke for each vertex receiving an edge from this instance.
		/// </param>
		void Vertex::ForEachStartingEdge(const std::function<void(Vertex &)> &callback)
		{
			for (uint32_t idx = 0; idx < m_arraySize; ++idx)
			{
				auto edge = m_array[idx];

				// least significant bit activated means this is a starting edge:
				if (edge.GetBit0())
				{
					auto &vertex = *static_cast<Vertex *> (edge.Get());
					callback(vertex);
				}
			}
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
					auto edge = m_array[idx++];

					/* Least significant bit activated means starting edge,
					hence skip this, because a root vertex reaches this vertex
					via an edge received by this instance, not started. */
					if (edge.GetBit0())
						continue;

					/* at this point, because there is no root vertex in
					the array of edges, the cast below is always valid */
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
