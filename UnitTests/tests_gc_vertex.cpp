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

			EXPECT_EQ(ptr, memAddress.Get());
			EXPECT_FALSE(memAddress.GetBit0());

			memAddress.SetBit0(true);
			EXPECT_TRUE(memAddress.GetBit0());
			EXPECT_EQ(ptr, memAddress.Get());

			memAddress.SetBit0(false);
			EXPECT_FALSE(memAddress.GetBit0());
			EXPECT_EQ(ptr, memAddress.Get());

			memAddress.SetBit0(true);
			EXPECT_TRUE(memAddress.GetBit0());
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
			std::vector<MemBlock *> otherVertices(n);

			// Memory pool for MemBlock's:
			const size_t poolSize(512);
			utils::DynamicMemPool myPool(poolSize, sizeof(MemBlock), 1.0F);
			MemBlock::SetMemoryPool(myPool);

			// Generate some fake MemBlock's:
			for (int idx = 0; idx < n; ++idx)
				otherVertices[idx] = new MemBlock(this, sizeof *this, nullptr);

			// Add edges from regular vertices:
			for (auto vtx : otherVertices)
				obj.ReceiveEdge(vtx);

			ASSERT_FALSE(obj.IsReachable());

			// Add edges from root vertices:
			for (int idx = 0; idx < n; ++idx)
			{
				fakePtrs[idx] = &someVars[idx];
				obj.ReceiveEdge(fakePtrs[idx]);
			}

			ASSERT_TRUE(obj.IsReachable());

			// Remove all edges:
			obj.RemoveAllEdges();
			ASSERT_FALSE(obj.IsReachable());

			// Once again, add regular edges:
			for (auto vtx : otherVertices)
				obj.ReceiveEdge(vtx);

			ASSERT_FALSE(obj.IsReachable());

			// Once again, add root edges:
			for (int idx = 0; idx < n; ++idx)
				obj.ReceiveEdge(fakePtrs[idx]);

			ASSERT_TRUE(obj.IsReachable());

			// Remove regular edges:
			for (auto vtx : otherVertices)
				obj.RemoveEdge(vtx);

			ASSERT_TRUE(obj.IsReachable());

			// Remove root edges:
			for (auto ptr : fakePtrs)
				obj.RemoveEdge(ptr);

			ASSERT_FALSE(obj.IsReachable());

			// Return MemBlock's to the pool:
			for (auto vtx : otherVertices)
				delete vtx;
		}

	}// end of namespace unit_tests
}// end of namespace _3fd
