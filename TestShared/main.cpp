//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include <3fd/core/runtime.h>
#include <iostream>

#ifdef _MSC_VER
#   ifdef _3FD_PLATFORM_WIN32API // Win32 Console:
    int wmain(int argc, wchar_t *argv[])
    {
        std::cout << "Running wmain() from " __FILE__ << std::endl;
        _3fd::core::SetupMemoryLeakDetection();
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }

#   else // UWP Console:
    int main()
    {
        std::cout << "Running main() from " __FILE__ << std::endl;
        testing::InitGoogleTest(&__argc, __argv);

        // After InitGoogleTest! (takes snapshot of the heap later)
        _3fd::core::SetupMemoryLeakDetection();

        int rc = RUN_ALL_TESTS();
        getchar();
        return rc;
    }
#   endif

#else // POSIX:
int main(int argc, char *argv[])
{
    std::cout << "Running wmain() from " __FILE__ << std::endl;
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
