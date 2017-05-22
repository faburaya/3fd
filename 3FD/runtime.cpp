#include "stdafx.h"
#include "runtime.h"
#include "exceptions.h"
#include "callstacktracer.h"
#include "logger.h"
#include "gc.h"

#ifdef _3FD_PLATFORM_WINRT
#	include "sqlite3.h"
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
    /// <param name="comThreadModel">The COM thread model.</param>
    FrameworkInstance::FrameworkInstance(RO_INIT_TYPE comThreadModel)
        : FrameworkInstance()
    {
        // Initialize Windows Runtime API for COM usage of Microsoft Media Foundation
        auto hr = Windows::Foundation::Initialize(comThreadModel);

        if (FAILED(hr))
        {
            std::ostringstream oss;
            oss << "Failed to initialize Windows Runtime API! " << core::WWAPI::GetDetailsFromHResult(hr);

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

        std::ostringstream oss;
        oss << "3FD was shutdown in " << m_moduleName;

		Logger::Write(oss.str(), core::Logger::PRIO_DEBUG);
		Logger::Shutdown();

#ifdef _3FD_PLATFORM_WINRT
		sqlite3_free(sqlite3_temp_directory);

#elif defined _3FD_PLATFORM_WIN32API
        if (m_isComLibInitialized)
            Windows::Foundation::Uninitialize();
#endif
	}

}// end of namespace core
}// end of namespace _3fd