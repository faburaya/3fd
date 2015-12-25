#include "stdafx.h"
#include "runtime.h"
#include "gc_vertexstore.h"
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
		/// Tests <see cref="memory::VertexStore"/> class for retrieval of vertices and container logic.
		/// </summary>
		TEST(Framework_MemoryGC_TestCase, VertexStore_RetrievalTest)
		{
			// Ensures proper initialization/finalization of the framework
			core::FrameworkInstance _framework;

			const int n = 128;
			std::vector<Stuffed *> addrs;
			memory::VertexStore vtxStore;

			// Add some vertices:
			addrs.reserve(n);
			for (int count = 0; count < n; ++count)
			{
				auto ptr = ALIGNED_NEW(Stuffed, ());
				addrs.push_back(ptr);
				vtxStore.AddVertex(ptr, sizeof *ptr, &memory::FreeMemAddr<Stuffed>);
			}

			// Try retriving the vertices:
			for (auto ptr : addrs)
			{
				auto vtx = vtxStore.GetVertex(ptr);
				EXPECT_EQ(ptr, vtx->GetMemoryAddress().Get());
			}

			// Try retrieving container vertices:
			for (auto ptr : addrs)
			{
				auto vtx1 = vtxStore.GetContainerVertex(&(ptr->low));
				auto vtx2 = vtxStore.GetContainerVertex(&(ptr->middle));
				auto vtx3 = vtxStore.GetContainerVertex(&(ptr->high));

				EXPECT_EQ(vtx1, vtx2);
				EXPECT_EQ(vtx2, vtx3);
				EXPECT_EQ(vtx3, vtx1);

				EXPECT_EQ(ptr, vtx1->GetMemoryAddress().Get());
			}

			// Remove all vertices:
			for (int idx = 0; idx < n; ++idx)
			{
				auto vtx = vtxStore.GetVertex(addrs[idx]);
				vtxStore.RemoveVertex(vtx);
				vtx->ReleaseReprObjResources(true);
				delete vtx;
			}
		}

	}// end of namespace unit_tests
}// end of namespace _3fd
