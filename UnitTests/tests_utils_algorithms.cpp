#include "stdafx.h"
#include "preprocessing.h"
#include "utils_algorithms.h"

#include <ctime>
#include <cstdlib>
#include <vector>
#include <algorithm>

namespace _3fd
{
namespace unit_tests
{
    struct Object
    {
        int key;
        int val;
    };

    /// <summary>
    /// Tests the binary search for elements.
    /// </summary>
    TEST(Framework_Utils_TestCase, BinarySearch_Test)
    {
        const uint32_t numEntries(1UL << 8);

        std::vector<Object> list;
        list.reserve(numEntries);

        // Fill the vector:
        for (int idx = 0; idx < numEntries; ++idx)
            list.push_back(Object{ idx, idx });

        srand(time(nullptr));

        // Search it a few times (match):
        for (int idx = 0; idx < 4; ++idx)
        {
            int key = rand() % numEntries;

            auto iter = utils::BinarySearch(key, list.begin(), list.end());

            EXPECT_NE(list.end(), iter);
            EXPECT_EQ(key, iter->key);
        }

        // Search it a few times (NO match):
        for (int idx = 0; idx < 4; ++idx)
        {
            int key = (rand() % 69) + numEntries;

            auto iter = utils::BinarySearch(key, list.begin(), list.end());

            EXPECT_EQ(list.end(), iter);
        }
    }

	/// <summary>
	/// Tests the binary search for sub-range.
	/// </summary>
	TEST(Framework_Utils_TestCase, BinSearchSubRange_Test)
	{
        const uint32_t numEntries(1UL << 12);

        std::vector<Object> list;
        list.reserve(numEntries);

        // Fill the vector:

        int repetitions(0);
        while (list.size() < numEntries)
        {
            ++repetitions;

            for (int idx = 0; idx < repetitions; ++idx)
                list.push_back(Object{ idx, idx });
        }
        
        srand(time(nullptr));

        // Search it a few times (match):
        for (int idx = 0; idx < 4; ++idx)
        {
            int key = rand() % (list.back().key + 1);

            auto subRangeBegin = list.begin();
            auto subRangeEnd = list.end();

            EXPECT_TRUE(utils::BinSearchSubRange(key, subRangeBegin, subRangeEnd));
            EXPECT_NE(subRangeEnd, subRangeEnd);

            std::for_each(subRangeBegin, subRangeEnd, [key](const Object &obj)
            {
                EXPECT_EQ(key, obj.key);
            });
        }

        // Search it a few times (NO match):
        for (int idx = 0; idx < 4; ++idx)
        {
            int key = (rand() % 69) + numEntries;

            auto subRangeBegin = list.begin();
            auto subRangeEnd = list.end();

            EXPECT_FALSE(utils::BinSearchSubRange(key, subRangeBegin, subRangeEnd));
            EXPECT_EQ(subRangeEnd, subRangeEnd);
        }
	}

}// end of namespace unit_tests
}// end of namespace _3fd