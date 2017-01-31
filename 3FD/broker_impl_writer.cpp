#include "stdafx.h"
#include "broker_impl.h"
#include "callstacktracer.h"
#include "exceptions.h"
#include "configuration.h"
#include "logger.h"
#include <sstream>
#include <thread>

namespace _3fd
{
namespace broker
{
    using std::string;

    /// <summary>
    /// Initializes a new instance of the <see cref="QueueWriter"/> class.
    /// </summary>
    /// <param name="svcBrokerBackend">The broker backend to use.</param>
    /// <param name="connString">The backend connection string.</param>
    /// <param name="fromServiceURL">The URL of the service that writes the messages.</param>
    /// <param name="toServiceURL">The URL of the service that reads the messages.</param>
    /// <param name="msgTypeSpec">The specification for the message type.</param>
    /// <param name="queueId">The queue number identifier.</param>
    QueueWriter::QueueWriter(Backend svcBrokerBackend,
                             const string &connString,
                             const string &fromServiceURL,
                             const string &toServiceURL,
                             const MessageTypeSpec &msgTypeSpec,
                             uint8_t queueId)
    try :
        m_dbSession("ODBC", connString),
        m_queueId(queueId)
    {
        CALL_STACK_TRACE;

        _ASSERTE(svcBrokerBackend == Backend::MsSqlServer);

        using namespace Poco::Data;
        using namespace Poco::Data::Keywords;

        // Create message types, contract, queue and service:
        m_dbSession << R"(
            IF NOT EXISTS ( SELECT * FROM sys.service_queues WHERE name = N'Queue%d' )
            BEGIN
                CREATE MESSAGE TYPE [%s/Message] VALIDATION = %s;
                CREATE CONTRACT [%s/Contract] ([%s/Message] SENT BY INITIATOR);
                CREATE QUEUE Queue%d;
                CREATE SERVICE [%s] ON QUEUE Queue%d ([%s/Contract]);
            END;
            )"
            , (int)queueId
            , toServiceURL, ToString(msgTypeSpec.contentValidation)
            , toServiceURL, toServiceURL
            , (int)queueId
            , toServiceURL, (int)queueId, toServiceURL
            , now;

        // Create content data type:
        m_dbSession << R"(
            IF NOT EXISTS (
	            SELECT * FROM sys.systypes
		            WHERE name = 'EncodedContent'
            )
            BEGIN
	            CREATE TYPE EncodedContent FROM VARCHAR(%d);
            END;
            )"
            , core::AppConfig::GetSettings().framework.broker.maxMessageSizeBytes
            , now;

        CreateTempTableForQueueInput();

