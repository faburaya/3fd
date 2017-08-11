#include "stdafx.h"
#include "gc_vertex.h"

#include <algorithm>
#include <vector>

namespace _3fd
{
namespace unit_tests
{
	/// <summary>
	/// Tests resource management in a graph of linked <see cref="Vertex" />
	/// objects coming from the same pool.
	/// </summary>
	TEST(Framework_MemoryGC_TestCase, Vertex_ResourceManTest)
	{
		using namespace memory;

		// Sets the memory pool:

		const size_t poolSize(512);

		utils::DynamicMemPool myPool(poolSize, sizeof(Vertex), 1.0F);

		Vertex::SetMemoryPool(myPool);

		// Creates a 'graph' which is a chain of memory blocks:

		std::vector<Vertex *> vertices(poolSize * 2.5);

        size_t index(0);
		std::generate(begin(vertices), end(vertices), [&index]()
		{
			auto vertex = new Vertex(reinterpret_cast<void *> (index), 42, nullptr);
			index += sizeof(void *);
			return vertex;
		});

		for (index = 0; index < vertices.size() - 1; ++index)
			vertices[index]->ReceiveEdgeFrom(vertices[index + 1]);

		// Get rid of half the graph, then shrink resource usage:

		const auto half = vertices.size() / 2;

		while (vertices.size() > half)
		{
			auto previousBack = vertices.back();
			vertices.pop_back();
			vertices.back()->RemoveEdgeFrom(previousBack);
			delete previousBack;
		}

		myPool.Shrink();

		// Expand the graph again:
        index = reinterpret_cast<size_t> (vertices.back()->GetMemoryAddress().Get());
		while (vertices.size() < poolSize * 2)
		{
			index += sizeof(void *);

			vertices.push_back(
				new Vertex(reinterpret_cast<void *> (index), 42, nullptr)
			);
		}

		// Now get rid of all vertices:
		for (auto vtx : vertices)
			delete vtx;
	}

	/// <summary>
	/// Tests a <see cref="Vertex"/> handling
	/// the resouces of its represented object.
	/// </summary>
	TEST(Framework_MemoryGC_TestCase, Vertex_ObjResourcesTest)
	{
		using namespace memory;

#	ifdef _WIN32
		auto ptr = (int *)_aligned_malloc(sizeof (int), 2);
#	else
		auto ptr = (int *)aligned_alloc(2, sizeof (int));
#	endif
		Vertex memBlock(ptr, sizeof *ptr, &FreeMemAddr<int>);

		EXPECT_EQ(ptr, memBlock.GetMemoryAddress().Get());
		EXPECT_FALSE(memBlock.AreReprObjResourcesReleased());

		memBlock.ReleaseReprObjResources(true);

		EXPECT_TRUE(memBlock.AreReprObjResourcesReleased());
	}

	/// <summary>
	/// Tests reachability analysis in a graph of linked
	/// <see cref="Vertex"/> objects in a chain.
	/// </summary>
	TEST(Framework_MemoryGC_TestCase, Vertex_GraphReachabilityAnalysis_ChainTest)
	{
		using namespace memory;

		// Sets the memory pool:
		const size_t poolSize(16);
		utils::DynamicMemPool myPool(poolSize, sizeof(Vertex), 1.0F);
		Vertex::SetMemoryPool(myPool);

		// Creates a 'graph' which is a chain of memory blocks:

		std::vector<Vertex *> vertices(poolSize * 1.5);

		int index(0);
		std::generate(begin(vertices), end(vertices), [&index]()
		{
			auto vertex = new Vertex(reinterpret_cast<void *> (index), 42, nullptr);
			index += sizeof (void *);
			return vertex;
		});

		// All vertices are regular and unreachable:
		for (auto vtx : vertices)
		{
			EXPECT_FALSE(vtx->IsMarked());
			EXPECT_FALSE(vtx->HasRootEdges());
			EXPECT_FALSE(vtx->HasAnyEdges());
			EXPECT_FALSE(IsReachable(vtx));
		}

		// Create a chain of regular vertices:
		for (index = 0; index < vertices.size() - 1; ++index)
		{
			auto thisVtx = vertices[index];
			auto nextVtx = vertices[index + 1];
				
			thisVtx->ReceiveEdgeFrom(nextVtx);
			nextVtx->IncrementOutgoingEdgeCount();
		}

		// Because no root vertex has been added, all vertices are unreachable:
		index = 0;
		for (auto vtx : vertices)
		{
			EXPECT_FALSE(vtx->IsMarked());
			EXPECT_FALSE(vtx->HasRootEdges());
			EXPECT_TRUE(vtx->HasAnyEdges());
			EXPECT_FALSE(IsReachable(vtx));
		}

		// The addition of an edge from a root vertex at the end of the chain should make everyone reachable:
		void *fakeRootVtx = &index;
		vertices.back()->ReceiveEdgeFrom(fakeRootVtx);

		index = 0;
		for (auto vtx : vertices)
		{
			EXPECT_FALSE(vtx->IsMarked());
			EXPECT_EQ((index++ == vertices.size() - 1), vtx->HasRootEdges());
			EXPECT_TRUE(vtx->HasAnyEdges());
			EXPECT_TRUE(IsReachable(vtx));
		}

		// The removal of the single root vertex should make everyone unreachable:
		vertices.back()->RemoveEdgeFrom(fakeRootVtx);

		index = 0;
		for (auto vtx : vertices)
		{
			EXPECT_FALSE(vtx->IsMarked());
			EXPECT_FALSE(vtx->HasRootEdges());
			EXPECT_TRUE(vtx->HasAnyEdges());
			EXPECT_FALSE(IsReachable(vtx));
		}

		/* Now the root vertex will point to a vertex in the
		middle of the chain, making only half of it reachable: */

		const auto half = vertices.size() / 2;
		vertices[half]->ReceiveEdgeFrom(fakeRootVtx);

		for (index = 0; index <= half; ++index)
			EXPECT_TRUE(IsReachable(vertices[index]));

		for (; index < vertices.size(); ++index)
			EXPECT_FALSE(IsReachable(vertices[index]));

		// Return vertices to the pool:
		for (auto vtx : vertices)
			delete vtx;
	}

