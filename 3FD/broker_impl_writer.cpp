#include "stdafx.h"
#include "broker_impl.h"
#include "callstacktracer.h"
#include "exceptions.h"
#include "logger.h"
#include <sstream>
#include <future>

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

        // Create message type, contract, queue and service:
        m_dbSession << R"(
            IF NOT EXISTS ( SELECT * FROM sys.service_queues WHERE name = N'_3fdImpl_Queue%d' )
            BEGIN
                CREATE MESSAGE TYPE [%s/Message] VALIDATION = %s;
                CREATE CONTRACT [%s/Contract] ([%s/Message] SENT BY INITIATOR);
                CREATE QUEUE _3fdImpl_Queue%d WITH RETENTION = ON;
                CREATE SERVICE [%s] ON QUEUE _3fdImpl_Queue%d ([%s/Contract]);
            END;
            )"
            , (int)queueId
            , toServiceURL, ToString(msgTypeSpec.contentValidation)
            , toServiceURL, toServiceURL
            , (int)queueId
            , toServiceURL, (int)queueId, toServiceURL
            , now;

        // Create message content data type:
        m_dbSession << R"(
            IF NOT EXISTS (
	            SELECT * FROM sys.systypes
		            WHERE name = '_3fdImpl_Queue%dMsgContent'
            )
            BEGIN
	            CREATE TYPE _3fdImpl_Queue%dMsgContent FROM VARCHAR(%u);
            END;
            )"
            , (int)queueId
            , (int)queueId
            , msgTypeSpec.nBytes
            , now;

        m_dbSession << R"(
            IF NOT EXISTS (
	            SELECT * FROM sys.tables
		            WHERE name = '_3fdImpl_StageInputForQueue%d'
            )
            BEGIN
                CREATE TABLE _3fdImpl_StageInputForQueue%d (content _3fdImpl_Queue%dMsgContent);
            END;
            )"
            , (int)queueId
            , (int)queueId
            , (int)queueId
            , now;

        // Create stored procedure for reading messages from queue:
        m_dbSession << R"(
            IF OBJECT_ID(N'dbo._3fdImpl_SendMessagesToSvcQueue%d', 'P') IS NOT NULL
                DROP PROCEDURE _3fdImpl_SendMessagesToSvcQueue%d;
            )"
            , (int)queueId
            , (int)queueId
            , now;

        m_dbSession << R"(
            CREATE PROCEDURE _3fdImpl_SendMessagesToSvcQueue%d AS
            BEGIN TRY
                BEGIN TRANSACTION;

                    SET NOCOUNT ON;

                    DECLARE @dialogHandle UNIQUEIDENTIFIER;

                    BEGIN DIALOG @dialogHandle
			            FROM SERVICE [%s]
			            TO SERVICE '%s'
			            ON CONTRACT [%s/Contract]
			            WITH ENCRYPTION = OFF;

	                DECLARE @msgContent _3fdImpl_Queue%dMsgContent;

	                DECLARE cursorMsg CURSOR FOR (
		                SELECT * FROM _3fdImpl_StageInputForQueue%d
	                );

	                OPEN cursorMsg;
	                FETCH NEXT FROM cursorMsg INTO @msgContent;

	                WHILE @@FETCH_STATUS = 0
	                BEGIN
		                SEND ON CONVERSATION @dialogHandle
			                MESSAGE TYPE [%s/Message]
			                (@msgContent);

		                FETCH NEXT FROM cursorMsg INTO @msgContent;
	                END;

	                CLOSE cursorMsg;
	                DEALLOCATE cursorMsg;
                    DELETE FROM _3fdImpl_StageInputForQueue%d;

                COMMIT TRANSACTION;
            END TRY
            BEGIN CATCH

                ROLLBACK TRANSACTION;
                THROW;

            END CATCH;
            )"
            , (int)queueId
            , fromServiceURL
            , toServiceURL
            , toServiceURL
            , (int)queueId
            , (int)queueId
            , toServiceURL
            , (int)queueId
            , now;

        m_dbSession.setFeature("autoCommit", false);

        std::ostringstream oss;
        oss << "Initialized successfully the writer for broker queue " << (int)queueId
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
    /// Helps synchronizing with an asynchronous write to a broker queue.
    /// </summary>
    /// <seealso cref="notcopiable" />
    class AsyncWriteImpl : public IAsyncWrite, public std::promise<void>, notcopiable
    {
    private:

        std::unique_ptr<std::future<void>> m_future;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="AsyncWriteImpl" /> class.
        /// </summary>
        AsyncWriteImpl()
        try :
            std::promise<void>()
        {
            m_future.reset(
                new std::future<void>(this->get_future())
            );
        }
        catch (std::future_error &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "System error when instantiating helper for synchronization of write operation "
                   " in broker queue: " << core::StdLibExt::GetDetailsFromFutureError(ex);

            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (std::system_error &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "System error when instantiating helper for synchronization of write operation "
                   " in broker queue: " << core::StdLibExt::GetDetailsFromSystemError(ex);

            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Generic failure prevented instantiation of helper for synchronization "
                   " of write operation in broker queue: " << ex.what();

            throw core::AppException<std::runtime_error>(oss.str());
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="AsyncWriteImpl"/> class.
        /// </summary>
        virtual ~AsyncWriteImpl() {}

        /// <summary>
        /// Evaluates whether the last asynchronous write operation is finished.
        /// </summary>
        /// <returns>
        ///   <c>true</c> when finished, otherwise, <c>false</c>.
        /// </returns>
        virtual bool IsFinished() const override
        {
            return m_future->wait_for(std::chrono::seconds(0)) == std::future_status::ready;
        }

        /// <summary>
        /// Waits for the last asynchronous write operation to finish.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>
        ///   <c>true</c> if the operation is finished, otherwise, <c>false</c>.
        /// </returns>
        virtual bool TryWait(uint16_t timeout) override
        {
            return m_future->wait_for(
                std::chrono::milliseconds(timeout)
            ) == std::future_status::ready;
        }

        /// <summary>
        /// Rethrows any eventual exception captured in the worker thread.
        /// </summary>
        virtual void Rethrow() override
        {
            CALL_STACK_TRACE;

            try
            {
                m_future->get();
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
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure prevented writing into broker queue: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

    };// end of AsyncWriteImpl class

    // Synchronously put the messages into the broker queue
    static void PutInQueueImpl(Poco::Data::Session &dbSession,
                               const std::vector<string> &messages,
                               uint8_t queueId,
                               std::promise<void> &result)
    {
        using namespace Poco::Data;
        using namespace Poco::Data::Keywords;

        try
        {
            if (!dbSession.isConnected())
                dbSession.reconnect();

            dbSession.begin();

            dbSession << "INSERT INTO _3fdImpl_StageInputForQueue%d (content) VALUES (?);", (int)queueId, useRef(messages), now;

            dbSession.commit();

            dbSession << "EXEC _3fdImpl_SendMessagesToSvcQueue%d;", (int)queueId, now;

            result.set_value();
        }
        catch (...)
        {
            if (dbSession.isConnected() && dbSession.isTransaction())
                dbSession.rollback();

            // transport exception to main thread via promise
            result.set_exception(std::current_exception());
        }
    }

    /// <summary>
    /// Asychonously writes the messages into the queue.
    /// </summary>
    /// <param name="messages">The messages to write.</param>
    /// <return>An object to help synchronizing with the asynchronous operation.</return>
    std::unique_ptr<IAsyncWrite> QueueWriter::WriteMessages(const std::vector<string> &messages)
    {
        CALL_STACK_TRACE;

        try
        {
            if (m_workerThread && m_workerThread->joinable())
                m_workerThread->join();

            std::unique_ptr<IAsyncWrite> result(new AsyncWriteImpl());
            
            m_workerThread.reset(
                new std::thread(&PutInQueueImpl,
                                std::ref(m_dbSession),
                                std::ref(messages),
                                m_queueId,
                                std::ref(static_cast<AsyncWriteImpl &> (*result)))
            );

            return std::move(result);
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

    /// <summary>
    /// Finalizes an instance of the <see cref="QueueWriter"/> class.
    /// </summary>
    QueueWriter::~QueueWriter()
    {
        CALL_STACK_TRACE;

        try
        {
            if (m_workerThread && m_workerThread->joinable())
                m_workerThread->join();
        }
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when awaiting for worker thread of broker queue writer"
                << core::StdLibExt::GetDetailsFromSystemError(ex);

            core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL);
        }
    }

}// end of namespace broker
}// end of namespace _3fd
