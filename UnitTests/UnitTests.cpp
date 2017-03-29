// UnitTests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "preprocessing.h"

#ifdef _3FD_CONSOLE_AVAILABLE
#	include <iostream>
#endif

namespace application
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
}
}

#ifdef _MSC_VER
#   include <vld.h>

    int wmain(int argc, wchar_t *argv[])
    {
#   ifdef _3FD_CONSOLE_AVAILABLE
        std::cout << "Running main() from UnitTests.cpp\n";
#   endif
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
#else
    int main(int argc, char *argv[])
    {
#   ifdef _3FD_CONSOLE_AVAILABLE
        std::cout << "Running main() from UnitTests.cpp\n";
#   endif
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
#endif
