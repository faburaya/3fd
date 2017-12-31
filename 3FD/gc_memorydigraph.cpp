#include "stdafx.h"
#include "gc_memorydigraph.h"

#include <cassert>

namespace _3fd
{
namespace memory
{
    /// <summary>
    /// Shrinks the pool of <see cref="Vertex"/> objects.
    /// </summary>
    void MemoryDigraph::ShrinkVertexPool()
    {
        m_vertices.ShrinkPool();
    }

    /// <summary>
    /// Sets the connection between a pointer and its referred memory address,
    /// creating an edge in the graph.
    /// </summary>
    /// <param name="sptrObjHashTableElem">
    /// A hashtable element which represents the pointer.
    /// </param>
    /// <param name="pointedAddr">The referred memory address.</param>
    void MemoryDigraph::MakeReference(
        AddressesHashTable::Element &sptrObjHashTableElem,
        void *pointedAddr)
    {
        auto pointedMemBlock = m_vertices.GetVertex(pointedAddr);

        /* By the time the connection is to be set, the vertex representing
        the pointed memory block must already exist in the graph. If not present,
        that means it has been unappropriately collected too soon, or the vertex
        has been removed from the graph too early... */
        _ASSERTE(pointedMemBlock != nullptr);

        MakeReference(sptrObjHashTableElem, pointedMemBlock);
    }

    /// <summary>
    /// Sets the connection between a pointer and its referred memory address,
    /// creating an edge in the graph.
    /// </summary>
    /// <param name="sptrObjHashTableElem">
    /// A hashtable element which represents the pointer.
    /// </param>
    /// <param name="pointedAddr">
    /// The vertex representing the referred memory address.
    /// </param>
    void MemoryDigraph::MakeReference(
        AddressesHashTable::Element &sptrObjHashTableElem,
        Vertex *pointedMemBlock)
    {
        sptrObjHashTableElem.SetPointedMemBlock(pointedMemBlock);

        if (sptrObjHashTableElem.IsRoot())
            pointedMemBlock->ReceiveEdgeFrom(sptrObjHashTableElem.GetSptrObjectAddr());
        else
        {
            auto originatorVtx = sptrObjHashTableElem.GetContainerMemBlock();
            originatorVtx->IncrementOutgoingEdgeCount();
            pointedMemBlock->ReceiveEdgeFrom(originatorVtx);
        }
    }

    /// <summary>
    /// Determines whether this vertex is reachable by any root vertex
    /// using depth-first search algorithm.
    /// </summary>
    /// <returns>
    /// <c>true</c> if there is a path through with a root vertex
    /// can reach this vertex, otherwise, <c>false</c>.
    /// </returns>
    bool IsReachable(Vertex *memBlock)
    {
        if (memBlock->HasRootEdges())
            return true;

        // mark this vertex before going deeper
        memBlock->Mark(true);

        // iterate over the vertices of receiving edges:
        bool rootFound(false);
        memBlock->ForEachRegularReceivingVertex(
            [&rootFound](Vertex *recvEdgeVtx)
            {
                return rootFound =
                    !recvEdgeVtx->IsMarked() // prevent infinite recursion
                    && !recvEdgeVtx->AreReprObjResourcesReleased() // already collected, skip
                    && IsReachable(recvEdgeVtx); // recursive search
            }
        );

        // unmark this vertex before leaving
        memBlock->Mark(false);

        return rootFound;
    }

