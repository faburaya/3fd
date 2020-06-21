#include "pch.h"
#include <3fd/core/configuration.h>
#include <3fd/core/exceptions.h>
#include <3fd/utils/serialization.h>
#include <3fd/utils/xml.h>

#ifdef _3FD_PLATFORM_WINRT
#    include <3fd/utils/winrt.h>
#    include <winrt\Windows.ApplicationModel.h>
#endif

#include <array>
#include <atomic>
#include <cinttypes>
#include <codecvt>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <system_error>
#include <vector>

#if defined __linux__ || defined __unix__ // POSIX:
#   include <unistd.h>
#endif

namespace _3fd
{
namespace core
{
    using std::ostringstream;

    //////////////////////////
    // AppFlexSettings Class
    //////////////////////////

    /// <summary>
    /// Adds a pair key-value loaded from the XML configuration file.
    /// </summary>
    /// <param name="key">The key.</param>
    /// <param name="value">The value.</param>
    void AppFlexSettings::Add(const string &key, const string &value)
    {
        m_settings[key] = value;
    }


    /// <summary>
    /// Gets the string value for a given key.
    /// </summary>
    /// <param name="key">The key.</param>
    /// <param name="defValue">The default value.</param>
    /// <returns>The value for the key when defined, otherwise, the default.</returns>
    string AppFlexSettings::GetString(const string &key, const char *defValue) const
    {
        auto iter = m_settings.find(key);

        if (m_settings.end() != iter)
            return iter->second;
        else
            return defValue;
    }


    /// <summary>
    /// Gets the boolean value for a given key.
    /// </summary>
    /// <param name="key">The key.</param>
    /// <param name="defValue">The default value.</param>
    /// <returns>
    /// The value for the key when defined and succesfully
    /// converted to the target type, otherwise, the default.
    /// </returns>
    bool AppFlexSettings::GetBool(const string &key, bool defValue) const
    {
        auto iter = m_settings.find(key);

        if (m_settings.end() != iter)
            return (strcmp(iter->second.c_str(), "true") == 0);
        else
            return defValue;
    }


    /// <summary>
    /// Gets the integer value for a given key.
    /// </summary>
    /// <param name="key">The key.</param>
    /// <param name="defValue">The default value.</param>
    /// <returns>The parsed value for the key when defined, otherwise, the default.</returns>
    int32_t AppFlexSettings::GetInt(const string &key, int32_t defValue) const
    {
        auto iter = m_settings.find(key);

        if (m_settings.end() != iter)
            return strtol(iter->second.c_str(), nullptr, 10);
        else
            return defValue;
    }


    /// <summary>
    /// Gets the unsigned integer value for a given key.
    /// </summary>
    /// <param name="key">The key.</param>
    /// <param name="defValue">The default value.</param>
    /// <returns>The parsed value for the key when defined, otherwise, the default.</returns>
    uint32_t AppFlexSettings::GetUInt(const string &key, uint32_t defValue) const
    {
        auto iter = m_settings.find(key);

        if (m_settings.end() != iter)
            return strtoul(iter->second.c_str(), nullptr, 10);
        else
            return defValue;
    }


    /// <summary>
    /// Gets the 'floating point' number value for a given key.
    /// </summary>
    /// <param name="key">The key.</param>
    /// <param name="defValue">The default value.</param>
    /// <returns>The parsed value for the key when defined, otherwise, the default.</returns>
    float AppFlexSettings::GetFloat(const string &key, float defValue) const
    {
        auto iter = m_settings.find(key);

        if (m_settings.end() != iter)
            return static_cast<float> (strtod(iter->second.c_str(), nullptr));
        else
            return defValue;
    }


    //////////////////////////
    // AppConfig Class
    //////////////////////////

    std::unique_ptr<AppConfig> AppConfig::uniqueObject;

    std::mutex AppConfig::initializationMutex;

