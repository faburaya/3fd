#include "stdafx.h"
#include "runtime.h"
#include "gc_addresseshashtable.h"

#include <vector>

namespace _3fd
{
	namespace unit_tests
	{
		struct Dummy
		{
			void *ptr;
			int someVar;
			memory::MemAddrContainer memAddrCtnr;

			Dummy() :
				someVar(42),
				memAddrCtnr(this)
			{
				ptr = &someVar;
			}
		};

		/// <summary>
		/// Tests <see cref="memory::AddressesHashTable"/> class for basic insertion / retrieval / removal.
		/// </summary>
		TEST(Framework_MemoryGC_TestCase, AddressesHashTable_BasicTest)
		{
			// Ensures proper initialization/finalization of the framework
			core::FrameworkInstance _framework;

			memory::AddressesHashTable hashtable;
			std::vector<Dummy> entries(4096);

			// Fill the hash table with some dummy entries:
			for (auto &entry : entries)
			{
				auto &ref = hashtable.Insert(&entry.ptr, entry.ptr, &entry.memAddrCtnr);

				EXPECT_EQ(&entry.ptr, ref.GetSptrObjectAddr());
				EXPECT_EQ(entry.ptr, ref.GetPointedAddr());
				EXPECT_EQ(entry.memAddrCtnr.memoryAddress().Get(),
					ref.GetContainerMemBlock()->memoryAddress().Get());
			}

			// Try retrieving the entries:
			for (auto &entry : entries)
			{
				auto &ref = hashtable.Lookup(&entry.ptr);

				EXPECT_EQ(&entry.ptr, ref.GetSptrObjectAddr());
				EXPECT_EQ(entry.ptr, ref.GetPointedAddr());
				EXPECT_EQ(entry.memAddrCtnr.memoryAddress().Get(),
					ref.GetContainerMemBlock()->memoryAddress().Get());

				hashtable.Remove(&entry.ptr);
			}
		}

	}// end of namespace unit_tests
}// end of namespace _3fd