	/// <summary>
	/// Tests reachability analysis in a graph of
	/// linked <see cref="Vertex"/> objects, with a cycle.
	/// </summary>
	TEST(Framework_MemoryGC_TestCase, Vertex_GraphReachabilityAnalysis_CycleTest)
	{
		using namespace memory;

		// Sets the memory pool:
		const size_t poolSize(16);
		utils::DynamicMemPool myPool(poolSize, sizeof(Vertex), 1.0F);
		Vertex::SetMemoryPool(myPool);

		// Creates a 'graph' which is a chain of memory blocks:

		std::vector<Vertex *> vertices(poolSize * 1.5);

		int index(0);
		std::generate(begin(vertices), end(vertices), [&index]()
		{
			auto vertex = new Vertex(reinterpret_cast<void *> (index), 42, nullptr);
			index += sizeof(void *);
			return vertex;
		});

		// Create a chain of regular vertices:
		for (index = 0; index < vertices.size() - 1; ++index)
		{
			auto thisVtx = vertices[index];
			auto nextVtx = vertices[index + 1];

			thisVtx->ReceiveEdgeFrom(nextVtx);
			nextVtx->IncrementOutgoingEdgeCount();
		}

		// Close a cycle making the end of the chain receive an edge from the middle:
		const auto middle = vertices.size() / 2;
		vertices.back()->ReceiveEdgeFrom(vertices[middle]);
		vertices[middle]->IncrementOutgoingEdgeCount();
			
		// Because no root vertex has been added, all vertices are unreachable:
		for (auto vtx : vertices)
		{
			EXPECT_FALSE(vtx->IsMarked());
			EXPECT_FALSE(vtx->HasRootEdges());
			EXPECT_TRUE(vtx->HasAnyEdges());
			EXPECT_FALSE(IsReachable(vtx));
		}

		// The addition of an edge from a root vertex at the end of the chain should make everyone reachable:
		void *fakeRootVtx = &index;
		vertices.back()->ReceiveEdgeFrom(fakeRootVtx);

		index = 0;
		for (auto vtx : vertices)
		{
			EXPECT_FALSE(vtx->IsMarked());
			EXPECT_EQ((index++ == vertices.size() - 1), vtx->HasRootEdges());
			EXPECT_TRUE(vtx->HasAnyEdges());
			EXPECT_TRUE(IsReachable(vtx));
		}

		// The removal of the single root vertex should make everyone unreachable:
		vertices.back()->RemoveEdgeFrom(fakeRootVtx);

		for (auto vtx : vertices)
		{
			EXPECT_FALSE(vtx->IsMarked());
			EXPECT_FALSE(vtx->HasRootEdges());
			EXPECT_TRUE(vtx->HasAnyEdges());
			EXPECT_FALSE(IsReachable(vtx));
		}

		// Return vertices to the pool:
		for (auto vtx : vertices)
			delete vtx;
	}

}// end of namespace unit_tests
}// end of namespace _3fd
