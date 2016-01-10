#include "stdafx.h"
#include "utils_lockfreequeue.h"
#include "preprocessing.h"

#include <vector>
#include <thread>
#include <cassert>

#ifdef _3FD_CONSOLE_AVAILABLE
#	include <iostream>
#endif

namespace _3fd
{
	namespace unit_tests
	{
		void PrintProgressBar(double fraction)
		{
#ifdef _3FD_CONSOLE_AVAILABLE
			assert(fraction <= 1.0);

			// Amount of symbols = inside the progress bar
			const int qtBarSteps(50);

			static int done;

			// Only update the progress bar if there is a change:
			int update = (int)(qtBarSteps * fraction);
			if (update == done)
				return;
			else
				done = update;

			// Print the progress bar:

			std::cout << "\rProgress: [";

			for (int idx = 0; idx < done; ++idx)
				std::cout << '=';

			int remaining = qtBarSteps - done;
			for (int idx = 0; idx < remaining; ++idx)
				std::cout << ' ';

			std::cout << "] " << (int)(100 * fraction) << " % done" << std::flush;

			if (fraction == 1.0)
				std::cout << std::endl;
#endif
		}

		/// <summary>
		/// Generic tests for <see cref="utils::LockFreeQueue{}"/> class.
		/// </summary>
		TEST(Framework_Utils_TestCase, LockFreeQueue_ParallelProducerTest)
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

	}// end of namespace unit_tests
}// end of namespace _3fd
