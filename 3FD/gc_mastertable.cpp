#include "stdafx.h"
#include "gc_memaddress.h"
#include "gc_mastertable.h"

namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// Shrinks the amount of memory allocated by the resources used by this master table.
		/// </summary>
		void MasterTable::Shrink()
		{
			m_memDigraph.ShrinkObjectPool();
		}

		/// <summary>
		/// Sets in master table the connection between a safe pointer and its referred memory address.
		/// </summary>
		/// <param name="sptrObjHashTableElem">A hashtable element which represents the safe pointer.</param>
		/// <param name="pointedAddr">The referred memory address.</param>
		void MasterTable::MakeReference(AddressesHashTable::Element *sptrObjHashTableElem, void *pointedAddr)
		{
			if (pointedAddr != 0)
			{
				if (sptrObjHashTableElem->IsRoot())
					m_memDigraph.AddRootEdge(sptrObjHashTableElem->GetSptrObjectAddr(), pointedAddr);
				else
					m_memDigraph.AddRegularEdge(sptrObjHashTableElem->GetSptrObjectAddr(), pointedAddr);
			}

			sptrObjHashTableElem->SetAddrPointed(pointedAddr);
		}

		/// <summary>
		/// Unsets in master table the connection between a safe pointer and its referred memory address.
		/// </summary>
		/// <param name="sptrObjHashTableElem">A hashtable element which represents the safe pointer.</param>
		/// <param name="allowDestruction">
		/// Whether the pointed object should have its destructor invoked just in case it is to be collected.
		/// The destruction must not be allowed when the object construction has failed due to a thrown exception.
		/// </param>
		void MasterTable::UnmakeReference(AddressesHashTable::Element *sptrObjHashTableElem, bool allowDestruction)
		{
			void *pointedAddr = sptrObjHashTableElem->GetPointedAddr();

			if (pointedAddr != 0)
			{
				if (sptrObjHashTableElem->IsRoot())
				{
					m_memDigraph.RemoveRootEdge(sptrObjHashTableElem->GetSptrObjectAddr(),
												sptrObjHashTableElem->GetPointedAddr(),
												allowDestruction);
				}
				else
				{
					m_memDigraph.RemoveRegularEdge(sptrObjHashTableElem->GetSptrObjectAddr(),
												   sptrObjHashTableElem->GetPointedAddr(),
												   allowDestruction);
				}
			}
		}

		/// <summary>
		/// Performs registering a new memory address to be managed by the GC.
		/// </summary>
		/// <param name="addr">The memory address.</param>
		/// <param name="blockSize">Size of the block.</param>
		/// <param name="freeMemCallback">The callback used to free the memory and destroy the object.</param>
		void MasterTable::DoRegisterMemAddr(void *addr, size_t blockSize, FreeMemProc freeMemCallback) throw()
		{
			m_memDigraph.AddVertex(addr, blockSize, freeMemCallback);
		}

		/// <summary>
		/// Performs registering a new <see cref="sptr"/> object to be tracked by the GC.
		/// </summary>
		/// <param name="sptrObjAddr">The safe pointer object address.</param>
		/// <param name="pointedAddr">The memory address referred by the safe pointer.</param>
		void MasterTable::DoRegisterSptr(void *sptrObjAddr, void *pointedAddr) throw()
		{
			bool isRoot = (m_memDigraph.GetContainerVertex(sptrObjAddr) == nullptr);
			auto &sptrObjHashTableElem = m_sptrObjects.Insert(isRoot, sptrObjAddr, nullptr);
			MakeReference(&sptrObjHashTableElem, pointedAddr);
		}

		/// <summary>
		/// Performs unregistering a <see cref="sptr"/> object which no longer will be tracked by the GC.
		/// </summary>
		/// <param name="sptrObjAddr">The safe pointer object address.</param>
		void MasterTable::DoUnregisterSptr(void *sptrObjAddr) throw()
		{
			auto &sptrObjHashTableElem = m_sptrObjects.Lookup(sptrObjAddr);
			UnmakeReference(&sptrObjHashTableElem, true);
			m_sptrObjects.Remove(sptrObjAddr);
		}

		/// <summary>
		/// Performs updating the register of a <see cref="sptr" /> object which was changed to refer to another memory address.
		/// </summary>
		/// <param name="sptrObjAddr">The safe pointer object address.</param>
		/// <param name="pointedAddr">The new memory address referred by the safe pointer.</param>
		/// <param name="allowRefObjDtion">
		/// Whether the referred object should have its destructor invoked just in case it is to be collected.
		/// The destruction must not be allowed when the object construction has failed due to a thrown exception.
		/// </param>
		void MasterTable::DoUpdateReference(void *sptrObjAddr, void *pointedAddr, bool allowRefObjDtion) throw()
		{
			auto &sptrObjHashTableElem = m_sptrObjects.Lookup(sptrObjAddr);
			UnmakeReference(&sptrObjHashTableElem, allowRefObjDtion);
			MakeReference(&sptrObjHashTableElem, pointedAddr);
		}

	}// end of namespace _3fd
}// end of namespace memory
