#include "stdafx.h"
#include "runtime.h"
#include "gc_addresseshashtable.h"
#include "gc_memblock.h"

#include <vector>

namespace _3fd
{
	namespace unit_tests
	{
		struct Dummy
		{
			void *ptr;
			int someVar;
			memory::MemBlock memBlock;

			Dummy() :
				someVar(42),
				memBlock(this, sizeof *this, nullptr)
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
				auto &ref = hashtable.Insert(&entry.ptr, entry.ptr, &entry.memBlock);

				EXPECT_EQ(&entry.ptr, ref.GetSptrObjectAddr());
				EXPECT_EQ(entry.ptr, ref.GetPointedAddr());
				EXPECT_EQ(entry.memBlock.GetMemoryAddress().Get(),
					ref.GetContainerMemBlock()->GetMemoryAddress().Get());
			}

			// Try retrieving the entries:
			for (auto &entry : entries)
			{
				auto &ref = hashtable.Lookup(&entry.ptr);

				EXPECT_EQ(&entry.ptr, ref.GetSptrObjectAddr());
				EXPECT_EQ(entry.ptr, ref.GetPointedAddr());
				EXPECT_EQ(entry.memBlock.GetMemoryAddress().Get(),
					ref.GetContainerMemBlock()->GetMemoryAddress().Get());

				hashtable.Remove(&entry.ptr);
			}
		}

	}// end of namespace unit_tests
}// end of namespace _3fd
