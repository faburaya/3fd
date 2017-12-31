#include "stdafx.h"
#include "utils.h"

namespace _3fd
{
namespace unit_tests
{
    class Framework_Utils_TestCase : public ::testing::TestWithParam<unsigned int> {};

    /// <summary>
    /// Basic tests for <see cref="utils::ArrayOfBits"/> class, with focus on the activated bits.
    /// </summary>
    TEST_P(Framework_Utils_TestCase, ArrayOfBits_FocusOnActivated_BasicTest)
    {
        const auto n = GetParam();
        utils::ArrayOfBits array(n, false);

        EXPECT_EQ(n, array.Size());
        EXPECT_EQ(0, array.GetActivatedCount());
        EXPECT_FALSE(array.IsAnyActivated());

        unsigned int count(0);
        const unsigned int step = 5;
        auto firstOccur = step;
        auto lastOccur = firstOccur;

        for (auto idx = firstOccur; idx < n; idx += step)
        {
            array.Activate(idx);
            lastOccur = idx;
            ++count;
        }
            
        EXPECT_EQ(count, array.GetActivatedCount());
        EXPECT_EQ(firstOccur, array.FindFirstActivated());
        EXPECT_EQ(lastOccur, array.FindLastActivated());

        array.Activate(0);
        array.Activate(n - 1);
        count += 2;

        EXPECT_EQ(count, array.GetActivatedCount());
        EXPECT_EQ(0, array.FindFirstActivated());
        EXPECT_EQ(n - 1, array.FindLastActivated());

        for (uint32_t idx = 0; idx < n; ++idx)
        {
            if (array[idx])
            {
                array.Deactivate(idx);
                --count;
            }
        }

        EXPECT_EQ(count, array.GetActivatedCount());
        EXPECT_EQ(0, count);
        EXPECT_FALSE(array.IsAnyActivated());
        EXPECT_EQ(n, array.Size());
    }

    /// <summary>
    /// Basic tests for <see cref="utils::ArrayOfBits"/> class, with focus on the deactivated bits.
    /// </summary>
    TEST_P(Framework_Utils_TestCase, ArrayOfBits_FocusOnDeactivated_BasicTest)
    {
        const auto n = GetParam();
        utils::ArrayOfBits array(n, true);

        EXPECT_EQ(n, array.Size());
        EXPECT_EQ(n, array.GetActivatedCount());
        EXPECT_TRUE(array.IsAnyActivated());

        unsigned int count(n);
        const unsigned int step = 5;
        auto firstOccur = step;
        auto lastOccur = firstOccur;

        for (auto idx = firstOccur; idx < n; idx += step)
        {
            array.Deactivate(idx);
            lastOccur = idx;
            --count;
        }

        EXPECT_EQ(count, array.GetActivatedCount());
        EXPECT_EQ(firstOccur, array.FindFirstDeactivated());
        EXPECT_EQ(lastOccur, array.FindLastDeactivated());

        array.Deactivate(0);
        array.Deactivate(n - 1);
        count -= 2;

        EXPECT_EQ(count, array.GetActivatedCount());
        EXPECT_EQ(0, array.FindFirstDeactivated());
        EXPECT_EQ(n - 1, array.FindLastDeactivated());

        for (uint32_t idx = 0; idx < n; ++idx)
        {
            if (array[idx])
            {
                array.Activate(idx);
                ++count;
            }
        }

        EXPECT_EQ(count, array.GetActivatedCount());
        EXPECT_EQ(n, count);
        EXPECT_TRUE(array.IsAnyActivated());
        EXPECT_EQ(n, array.Size());
    }

    INSTANTIATE_TEST_CASE_P(Switch_NumberOfBits,
                            Framework_Utils_TestCase,
                            ::testing::Values(30, 100));

}// end of namespace unit_tests
}// end of namespace _3fd
