#include "stdafx.h"
#include "gc_memblock.h"

#include <vector>

namespace _3fd
{
	namespace unit_tests
	{
		/// <summary>
		/// Generic tests for <see cref="memory::MemAddress"/> class.
		/// </summary>
		TEST(Framework_MemoryGC_TestCase, MemAddress_Test)
		{
			auto ptr = new int(696);
			memory::MemAddress memAddress(ptr);

			// Upon initialization, both bit in positions 0 and 1 are deactivated:
			EXPECT_EQ(ptr, memAddress.Get());
			EXPECT_FALSE(memAddress.GetBit0());
			EXPECT_FALSE(memAddress.GetBit1());

			// Test bit 0 with bit 1 deactivated:
			memAddress.SetBit0(true);
			EXPECT_TRUE(memAddress.GetBit0());
			EXPECT_FALSE(memAddress.GetBit1());
			EXPECT_EQ(ptr, memAddress.Get());

			memAddress.SetBit0(false);
			EXPECT_FALSE(memAddress.GetBit0());
			EXPECT_FALSE(memAddress.GetBit1());
			EXPECT_EQ(ptr, memAddress.Get());

			memAddress.SetBit0(true);
			EXPECT_TRUE(memAddress.GetBit0());
			EXPECT_EQ(ptr, memAddress.Get());

			// Test bit 0 with bit 1 activated:
			memAddress.SetBit1(true);
			memAddress.SetBit0(false);
			EXPECT_FALSE(memAddress.GetBit0());
			EXPECT_TRUE(memAddress.GetBit1());
			EXPECT_EQ(ptr, memAddress.Get());

			memAddress.SetBit0(true);
			EXPECT_TRUE(memAddress.GetBit0());
			EXPECT_TRUE(memAddress.GetBit1());
			EXPECT_EQ(ptr, memAddress.Get());

			// Now deactivate bit 1 and test bit 0:
			memAddress.SetBit1(false);
			EXPECT_TRUE(memAddress.GetBit0());
			EXPECT_FALSE(memAddress.GetBit1());
			EXPECT_EQ(ptr, memAddress.Get());

			delete ptr;
		}

		/// <summary>
		/// Generic tests for <see cref="memory::Vertex"/> class.
		/// </summary>
		TEST(Framework_MemoryGC_TestCase, Vertex_Test)
		{
			using memory::Vertex;
			using memory::MemBlock;

			MemBlock derivedObj(this, sizeof *this, nullptr);
			Vertex &obj = derivedObj;

			ASSERT_FALSE(obj.IsReachable());

			const int n = 1024;
			std::vector<int> someVars(n, 696);
			std::vector<void *> fakePtrs(n);

			std::vector<MemBlock *> fromVertices(n),
									toVertices(n);

			// Memory pool for MemBlock's:
			utils::DynamicMemPool myPool(n, sizeof(MemBlock), 1.0F);
			MemBlock::SetMemoryPool(myPool);

			// Generate some fake MemBlock's:
			for (int idx = 0; idx < n; ++idx)
				fromVertices[idx] = new MemBlock(this, sizeof *this, nullptr);

			// Add edges FROM regular vertices:
			for (auto vtx : fromVertices)
				obj.ReceiveEdge(vtx);

			uint32_t count(0);
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(0, count);

			ASSERT_FALSE(obj.IsReachable());

			// Add edges TO other vertices:
			for (auto vtx : toVertices)
				obj.StartEdge(vtx);

			count = 0;
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(toVertices.size(), count);

			ASSERT_FALSE(obj.IsReachable());

			// Add edges FROM root vertices:
			for (int idx = 0; idx < n; ++idx)
			{
				fakePtrs[idx] = &someVars[idx];
				obj.ReceiveEdge(fakePtrs[idx]);
			}

			count = 0;
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(toVertices.size(), count);

			ASSERT_TRUE(obj.IsReachable());

			// Remove all edges:
			obj.RemoveAllEdges();
			ASSERT_FALSE(obj.IsReachable());

			count = 0;
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(0, count);

			// Once again, add regular edges FROM other vertices:
			for (auto vtx : fromVertices)
				obj.ReceiveEdge(vtx);

			count = 0;
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(0, count);

			ASSERT_FALSE(obj.IsReachable());

			// Once again, add edges TO other vertices:
			for (auto vtx : toVertices)
				obj.StartEdge(vtx);

			count = 0;
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(toVertices.size(), count);

			ASSERT_TRUE(obj.IsReachable());

			// Once again, add root edges:
			for (int idx = 0; idx < n; ++idx)
				obj.ReceiveEdge(fakePtrs[idx]);

			count = 0;
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(toVertices.size(), count);

			ASSERT_TRUE(obj.IsReachable());

			// Remove receiving regular edges:
			for (auto vtx : fromVertices)
				obj.RemoveReceivingEdge(vtx);

			count = 0;
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(toVertices.size(), count);

			ASSERT_TRUE(obj.IsReachable());

			// Remove starting edges:
			for (auto vtx : toVertices)
				obj.RemoveStartingEdge(vtx);

			count = 0;
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(0, count);

			ASSERT_TRUE(obj.IsReachable());

			// Remove root edges:
			for (auto ptr : fakePtrs)
				obj.RemoveReceivingEdge(ptr);

			count = 0;
			obj.ForEachStartingEdge([&count](Vertex &vtx){ ++count; });
			ASSERT_EQ(0, count);

			ASSERT_FALSE(obj.IsReachable());

			// Return MemBlock's to the pool:
			for (auto vtx : fromVertices)
				delete vtx;
		}

	}// end of namespace unit_tests
}// end of namespace _3fd
