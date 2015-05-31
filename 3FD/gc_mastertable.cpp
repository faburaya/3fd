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
			// If the reference is not null, look for the corresponding MemBlock object and sets a connection
			if (pointedAddr != 0)
			{
				MemAddrContainer *receiver = m_memDigraph.GetVertex(pointedAddr),
								 *originator = sptrObjHashTableElem->GetContainerMemBlock();

				if (originator == nullptr)
					// Sets the connection with the safe pointer ('root vertex' or 'access point')
					m_memDigraph.AddEdge(sptrObjHashTableElem->GetSptrObjectAddr(), receiver);
				else
					/* It is not a safe pointer ('root vertex' or 'access point'), so set up a connection
					between the container memory address and the pointed memory address: */
					m_memDigraph.AddEdge(originator, receiver);
			}

			sptrObjHashTableElem->SetAddrPointed(pointedAddr);
		}

		/// <summary>
		/// Unsets in master table the connection between a safe pointer and its referred memory address.
		/// </summary>
		/// <param name="sptrObjHashTableElem">A hashtable element which represents the safe pointer.</param>
		/// <param name="allowDestruction">Whether the pointed object should have its destructor invoked just in case it is to be collected.
		/// The destruction must not be allowed when the object construction has failed due to a thrown exception.</param>
		void MasterTable::UnmakeReference(AddressesHashTable::Element *sptrObjHashTableElem, bool allowDestruction)
		{
			void *pointedAddr = sptrObjHashTableElem->GetPointedAddr();

			// If the sptr object currently references an address, unmake the reference:
			if (pointedAddr != 0)
			{
				MemAddrContainer *receiver = m_memDigraph.GetVertex(pointedAddr);

				if (receiver != nullptr)
				{
					MemAddrContainer *originator = sptrObjHashTableElem->GetContainerMemBlock();

					// Undo the reference of the sptr object:
					if (originator == nullptr)
						m_memDigraph.RemoveEdge(sptrObjHashTableElem->GetSptrObjectAddr(), receiver, allowDestruction);
					else
						m_memDigraph.RemoveEdge(originator, receiver, allowDestruction);
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
			auto container = m_memDigraph.GetContainerVertex(sptrObjAddr);
			auto &sptrObjHashTableElem = m_sptrObjects.Insert(sptrObjAddr, nullptr, container);
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
		/// <param name="allowRefObjDtion">Whether the referred object should have its destructor invoked just in case it is to be collected.
		/// The destruction must not be allowed when the object construction has failed due to a thrown exception.</param>
		void MasterTable::DoUpdateReference(void *sptrObjAddr, void *pointedAddr, bool allowRefObjDtion) throw()
		{
			auto &sptrObjHashTableElem = m_sptrObjects.Lookup(sptrObjAddr);
			UnmakeReference(&sptrObjHashTableElem, allowRefObjDtion);
			MakeReference(&sptrObjHashTableElem, pointedAddr);
		}

	}// end of namespace _3fd
}// end of namespace memory