        // Create stored procedure for reading messages from queue:
        m_dbSession << R"(
            IF OBJECT_ID(N'dbo.SendMessagesToService%d', 'P') IS NOT NULL
                DROP PROCEDURE SendMessagesToService%d;
            )"
            , (int)queueId
            , (int)queueId
            , now;

        m_dbSession << R"(
            CREATE PROCEDURE SendMessagesToService%d AS
            BEGIN
	            DECLARE @MsgContent EncodedContent;

	            DECLARE MsgCursor CURSOR FOR
	            (
		            SELECT * FROM #Queue%dInput
	            );
	            OPEN MsgCursor;
	            FETCH NEXT FROM MsgCursor INTO @MsgContent;
	
	            DECLARE @InitDlgHandle UNIQUEIDENTIFIER;

	            WHILE @@FETCH_STATUS = 0
	            BEGIN
		            BEGIN DIALOG @InitDlgHandle
			            FROM SERVICE [%s]
			            TO SERVICE '%s'
			            ON CONTRACT [%s/Contract]
			            WITH ENCRYPTION = OFF;

		            SEND ON CONVERSATION @InitDlgHandle
			            MESSAGE TYPE [%s/Message]
			            (@MsgContent);

		            FETCH NEXT FROM MsgCursor INTO @MsgContent;
	            END;

	            CLOSE MsgCursor;
	            DEALLOCATE MsgCursor;

                DELETE FROM #Queue%dInput;
            END;
            )"
            , (int)queueId
            , (int)queueId
            , fromServiceURL
            , toServiceURL
            , toServiceURL
            , toServiceURL
            , now;

        std::ostringstream oss;
        oss << "Initialized successfully the writer for broker queue " << queueId
            << " at '" << toServiceURL << "' backed by "
            << ToString(svcBrokerBackend) << " via ODBC";

        core::Logger::Write(oss.str(), core::Logger::PRIO_INFORMATION);
    }
    catch (Poco::Data::DataException &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create broker queue writer. "
               "POCO C++ reported a data access error: " << ex.name();

        throw core::AppException<std::runtime_error>(oss.str(), ex.message());
    }
    catch (Poco::Exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create broker queue writer. "
               "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

        throw core::AppException<std::runtime_error>(oss.str());
    }
    catch (std::exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Generic failure prevented creation of broker queue writer: " << ex.what();
        throw core::AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Creates a temporary table for queue input.
    /// </summary>
    void QueueWriter::CreateTempTableForQueueInput()
    {
        CALL_STACK_TRACE;

        using namespace Poco::Data::Keywords;

        m_dbSession << R"(
            IF NOT EXISTS (
	            SELECT * FROM tempdb.sys.objects
		            WHERE name LIKE '#Queue%dInput%%'
            )
            BEGIN
                CREATE TABLE #Queue%dInput (content VARCHAR(%d));
            END;
            )"
            , (int)m_queueId
            , (int)m_queueId
            , core::AppConfig::GetSettings().framework.broker.maxMessageSizeBytes
            , now;
    }

    // Synchronously put the messages into the broker queue
    static void PutInQueueImpl(Poco::Data::Session &dbSession,
                               const std::vector<string> &messages,
                               uint8_t queueId,
                               std::promise<void> &result)
    {
        CALL_STACK_TRACE;

        using namespace Poco::Data;
        using namespace Poco::Data::Keywords;

        try
        {
            dbSession.begin();

            dbSession << "INSERT INTO #Queue%dInput (Content) VALUES (?);", (int)queueId, useRef(messages), now;
            dbSession << "EXEC SendMessagesToService%d;", (int)queueId, now;

            dbSession.commit();
        }
        catch (DataException &ex)
        {
            if (dbSession.isConnected() && dbSession.isTransaction())
                dbSession.rollback();

            std::ostringstream oss;
            oss << "Failed to write messages into broker queue. "
                   "POCO C++ reported a data access error: " << ex.name();

            result.set_exception(
                std::make_exception_ptr(core::AppException<std::runtime_error>(oss.str(), ex.message()))
            );
        }
        catch (Poco::Exception &ex)
        {
            if (dbSession.isConnected() && dbSession.isTransaction())
                dbSession.rollback();

            std::ostringstream oss;
            oss << "Failed to write messages into broker queue. "
                   "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

            result.set_exception(
                std::make_exception_ptr(core::AppException<std::runtime_error>(oss.str()))
            );
        }
        catch (std::exception &ex)
        {
            if (dbSession.isConnected() && dbSession.isTransaction())
                dbSession.rollback();

            std::ostringstream oss;
            oss << "Generic failure prevented writing into broker queue: " << ex.what();

            result.set_exception(
                std::make_exception_ptr(core::AppException<std::runtime_error>(oss.str()))
            );
        }

        result.set_value();
    }

    /// <summary>
    /// Asychonously writes the messages into the queue.
    /// </summary>
    /// <param name="messages">The messages to write.</param>
    /// <returns>An object that represents the asynchronous operation.</returns>
    std::future<void> QueueWriter::WriteMessages(const std::vector<string> &messages)
    {
        CALL_STACK_TRACE;

        try
        {
            if (!m_dbSession.isConnected())
            {
                m_dbSession.reconnect();
                CreateTempTableForQueueInput();
            }
            
            std::promise<void> result;
            auto workerThread = std::thread(&PutInQueueImpl,
                                            std::ref(m_dbSession),
                                            std::ref(messages),
                                            m_queueId,
                                            std::ref(result));

            workerThread.detach();
            return result.get_future();
        }
        catch (core::IAppException &)
        {
            throw; // just forward exceptions from errors known to have been already handled
        }
        catch (Poco::Data::DataException &ex)
        {
            std::ostringstream oss;
            oss << "Failed to write messages into broker queue. "
                   "POCO C++ reported a data access error: " << ex.name();

            throw core::AppException<std::runtime_error>(oss.str(), ex.message());
        }
        catch (Poco::Exception &ex)
        {
            std::ostringstream oss;
            oss << "Failed to write messages into broker queue. "
                   "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (std::future_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when attempting to write asynchronously into broker queue: "
                << core::StdLibExt::GetDetailsFromFutureError(ex);

            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when attempting to write asynchronously into broker queue: "
                << core::StdLibExt::GetDetailsFromSystemError(ex);

            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure prevented writing into broker queue: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace broker
}// end of namespace _3fd
