#include "pch.h"
#include "gc_arrayofedges.h"
#include "preprocessing.h"
#include <algorithm>
#include <cstdlib>
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
        static void InsertionSort(void **begin, void **end)
        {
            auto &right = end;
            while (begin < --right)
            {
                auto left = right - 1;
                if (*left > *right)
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
        /// Addresses of root vertices must have the least significant bit set.
        /// </param>
        /// <returns>An iterator to the position where it was found, otherwise, <c>nullptr</c>.</returns>
        static void **Search(void **left, void **right, void *what)
        {
            // Use scan if the vector is small enough:
            auto size = right - left;
            if (size <= 7)
                return std::find(left, right, what);

            // Otherwise, do binary search:
            do
            {
                auto middle = left + size / 2;

                if (*middle > what)
                    right = middle;
                else if (*middle < what)
                    left = middle + 1;
                else
                    return middle;

                size = right - left;
            }
            while (size > 0);

            return nullptr;
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="ArrayOfEdges"/> class.
        /// </summary>
        ArrayOfEdges::ArrayOfEdges() :
            m_array(nullptr),
            m_arraySize(0),
            m_arrayCapacity(0),
            m_rootCount(0)
        {}

        /// <summary>
        /// Finalizes an instance of the <see cref="ArrayOfEdges"/> class.
        /// </summary>
        ArrayOfEdges::~ArrayOfEdges()
        {
            if (m_array != nullptr)
                free(m_array);
        }

        /// <summary>
        /// Implementation to create a new edge.
        /// </summary>
        /// <param name="vtx">The address of the vertex to connect.</param>
        void ArrayOfEdges::CreateEdgeImpl(void *vtx)
        {
            if (m_arrayCapacity > m_arraySize) // there is room in the array
            {
                m_array[m_arraySize++] = vtx;
            }
            else if (m_arrayCapacity > 0) // it must expand the array so as to make room
            {
                m_arrayCapacity *= 2;
                auto temp = (void **)realloc(m_array, m_arrayCapacity * sizeof(void *));

                if (temp != nullptr)
                    m_array = temp;
                else
                    throw std::bad_alloc();

                m_array[m_arraySize++] = vtx;
            }
            else // the array is not yet allocated
            {
                m_array = (void **)malloc(sizeof(void *));

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
        /// Implementation to remove an edge.
        /// </summary>
        /// <param name="vtx">The vertex to remove.</param>
        void ArrayOfEdges::RemoveEdgeImpl(void *vtx)
        {
            auto where = Search(m_array, m_array + m_arraySize--, vtx);
            _ASSERTE(where != nullptr); // cannot handle removal of unexistent edge

            for (auto idx = where - m_array; idx < m_arraySize; ++idx)
                m_array[idx] = m_array[idx + 1];

            EvaluateShrinkCapacity();
        }

        /// <summary>
        /// Evaluates whether capacity should shrink, then execute it.
        /// </summary>
        void ArrayOfEdges::EvaluateShrinkCapacity()
        {
            if (m_arraySize < m_arrayCapacity / 4)
            {
                m_arrayCapacity /= 2;
                auto temp = (void **)realloc(m_array, m_arrayCapacity * sizeof(void *));

                if (temp != nullptr)
                    m_array = temp;
                else
                    throw std::bad_alloc();
            }
        }

        /// <summary>
        /// Adds an edge with a root vertex.
        /// </summary>
        /// <param name="vtxRoot">The root vertex.</param>
        void ArrayOfEdges::AddEdge(void *vtxRoot)
        {
            CreateEdgeImpl(vtxRoot);
            ++m_rootCount;
        }

        /// <summary>
        /// Adds an edge with a regular vertex.
        /// </summary>
        /// <param name="vtxRegular">The regular vertex starting the edge.</param>
        void ArrayOfEdges::AddEdge(Vertex *vtxRegular)
        {
            CreateEdgeImpl(vtxRegular);
        }

        /// <summary>
        /// Removes an edge with a root vertex.
        /// </summary>
        /// <param name="vtxFrom">The root vertex.</param>
        void ArrayOfEdges::RemoveEdge(void *vtxRoot)
        {
            RemoveEdgeImpl(vtxRoot);
            --m_rootCount;
        }

        /// <summary>
        /// Removes an edge with a regular vertex.
        /// </summary>
        /// <param name="vtxFrom">The regular vertex.</param>
        void ArrayOfEdges::RemoveEdge(Vertex *vtxRegular)
        {
            RemoveEdgeImpl(vtxRegular);
        }

        /// <summary>
        /// Removes all edges from this array.
        /// </summary>
        void ArrayOfEdges::Clear()
        {
            m_arraySize = m_rootCount = 0;
            EvaluateShrinkCapacity();
        }

        /// <summary>
        /// Gets how many edges are stored in this array.
        /// </summary>
        /// <returns>The amount of edges stored in this array.</returns>
        uint32_t ArrayOfEdges::Size() const
        {
            return m_arraySize;
        }

        /// <summary>
        /// Determines whether this array has any edge from a root vertex.
        /// </summary>
        /// <returns>
        /// <c>true</c> if any edge from root vertex is present, otherwise, <c>false</c>.
        /// </returns>
        bool ArrayOfEdges::HasRootEdges() const
        {
            return m_rootCount > 0;
        }

        /// <summary>
        /// Iterates over each edge with regular vertex in this array.
        /// This can only be used when this array has no edges with root vertices.
        /// </summary>
        /// <param name="callback">
        /// The callback to invoke for each vertex with an edge in this array.
        /// The iteration goes on while it returns <c>true</c>.
        /// </param>
        void ArrayOfEdges::ForEachRegular(const std::function<bool(Vertex *)> &callback)
        {
            /* if any edge with root vertex is present,
            the cast below will be invalid for such edge */
            _ASSERTE(!HasRootEdges());

            uint32_t idx(0);
            while (idx < m_arraySize)
            {
                auto vertex = static_cast<Vertex *> (m_array[idx++]);
                
                if (!callback(vertex))
                    break;
            }
        }

    }// end of namespace memory
}// end of namespace _3fd
