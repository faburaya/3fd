#include "stdafx.h"
#include "runtime.h"
#include "gc_memorydigraph.h"
#include "gc_memblock.h"
#include "sptr.h"

#include <vector>

#ifdef _WIN32
#	define ALIGNED_NEW(TYPE, INITIALIZER) new (_aligned_malloc(sizeof TYPE, 2)) TYPE INITIALIZER
#else
#	define ALIGNED_NEW(TYPE, INITIALIZER) new (aligned_alloc(2, sizeof TYPE)) TYPE INITIALIZER
#endif

namespace _3fd
{
	namespace unit_tests
	{
		struct Stuffed
		{
			int low;
			int middle;
			int high;
		};

		/// <summary>
		/// Tests <see cref="memory::MemoryDigraph"/> class for retrieval of vertices and container logic.
		/// </summary>
		TEST(Framework_MemoryGC_TestCase, MemoryDigraph_RetrievalTest)
		{
			// Ensures proper initialization/finalization of the framework
			core::FrameworkInstance _framework;

			const int n = 128;
			std::vector<int> fakeRootVtsx(n, 696);
			std::vector<Stuffed *> addrs;
			memory::MemoryDigraph graph;

			// Add some vertices:
			for (int count = 0; count < n; ++count)
			{
				auto ptr = ALIGNED_NEW(Stuffed, ());
				addrs.push_back(ptr);
				auto vtx = graph.AddVertex(ptr, sizeof *ptr, &memory::FreeMemAddr<Stuffed>);
				graph.AddEdge(&fakeRootVtsx[count], vtx);
			}

			// Try retriving the vertices:
			for (auto ptr : addrs)
			{
				auto vtx = graph.GetVertex(ptr);
				EXPECT_EQ(ptr, vtx->GetMemoryAddress().Get());
			}

			// Try retrieving container vertices:
			for (auto ptr : addrs)
			{
				auto vtx = graph.GetContainerVertex(&(ptr->low));
				EXPECT_EQ(ptr, vtx->GetMemoryAddress().Get());
			}

			// Remove all edges from the graph, and the vertices will be gone too:
			for (int idx = 0; idx < n; ++idx)
			{
				auto vtx = graph.GetVertex(addrs[idx]);
				graph.RemoveEdge(&fakeRootVtsx[idx], vtx, true);
			}
		}

	}// end of namespace unit_tests
}// end of namespace _3fd
