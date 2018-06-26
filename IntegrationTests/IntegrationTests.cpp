#include "stdafx.h"
#include "exceptions.h"
#include "logger.h"
#include <exception>
#include <iostream>

#ifndef _3FD_PLATFORM_WINRT
#   ifdef _MSC_VER
#   include <vld.h>

    int wmain(int argc, wchar_t *argv[])
    {
        std::cout << "Running main() from \'IntegrationTests.cpp\'\n";
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
#   else

    int main(int argc, char *argv[])
    {
        std::cout << "Running main() from \'IntegrationTests.cpp\'\n";
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
#   endif
#endif

namespace _3fd
{
namespace integration_tests
{
    using namespace _3fd::core;

    /// <summary>
    /// Handles several types of exception.
    /// </summary>
    void HandleException()
    {
        try
        {
            throw;
        }
        catch(IAppException &appEx)
        {
            //std::cerr << appEx.ToString() << std::endl;
            core::Logger::Write(appEx, core::Logger::PRIO_ERROR);
        }
        catch(std::exception &stdEx)
        {
            std::cerr << stdEx.what() << std::endl;
        }
        catch(...)
        {
            std::cerr << "An unexpected exception has been caught." << std::endl;
        }

        FAIL();
    }

}// end of namespace integration_tests
}// end of namespace _3fd