    /// <summary>
    /// Gets the instance already initialized.
    /// </summary>
    /// <returns>The unique instance of <see cref="AppConfig"/> already initialized.</returns>
    AppConfig & AppConfig::GetInstanceInitialized()
    {
        if (uniqueObject)
            return *uniqueObject;
        else
        {
            try
            {
                std::lock_guard<std::mutex> lock(initializationMutex);

                if(!uniqueObject)
                {
                    std::unique_ptr<AppConfig> temp(dbg_new AppConfig());
                    temp->Initialize();
                    uniqueObject = std::move(temp);
                }

                return *uniqueObject;
            }
            catch(core::IAppException &)
            {
                throw; // just forward the exceptions regarding error that are known to have been previously handled
            }
            catch(std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "3FD function is compromised by a critical error! "
                       "Failed to acquire lock before loading framework configuration: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);

                throw AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "3FD function is compromised by a critical error! "
                       "Generic failure when initializing framework configuration: " << ex.what();

                throw AppException<std::runtime_error>(oss.str());
            }
        }
    }

    /// <summary>
    /// Gets the application identifier.
    /// </summary>
    /// <returns>The application ID, which is the name of the current executable.</returns>
    const string & AppConfig::GetApplicationId()
    {
        return GetInstanceInitialized().m_applicationId;
    }

    /// <summary>
    /// Gets the settings.
    /// </summary>
    /// <returns>A reference to the hierarchy of settings loaded from the XML configuration file.</returns>
    const AppConfig::Tree & AppConfig::GetSettings()
    {
        return GetInstanceInitialized().settings;
    }

#if _WIN32 // Microsoft Windows:

#    ifdef _3FD_PLATFORM_WIN32API // Windows Desktop Apps only:

    /// <summary>
    /// Gets an ID for the running application, invoking a system call.
    /// </summary>
    /// <param name="appFilePath">Receives the file path for the running application executable.</param>
    /// <returns>A text ID (UTF-8 encoded) for the running application.</returns>
    static string CallSysForApplicationId(string &appFilePath)
    {
        std::array<wchar_t, 300> chBuffer;

        if (GetModuleFileNameW(nullptr, chBuffer.data(), chBuffer.size()) == 0)
            throw AppException<std::runtime_error>("It was not possible to get the full file name of the executable.", "Windows API: GetModuleFileName");

        try
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            appFilePath = converter.to_bytes(chBuffer.data());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failures when determining the file name of the framework configuration file: " << ex.what();
            throw AppException<std::runtime_error>(oss.str());
        }

        // The application ID is the name of the executable without the extension:
        return string(appFilePath.begin() + appFilePath.rfind('\\') + 1,
            appFilePath.begin() + appFilePath.rfind('.'));
    }

#    elif defined _3FD_PLATFORM_WINRT // Store Apps only:

    /// <summary>
    /// Gets an ID for the running application, invoking a system call.
    /// </summary>
    /// <remarks>
    /// In this overload the output parameter has a different meaning. In classic desktop applications
    /// on Windows and POSIX there is a way to capture the name of the current running executable and
    /// use it to figure out the name of the configuration file (appending the suffix .3fd.config). In
    /// UWP apps, however, all you can get is the ID. So, here we set the output parameter with the path
    /// for the app executable + ID. Such location does not actually exist, but that allows the code in
    /// <see cref="AppConfig::Initialize"/> to just append the same suffix and come out with a path that
    /// we can use to name the configuration file.
    /// </remarks>
    /// <param name="appFilePath">Receives the file path for the running application executable.</param>
    /// <returns>A text ID (UTF-8 encoded) for the running application.</returns>
    static string CallSysForApplicationId(string &appFilePath)
    {
        winrt::hstring curPackageIdName = winrt::Windows::ApplicationModel::Package::Current().Id().Name();

        try
        {
            string id = winrt::to_string(curPackageIdName);
            appFilePath = utils::WinRTExt::GetFilePathUtf8(id, utils::WinRTExt::FileLocation::InstallFolder);
            return id;
        }
        catch (winrt::hresult_error &ex)
        {
            std::ostringstream oss;
            oss << "Failed to determine name of framework configuration file - " << core::WWAPI::GetDetailsFromWinRTEx(ex);
            throw AppException<std::runtime_error>(oss.str());
        }
    }

#    endif

#elif defined __linux__ || defined __unix__ // POSIX only:

