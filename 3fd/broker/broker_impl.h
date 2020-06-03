//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#ifndef BROKER_IMPL_H // header guard
#define BROKER_IMPL_H

#include "broker.h"
#include <3fd/utils/utils_concurrency.h>
#include <3fd/utils/utils_string.h>

#ifdef _WIN32
#   define NANODBC_ENABLE_UNICODE
#endif
#include <nanodbc/nanodbc.h>
#include <future>
#include <memory>
#include <mutex>

#ifdef _WIN32
// nanodbc for Windows uses UCS-2
#   define _U(TEXT) L ## TEXT
typedef wchar_t char_t;
typedef _3fd::utils::TextUcs2 Text;
#else
// nanodbc for POSIX uses UTF-8
#   define _U(TEXT) TEXT
typedef char char_t;
typedef _3fd::utils::TextUtf8 Text;
#endif

#define catch_and_handle_exception(WHEN) catch(...) { HandleException(WHEN); }

#define catch_and_log_exception(WHEN) catch(...) { LogException(WHEN); }

namespace _3fd
{
namespace broker
{
    void HandleException(const char *when);

    void LogException(const char *when) noexcept;

    const char_t *ToString(Backend backend);

    const char_t *ToString(MessageContentValidation msgContentValidation);

    /// <summary>
    /// Provides a resilient database ODBC connection.
    /// </summary>
    class DatabaseSession
    {
    private:

        nanodbc::connection m_dbConnection;
        const std::string m_connectionString;

    public:

        DatabaseSession(const std::string &connString);

        DatabaseSession(const DatabaseSession &) = delete;

        nanodbc::connection &GetConnection();
    };

    /// <summary>
    /// Provides locks to extract messages from the service broker queue.
    /// </summary>
    class LockProvider
    {
    private:

        static std::unique_ptr<LockProvider> uniqueInstance;
        static std::once_flag initSyncFlag;

        utils::CacheForSharedResources<std::string, std::mutex> m_cacheOfMutexes;

        LockProvider() = default;

        struct Lock
        {
            std::shared_ptr<std::mutex> mutex;
            std::unique_lock<std::mutex> lock;

            Lock(const Lock &) = delete;

            Lock(std::shared_ptr<std::mutex> p_mutex)
                : mutex(p_mutex)
                , lock(*p_mutex)
            {
            }

            Lock(Lock &&ob)
                : mutex(std::move(ob.mutex))
                , lock(std::move(ob.lock))
            {
            }

            ~Lock()
            {
                lock.unlock(); // first unlock
                mutex.reset(); // then release reference to the mutex
            }
        };

    public:

        LockProvider(const LockProvider &) = delete;

        static LockProvider &GetInstance();

        Lock GetLockFor(const std::string &brokerSvcUrl);
    };

}// end of namespace broker
}// end of namespace _3fd

#endif // end of header guard
