#ifndef UTILS_ALGORITHMS_H // header guard
#define UTILS_ALGORITHMS_H

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <algorithm>

namespace _3fd
{
namespace utils
{
    /// <summary>
    /// Binary search in sub-range of vector containing map cases entries.
    /// </summary>
    /// <param name="begin">An iterator to the first position of the sub-range.
    /// In the end, this parameter keeps the beginning of the last sub-range this
    /// iterative algorithm has delved into.</param>
    /// <param name="begin">An iterator to one past the last position of the sub-range.
    /// In the end, this parameter keeps the end of the last sub-range this iterative
    /// algorithm has delved into.</param>
    /// <param name="searchKey">The key to search for.</param>
    /// <param name="getKey">A functor that retrieves the key for an object.</param>
    /// <param name="lessThan">A functor that evaluates when a key value is less than another.</param>
    /// <returns>An iterator to the found entry. If there was no match,
    /// 'begin == end' in the position it was supposed to be found.</returns>
    template <typename IterType, typename SearchKeyType, typename GetKeyFnType, typename LessFnType>
    IterType BinarySearch(IterType &begin,
                          IterType &end,
                          SearchKeyType searchKey,
                          GetKeyFnType getKey,
                          LessFnType lessThan) NOEXCEPT
    {
        auto length = std::distance(begin, end);

        if (length > 7)
        {
            while (begin != end)
            {
                auto middle = begin + std::distance(begin, end) / 2;

                if (lessThan(getKey(*middle), searchKey))
                    begin = middle + 1;
                else if (lessThan(searchKey, getKey(*middle)))
                    end = middle;
                else
                    return middle;
            }

            return begin;
        }

        // Linear search when container is small:

        while (begin != end)
        {
            if (lessThan(getKey(*begin), searchKey))
                ++begin;
            else if (lessThan(searchKey, getKey(*begin)))
                return end = begin;
            else
                return begin;
        }

        return begin;
    }

    template <typename IterType1,
              typename IterType2,
              typename SearchKeyType,
              typename GetKeyFnType,
              typename LessFnType>
    IterType1 BinarySearch(IterType1 &&begin,
                           IterType2 &&end,
                           SearchKeyType searchKey,
                           GetKeyFnType getKey,
                           LessFnType lessThan) NOEXCEPT
    {
        return BinarySearch(begin,
                            end,
                            searchKey,
                            getKey,
                            lessThan);
    }

    /// <summary>
    /// Gets the sub range of entries that match the given key (using binary search).
    /// </summary>
    /// <param name="subRangeBegin">An iterator to the first position of
    /// the range to search, and receives the same for the found sub-range.</param>
    /// <param name="subRangeEnd">An iterator to one past the last position of
    /// the range to search, and receives the same for the found sub-range.</param>
    /// <param name="searchKey">The key to search.</param>
    /// <param name="getKey">A functor that retrieves the key for an object.</param>
    /// <param name="lessThan">A functor that evaluates when a key value is less than another.</param>
    /// <returns>When a sub-range has been found, returns <c>true</c> and
    /// 'subRangeBegin != subRangeEnd', otherwise, returns <c>false</c> and
    /// 'subRangeBegin == subRangeEnd' in the position it was supposed to be found.</returns>
    template <typename IterType, typename SearchKeyType, typename GetKeyFnType, typename LessFnType>
    bool BinSearchSubRange(IterType &subRangeBegin,
                           IterType &subRangeEnd,
                           SearchKeyType searchKey,
                           GetKeyFnType getKey,
                           LessFnType lessThan) NOEXCEPT
    {
        auto firstMatch = BinarySearch(subRangeBegin,
                                       subRangeEnd,
                                       searchKey,
                                       getKey,
                                       lessThan);

        // no match?
        if (subRangeBegin == subRangeEnd)
            return false;

        auto end = firstMatch;

        do // iterate to find lower bound:
        {
            // search another match in the left partition:
            end = BinarySearch(subRangeBegin,
                               end,
                               searchKey,
                               getKey,
                               lessThan);

        } while (subRangeBegin != end);

        /* Now go back to the partition at the right of the first
        match, and start looking for the end of the range: */

        auto begin = firstMatch;

        do  // iterate to find upper bound:
        {
            // search another match in the right partition:
            begin = BinarySearch(begin + 1,
                                 subRangeEnd,
                                 searchKey,
                                 getKey,
                                 lessThan);

        } while (begin != subRangeEnd);

        return true;
    }

    /// <summary>
    /// Calculates the exponential back off given the attempt and time slot.
    /// </summary>
    /// <param name="attempt">The attempt (base 0).</param>
    /// <param name="timeSlot">The time slot.</param>
    /// <returns>A timespan of how long to wait before the next retry.</returns>
    template <typename RepType, typename PeriodType>
    std::chrono::duration<RepType, PeriodType> CalcExponentialBackOff(unsigned int attempt,
                                                                      std::chrono::duration<RepType, PeriodType> timeSlot)
    {
        static bool first(true);

        if (first)
        {
            srand(time(nullptr));
            first = false;
        }

        auto k = static_cast<unsigned int> (
            std::abs(1.0F * rand() / RAND_MAX) * ((1 << attempt) - 1)
        );

        return timeSlot * k;
    }

}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard
