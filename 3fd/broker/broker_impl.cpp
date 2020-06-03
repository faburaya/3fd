//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include "broker_impl.h"
#include <3fd/core/configuration.h>
#include <3fd/core/logger.h>
#include <3fd/utils/utils_io.h>
#include <3fd/utils/utils_string.h>

namespace _3fd
{
namespace broker
{
    /// <summary>
    /// Handles any exception caught within a catch block during execution of this module.
    /// </summary>
    void HandleException(const char *when)
    {
        std::ostringstream oss;

        try
        {
            throw;
        }
        catch (core::IAppException &)
        {
            throw; // just forward already handled error
        }
        catch (nanodbc::programming_error &ex)
        {
            oss << "ODBC SQL error when " << when << ": " << ex.what();
            throw core::AppException<std::logic_error>(oss.str());
        }
        catch (nanodbc::database_error &ex)
        {
            oss << "ODBC database error when " << when << ": " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (std::future_error &ex)
        {
            oss << "Synchronization error when " << when << ": " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str(),
                                                         core::StdLibExt::GetDetailsFromFutureError(ex));
        }
        catch (std::system_error &ex)
        {
            oss << "System error when " << when << ": " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str(),
                                                         core::StdLibExt::GetDetailsFromSystemError(ex));
        }
        catch (std::exception &ex)
        {
            oss << "Generic error when " << when << ": " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (...)
        {
            oss << "Unexpected exception when " << when;
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Log any exception caught within a catch block during execution of the module.
    /// </summary>
    void LogException(const char *when) noexcept
    {
        try
        {
            HandleException(when);
        }
        catch (core::IAppException &appEx)
        {
            core::Logger::Write(appEx, core::Logger::PRIO_ERROR);
        }
    }

    /// <summary>
    /// Converts an enumerated option of backend to a label.
    /// </summary>
    /// <param name="backend">The backend option.</param>
    /// <returns>A label for the backend.</returns>
    const char_t *ToString(Backend backend)
    {
        switch (backend)
        {
        case Backend::MsSqlServer:
            return _U("Microsoft SQL Server");
        default:
            _ASSERTE(false);
            return _U("UNKNOWN");
        }
    }

    /// <summary>
    /// Converts an enumerated type of message content validation to a label.
    /// </summary>
    /// <param name="msgContentValidation">What validation to use in message content.</param>
    /// <returns>A label as expected by T-SQL.</returns>
    const char_t *ToString(MessageContentValidation msgContentValidation)
    {
        switch (msgContentValidation)
        {
        case MessageContentValidation::None:
            return _U("NONE");
        case MessageContentValidation::WellFormedXml:
            return _U("WELL_FORMED_XML");
        default:
            _ASSERTE(false);
            return _U("UNKNOWN");
        }
    }

    /////////////////////////////////
    // DatabaseSession class
    /////////////////////////////////

    /// <summary>
    /// Gets an ODBC database connection.
    /// Fails with an exception only after timeout and a number
    /// of retries specified in framework configuration file.
    /// </summary>
    /// <param name="connString">The connection string.</param>
    /// <returns>The succesfully established connection.</returns>
    DatabaseSession::DatabaseSession(const std::string &connString)
        : m_connectionString(connString)
    {
        uint32_t retryCount(1);

        while (true)
        {
            try
            {
                static const auto timeout =
                    core::AppConfig::GetSettings().framework.broker.dbConnTimeoutSecs;

                m_dbConnection = nanodbc::connection(utils::to_unicode(connString), timeout);
                break;
            }
            catch (nanodbc::database_error &)
            {
                static const auto maxRetries =
                    core::AppConfig::GetSettings().framework.broker.dbConnMaxRetries;

                if (retryCount > maxRetries)
                    throw;

                std::array<char, 128> bufErrMsg;
                auto length = utils::SerializeTo(bufErrMsg,
                    "Could not connect to broker queue database - Attempt ",
                    retryCount, " of ", maxRetries);

                core::Logger::Write(std::string(bufErrMsg.data(), length), core::Logger::PRIO_WARNING);
            }

            ++retryCount;
        }
    }

    /// <summary>
    /// Checks the state of a database connection.
    /// Fails with an exception only if disconnected and after a
    /// number of retries specified in framework configuration file.
    /// </summary>
    /// <param name="connString">The connection string.</param>
    /// <returns>A refence to the checked connection.</returns>
    nanodbc::connection &DatabaseSession::GetConnection()
    {
        if (m_dbConnection.connected())
            return m_dbConnection;

        static const auto maxRetries =
            core::AppConfig::GetSettings().framework.broker.dbConnMaxRetries;

        uint32_t retryCount(1);

        if (retryCount <= maxRetries)
        {
            std::array<char, 128> bufErrMsg;

            auto length = utils::SerializeTo(bufErrMsg,
                "Connection to database is lost! Re-connection attempt ",
                retryCount, " of ", maxRetries);

            core::Logger::Write(std::string(bufErrMsg.data(), length), core::Logger::PRIO_WARNING);
        }

        // loop for reconnection:
        while (true)
        {
            try
            {
                static const auto timeout =
                    core::AppConfig::GetSettings().framework.broker.dbConnTimeoutSecs;

                m_dbConnection.connect(utils::to_unicode(m_connectionString), timeout);

                std::array<char, 512> bufErrMsg;

                auto length = utils::SerializeTo(bufErrMsg,
                    "Succesfully reconnected to broker queue in database '[",
                    m_dbConnection.dbms_name(), "]:", m_dbConnection.database_name(), '\'');

                core::Logger::Write(std::string(bufErrMsg.data(), length), core::Logger::PRIO_WARNING);

                return m_dbConnection;
            }
            catch (nanodbc::database_error &)
            {
                if (retryCount >= maxRetries)
                    throw;
            }

            ++retryCount;

        }// end of loop
    }

    /////////////////////////////////
    // LockProvider class
    /////////////////////////////////

    std::unique_ptr<LockProvider> LockProvider::uniqueInstance;

    std::once_flag LockProvider::initSyncFlag;

    LockProvider & LockProvider::GetInstance()
    {
        std::call_once(initSyncFlag, []()
        {
            uniqueInstance.reset(dbg_new LockProvider());
        });

        _ASSERTE(uniqueInstance);
        return *uniqueInstance;
    }

    LockProvider::Lock LockProvider::GetLockFor(const std::string &brokerSvcUrl)
    {
        // we have 1 lock per service and the ID is the URL,
        // because T-SQL is case insensitive, it is normalized here to lower case:
        std::string id;
        id.reserve(brokerSvcUrl.size());
        std::transform(brokerSvcUrl.cbegin(),
                       brokerSvcUrl.end(),
                       std::back_inserter(id), [](char ch){ return tolower(ch); });

        return Lock(m_cacheOfMutexes.GetObject(id));
    }

}// end of namespace broker
}// end of namespace _3fd