#include "stdafx.h"
#include "broker_impl.h"
#include "callstacktracer.h"
#include "exceptions.h"
#include "logger.h"
#include <sstream>

namespace _3fd
{
namespace broker
{
    using std::string;

    /// <summary>
    /// Initializes a new instance of the <see cref="QueueReader"/> class.
    /// </summary>
    /// <param name="svcBrokerBackend">The broker backend to use.</param>
    /// <param name="connString">The backend connection string.</param>
    /// <param name="serviceURL">The service URL.</param>
    /// <param name="msgTypeSpec">The specification for the message type.</param>
    QueueReader::QueueReader(Backend svcBrokerBackend,
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

        // Create message type, contract, queue and service:
        m_dbSession << R"(
            IF NOT EXISTS ( SELECT * FROM sys.service_queues WHERE name = N'%s/v1_0_0/Queue' )
            BEGIN
                CREATE MESSAGE TYPE [%s/v1_0_0/Message] VALIDATION = %s;
                CREATE CONTRACT [%s/v1_0_0/Contract] ([%s/v1_0_0/Message] SENT BY INITIATOR);
                CREATE QUEUE [%s/v1_0_0/Queue] WITH RETENTION = ON;
                CREATE SERVICE [%s/v1_0_0] ON QUEUE [%s/v1_0_0/Queue] ([%s/v1_0_0/Contract]);
            END;
            )"
            , serviceURL
            , serviceURL, ToString(msgTypeSpec.contentValidation)
            , serviceURL, serviceURL
            , serviceURL
            , serviceURL, serviceURL, serviceURL
            , now;

        // Create message content data type:
        m_dbSession << R"(
            IF NOT EXISTS (
	            SELECT * FROM sys.systypes
		            WHERE name = N'%s/v1_0_0/Message/ContentType'
            )
            BEGIN
	            CREATE TYPE [%s/v1_0_0/Message/ContentType] FROM VARCHAR(%u);
            END;
            )"
            , serviceURL
            , serviceURL, msgTypeSpec.nBytes
            , now;

        // Create stored procedure to read messages from queue:

