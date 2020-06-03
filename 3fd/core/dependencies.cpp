#include "pch.h"
#include "configuration.h"
#include "dependencies.h"
#include "exceptions.h"

#if defined __linux__ || defined __unix__ // POSIX only:
#   include <dlfcn.h>
#endif

namespace _3fd
{
namespace core
{
    std::unique_ptr<Dependencies> Dependencies::singleInstancePtr;
    std::mutex                    Dependencies::singleInstanceCreationMutex;

#ifdef _3FD_PLATFORM_WIN32API

    /// <summary>
    /// Prevents a default instance of the <see cref="Dependencies"/> class from being created.
    /// </summary>
    Dependencies::Dependencies() : 
        m_openclDllHandle(nullptr), 
        OpenCLDllHandle(m_openclDllHandle)
    {
        if (AppConfig::GetSettings().framework.dependencies.opencl)
        {
            m_openclDllHandle = ::LoadLibraryW(L"OpenCL.dll");

            if (m_openclDllHandle == nullptr)
                throw AppException<std::runtime_error>("Could not load OpenCL.dll", "Windows API: LoadLibrary");
        }
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="Dependencies"/> class.
    /// </summary>
    Dependencies::~Dependencies()
    {
        if (m_openclDllHandle != nullptr)
            FreeLibrary(m_openclDllHandle);
    }

#elif defined __linux__

    /// <summary>
    /// Prevents a default instance of the <see cref="Dependencies"/> class from being created.
    /// </summary>
    Dependencies::Dependencies() : 
        m_openclDllHandle(nullptr), 
        OpenCLDllHandle(m_openclDllHandle)
    {
        if (AppConfig::GetSettings().framework.dependencies.opencl)
        {
            std::array<const char *, 3> libFileNames = { "libOpenCL.so", "libOpenCL.so.1", "libOpenCL.so.1.0.0" };

            int idx(0);
            while (idx < libFileNames.size()
                   && (m_openclDllHandle = dlopen(libFileNames[idx], RTLD_LAZY)) == nullptr)
            {
                ++idx;
            }

            if (m_openclDllHandle == nullptr)
            {
                std::ostringstream oss;

                auto errdesc = dlerror();
                if (errdesc != nullptr)
                    oss << errdesc << " - POSIX API: dlopen";
                else
                    oss << "POSIX API: dlopen";

                throw AppException<std::runtime_error>("Could not load OpenCL library", oss.str());
            }
        }
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="Dependencies"/> class.
    /// </summary>
    Dependencies::~Dependencies()
    {
        if (m_openclDllHandle != nullptr)
            dlclose(m_openclDllHandle);
    }

#else

    /// <summary>
    /// Prevents a default instance of the <see cref="Dependencies"/> class from being created.
    /// </summary>
    Dependencies::Dependencies()
    {
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="Dependencies"/> class.
    /// </summary>
    Dependencies::~Dependencies()
    {
    }

#endif

    /// <summary>
    /// Gets the singleton <see cref="Dependencies" />.
    /// </summary>
    /// <returns>The unique instance</returns>
    Dependencies & Dependencies::Get()
    {
        if(singleInstancePtr)
            return *singleInstancePtr;
        else
        {
            try
            {
                std::lock_guard<std::mutex> lock(singleInstanceCreationMutex);

                if(singleInstancePtr.get() == nullptr)
                    singleInstancePtr.reset(dbg_new Dependencies ());

                return *singleInstancePtr;
            }
            catch(std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to acquire lock when instantiating the dependencies manager: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch(std::bad_alloc &)
            {
                throw core::AppException<std::runtime_error>("Failed to allocate memory for the dependencies manager.");
            }
        }
    }

}// end of namespace core
}// end of namespace _3fd
