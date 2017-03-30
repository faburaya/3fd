#ifndef UTILS_ALGORITHMS_H // header guard
#define UTILS_ALGORITHMS_H

#include <iterator>
#include <functional>

namespace _3fd
{
namespace utils
{
    /// <summary>
    /// Binary search in sub-range of vector containing map cases entries.
    /// </summary>
    /// <param name="searchKey">The key to search for.</param>
    /// <param name="begin">An iterator to the first position of the sub-range.
    /// In the end, this parameter keeps the beginning of the last sub-range this
    /// iterative algorithm has delved into.</param>
    /// <param name="begin">An iterator to one past the last position of the sub-range.
    /// In the end, this parameter keeps the end of the last sub-range this
    /// iterative algorithm has delved into.</param>
    /// <returns>An iterator to the found entry. Naturally, it cannot refer to
    /// the same position in the parameter 'end', unless there was no match.</returns>
    template <typename KeyType, typename IterType>
    IterType BinarySearch(KeyType searchKey,
                          IterType &begin,
                          IterType &end)
    {
        std::less<KeyType> lessThan;

        auto endOfRange = end;
        auto length = std::distance(begin, end);

        if (length > 32)
        {
            while (begin != end)
            {
                auto middle = begin + std::distance(begin, end) / 2;
                
                if (lessThan(middle->GetKey(), searchKey))
                    begin = middle + 1;
                else if (lessThan(searchKey, middle->GetKey()))
                    end = middle;
                else
                    return middle;
            }

            return begin = end = endOfRange;
        }

        // Linear search when container is small:

        while (begin != end)
        {
            if (lessThan(begin->GetKey(), searchKey))
                ++begin;
            else
                break;
        }

        if (!lessThan(searchKey, begin->GetKey()))
            return begin;
        else
            return begin = end = endOfRange;
    }


    /// <summary>
    /// Gets the sub range of entries that match the given key (using binary search).
    /// </summary>
    /// <param name="searchKey">The key to search.</param>
    /// <param name="subRangeBegin">An iterator to the first position of
    /// the range to search, and receives the same for the found sub-range.</param>
    /// <param name="subRangeEnd">An iterator to one past the last position of
    /// the range to search, and receives the same for the found sub-range.</param>
    /// <returns>When a sub-range has been found, <c>true</c>, otherwise, <c>false</c>.</returns>
    template <typename KeyType, typename IterType>
    bool BinSearchSubRange(KeyType searchKey,
                           IterType &subRangeBegin,
                           IterType &subRangeEnd)
    {
        struct {
            IterType begin;
            IterType end;
        } firstMatchRightPart;

        auto endOfRange = subRangeEnd;
        auto end = subRangeEnd;
        auto begin = subRangeBegin;
        auto match = BinarySearch(searchKey, begin, end);

        // match? this is the first, so keep the partition at the right:
        if (match != end)
        {
            firstMatchRightPart.begin = match; // match inclusive
            firstMatchRightPart.end = end;
        }
        else
        {
            subRangeBegin = subRangeEnd = endOfRange;
            return false; // no match found!
        }

        do
        {
            subRangeBegin = end = match; // last match is the smallest entry found!
            match = BinarySearch(searchKey, begin, end); // continue to search in the left partition
        } while (match != end);

        /* Now go back to the partition at the right of the first
        match, and start looking for the end of the range: */

        match = firstMatchRightPart.begin;
        end = firstMatchRightPart.end;

        do
        {
            subRangeEnd = begin = match + 1; // last match is the greatest entry found!
            match = BinarySearch(searchKey, begin, end); // continue to search in the right partition
        } while (match != end);

        return true;
    }

}// end of namespace utils
}// end of namespace _3fd

#endif // end of header guard