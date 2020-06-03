//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/core/preprocessing.h>
#include <3fd/utils/utils_algorithms.h>

#include <ctime>
#include <cmath>
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
        const uint32_t numEntries(1UL << 10);

        std::vector<Object> list;
        list.reserve(numEntries);

        // Fill the vector:
        for (int idx = 0; idx < numEntries; ++idx)
            list.push_back(Object{ idx, idx });

        srand(time(nullptr));

        // Search it a few times (match):
        for (int idx = 0; idx < 100; ++idx)
        {
            int key = abs(rand()) % numEntries;

            auto iter = utils::BinarySearch(list.cbegin(), list.cend(),
                key, [](const Object &x) noexcept { return x.key; }, std::less<int>());

            EXPECT_NE(list.end(), iter);

            if (iter != list.end())
            {
                EXPECT_EQ(key, iter->key);
            }
        }

        // Search it a few times (NO match):
        for (int idx = 0; idx < 100; ++idx)
        {
            int key = (abs(rand()) % 69) + numEntries;

            auto iter = utils::BinarySearch(list.cbegin(), list.cend(),
                key, [](const Object &x) noexcept { return x.key; }, std::less<int>());

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

        // Fill the vector with a pattern 1 2 2 3 3 3 4 4 4 4 ...

        int val(0);
        do
        {
            ++val;

            for (int idx = 0; idx < val && list.size() < numEntries; ++idx)
                list.push_back(Object{ val, val });

        } while (list.size() < numEntries);

        srand(time(nullptr));

        // Search it a few times (match):
        for (int idx = 0; idx < 100; ++idx)
        {
            int key = 1 + abs(rand()) % list.back().key;

            auto subRangeBegin = list.cbegin();
            auto subRangeEnd = list.cend();

            EXPECT_TRUE(
                utils::BinSearchSubRange(subRangeBegin, subRangeEnd,
                    key, [](const Object &x) noexcept { return x.key; }, std::less<int>())
            );

            EXPECT_NE(subRangeBegin, subRangeEnd) << "Could not find key " << key;

            if (subRangeEnd != list.end())
            {
                EXPECT_EQ(key, std::distance(subRangeBegin, subRangeEnd)) << "Incomplete range for key " << key;
            }

            std::for_each(subRangeBegin, subRangeEnd, [key](const Object &obj)
            {
                EXPECT_EQ(key, obj.key);
            });
        }

        // Search it a few times (NO match):
        for (int idx = 0; idx < 100; ++idx)
        {
            int key = (abs(rand()) % 69) + list.back().key + 1;

            auto subRangeBegin = list.cbegin();
            auto subRangeEnd = list.cend();

            EXPECT_FALSE(
                utils::BinSearchSubRange(subRangeBegin, subRangeEnd,
                    key, [](const Object &x) noexcept { return x.key; }, std::less<int>())
            );

            EXPECT_EQ(subRangeBegin, subRangeEnd) << "Not supposed to find key " << key;
        }
    }

}// end of namespace unit_tests
}// end of namespace _3fd