    /// <summary>
    /// Unsets the connection between a pointer and its referred memory address,
    /// changing the graph edges and vertices accordingly.
    /// </summary>
    /// <param name="sptrObjHashTableElem">
    /// A hashtable element which represents the pointer.
    /// </param>
    /// <param name="allowDtion">
    /// Whether the pointed object should have its destructor invoked just
    /// in case it is to be collected. The destruction must not be allowed
    /// when the object construction has failed due to a thrown exception.
    /// </param>
    /// <remarks>
    /// If the previously pointed memory block becomes unreachable as a result
    /// of this operation, the resources allocated to it will be released.
    /// </remarks>
    void MemoryDigraph::UnmakeReference(AddressesHashTable::Element &sptrObjHashTableElem, bool allowDtion)
    {
        // the receiving vertex is the pointed memory block, and has been cached
        auto receivingVtx = sptrObjHashTableElem.GetPointedMemBlock();
        sptrObjHashTableElem.SetPointedMemBlock(nullptr);

        if (receivingVtx == nullptr)
            return;

        if (sptrObjHashTableElem.IsRoot())
            receivingVtx->RemoveEdgeFrom(sptrObjHashTableElem.GetSptrObjectAddr());
        else
        {
            auto originatorVtx = sptrObjHashTableElem.GetContainerMemBlock();
            originatorVtx->DecrementOutgoingEdgeCount();
            receivingVtx->RemoveEdgeFrom(originatorVtx);

            /* if no longer starts or receives any edge, then
            this vertex became isolated in the graph and can
            be safely returned to the object pool... */
            if (!originatorVtx->HasAnyEdges())
            {
                /* ... but the represented object resources have
                to be released before this vertex disappears */
                _ASSERTE(originatorVtx->AreReprObjResourcesReleased());
                delete originatorVtx;
            }
        }

        if (!receivingVtx->AreReprObjResourcesReleased())
        {
            /* If the memory block has just now became unreachable,
            release its resources and remove it from the graph: */
            if (!IsReachable(receivingVtx))
            {
                // First remove the vertex from the ordered set of vertices...
                m_vertices.RemoveVertex(receivingVtx);

                /* When the piece of memory represented by this vertex is
                released, the data member in the vertex object that holds
                its memory address is set to zero. Because the ordered set
                of vertices organizes the elements by such address, that
                should not be altered before anything that performs a search
                in the set, like the removal performed in the line above. */
                receivingVtx->ReleaseReprObjResources(allowDtion);

                /* if isolated in the graph, it can be
                safely returned to the object pool... */
                if (!receivingVtx->HasAnyEdges())
                    delete receivingVtx;
            }
        }
        /* otherwise, if the memory block was already unreachable, just
        check if the corresponding vertex is isolated in the graph, hence
        able to be safely returned to the object pool: */
        else if (!receivingVtx->HasAnyEdges())
        {
            delete receivingVtx;
        }
    }

    /// <summary>
    /// Adds a new vertex to the graph.
    /// </summary>
    /// <param name="memAddr">The memory address represented by the new vertex.</param>
    /// <param name="blockSize">Size of the represented memory block.</param>
    /// <param name="freeMemCallback">The callback that frees the memory block.</param>
    void MemoryDigraph::AddRegularVertex(void *memAddr, size_t blockSize, FreeMemProc freeMemCallback)
    {
        m_vertices.AddVertex(memAddr, blockSize, freeMemCallback);
    }

    /// <summary>
    /// Adds a new pointer to the graph.
    /// </summary>
    /// <param name="pointerAddr">The address of the pointer object.</param>
    /// <param name="pointedAddr">The memory address referred by the pointer.</param>
    void MemoryDigraph::AddPointer(void *pointerAddr, void *pointedAddr)
    {
        /* The edge origin is always a pointer. If the pointer is not a root
        vertex, find the container vertex, which is a piece of memory containing
        the pointer, so the connection to the pointed memory address will be set
        using the container rather than the pointer: */
        auto containerMemBlock = m_vertices.GetContainerVertex(pointerAddr); // null if root
        auto &sptrObjHashTableElem = m_sptrObjects.Insert(pointerAddr, nullptr, containerMemBlock);

        if (pointedAddr != nullptr)
            MakeReference(sptrObjHashTableElem, pointedAddr);
    }

