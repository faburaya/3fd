// UnitTests.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>

#ifdef _MSC_VER
#   include <vld.h>

    int wmain(int argc, wchar_t *argv[])
    {
        std::cout << "Running main() from UnitTests.cpp\n";
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
#else
    int main(int argc, char *argv[])
    {
        std::cout << "Running main() from UnitTests.cpp\n";
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
#endif