        m_dbSession << R"(
            IF OBJECT_ID(N'dbo.%s/v1_0_0/ReadMessagesProc', N'P') IS NOT NULL
	            DROP PROCEDURE [%s/v1_0_0/ReadMessagesProc];
            )"
            , serviceURL
            , serviceURL
            , now;

        m_dbSession << R"(
            CREATE PROCEDURE [%s/v1_0_0/ReadMessagesProc]
	             @recvTimeoutMilisecs INT
            AS
            BEGIN TRY
                BEGIN TRANSACTION;

                    SET NOCOUNT ON;

                    DECLARE @ReceivedMessages TABLE (
                        queuing_order        BIGINT
                        ,conversation_handle UNIQUEIDENTIFIER
                        ,message_type_name   SYSNAME
                        ,message_body        [%s/v1_0_0/Message/ContentType]
                    );

	                WAITFOR(
		                RECEIVE queuing_order
                                ,conversation_handle
			                    ,message_type_name
			                    ,message_body
		                    FROM [%s/v1_0_0/Queue]
                            INTO @ReceivedMessages
	                )
	                ,TIMEOUT @recvTimeoutMilisecs;

                    DECLARE @RowsetOut        TABLE (content [%s/v1_0_0/Message/ContentType]);
                    DECLARE @prevDialogHandle UNIQUEIDENTIFIER;
                    DECLARE @dialogHandle     UNIQUEIDENTIFIER;
	                DECLARE @msgTypeName      SYSNAME;
	                DECLARE @msgContent       [%s/v1_0_0/Message/ContentType];

                    DECLARE cursorMsg
                        CURSOR FORWARD_ONLY READ_ONLY
                        FOR SELECT conversation_handle
                                   ,message_type_name
                                   ,message_body
                            FROM @ReceivedMessages
                            ORDER BY queuing_order;

                    OPEN cursorMsg;
                    FETCH NEXT FROM cursorMsg INTO @dialogHandle, @msgTypeName, @msgContent;

	                WHILE @@FETCH_STATUS = 0
	                BEGIN
                        IF @prevDialogHandle <> @dialogHandle AND @dialogHandle IS NOT NULL
                            END CONVERSATION @dialogHandle;

                        IF @msgTypeName = '%s/v1_0_0/Message'
		                    INSERT INTO @RowsetOut VALUES (@msgContent);

                        ELSE IF @msgTypeName = 'http://schemas.microsoft.com/SQL/ServiceBroker/Error'
		                    THROW 50001, 'There was an error during conversation with service', 1;

                        ELSE IF @msgTypeName <> 'http://schemas.microsoft.com/SQL/ServiceBroker/EndDialog'
		                    THROW 50000, 'Message received in service broker queue had unexpected type', 1;

                        SET @prevDialogHandle = @dialogHandle;
                        FETCH NEXT FROM cursorMsg INTO @dialogHandle, @msgTypeName, @msgContent;
	                END;

                    CLOSE cursorMsg;
                    DEALLOCATE cursorMsg;

	                SELECT content FROM @RowsetOut;

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
            , now;

        std::ostringstream oss;
        oss << "Initialized successfully the reader for broker queue '" << serviceURL
            << "/v1_0_0/Queue' backed by " << ToString(svcBrokerBackend) << " via ODBC";

        core::Logger::Write(oss.str(), core::Logger::PRIO_INFORMATION);
    }
    catch (Poco::Data::DataException &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create broker queue reader. "
               "POCO C++ reported a data access error: " << ex.name();

        throw core::AppException<std::runtime_error>(oss.str(), ex.message());
    }
    catch (Poco::Exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create broker queue reader. "
               "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

        throw core::AppException<std::runtime_error>(oss.str());
    }
    catch (std::exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Generic failure prevented creation of broker queue reader: " << ex.what();
        throw core::AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Controls retrieval of results from asynchronous read of queue.
    /// </summary>
    /// <seealso cref="notcopiable" />
    class AsyncReadImpl : public IAsyncRead, notcopiable
    {
    private:

        std::unique_ptr<Poco::Data::Statement> m_stoProcExecStmt;
        std::unique_ptr<Poco::ActiveResult<size_t>> m_stoProcActRes;
        std::vector<string> m_messages;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="AsyncReadImpl" /> class.
        /// </summary>
        /// <param name="dbSession">The database session.</param>
        /// <param name="msgCountStepLimit">How many messages are to be
        /// retrieved at most at each asynchronous execution step.</param>
        /// <param name="msgRecvTimeout">The timeout (in ms) when the backend awaits for messages.</param>
        /// <param name="serviceURL">The service URL.</param>
        AsyncReadImpl(Poco::Data::Session &dbSession,
                      uint16_t msgCountStepLimit,
                      uint16_t msgRecvTimeout,
                      const string &serviceURL)
        try
        {
            CALL_STACK_TRACE;

            using namespace Poco::Data::Keywords;

            if (!dbSession.isConnected())
                dbSession.reconnect();

            char queryStrBuf[128];
            sprintf(queryStrBuf, "EXEC [%s/v1_0_0/ReadMessagesProc] %d;", serviceURL.c_str(), (int)msgRecvTimeout);

            m_stoProcExecStmt.reset(new Poco::Data::Statement(
                (dbSession << queryStrBuf, into(m_messages), limit(msgCountStepLimit))
            ));

            m_messages.reserve(msgCountStepLimit);
            
            m_stoProcActRes.reset(
                new Poco::ActiveResult<size_t>(m_stoProcExecStmt->executeAsync())
            );
        }
        catch (Poco::Data::DataException &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Failed to read messages from broker queue. "
                   "POCO C++ reported a data access error: " << ex.name();

            throw core::AppException<std::runtime_error>(oss.str(), ex.message());
        }
        catch (Poco::Exception &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Failed to read messages from broker queue. "
                   "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

            throw core::AppException<std::runtime_error>(oss.str());
        }
        catch (std::exception &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Generic failure prevented reading from broker queue: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="AsyncReadImpl"/> class.
        /// </summary>
        virtual AsyncReadImpl::~AsyncReadImpl() {}

        /// <summary>
        /// Evaluates whether the last asynchronous read step is finished.
        /// </summary>
        /// <returns>
        ///   <c>true</c> when finished, otherwise, <c>false</c>.
        /// </returns>
        virtual bool IsFinished() const override
        {
            CALL_STACK_TRACE;

            try
            {
                return m_stoProcActRes->available();
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "Failed to evaluate status of broker queue read operation. "
                       "POCO C++ reported an error - " << ex.name() << ": " << ex.message();

                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Waits for the last asynchronous read step to finish.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>
        ///   <c>true</c> if any message could be retrieved, otherwise, <c>false</c>.
        /// </returns>
        virtual bool TryWait(uint16_t timeout) override
        {
            CALL_STACK_TRACE;

            bool isAwaitEndOkay;

            try
            {
                isAwaitEndOkay = m_stoProcActRes->tryWait(timeout);
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "There was a failure when awaiting for the end of a read operation step in broker queue. "
                       "POCO C++ reported an error - " << ex.name() << ": " << ex.message();

                throw core::AppException<std::runtime_error>(oss.str());
            }

            if (isAwaitEndOkay && m_stoProcActRes->failed())
            {
                std::ostringstream oss;
                oss << "Failed to read broker queue: " << m_stoProcActRes->error();
                throw core::AppException<std::runtime_error>(oss.str());
            }

            return isAwaitEndOkay;
        }

        /// <summary>
        /// Start the asynchronous execution of the next step.
        /// </summary>
        virtual void Step() override
        {
            CALL_STACK_TRACE;

            try
            {
                if (!m_stoProcActRes->available())
                {
                    throw core::AppException<std::logic_error>(
                        "Could not step into execution because the last asynchronous operation is pending!"
                    );
                }

                m_messages.clear();

                m_stoProcActRes.reset(
                    new Poco::ActiveResult<size_t>(m_stoProcExecStmt->executeAsync())
                );
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions raised for errors known to have been already handled
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "Failed to step into execution of broker queue read. "
                       "POCO C++ reported an error - " << ex.name() << ": " << ex.message();

                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure prevented stepping into execution of broker queue read: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Gets the count of retrieved messages in the last step execution.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>How many messages were read from the queue.</returns>
        virtual uint32_t GetStepMessageCount(uint16_t timeout) override
        {
            CALL_STACK_TRACE;

            if (TryWait(timeout))
                return static_cast<uint32_t> (m_stoProcActRes->data());

            return 0;
        }

        /// <summary>
        /// Gets the result from the last asynchronous step execution.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>A vector of read messages.</returns>
        virtual std::vector<string> GetStepResult(uint16_t timeout) override
        {
            CALL_STACK_TRACE;

            if (!TryWait(timeout))
                return std::vector<string>();

            std::vector<string> result;

            if (m_stoProcActRes->data() > 0)
                m_messages.swap(result);

            return std::move(result);
        }
    };

    /// <summary>
    /// Asynchronously reads the messages from the queue into a vector.
    /// </summary>
    /// <param name="msgCountStepLimit">How many messages are to be retrieved
    /// at most for each asynchronous execution step.</param>
    /// <param name="msgRecvTimeout">The timeout (in ms) when the backend awaits for messages.</param>
    /// <return>An object to control the result of the asynchronous operation.</return>
    std::unique_ptr<IAsyncRead> QueueReader::ReadMessages(uint16_t msgCountStepLimit, uint16_t msgRecvTimeout)
    {
        CALL_STACK_TRACE;

        try
        {
            return std::unique_ptr<IAsyncRead>(
                new AsyncReadImpl(m_dbSession, msgCountStepLimit, msgRecvTimeout, m_serviceURL)
            );
        }
        catch (core::IAppException &)
        {
            throw; // just forward exceptions raised for errors known to have been already handled
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure prevented reading broker queue: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

}// end of namespace broker
}// end of namespace _3fd
