#include "stdafx.h"
#include "runtime.h"
#include "gc_addresseshashtable.h"
#include "gc_vertex.h"

#include <vector>

namespace _3fd
{
	namespace unit_tests
	{
		using memory::Vertex;

		struct Dummy
		{
			int someVar1,
				someVar2,
				someVar3;

			void *ptr;
			Vertex *pointedVtx;
			Vertex *containerVtx;

			Dummy() :
				someVar1(42),
				someVar2(42),
				someVar3(42)
			{
				ptr = &someVar1;
				pointedVtx = (Vertex *)&someVar2;
				containerVtx = (Vertex *)&someVar3;
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
				auto &ref = hashtable.Insert(&entry.ptr, entry.pointedVtx, entry.containerVtx);

				EXPECT_EQ(&entry.ptr, ref.GetSptrObjectAddr());
				EXPECT_EQ(entry.pointedVtx, ref.GetPointedMemBlock());
				EXPECT_EQ(entry.containerVtx, ref.GetContainerMemBlock());
				EXPECT_FALSE(ref.IsRoot());
			}

			// Try retrieving the entries:
			for (auto &entry : entries)
			{
				auto &ref = hashtable.Lookup(&entry.ptr);

				EXPECT_EQ(&entry.ptr, ref.GetSptrObjectAddr());
				EXPECT_EQ(entry.pointedVtx, ref.GetPointedMemBlock());
				EXPECT_EQ(entry.containerVtx, ref.GetContainerMemBlock());
				EXPECT_FALSE(ref.IsRoot());

				hashtable.Remove(&entry.ptr);
			}

			// Do it all over again, for "root" elements:

			for (auto &entry : entries)
			{
				auto &ref = hashtable.Insert(&entry.ptr, entry.pointedVtx, nullptr);

				EXPECT_EQ(&entry.ptr, ref.GetSptrObjectAddr());
				EXPECT_EQ(entry.pointedVtx, ref.GetPointedMemBlock());
				EXPECT_EQ(nullptr, ref.GetContainerMemBlock());
				EXPECT_TRUE(ref.IsRoot());
			}

			for (auto &entry : entries)
			{
				auto &ref = hashtable.Lookup(&entry.ptr);

				EXPECT_EQ(&entry.ptr, ref.GetSptrObjectAddr());
				EXPECT_EQ(entry.pointedVtx, ref.GetPointedMemBlock());
				EXPECT_EQ(nullptr, ref.GetContainerMemBlock());
				EXPECT_TRUE(ref.IsRoot());

				hashtable.Remove(ref);
			}
		}

	}// end of namespace unit_tests
}// end of namespace _3fd