    /// <summary>
    /// Gets an ID for the running application, invoking a system call.
    /// </summary>
    /// <param name="appFilePath">Receives the file path for the running application executable.</param>
    /// <returns>A text ID (UTF-8 encoded) for the running application.</returns>
    static string CallSysForApplicationId(string &appFilePath)
    {
        std::array<char, 300> chBuffer;
        auto rval = readlink("/proc/self/exe", chBuffer.data(), chBuffer.size());

        if (rval != -1)
        {
            // The application ID is the name of the executable:
            chBuffer[rval] = 0;
            appFilePath = string(chBuffer.begin(), chBuffer.begin() + rval);
            return string(appFilePath.begin() + appFilePath.rfind('/') + 1, appFilePath.end());
        }
        else
            throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), "POSIX API: readlink");
    }

#endif

    template <typename ValType>
    std::shared_ptr<xml::XmlQueryNode> ParseKeyValue(const char *key, ValType &value)
    {
        return xml::QueryElement("entry", xml::Optional, {
            xml::QueryAttribute("key", xml::equal_to_copy_of(key)),
            xml::QueryAttribute("value", xml::Required, xml::parse_into(value))
        });
    }

    /// <summary>
    /// Initializes this instance with data from the XML configuration file.
    /// </summary>
    void AppConfig::Initialize()
    {
        try
        {
            string appFilePath;
            m_applicationId = CallSysForApplicationId(appFilePath);
            string cfgFilePath = appFilePath + ".3fd.config";

            std::vector<char> buffer;
            rapidxml::xml_document<char> dom;

            auto root = xml::ParseXmlFromFile(cfgFilePath, buffer, dom, "configuration");

            if (root == nullptr)
                throw AppException<std::runtime_error>("Failed to load configurations from XML: element /configuration not found!");

            const rapidxml::xml_node<char> *appNode(nullptr);

            auto xmlQuery = xml::QueryElement("configuration", xml::Required, {
                xml::QueryElement("common", xml::Required, {
                    // XPath /configuration/common/log:
                    xml::QueryElement("log", xml::Required, {
                        ParseKeyValue("sizeLimit", settings.common.log.sizeLimit = 1024),
                        ParseKeyValue("writeToConsole", settings.common.log.writeToConsole = false)
                    })
                }),
                xml::QueryElement("framework", xml::Required, {
#   ifndef _3FD_PLATFORM_WINRT
                    // XPath /configuration/framework/dependencies:
                    xml::QueryElement("dependencies", xml::Optional, {
                        ParseKeyValue("opencl", settings.framework.dependencies.opencl = false)
                    }),
#   endif
                    // XPath /configuration/framework/stackTracing:
                    xml::QueryElement("stackTracing", xml::Optional, {
                        ParseKeyValue("stackLogInitialCap", settings.framework.stackTracing.stackLogInitialCap = 32)
                    }),
                    // XPath /configuration/framework/gc:
                    xml::QueryElement("gc", xml::Optional, {
                        ParseKeyValue("msgLoopSleepTimeoutMilisecs", settings.framework.gc.msgLoopSleepTimeoutMilisecs = 100),
                        ParseKeyValue("memoryBlocksPoolInitialSize", settings.framework.gc.memBlocksMemPool.initialSize = 128),
                        ParseKeyValue("memoryBlocksPoolGrowingFactor", settings.framework.gc.memBlocksMemPool.growingFactor = 1.0),
                        ParseKeyValue("sptrObjsHashTabInitSizeLog2", settings.framework.gc.sptrObjectsHashTable.initialSizeLog2 = 8),
                        ParseKeyValue("sptrObjsHashTabLoadFactorThreshold", settings.framework.gc.sptrObjectsHashTable.loadFactorThreshold = 0.7F)
                    }),
#    ifdef _3FD_OPENCL_SUPPORT
                    // XPath /configuration/framework/opencl:
                    xml::QueryElement("opencl", xml::Optional, {
                        ParseKeyValue("maxSourceCodeLineLength", settings.framework.opencl.maxSourceCodeLineLength = 128),
                        ParseKeyValue("maxBuildLogSize", settings.framework.opencl.maxBuildLogSize = 5120)
                    }),
#    endif
#    ifdef _3FD_ESENT_SUPPORT
                    // XPath /configuration/framework/isam:
                    xml::QueryElement("isam", xml::Optional, {
                        ParseKeyValue("useWindowsFileCache", settings.framework.isam.useWindowsFileCache = true)
                    }),
#    endif
#   ifndef _3FD_PLATFORM_WINRT
                    // XPath /configuration/framework/broker:
                    xml::QueryElement("broker", xml::Optional, {
                        ParseKeyValue("dbConnTimeoutSecs", settings.framework.broker.dbConnTimeoutSecs = 60),
                        ParseKeyValue("dbConnMaxRetries", settings.framework.broker.dbConnMaxRetries = 1)
                    }),
#   endif
#   ifdef _3FD_PLATFORM_WIN32API
                    // XPath /configuration/framework/rpc:
                    xml::QueryElement("rpc", xml::Optional, {
                        ParseKeyValue("cliSrvConnectMaxRetries", settings.framework.rpc.cliSrvConnectMaxRetries = 10),
                        ParseKeyValue("cliSrvConnRetrySleepSecs", settings.framework.rpc.cliSrvConnRetrySleepSecs = 5),
                        ParseKeyValue("cliSrvConnectMaxRetries", settings.framework.rpc.cliCallMaxRetries = 10),
                        ParseKeyValue("cliCallRetrySleepMs", settings.framework.rpc.cliCallRetrySleepMs = 1000),
                        ParseKeyValue("cliCallRetryTimeSlotMs", settings.framework.rpc.cliCallRetryTimeSlotMs = 500)
                    }),
                    // XPath /configuration/framework/wws:
                    xml::QueryElement("wws", xml::Optional, {
                        ParseKeyValue("proxyConnMaxRetries", settings.framework.wws.proxyConnMaxRetries = 10),
                        ParseKeyValue("proxyCallMaxRetries", settings.framework.wws.proxyCallMaxRetries = 10),
                        ParseKeyValue("proxyRetrySleepSecs", settings.framework.wws.proxyRetrySleepSecs = 5),
                        ParseKeyValue("proxyRetryTimeSlotMs", settings.framework.wws.proxyRetryTimeSlotMs = 750)
                    })
#   endif
                }),
                xml::QueryElement("application", xml::Required, {}, &appNode)
            });

            if (!xmlQuery->Execute(root, xml::QueryStrategy::TestsOnlyGivenElement))
            {
                throw AppException<std::runtime_error>("Failed to load configurations from XML: there are missing required elements!");
            }

            // Now load the flat custom settings for applications making use of this framework:

            if (appNode == nullptr)
                throw AppException<std::runtime_error>("Failed to load configurations from XML: element /configuration/application not found!");

            auto node = appNode->first_node("entry");

            while (node != nullptr)
            {
                rapidxml::xml_attribute<char> *keyAttr, *valAttr;

                if ((keyAttr = node->first_attribute("key")) != nullptr
                    && (valAttr = node->first_attribute("value")) != nullptr)
                {
                    settings.application.Add(xml::GetValueSubstring(keyAttr).to_string(),
                                             xml::GetValueSubstring(valAttr).to_string());
                }
                
                node = node->next_sibling("entry");
            }
        }
        catch (IAppException &ex)
        {
            throw AppException<std::runtime_error>("3FD function is compromised by a critical error!", ex);
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "3FD function is compromised by a critical error! "
                   "Failed to initialize the application settings: " << StdLibExt::GetDetailsFromSystemError(ex);

            throw AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "3FD function is compromised by a critical error! "
                   "Failed to initialize the application settings: " << ex.what();

            throw AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace core
}// end of namespace _3fd
