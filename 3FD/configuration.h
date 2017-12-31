#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "preprocessing.h"
#include <cinttypes>
#include <memory>
#include <string>
#include <chrono>
#include <mutex>
#include <map>

namespace _3fd
{
namespace core
{
    using std::string;


    /// <summary>
    /// Holds a flexible and flat set of application settings
    /// to be loaded from the XML configuration file.
    /// </summary>
    class AppFlexSettings
    {
    private:

        std::map<string, string> m_settings;

    public:

        void Add(const string &key, const string &value);

        string GetString(const string &key, const char *defValue) const;

        bool GetBool(const string &key, bool defValue) const;

        int32_t GetInt(const string &key, int32_t defValue) const;

        uint32_t GetUInt(const string &key, uint32_t defValue) const;

        float GetFloat(const string &key, float defValue) const;
    };


    /// <summary>
    /// A singleton that holds the application settings.
    /// </summary>
    class AppConfig
    {
    private:

        struct Tree
        {
            // Used by both application and framework:
            struct
            {
                struct
                {
#ifdef _3FD_POCO_SUPPORT // For POCO C++ logging facilities:
                    uint32_t purgeAge;
                    uint32_t purgeCount;
                    bool     writeToConsole;
#endif
                    uint32_t sizeLimit;
                } log;
            } common;

            // Required by the framework and for its exclusive use:
            struct
            {
                struct
                {
#ifdef _3FD_OPENCL_SUPPORT
                    bool opencl;
#endif
                } dependencies;

                struct
                {
                    uint32_t stackLogInitialCap;
                } stackTracing;

                struct
                {
                    uint32_t msgLoopSleepTimeoutMilisecs;
                        
                    struct
                    {
                        uint32_t initialSize;
                        float    growingFactor;
                    } memBlocksMemPool;
                        
                    struct
                    {
                        uint32_t initialSizeLog2;
                        float    loadFactorThreshold;
                    } sptrObjectsHashTable;
                } gc;

#ifdef _3FD_OPENCL_SUPPORT
                struct
                {
                    uint32_t maxSourceCodeLineLength;
                    uint32_t maxBuildLogSize;
                } opencl;
#endif

#ifdef _3FD_ESENT_SUPPORT
                struct
                {
                    bool useWindowsFileCache;
                } isam;
#endif

#ifdef _3FD_POCO_SUPPORT
                struct
                {
                    uint32_t dbConnTimeoutSecs;
                    uint32_t dbConnMaxRetries;
                } broker;
#endif

#ifdef _3FD_PLATFORM_WIN32API
                struct
                {
                    uint32_t cliSrvConnectMaxRetries;
                    uint32_t cliSrvConnRetrySleepSecs;
                    uint32_t cliCallMaxRetries;
                    uint32_t cliCallRetrySleepMs;
                    uint32_t cliCallRetryTimeSlotMs;
                } rpc;

                struct
                {
                    uint32_t proxyConnMaxRetries;
                    uint32_t proxyCallMaxRetries;
                    uint32_t proxyRetrySleepSecs;
                    uint32_t proxyRetryTimeSlotMs;
                } wws;
#endif
            } framework;

            AppFlexSettings application;

        } settings;

        string m_applicationId;

        static std::unique_ptr<AppConfig> uniqueObject;
        static std::mutex initializationMutex;

        static AppConfig &GetInstanceInitialized();

        void Initialize();

        AppConfig() {} // empty private constructor

    public:

        static const string &GetApplicationId();

        static const Tree &GetSettings();
    };

}// end of namespace core
}// end of namespace _3fd

#endif // end of header guard
