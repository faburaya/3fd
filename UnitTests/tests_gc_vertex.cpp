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
			EXPECT_FALSE(memAddress.isMarked());

			memAddress.Mark(true);
			EXPECT_TRUE(memAddress.isMarked());
			EXPECT_EQ(ptr, memAddress.Get());

			memAddress.Mark(false);
			EXPECT_FALSE(memAddress.isMarked());
			EXPECT_EQ(ptr, memAddress.Get());

			delete ptr;
		}

		/// <summary>
		/// Generic tests for <see cref="memory::Vertex"/> class.
		/// </summary>
		TEST(Framework_MemoryGC_TestCase, Vertex_Test)
		{
			using memory::Vertex;

			Vertex obj;

			ASSERT_FALSE(obj.HasRootEdge());

			const int n = 32;
			std::vector<int> someVars(n, 696);
			std::vector<void *> fakePtrs(n);
			std::vector<Vertex> otherVertices(n);

			// Add edges from regular vertices:
			for (auto &vtx : otherVertices)
				obj.ReceiveEdge(&vtx);

			ASSERT_FALSE(obj.HasRootEdge());

			// Count edges from regular vertices:
			int count(0);
			obj.ForEachRegularEdgeWhile([&count](const Vertex &vtx)
			{
				++count;
				return true;
			});

			ASSERT_EQ(n, count);

			// Add edges from root vertices:
			for (int idx = 0; idx < n; ++idx)
			{
				fakePtrs[idx] = &someVars[idx];
				obj.ReceiveEdge(fakePtrs[idx]);
			}

			ASSERT_TRUE(obj.HasRootEdge());

			// Count regular edges:
			count = 0;
			obj.ForEachRegularEdgeWhile([&count](const Vertex &vtx)
			{
				++count;
				return true;
			});

			ASSERT_EQ(n, count);

			// Remove all edges:
			obj.RemoveAllEdges();
			ASSERT_FALSE(obj.HasRootEdge());

			// Count regular edges:
			count = 0;
			obj.ForEachRegularEdgeWhile([&count](const Vertex &vtx)
			{
				++count;
				return true;
			});

			ASSERT_EQ(0, count);

			// Once again, add regular edges:
			for (auto &vtx : otherVertices)
				obj.ReceiveEdge(&vtx);

			ASSERT_FALSE(obj.HasRootEdge());

			// Count regular edges:
			count = 0;
			obj.ForEachRegularEdgeWhile([&count](const Vertex &vtx)
			{
				++count;
				return true;
			});

			ASSERT_EQ(n, count);

			// Once again, add root edges:
			for (int idx = 0; idx < n; ++idx)
				obj.ReceiveEdge(fakePtrs[idx]);

			ASSERT_TRUE(obj.HasRootEdge());

			// Count regular edges:
			count = 0;
			obj.ForEachRegularEdgeWhile([&count](const Vertex &vtx)
			{
				++count;
				return true;
			});

			ASSERT_EQ(n, count);

			// Remove regular edges:
			for (auto &vtx : otherVertices)
				obj.RemoveEdge(&vtx);

			ASSERT_TRUE(obj.HasRootEdge());

			// Count regular edges:
			count = 0;
			obj.ForEachRegularEdgeWhile([&count](const Vertex &vtx)
			{
				++count;
				return true;
			});

			ASSERT_EQ(0, count);

			// Remove root edges:
			for (auto ptr : fakePtrs)
				obj.RemoveEdge(ptr);

			ASSERT_FALSE(obj.HasRootEdge());

			// Count regular edges:
			count = 0;
			obj.ForEachRegularEdgeWhile([&count](const Vertex &vtx)
			{
				++count;
				return true;
			});

			ASSERT_EQ(0, count);
		}

	}// end of namespace unit_tests
}// end of namespace _3fd
