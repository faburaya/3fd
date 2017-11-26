#include "stdafx.h"
#include "runtime.h"
#include "exceptions.h"
#include "callstacktracer.h"
#include "logger.h"
#include "gc.h"

#ifdef _WIN32
#    ifdef _USING_V110_SDK71_
#        include <ObjBase.h>
#    else
#        include <roapi.h>
#    endif
#endif

#ifdef _3FD_PLATFORM_WINRT
#    include "sqlite3.h"
#endif

#include <codecvt>
#include <sstream>
#include <array>

namespace _3fd
{
namespace core
{
#ifdef _3FD_PLATFORM_WIN32API

    /// <summary>
    /// Gets the name of the current component, even
    /// if this is running inside a dynamic library.
    /// </summary>
    /// <returns>The name of the current running module.</returns>
    static std::string GetCurrentComponentName()
    {
        HMODULE thisModule;

        auto rv = GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
            reinterpret_cast<LPCWSTR> (&GetCurrentComponentName),
            &thisModule
        );

        if (rv == 0)
            return "UNKNOWN";

        std::array<wchar_t, 256> modFilePath;
        rv = GetModuleFileNameW(thisModule, modFilePath.data(), modFilePath.size());

        if (rv == 0)
            return "UNKNOWN";

        auto modNameStr = wcsrchr(modFilePath.data(), L'\\');
        if (modNameStr != nullptr)
            ++modNameStr;
        else
            modNameStr = modFilePath.data();

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
        return transcoder.to_bytes(modNameStr);
    }
    

    /// <summary>
    /// Initializes a new instance of the <see cref="FrameworkInstance" /> class.
    /// </summary>
    /// <param name="threadModel">The COM thread model.</param>
    FrameworkInstance::FrameworkInstance(MsComThreadModel threadModel)
        : FrameworkInstance()
    {
        const char *libLabel;
        bool unsupported(false);
        HRESULT hr;

#   ifdef _USING_V110_SDK71_

        libLabel = "COM library";

        // Initialize COM library
        switch (threadModel)
        {
        case ComSingleThreaded:
            hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            break;
        case ComMultiThreaded:
            hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            break;
        default:
            _ASSERTE(false); // unknown or unsupported COM thread model
            unsupported = true;
            break;
        }
#   else
        libLabel = "Windows Runtime API";

        /* Initialize Windows Runtime API for COM library usage in classic
        desktop apps (for use of libraries like Microsoft Media Foundation) */
        switch (threadModel)
        {
        case ComSingleThreaded:
            hr = Windows::Foundation::Initialize(RO_INIT_SINGLETHREADED);
            break;
        case ComMultiThreaded:
            hr = Windows::Foundation::Initialize(RO_INIT_MULTITHREADED);
            break;
        default:
            _ASSERTE(false); // unknown or unsupported COM thread model
            unsupported = true;
            break;
        }
#   endif

        if (FAILED(hr) || unsupported)
        {
            std::ostringstream oss;
            oss << "Failed to initialize" << libLabel << "! ";

            if (unsupported)
                oss << "Specified thread model is unknown or unsupported";
            else
                oss << core::WWAPI::GetDetailsFromHResult(hr);

            Logger::Write(oss.str(), Logger::PRIO_ERROR);

            oss.str("");
            oss << "3FD was shutdown in " << m_moduleName;
            Logger::Write(oss.str(), core::Logger::PRIO_DEBUG);
            Logger::Shutdown();

            exit(EXIT_FAILURE);
        }

        m_isComLibInitialized = true;
    }


    /// <summary>
    /// Initializes a new instance of the <see cref="FrameworkInstance" /> class.
    /// </summary>
    FrameworkInstance::FrameworkInstance()
        : m_moduleName(GetCurrentComponentName())
        , m_isComLibInitialized(false)
    {
        std::ostringstream oss;
        oss << "3FD has been initialized in " << m_moduleName;
        Logger::Write(oss.str(), core::Logger::PRIO_DEBUG);
    }

#elif defined _3FD_PLATFORM_WINRT

    /// <summary>
    /// Initializes a new instance of the <see cref="FrameworkInstance" /> class.
    /// </summary>
    /// <param name="thisComName">Name of the this WinRT component or app.</param>
    FrameworkInstance::FrameworkInstance(const char *thisComName)
        : m_moduleName(thisComName)
    {
        using namespace Windows::Storage;

        std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

        string tempFolderPath = transcoder.to_bytes(
            ApplicationData::Current->TemporaryFolder->Path->Data()
        );

        auto tempDirStrSize = tempFolderPath.length() + 1;
        sqlite3_temp_directory = (char *)sqlite3_malloc(tempDirStrSize);

        if (sqlite3_temp_directory == nullptr)
            throw std::bad_alloc();

        strncpy(sqlite3_temp_directory, tempFolderPath.data(), tempDirStrSize);

        std::ostringstream oss;
        oss << "3FD has been initialized in " << m_moduleName;
        Logger::Write(oss.str(), core::Logger::PRIO_DEBUG);
    }

#endif

	/// <summary>
	/// Finalizes an instance of the <see cref="FrameworkInstance"/> class.
	/// </summary>
	FrameworkInstance::~FrameworkInstance()
	{
		memory::GarbageCollector::Shutdown();

#ifdef _WIN32
        std::ostringstream oss;
        oss << "3FD was shutdown in " << m_moduleName;

		Logger::Write(oss.str(), core::Logger::PRIO_DEBUG);
#endif
		Logger::Shutdown();

#ifdef _3FD_PLATFORM_WINRT
		sqlite3_free(sqlite3_temp_directory);

#elif defined _3FD_PLATFORM_WIN32API
        if (m_isComLibInitialized)
        {
#   ifdef _USING_V110_SDK71_
            CoUninitialize();
#   else
            Windows::Foundation::Uninitialize();
#   endif
        }
#endif
	}

}// end of namespace core
}// end of namespace _3fd
