#ifndef GC_MASTERTABLE_H // header guard
#define GC_MASTERTABLE_H

#include "gc_common.h"
#include "gc_memorydigraph.h"
#include "gc_addresseshashtable.h"

namespace _3fd
{
	namespace memory
	{
		/// <summary>
		/// Master table of memory addresses and sptr objects
		/// </summary>
		class MasterTable : notcopiable
		{
		private:

			MemoryDigraph		m_memDigraph; 
			AddressesHashTable	m_sptrObjects; /* A hash table here might improve performance and
											   can be used because it does not have to be sorted */

			void MakeReference(AddressesHashTable::Element *sptrObjHashTableElem, void *pointedAddr);

			void UnmakeReference(AddressesHashTable::Element *sptrObjHashTableElem, bool allowDestruction);

		public:

			void Shrink();

			void DoRegisterMemAddr(void *addr, size_t blockSize, FreeMemProc freeMemCallback) throw();

			void DoRegisterSptr(void *sptrObjAddr, void *pointedAddr) throw();

			void DoUnregisterSptr(void *sptrObjAddr) throw();

			void DoUnregisterMemAddr(void *pointedAddr) throw();

			void DoUpdateReference(void *sptrObjAddr, void *pointedAddr, bool allowRefObjDtion = true) throw();
		};

	}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
