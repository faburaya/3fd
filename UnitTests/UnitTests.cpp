// UnitTests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "preprocessing.h"

#ifdef _3FD_CONSOLE_AVAILABLE
#	include <iostream>
#endif

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
