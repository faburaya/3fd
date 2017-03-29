#include "stdafx.h"
#include "utils_lockfreequeue.h"
#include "preprocessing.h"

#include <vector>
#include <thread>
#include <cassert>

namespace _3fd
{
namespace unit_tests
{
    void PrintProgressBar(double fraction);

	/// <summary>
	/// Generic tests for <see cref="utils::LockFreeQueue{}"/> class.
	/// </summary>
	TEST(Framework_Utils_TestCase, LockFreeQueue_InHouse_ParallelProducerTest)
	{
		const unsigned long seqLen = 1UL << 18;

		utils::LockFreeQueue<unsigned long> queue;

		std::vector<unsigned long> seqOfNums(seqLen);

		// Generate a sequence of increasing numbers:
		unsigned long idx(0);
		for (idx = 0; idx < seqLen; ++idx)
			seqOfNums[idx] = idx;

		// Launch a parallel thread to insert entries in the queue:
		std::thread producerThread(
			[seqLen, &queue, &seqOfNums]()
			{
				for (auto &num : seqOfNums)
				{
					PrintProgressBar((double)num / seqOfNums.back());
					queue.Add(&num);
				}
			}
		);

		// Consume the entries being inserted asynchronously in the queue:

		idx = 0;

		do
		{
			auto *numPtr = queue.Remove();
				
			if (numPtr != nullptr)
				ASSERT_EQ(idx++, *numPtr);
			else
				std::this_thread::sleep_for(std::chrono::milliseconds(5));

		} while (idx < seqLen);

		// wait for producer thread to finalize
		if (producerThread.joinable())
			producerThread.join();
	}

#   ifdef _WIN32
    /// <summary>
    /// Generic tests for <see cref="utils::Win32ApiWrappers::LockFreeQueue{}"/> class.
    /// </summary>
    TEST(Framework_Utils_TestCase, LockFreeQueue_Win32Api_ParallelProducerTest)
    {
        const unsigned long seqLen = 1UL << 18;

        utils::Win32ApiWrappers::LockFreeQueue<unsigned long> queue;

        std::vector<unsigned long> seqOfNums(seqLen);

        // Generate a sequence of increasing numbers:
        unsigned long idx(0);
        for (idx = 0; idx < seqLen; ++idx)
            seqOfNums[idx] = idx;

        // Launch a parallel thread to insert entries in the queue:
        std::thread producerThread(
            [seqLen, &queue, &seqOfNums]()
            {
                for (auto &num : seqOfNums)
                {
                    PrintProgressBar((double)num / seqOfNums.back());
                    queue.Push(num);
                }
            }
        );

        // Consume the entries being inserted asynchronously in the queue:

        idx = 0;

        do
        {
            if (queue.ForEach([&idx](const unsigned long &num) { ASSERT_EQ(idx++, num); }) == 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

            if (HasFatalFailure())
                FAIL();

        } while (idx < seqLen);

        // wait for producer thread to finalize
        if (producerThread.joinable())
            producerThread.join();
    }
#   endif
}// end of namespace unit_tests
}// end of namespace _3fd
