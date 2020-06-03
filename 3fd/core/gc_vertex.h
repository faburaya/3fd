#ifndef GC_VERTEX_H // header guard
#define GC_VERTEX_H

#include <3fd/core/gc_common.h>
#include <3fd/core/gc_memaddress.h>
#include <3fd/core/gc_arrayofedges.h>
#include <3fd/utils/utils_memory.h>

#include <cstdint>
#include <cstdlib>
#include <functional>

namespace _3fd
{
namespace memory
{
    /// <summary>
    /// Base class for <see cref="Vertex"/>.
    /// Its only purpose is to ease searching a std::set{Vertex *}.
    /// </summary>
    class MemAddrContainer
    {
    private:

        MemAddress m_memAddr;

    protected:

        void SetMemoryAddress(void *memAddr) { m_memAddr = MemAddress(memAddr); }

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="MemAddrContainer"/> class.
        /// </summary>
        /// <param name="memAddress">The memory address to store.</param>
        MemAddrContainer(void *memAddress)
            : m_memAddr(memAddress) {}

        MemAddress &GetMemoryAddress() { return m_memAddr; }

        const MemAddress &GetMemoryAddress() const { return m_memAddr; }
    };

    /// <summary>
    /// Represents a vertex in the directed graph of memory pieces
    /// (and a memory block region managed by the GC).
    /// </summary>
    class Vertex : public MemAddrContainer
    {
    private:

        ArrayOfEdges m_incomingEdges;
        FreeMemProc  m_freeMemCallback;
        uint32_t     m_blockSize;
        uint32_t     m_outEdgeCount;

        static utils::DynamicMemPool *dynMemPool;

    public:

        static void SetMemoryPool(utils::DynamicMemPool &ob);

        void *operator new(size_t);

        void operator delete(void *ptr);

        Vertex(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback);

		Vertex(const Vertex &) = delete;

        ~Vertex();

        bool Contains(void *someAddr) const;

        void IncrementOutgoingEdgeCount();

        void DecrementOutgoingEdgeCount();

        /// <summary>
        /// Adds an incoming edge from a root vertex.
        /// </summary>
        /// <param name="vtxRoot">The root vertex.</param>
        void ReceiveEdgeFrom(void *vtxRoot) { m_incomingEdges.AddEdge(vtxRoot); }

        /// <summary>
        /// Adds an incoming edge from a regular vertex.
        /// </summary>
        /// <param name="vtxRegular">The regular vertex.</param>
        void ReceiveEdgeFrom(Vertex *vtxRegular) { m_incomingEdges.AddEdge(vtxRegular); }

        /// <summary>
        /// Removes an incoming edge from root vertex.
        /// </summary>
        /// <param name="vtxRoot">The root vertex.</param>
        void RemoveEdgeFrom(void *vtxRoot) { m_incomingEdges.RemoveEdge(vtxRoot); }

        /// <summary>
        /// Removes an incoming edge from regular vertex.
        /// </summary>
        /// <param name="vtxRegular">The regular vertex.</param>
        void RemoveEdgeFrom(Vertex *vtxRegular) { m_incomingEdges.RemoveEdge(vtxRegular); }

        /// <summary>
        /// Iterates over each receiving edge from regular vertices in this array.
        /// This can only be used when this array has no edges with root vertices.
        /// </summary>
        /// <param name="callback">
        /// The callback to invoke for each vertex with an edge in this array.
        /// The iteration goes on while it returns <c>true</c>.
        /// </param>
        void ForEachRegularReceivingVertex(const std::function<bool(Vertex *)> &callback)
        {
            m_incomingEdges.ForEachRegular(callback);
        }
            
        /// <summary>
        /// Determines whether this vertex has any edge
        /// coming from a root vertex.
        /// </summary>
        /// <returns>
        /// <c>true</c> if this vertex is connected to at least
        /// one root vertex, otherwise, <c>false</c>.
        /// </returns>
        bool HasRootEdges() const { return m_incomingEdges.HasRootEdges(); }
            
        bool HasAnyEdges() const;

        void Mark(bool on);

        bool IsMarked() const;

        void ReleaseReprObjResources(bool destroy);

        bool AreReprObjResourcesReleased() const;
    };

    bool IsReachable(Vertex *vtx);

    /// <summary>
    /// Implementation of functor for std::set{MemAddrContainer *} that takes into consideration 
    /// the address of the represented memory pieces, rather than the addresses of the actual
    /// <see cref="MemAddrContainer"/> objects.
    /// </summary>
    struct LessOperOnVertexRepAddr
    {
        bool operator()(MemAddrContainer *left, MemAddrContainer *right) const
        {
            return left->GetMemoryAddress().Get()
                < right->GetMemoryAddress().Get();
        }
    };

}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
