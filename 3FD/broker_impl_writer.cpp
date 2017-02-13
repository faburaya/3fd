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
    /// <param name="serviceURL">The URL of the service that reads the messages.</param>
    /// <param name="msgTypeSpec">The specification for creation of message type.
    /// Such type is createad in the backend at the first time a reader or writer
    /// for this queue is instantiated. Subsequent instantiations will not effectively
    /// alter the message type by simply using different values in this parameter.</param>
    QueueWriter::QueueWriter(Backend svcBrokerBackend,
                             const string &connString,
                             const string &serviceURL,
                             const MessageTypeSpec &msgTypeSpec)
    try :
        m_dbSession("ODBC", connString),
        m_serviceURL(serviceURL)
    {
        CALL_STACK_TRACE;

        _ASSERTE(svcBrokerBackend == Backend::MsSqlServer);

        using namespace Poco::Data;
        using namespace Poco::Data::Keywords;

        // Create message type, contract, queue, service, message content data type and input stage table:
        m_dbSession << R"(
            IF NOT EXISTS ( SELECT * FROM sys.service_queues WHERE name = N'%s/v1_0_0/Queue' )
            BEGIN
                CREATE MESSAGE TYPE [%s/v1_0_0/Message] VALIDATION = %s;
                CREATE CONTRACT [%s/v1_0_0/Contract] ([%s/v1_0_0/Message] SENT BY INITIATOR);
                CREATE QUEUE [%s/v1_0_0/Queue] WITH RETENTION = ON, POISON_MESSAGE_HANDLING (STATUS = OFF);
                CREATE SERVICE [%s/v1_0_0] ON QUEUE [%s/v1_0_0/Queue] ([%s/v1_0_0/Contract]);
            END;

            IF NOT EXISTS ( SELECT * FROM sys.service_queues WHERE name = N'%s/v1_0_0/ResponseQueue' )
            BEGIN
                CREATE QUEUE [%s/v1_0_0/ResponseQueue] WITH RETENTION = ON;
                CREATE SERVICE [%s/v1_0_0/Sender] ON QUEUE [%s/v1_0_0/ResponseQueue];
            END;

            IF NOT EXISTS (
	            SELECT * FROM sys.systypes
		            WHERE name = N'%s/v1_0_0/Message/ContentType'
            )
            BEGIN
	            CREATE TYPE [%s/v1_0_0/Message/ContentType] FROM VARCHAR(%u);
            END;

            IF NOT EXISTS (
	            SELECT * FROM sys.tables
		            WHERE name = N'%s/v1_0_0/InputStageTable'
            )
            BEGIN
                CREATE TABLE [%s/v1_0_0/InputStageTable] (content [%s/v1_0_0/Message/ContentType]);
            END;

            IF OBJECT_ID(N'dbo.%s/v1_0_0/SendMessagesProc', N'P') IS NOT NULL
                DROP PROCEDURE [%s/v1_0_0/SendMessagesProc];

            IF OBJECT_ID(N'dbo.%s/v1_0_0/FinishDialogsOnEndptInitProc', N'P') IS NOT NULL
	            DROP PROCEDURE [%s/v1_0_0/FinishDialogsOnEndptInitProc];
            )"
            , serviceURL
            , serviceURL, ToString(msgTypeSpec.contentValidation)
            , serviceURL, serviceURL
            , serviceURL
            , serviceURL, serviceURL, serviceURL

            // CREATE QUEUE ResponseQueue
            , serviceURL
            , serviceURL
            , serviceURL, serviceURL

            // CREATE TYPE Message/ContentType
            , serviceURL
            , serviceURL, msgTypeSpec.nBytes

            // CREATE TABLE InputStageTable
            , serviceURL
            , serviceURL, serviceURL

            // DROP PROCEDURE SendMessagesProc
            , serviceURL
            , serviceURL

            // DROP PROCEDURE FinishDialogsOnEndptInitProc
            , serviceURL
            , serviceURL
            , now;

        // Create stored procedure to send messages to service queue:
        m_dbSession << R"(
            CREATE PROCEDURE [%s/v1_0_0/SendMessagesProc] AS
            BEGIN TRY
                BEGIN TRANSACTION;

                    SET NOCOUNT ON;

                    DECLARE @dialogHandle UNIQUEIDENTIFIER;

                    BEGIN DIALOG @dialogHandle
                        FROM SERVICE [%s/v1_0_0/Sender]
                        TO SERVICE '%s/v1_0_0'
                        ON CONTRACT [%s/v1_0_0/Contract]
                        WITH ENCRYPTION = OFF;

                    DECLARE @msgContent [%s/v1_0_0/Message/ContentType];

                    DECLARE cursorMsg CURSOR FOR (
	                    SELECT * FROM [%s/v1_0_0/InputStageTable]
                    );

                    OPEN cursorMsg;
                    FETCH NEXT FROM cursorMsg INTO @msgContent;

                    WHILE @@FETCH_STATUS = 0
                    BEGIN
	                    SEND ON CONVERSATION @dialogHandle
		                    MESSAGE TYPE [%s/v1_0_0/Message] (@msgContent);

	                    FETCH NEXT FROM cursorMsg INTO @msgContent;
                    END;

                    CLOSE cursorMsg;
                    DEALLOCATE cursorMsg;
                    DELETE FROM [%s/v1_0_0/InputStageTable];

                COMMIT TRANSACTION;
            END TRY
            BEGIN CATCH

                ROLLBACK TRANSACTION;
                THROW;

            END CATCH;
            )"
            , serviceURL
            , serviceURL
            , serviceURL
            , serviceURL
            , serviceURL
            , serviceURL
            , serviceURL
            , serviceURL
            , now;

        // Create stored procedure to finish conversations in the initiator endpoint:
        m_dbSession << R"(
            CREATE PROCEDURE [%s/v1_0_0/FinishDialogsOnEndptInitProc] AS
            BEGIN TRY
                BEGIN TRANSACTION;

                    SET NOCOUNT ON;

                    DECLARE @ReceivedMessages TABLE (
                        conversation_handle  UNIQUEIDENTIFIER
                        ,message_type_name   SYSNAME
                    );

	                RECEIVE conversation_handle
                            ,message_type_name
		                FROM [%s/v1_0_0/ResponseQueue]
                        INTO @ReceivedMessages;
        
                    DECLARE @dialogHandle  UNIQUEIDENTIFIER;
	                DECLARE @msgTypeName   SYSNAME;
        
                    DECLARE cursorMsg
                        CURSOR FORWARD_ONLY READ_ONLY FOR
                            SELECT conversation_handle
                                   ,message_type_name
                                FROM @ReceivedMessages;

                    OPEN cursorMsg;
                    FETCH NEXT FROM cursorMsg INTO @dialogHandle, @msgTypeName;

	                WHILE @@FETCH_STATUS = 0
	                BEGIN
                        IF @msgTypeName = 'http://schemas.microsoft.com/SQL/ServiceBroker/EndDialog'
                            END CONVERSATION @dialogHandle;

		                FETCH NEXT FROM cursorMsg INTO @dialogHandle, @msgTypeName;
	                END;

                    CLOSE cursorMsg;
                    DEALLOCATE cursorMsg;

                COMMIT TRANSACTION;
            END TRY
            BEGIN CATCH

                ROLLBACK TRANSACTION;
                THROW;

            END CATCH;

            ALTER QUEUE [%s/v1_0_0/ResponseQueue]
                WITH ACTIVATION (
                    STATUS = ON,
                    MAX_QUEUE_READERS = 1,
                    PROCEDURE_NAME = [%s/v1_0_0/FinishDialogsOnEndptInitProc],
                    EXECUTE AS OWNER
                );
            )"
            , serviceURL
            , serviceURL

            // ALTER QUEUE ResponseQueue
            , serviceURL
            , serviceURL
            , now;

        m_dbSession.setFeature("autoCommit", false);

        std::ostringstream oss;
        oss << "Initialized successfully the writer for broker queue '" << serviceURL
            << "/v1_0_0/Queue' backed by " << ToString(svcBrokerBackend) << " via ODBC";

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
                               const string &serviceURL,
                               std::promise<void> &result)
    {
        using namespace Poco::Data;
        using namespace Poco::Data::Keywords;

        try
        {
            if (!dbSession.isConnected())
                dbSession.reconnect();

            dbSession.begin();

            dbSession << "INSERT INTO [%s/v1_0_0/InputStageTable] (content) VALUES (?);", serviceURL, useRef(messages), now;

            dbSession << "EXEC [%s/v1_0_0/SendMessagesProc];", serviceURL, now;

            dbSession.commit();

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
                                m_serviceURL,
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