    /// <summary>
    /// Adds a new pointer (constructed as a copy) to the graph.
    /// </summary>
    /// <param name="leftPointerAddr">The address of the pointer object to add.</param>
    /// <param name="rightPointerAddr">The address of the pointer object being copied.</param>
    void MemoryDigraph::AddPointerOnCopy(void *leftPointerAddr, void *rightPointerAddr)
    {
        /* The edge origin is always a pointer. If the pointer is not a root
        vertex, find the container vertex, which is a piece of memory containing
        the pointer, so the connection to the pointed memory address will be set
        using the container rather than the pointer: */
        auto containerMemBlock = m_vertices.GetContainerVertex(leftPointerAddr); // null if root

        auto &leftSptrObjHTabElem = m_sptrObjects.Insert(leftPointerAddr, nullptr, containerMemBlock);

        auto &rightSptrObjHTabElem = m_sptrObjects.Lookup(rightPointerAddr);

        auto receivingVtx = rightSptrObjHTabElem.GetPointedMemBlock();

        if (receivingVtx != nullptr)
            MakeReference(leftSptrObjHTabElem, receivingVtx);
    }

    /// <summary>
    /// Resets a given pointer to the memory address
    /// of a newly created object (never assigned before).
    /// </summary>
    /// <param name="pointerAddr">The address of the pointer object.</param>
    /// <param name="newPointedAddr">The new pointed memory address.</param>
    /// <param name="allowDtion">
    /// Whether the pointed object should have its destructor invoked just
    /// in case it is to be collected. The destruction must not be allowed
    /// when the object construction has failed due to a thrown exception.
    /// </param>
    void MemoryDigraph::ResetPointer(void *pointerAddr, void *newPointedAddr, bool allowDtion)
    {
        auto &sptrObjHashTableElem = m_sptrObjects.Lookup(pointerAddr);

        UnmakeReference(sptrObjHashTableElem, allowDtion);

        if (newPointedAddr != nullptr)
            MakeReference(sptrObjHashTableElem, newPointedAddr);
    }

    /// <summary>
    /// Resets a given pointer to the memory address
    /// of an object previously assigned to another pointer.
    /// </summary>
    /// <param name="pointerAddr">The address of the pointer object.</param>
    /// <param name="otherPointerAddr">The address of the other pointer object.</param>
    void MemoryDigraph::ResetPointer(void *pointerAddr, void *otherPointerAddr)
    {
        auto &leftSptrObjHashTabElem = m_sptrObjects.Lookup(pointerAddr);
        auto &rightSptrObjHashTabElem = m_sptrObjects.Lookup(otherPointerAddr);

        /* The reason this overload exists is that, differently from
        the other implementation of ResetPointer, where the vertex of
        the newly pointed address must be searched in the graph, here
        we can get this vertex cached with the sptr in the hash table.

        This allows the retrieval of vertices which no longer exists
        in the graph due to a previous collection, which despite not
        being the most common scenario, it is still possible. */
        auto newlyPointedMemBlock = rightSptrObjHashTabElem.GetPointedMemBlock();

        UnmakeReference(leftSptrObjHashTabElem, true);

        if (newlyPointedMemBlock != nullptr)
            MakeReference(leftSptrObjHashTabElem, newlyPointedMemBlock);
    }

    /// <summary>
    /// Releases the reference a pointer holds to a memory address.
    /// </summary>
    /// <param name="pointerAddr">The address of the pointer object.</param>
    void MemoryDigraph::ReleasePointer(void *pointerAddr)
    {
        auto &sptrObjHashTableElem = m_sptrObjects.Lookup(pointerAddr);
        UnmakeReference(sptrObjHashTableElem, true);
    }

    /// <summary>
    /// Removes a given pointer from the graph.
    /// </summary>
    /// <param name="pointerAddr">The address of the pointer object.</param>
    void MemoryDigraph::RemovePointer(void *pointerAddr)
    {
        auto &sptrObjHashTableElem = m_sptrObjects.Lookup(pointerAddr);
        UnmakeReference(sptrObjHashTableElem, true);
        m_sptrObjects.Remove(sptrObjHashTableElem);
    }

}// end of namespace memory
}// end of namespace _3fd
