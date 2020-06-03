//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/core/gc_arrayofedges.h>
#include <3fd/core/gc_vertex.h>

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
    /// Generic tests for <see cref="memory::ArrayOfEdges"/> class.
    /// </summary>
    TEST(Framework_MemoryGC_TestCase, ArrayOfEdges_Test)
    {
        using memory::Vertex;
        using memory::ArrayOfEdges;

        ArrayOfEdges array;
            
        ASSERT_EQ(0, array.Size());
        ASSERT_FALSE(array.HasRootEdges());

        uint32_t count(0);
        array.ForEachRegular([&count](Vertex *){ ++count; return true; });
        ASSERT_EQ(0, count);

        // Create dummy data:
        const int n = 1024;
        std::vector<int> someVars(n, 696);
        std::vector<void *> fakePtrs(n);

        std::vector<Vertex *> fromVertices(n),
                                toVertices(n);

        // Memory pool for vertices:
        utils::DynamicMemPool myPool(n, sizeof(Vertex), 1.0F);
        Vertex::SetMemoryPool(myPool);

        // Generate some fake vertices:
        for (int idx = 0; idx < n; ++idx)
            fromVertices[idx] = new Vertex(this, sizeof *this, nullptr);

        // Add edges with regular vertices:
        for (auto vtx : fromVertices)
            array.AddEdge(vtx);

        ASSERT_EQ(n, array.Size());
        ASSERT_FALSE(array.HasRootEdges());

        count = 0;
        array.ForEachRegular([&count](Vertex *){ ++count; return true; });
        ASSERT_EQ(n, count);

        // Add edges with root vertices:
        for (int idx = 0; idx < n; ++idx)
        {
            fakePtrs[idx] = &someVars[idx];
            array.AddEdge(fakePtrs[idx]);
        }

        ASSERT_EQ(2 * n, array.Size());
        ASSERT_TRUE(array.HasRootEdges());

        // Remove all edges
        array.Clear();

        ASSERT_EQ(0, array.Size());
        ASSERT_FALSE(array.HasRootEdges());

        count = 0;
        array.ForEachRegular([&count](Vertex *){ ++count; return true; });
        ASSERT_EQ(0, count);

        // Once again, add regular edges:
        for (auto vtx : fromVertices)
            array.AddEdge(vtx);

        ASSERT_EQ(n, array.Size());
        ASSERT_FALSE(array.HasRootEdges());

        count = 0;
        array.ForEachRegular([&count](Vertex *){ ++count; return true; });
        ASSERT_EQ(n, count);

        // Once again, add root edges:
        for (int idx = 0; idx < n; ++idx)
            array.AddEdge(fakePtrs[idx]);

        ASSERT_EQ(2 * n, array.Size());
        ASSERT_TRUE(array.HasRootEdges());

        // Remove root edges:
        for (auto ptr : fakePtrs)
            array.RemoveEdge(ptr);

        ASSERT_EQ(n, array.Size());
        ASSERT_FALSE(array.HasRootEdges());

        count = 0;
        array.ForEachRegular([&count](Vertex *){ ++count; return true; });
        ASSERT_EQ(n, count);

        // Remove regular edges:
        for (auto vtx : fromVertices)
            array.RemoveEdge(vtx);

        ASSERT_EQ(0, array.Size());
        ASSERT_FALSE(array.HasRootEdges());

        count = 0;
        array.ForEachRegular([&count](Vertex *){ ++count; return true; });
        ASSERT_EQ(0, count);

        // Return vertices to the pool:
        for (auto vtx : fromVertices)
            delete vtx;
    }

}// end of namespace unit_tests
}// end of namespace _3fd
