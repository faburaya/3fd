#include "stdafx.h"
#include "gc_messages.h"

namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// Executes in the memory graph the action corresponding
		/// to the message <see cref="NewObjectMsg"/>.
		/// </summary>
		/// <param name="graph">A reference to the memory graph.</param>
		void NewObjectMsg::Execute(MemoryDigraph &graph)
		{
			graph.AddRegularVertex(m_pointedAddr, m_blockSize, m_freeMemCallback);
			graph.ResetPointer(m_sptrObjectAddr, m_pointedAddr, true);
		}

		/// <summary>
		/// Executes in the memory graph the action corresponding
		/// to the message <see cref="ReferenceUpdateMsg"/>.
		/// </summary>
		/// <param name="graph">A reference to the memory graph.</param>
		void ReferenceUpdateMsg::Execute(MemoryDigraph &graph)
		{
			/* due to an assignment betweeen pointesr, resets the pointer
			in the left to make it reference the same object referenced
			by the pointer in the right */
			graph.ResetPointer(m_leftSptrObjAddr, m_rightSptrObjAddr);
		}

		/// <summary>
		/// Executes in the memory graph the action corresponding
		/// to the message <see cref="ReferenceReleaseMsg"/>.
		/// </summary>
		/// <param name="graph">A reference to the memory graph.</param>
		void ReferenceReleaseMsg::Execute(MemoryDigraph &graph)
		{
			/* release the reference made by a pointer, but do not
			unregister it, because it still hasn't gone out of scope */
			graph.ReleasePointer(m_sptrObjAddr);
		}

		/// <summary>
		/// Executes in the memory graph the action corresponding
		/// to the message <see cref="AbortedObjectMsg"/>.
		/// </summary>
		/// <param name="graph">A reference to the memory graph.</param>
		void AbortedObjectMsg::Execute(MemoryDigraph &graph)
		{
			/* due to an object whose ctor failed with a thrown exception,
			make the pointer stop referencing the memory allocated for the
			object, but do not allow the dtor to be invoked, because in C++
			that does not happen to "semi-constructed" objects */
			graph.ResetPointer(m_sptrObjAddr, nullptr, false);
		}

		/// <summary>
		/// Executes in the memory graph the action corresponding
		/// to the message <see cref="SptrRegistrationMsg"/>.
		/// </summary>
		/// <param name="graph">A reference to the memory graph.</param>
		void SptrRegistrationMsg::Execute(MemoryDigraph &graph)
		{
			/* adds a new pointer to the graph, already making it
			refence a given memory address */
			graph.AddPointer(m_sptrObjAddr, m_pointedAddr);
		}

		/// <summary>
		/// Executes in the memory graph the action corresponding
		/// to the message <see cref="SptrCopyRegistrationMsg"/>.
		/// </summary>
		/// <param name="graph">A reference to the memory graph.</param>
		void SptrCopyRegistrationMsg::Execute(MemoryDigraph &graph)
		{
			/* adds a new pointer to the graph, which has been constructed
			as a copy of another pointer, so make the first reference the
			object already referenced by the second */
			graph.AddPointerOnCopy(m_leftSptrObjAddr, m_rightSptrObjAddr);
		}

		/// <summary>
		/// Executes in the memory graph the action corresponding
		/// to the message <see cref="SptrUnregistrationMsg"/>.
		/// </summary>
		/// <param name="graph">A reference to the memory graph.</param>
		void SptrUnregistrationMsg::Execute(MemoryDigraph &graph)
		{
			/* a pointer has gone out of scope, so remove it from the
			graph and undo the reference it makes to the pointed object */
			graph.RemovePointer(m_sptrObjAddr);
		}

	}// end of namespace memory
}// end of namespace _3fd
