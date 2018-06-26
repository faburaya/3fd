#include "stdafx.h"
#include "configuration.h"
#include "exceptions.h"
#include "utils_io.h"
#include <rapidxml.hpp>

#ifdef _3FD_PLATFORM_WINRT
#    include "utils_winrt.h"
#endif

#include <map>
#include <array>
#include <atomic>
#include <memory>
#include <fstream>
#include <sstream>
#include <codecvt>
#include <cstdlib>
#include <cinttypes>
#include <system_error>

#ifdef _WIN32
#   define strcasecmp _stricmp
#elif defined __linux__ || defined __unix__ // POSIX:
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
            return (strcasecmp(iter->second.c_str(), "true") == 0);
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
                    std::unique_ptr<AppConfig> temp(new AppConfig());
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
        auto curPackageIdName = Windows::ApplicationModel::Package::Current->Id->Name;

        try
        {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            string id = converter.to_bytes( curPackageIdName->Data() );
            appFilePath = utils::WinRTExt::GetFilePathUtf8(id, utils::WinRTExt::FileLocation::InstallFolder);
            return id;
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when determining the file name of the framework configuration file: " << ex.what();
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


    typedef std::map<string, const char *> XmlDictionary;


    /// <summary>
    /// Loads all entries of key-value pair under a given node into a dictionary.
    /// </summary>
    /// <param name="parent">The node parenting the entries.</param>
    /// <param name="kvPairs">The dictionary to receive the key-value pairs.</param>
    static void LoadEntriesIntoDictionary(rapidxml::xml_node<char> *parent, XmlDictionary &kvPairs)
    {
        _ASSERTE(parent != nullptr);

        auto entry = parent->first_node("entry");

        while (entry != nullptr)
        {
            rapidxml::xml_attribute<char> *keyAttr, *valAttr;

            if ((keyAttr = entry->first_attribute("key")) != nullptr)
                if ((valAttr = entry->first_attribute("value")) != nullptr)
                    kvPairs[keyAttr->value()] = valAttr->value();

            entry = entry->next_sibling();
        }
    }

    /// <summary>
    /// Parses an unsigned integer value with a given key from dictionary data loaded from XML.
    /// </summary>
    /// <param name="kvPairs">The dictionary whose data was loaded from XML.</param>
    /// <param name="key">The key of the value to parse.</param>
    /// <param name="to">Reference to receive the parsed value.</param>
    /// <param name="defaultVal">The default value to set in case the key is not found in the dictionary.</param>
    static void ParseValue(const XmlDictionary &kvPairs, const char *key, uint32_t &to, uint32_t defaultVal)
    {
        auto iter = kvPairs.find(key);

        if (iter != kvPairs.end())
            to = strtoul(iter->second, nullptr, 10);
        else
            to = defaultVal;
    }

    /// <summary>
    /// Parses an integer value with a given key from dictionary data loaded from XML.
    /// </summary>
    /// <param name="kvPairs">The dictionary whose data was loaded from XML.</param>
    /// <param name="key">The key of the value to parse.</param>
    /// <param name="to">Reference to receive the parsed value.</param>
    /// <param name="defaultVal">The default value to set in case the key is not found in the dictionary.</param>
    static void ParseValue(const XmlDictionary &kvPairs, const char *key, int32_t &to, int32_t defaultVal)
    {
        auto iter = kvPairs.find(key);

        if (iter != kvPairs.end())
            to = strtol(iter->second, nullptr, 10);
        else
            to = defaultVal;
    }

    /// <summary>
    /// Parses a floating point value with a given key from dictionary data loaded from XML.
    /// </summary>
    /// <param name="kvPairs">The dictionary whose data was loaded from XML.</param>
    /// <param name="key">The key of the value to parse.</param>
    /// <param name="to">Reference to receive the parsed value.</param>
    /// <param name="defaultVal">The default value to set in case the key is not found in the dictionary.</param>
    static void ParseValue(const XmlDictionary &kvPairs, const char *key, float &to, float defaultVal)
    {
        auto iter = kvPairs.find(key);

        if (iter != kvPairs.end())
            to = strtof(iter->second, nullptr);
        else
            to = defaultVal;
    }

    /// <summary>
    /// Parses a boolean value with a given key from dictionary data loaded from XML.
    /// </summary>
    /// <param name="kvPairs">The dictionary whose data was loaded from XML.</param>
    /// <param name="key">The key of the value to parse.</param>
    /// <param name="to">Reference to receive the parsed value.</param>
    /// <param name="defaultVal">The default value to set in case the key is not found in the dictionary.</param>
    static void ParseValue(const XmlDictionary &kvPairs, const char *key, bool &to, bool defaultVal)
    {
        auto iter = kvPairs.find(key);

        if (iter != kvPairs.end())
            to = (strcasecmp(iter->second, "true") == 0);
        else
            to = defaultVal;
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

            // open XML configuration file:
            std::ifstream ifs(cfgFilePath, std::ios::binary);

            if (!ifs.is_open())
                throw AppException<std::runtime_error>("Failed to open configuration file", cfgFilePath);

            // get the file size:
            auto nBytes = static_cast<std::streamsize> (ifs.seekg(0, std::ios::end).tellg());
            ifs.seekg(0, std::ios::beg); // rewind

            // read the whole file at once:
            std::unique_ptr<char[]> buffer(new char[nBytes + 1]);
            ifs.read(buffer.get(), nBytes);
            _ASSERTE(ifs.gcount() == nBytes);
            buffer.get()[nBytes] = 0;

            if (ifs.bad())
                throw AppException<std::runtime_error>("Failure when reading configuration file", cfgFilePath);

            // parse the XML, then load the configurations:
            rapidxml::xml_document<char> dom;
            dom.parse<0>(buffer.get());

            auto root = dom.first_node("configuration");

            if (root == nullptr)
                throw AppException<std::runtime_error>("Failed to load configurations from XML: element /configuration not found!");

            rapidxml::xml_node<char> *node = root->first_node("common");

            if (node == nullptr)
                throw AppException<std::runtime_error>("Failed to load configurations from XML: element /configuration/common not found!");

            XmlDictionary dictionary;

            // XPath /configuration/common/log:
            if ((node = node->first_node("log")) != nullptr)
            {
                LoadEntriesIntoDictionary(node, dictionary);
                ParseValue(dictionary, "sizeLimit",      settings.common.log.sizeLimit, 1024);
#   ifndef _3FD_PLATFORM_WINRT
                ParseValue(dictionary, "purgeAge",       settings.common.log.purgeAge, 30);
                ParseValue(dictionary, "purgeCount",     settings.common.log.purgeCount, 16);
                ParseValue(dictionary, "writeToConsole", settings.common.log.writeToConsole, false);
#   endif
                dictionary.clear();
            }

            auto framework = root->first_node("framework");

            if (framework == nullptr)
                throw AppException<std::runtime_error>("Failed to load configurations from XML: element /configuration/framework not found!");

#   ifndef _3FD_PLATFORM_WINRT
            // XPath /configuration/framework/dependencies:
            if ((node = framework->first_node("dependencies")) != nullptr)
            {
                LoadEntriesIntoDictionary(node, dictionary);
                ParseValue(dictionary, "opencl", settings.framework.dependencies.opencl, false);
                dictionary.clear();
            }
#   endif
            // XPath /configuration/framework/stackTracing:
            if ((node = framework->first_node("stackTracing")) != nullptr)
            {
                LoadEntriesIntoDictionary(node, dictionary);
                ParseValue(dictionary, "stackLogInitialCap", settings.framework.stackTracing.stackLogInitialCap, 32);
                dictionary.clear();
            }

            // XPath /configuration/framework/gc:
            if ((node = framework->first_node("gc")) != nullptr)
            {
                LoadEntriesIntoDictionary(node, dictionary);
                ParseValue(dictionary, "msgLoopSleepTimeoutMillisecs",       settings.framework.gc.msgLoopSleepTimeoutMilisecs, 100);
                ParseValue(dictionary, "memoryBlocksPoolInitialSize",        settings.framework.gc.memBlocksMemPool.initialSize, 128);
                ParseValue(dictionary, "memoryBlocksPoolGrowingFactor",      settings.framework.gc.memBlocksMemPool.growingFactor, 1.0);
                ParseValue(dictionary, "sptrObjsHashTabInitSizeLog2",        settings.framework.gc.sptrObjectsHashTable.initialSizeLog2, 8);
                ParseValue(dictionary, "sptrObjsHashTabLoadFactorThreshold", settings.framework.gc.sptrObjectsHashTable.loadFactorThreshold, 0.7);
                dictionary.clear();
            }

#    ifdef _3FD_OPENCL_SUPPORT

            // XPath /configuration/framework/opencl:
            if ((node = framework->first_node("opencl")) != nullptr)
            {
                LoadEntriesIntoDictionary(node, dictionary);
                ParseValue(dictionary, "maxSourceCodeLineLength", settings.framework.opencl.maxSourceCodeLineLength, 128);
                ParseValue(dictionary, "maxBuildLogSize",         settings.framework.opencl.maxBuildLogSize, 5120);
                dictionary.clear();
            }
#    endif
#    ifdef _3FD_ESENT_SUPPORT

            // XPath /configuration/framework/isam:
            if ((node = framework->first_node("isam")) != nullptr)
            {
                LoadEntriesIntoDictionary(node, dictionary);
                ParseValue(dictionary, "useWindowsFileCache", settings.framework.isam.useWindowsFileCache, true);
                dictionary.clear();
            }
#    endif
#   ifndef _3FD_PLATFORM_WINRT
            // XPath /configuration/framework/broker:
            if ((node = framework->first_node("broker")) != nullptr)
            {
                LoadEntriesIntoDictionary(node, dictionary);
                ParseValue(dictionary, "dbConnTimeoutSecs", settings.framework.broker.dbConnTimeoutSecs, 60);
                ParseValue(dictionary, "dbConnMaxRetries", settings.framework.broker.dbConnMaxRetries, 1);
                dictionary.clear();
            }
#   endif
#   ifdef _3FD_PLATFORM_WIN32API

            // XPath /configuration/framework/rpc:
            if ((node = framework->first_node("rpc")) != nullptr)
            {
                LoadEntriesIntoDictionary(node, dictionary);
                ParseValue(dictionary, "cliSrvConnectMaxRetries",  settings.framework.rpc.cliSrvConnectMaxRetries, 10);
                ParseValue(dictionary, "cliSrvConnRetrySleepSecs", settings.framework.rpc.cliSrvConnRetrySleepSecs, 5);
                ParseValue(dictionary, "cliCallMaxRetries",        settings.framework.rpc.cliCallMaxRetries, 10);
                ParseValue(dictionary, "cliCallRetrySleepMs",      settings.framework.rpc.cliCallRetrySleepMs, 1000);
                ParseValue(dictionary, "cliCallRetryTimeSlotMs",   settings.framework.rpc.cliCallRetryTimeSlotMs, 500);
                dictionary.clear();
            }

            // XPath /configuration/framework/wws:
            if ((node = framework->first_node("wws")) != nullptr)
            {
                LoadEntriesIntoDictionary(node, dictionary);
                ParseValue(dictionary, "proxyConnMaxRetries",  settings.framework.wws.proxyConnMaxRetries, 10);
                ParseValue(dictionary, "proxyCallMaxRetries",  settings.framework.wws.proxyCallMaxRetries, 10);
                ParseValue(dictionary, "proxyRetrySleepSecs",  settings.framework.wws.proxyRetrySleepSecs, 5);
                ParseValue(dictionary, "proxyRetryTimeSlotMs", settings.framework.wws.proxyRetryTimeSlotMs, 750);
                dictionary.clear();
            }
#   endif
            // Now load the flat custom settings for applications making use of this framework:

            node = root->first_node("application");

            if (node == nullptr)
                throw AppException<std::runtime_error>("Failed to load configurations from XML: element /configuration/application not found!");

            node = node->first_node("entry");

            while (node != nullptr)
            {
                rapidxml::xml_attribute<char> *keyAttr, *valAttr;

                if ((keyAttr = node->first_attribute("key")) != nullptr)
                    if ((valAttr = node->first_attribute("value")) != nullptr)
                        settings.application.Add(keyAttr->value(), valAttr->value());

                node = node->next_sibling();
            }
        }
        catch (IAppException &ex)
        {
            throw AppException<std::runtime_error>("3FD function is compromised by a critical error!", ex);
        }
        catch (rapidxml::parse_error &ex)
        {
            ostringstream oss;
            oss << "RAPIDXML library reported: " << ex.what() << " at " << ex.where<char>();

            throw AppException<std::runtime_error>(
                "3FD function is compromised by a critical error! "
                "Failed to initialize the application settings",
                oss.str()
            );
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
