#include "stdafx.h"
#include "broker_impl.h"
#include "configuration.h"
#include "logger.h"
#include "utils_io.h"

#include <Poco/Data/ODBC/Connector.h>

namespace _3fd
{
namespace broker
{
    /// <summary>
    /// Converts an enumerated option of backend to a label.
    /// </summary>
    /// <param name="backend">The backend option.</param>
    /// <returns>A label for the backend.</returns>
    const char *ToString(Backend backend)
    {
        switch (backend)
        {
        case Backend::MsSqlServer:
            return "Microsoft SQL Server";
        default:
            _ASSERTE(false);
            return "UNKNOWN";
        }
    }


    /// <summary>
    /// Converts an enumerated type of message content validation to a label.
    /// </summary>
    /// <param name="msgContentValidation">What validation to use in message content.</param>
    /// <returns>A label as expected by T-SQL.</returns>
    const char *ToString(MessageContentValidation msgContentValidation)
    {
        switch (msgContentValidation)
        {
        case MessageContentValidation::None:
            return "NONE";
        case MessageContentValidation::WellFormedXml:
            return "WELL_FORMED_XML";
        default:
            _ASSERTE(false);
            return "UNKNOWN";
        }
    }


    /// <summary>
    /// Initializes a new instance of the <see cref="OdbcClient"/> class.
    /// </summary>
    OdbcClient::OdbcClient()
    {
        Poco::Data::ODBC::Connector::registerConnector();
    }


    /// <summary>
    /// Gets an ODBC database connection.
    /// Fails with an exception only after timeout and a number
    /// of retries specified in framework configuration file.
    /// </summary>
    /// <param name="connString">The connection string.</param>
    /// <returns>The succesfully established connection.</returns>
    std::unique_ptr<Poco::Data::Session> GetConnection(const string &connString)
    {
        int retryCount(0);

        while (true)
        {
            try
            {
                static const auto timeout =
                    core::AppConfig::GetSettings().framework.broker.dbConnTimeoutSecs;

                return std::unique_ptr<Poco::Data::Session>(
                    new Poco::Data::Session("ODBC", connString, timeout)
                );
            }
            catch (Poco::Data::ConnectionFailedException &ex)
            {
                static const auto maxRetries =
                    core::AppConfig::GetSettings().framework.broker.dbConnMaxRetries;

                if (retryCount >= maxRetries)
                    ex.rethrow();
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
    Poco::Data::Session &CheckConnection(Poco::Data::Session &dbSession)
    {
        if (dbSession.isConnected())
            return dbSession;

        static const auto maxRetries =
            core::AppConfig::GetSettings().framework.broker.dbConnMaxRetries;

        int retryCount(1);

        if (retryCount <= maxRetries)
        {
            std::array<char, 512> bufErrMsg;

            utils::SerializeTo(bufErrMsg,
                "Lost connection to broker queue in database '", dbSession.uri(),
                "'. Client will attempt reconnection up to ", maxRetries, " time(s)");

            core::Logger::Write(bufErrMsg.data(), core::Logger::PRIO_WARNING);
        }

        // loop for reconnection:
        while (true)
        {
            try
            {
                dbSession.reconnect();

                std::array<char, 512> bufErrMsg;

                utils::SerializeTo(bufErrMsg,
                    "Succesfully reconnected to broker queue in database '",
                    dbSession.uri(), '\'');

                core::Logger::Write(bufErrMsg.data(), core::Logger::PRIO_WARNING);

                return dbSession;
            }
            catch (Poco::Data::ConnectionFailedException &ex)
            {
                if (retryCount >= maxRetries)
                    ex.rethrow();
            }

            ++retryCount;

        }// end of loop
    }

}// end of namespace broker
}// end of namespace _3fd