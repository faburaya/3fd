#include "stdafx.h"
#include "gc_memblock.h"

#include <algorithm>
#include <vector>

namespace _3fd
{
	namespace unit_tests
	{
		/// <summary>
		/// Tests resource management in a graph of linked <see cref="MemBlock" /> objects coming from the same pool.
		/// </summary>
		TEST(Framework_MemoryGC_TestCase, MemBlock_ResourceManTest)
		{
			using namespace memory;

			// Sets the memory pool:

			const size_t poolSize(512);

			utils::DynamicMemPool myPool(poolSize, sizeof(MemBlock), 1.0F);

			MemBlock::SetMemoryPool(myPool);

			// Creates a 'graph' which is a chain of memory blocks:

			std::vector<MemBlock *> memBlocks(poolSize * 2.5);

			int index(0);
			std::generate(begin(memBlocks), end(memBlocks), [&index]()
			{
				return new MemBlock(reinterpret_cast<void *> (index++), 42, nullptr);
			});

			for (index = 0; index < memBlocks.size() - 1; ++index)
				memBlocks[index]->ReceiveEdge(memBlocks[index + 1]);

			// Get rid of half the graph, then shrink resource usage:

			const auto half = memBlocks.size() / 2;

			while (memBlocks.size() > half)
			{
				auto previousBack = memBlocks.back();
				memBlocks.pop_back();
				memBlocks.back()->RemoveEdge(previousBack);
				delete previousBack;
			}

			myPool.Shrink();

			// Expand the graph again:
			while (memBlocks.size() < poolSize * 2)
			{
				memBlocks.push_back(
					new MemBlock(reinterpret_cast<void *> (memBlocks.size()), 42, nullptr)
				);
			}

			// Now get rid of all vertices:
			for (auto mblock : memBlocks)
				delete mblock;
		}

		/// <summary>
		/// Tests reachability analysis in a graph of linked <see cref="MemBlock" /> objects.
		/// </summary>
		TEST(Framework_MemoryGC_TestCase, MemBlock_GraphAlgorithmTest)
		{
			using namespace memory;

			// Sets the memory pool:

			const size_t poolSize(16);

			utils::DynamicMemPool myPool(poolSize, sizeof(MemBlock), 1.0F);

			MemBlock::SetMemoryPool(myPool);

			// Creates a 'graph' which is a chain of memory blocks:

			std::vector<MemBlock *> memBlocks(poolSize * 1.5);

			int index(0);

			std::generate(begin(memBlocks), end(memBlocks), [&index]()
			{
				return new MemBlock(reinterpret_cast<void *> (index++), 42, nullptr);
			});

			for (index = 0; index < memBlocks.size() - 1; ++index)
				memBlocks[index]->ReceiveEdge(memBlocks[index + 1]);

			// Because no root vertex has been added, all vertices are regular and unreachable:
			for (auto mblock : memBlocks)
				EXPECT_FALSE(mblock->IsReachable());

			// The addition of an edge from a root vertex at the end of the chain should make everyone reachable:
			void *fakeRootVtx = &index;
			memBlocks.back()->ReceiveEdge(fakeRootVtx);

			for (auto mblock : memBlocks)
				EXPECT_TRUE(mblock->IsReachable());

			// The removal of the single root vertex should make everyone unreachable:
			memBlocks.back()->RemoveEdge(fakeRootVtx);

			for (auto mblock : memBlocks)
				EXPECT_FALSE(mblock->IsReachable());

			// Now the root vertex will point to a vertex in the middle of tha chain, making only half of it reachable:

			const auto half = memBlocks.size() / 2;
			memBlocks[half]->ReceiveEdge(fakeRootVtx);

			for (index = 0; index <= half; ++index)
				EXPECT_TRUE(memBlocks[index]->IsReachable());

			for (; index < memBlocks.size(); ++index)
				EXPECT_FALSE(memBlocks[index]->IsReachable());

			// Return vertices to the pool:
			for (auto mblock : memBlocks)
				delete mblock;
		}

	}// end of namespace unit_tests
}// end of namespace _3fd
