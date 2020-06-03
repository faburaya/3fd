#ifndef RUNTIME_H
#define RUNTIME_H

#include <3fd/core/preprocessing.h>
#include <3fd/core/callstacktracer.h>
#include <3fd/core/logger.h>
#include <string>

namespace _3fd
{
namespace core
{
    /// <summary>
    /// An object to be allocated on the stack which is responsible for 
    /// starting or stopping the framework instance for the current thread.
    /// </summary>
    class FrameworkInstance
    {
    private:

        std::string m_moduleName;

#ifdef _3FD_PLATFORM_WIN32API
        bool m_isComLibInitialized;
#endif
    public:

        FrameworkInstance(const FrameworkInstance &) = delete;

        ~FrameworkInstance();

#ifdef _3FD_PLATFORM_WIN32API

        enum MsComThreadModel { ComSingleThreaded, ComMultiThreaded };

		FrameworkInstance();

        FrameworkInstance(MsComThreadModel threadModel);

#elif defined _3FD_PLATFORM_WINRT

        FrameworkInstance(const char *thisComName = "UNKNOWN");
#else
		FrameworkInstance();
#endif
    };

    void SetupMemoryLeakDetection();

}// end of namespace core
}// end of namespace _3fd

#endif // header guard