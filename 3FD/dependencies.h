#include "base.h"
#include <memory>
#include <mutex>

#ifdef _WIN32 // Microsoft Windows:
#   include <windows.h>
#else // POSIX:
#   define HINSTANCE void *
#endif

namespace _3fd
{
namespace core
{
    /// <summary>
    /// Takes care of the framework's DLL dependencies.
    /// </summary>
    class Dependencies : notcopiable
    {
    private:
            
        static std::unique_ptr<Dependencies>    singleInstancePtr;
        static std::mutex                        singleInstanceCreationMutex;

        Dependencies();

    public:

        ~Dependencies(); // must be public so the 'unique_ptr<>' holding the single object can destroy it

        // Get the singleton instance.
        static Dependencies &Get();

#ifdef _3FD_OPENCL_SUPPORT
    private:
        HINSTANCE m_openclDllHandle;

    public:
        HINSTANCE &OpenCLDllHandle;
#endif
    };
}
}
