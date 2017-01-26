#include "stdafx.h"
#include "broker.h"
#include "callstacktracer.h"
#include "exceptions.h"
#include <sstream>

namespace _3fd
{
namespace broker
{
    using std::string;

    /// <summary>
    /// Converts an enumerated type of message content validation to a label.
    /// </summary>
    /// <param name="msgContentValidation">What validation to use in message content.</param>
    /// <returns>A label as expected by T-SQL.</returns>
    static const char *ToString(MessageContentValidation msgContentValidation)
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
    /// Initializes a new instance of the <see cref="QueueReader"/> class.
    /// </summary>
    /// <param name="svcBrokerBackend">The broker backend to use.</param>
    /// <param name="connString">The backend connection string.</param>
    /// <param name="serviceURL">The service URL.</param>
    /// <param name="msgTypeSpec">The specification for the message type.</param>
    /// <param name="queueId">The queue number identifier.</param>
    QueueReader::QueueReader(Backend svcBrokerBackend,
                             const string &connString,
                             const string &serviceURL,
                             const MessageTypeSpec &msgTypeSpec,
                             uint8_t queueId)
    try :
        m_queueId(queueId)
    {
        CALL_STACK_TRACE;

        _ASSERTE(svcBrokerBackend == Backend::MsSqlServer);

        using namespace Poco::Data;
        using namespace Poco::Data::Keywords;

        m_dbSession = new Session("ODBC", connString);

        // Create message and content types, contract, queue and service:
        *m_dbSession <<
            "IF NOT EXISTS ( SELECT * FROM sys.service_queues WHERE name = N'Queue%d' ) "
            "BEGIN "
            "    CREATE MESSAGE TYPE [%s/RequestMessage] VALIDATION = %s;"
            "    CREATE CONTRACT [%s/Contract] ([%s/RequestMessage] SENT BY INITIATOR);"
            "    CREATE QUEUE Queue%d';"
            "    CREATE SERVICE [%s] ON QUEUE Queue%d ([%s/Contract]);"
            "    CREATE TYPE EncodedContent%d FROM VARCHAR(2048);"
            "    CREATE TYPE EncodedMessages%d AS TABLE (Content EncodedContent%d);"
            "END;"
            , queueId
            , serviceURL, ToString(msgTypeSpec.contentValidation)
            , serviceURL, serviceURL
            , queueId
            , serviceURL, queueId, serviceURL
            , now;

        // Create stored procedure for reading messages from queue:
        *m_dbSession << R"(
            IF OBJECT_ID('dbo.ReceiveMessagesInQueue%d', 'P') IS NOT NULL
	            DROP PROCEDURE ReceiveMessagesInQueue%d;

            CREATE PROCEDURE ReceiveMessagesInQueue%d
	            @RecvTimeoutMilisecs INT
            AS
            BEGIN
	            DECLARE @RecvMsgDlgHandle	UNIQUEIDENTIFIER;
	            DECLARE @RecvMsgTypeName	SYSNAME;
	            DECLARE @RecvMsgContent		EncodedContent%d;

	            WAITFOR(
		            RECEIVE TOP(1)
			            @RecvMsgDlgHandle = conversation_handle
			            ,@RecvMsgTypeName = message_type_name
			            ,@RecvMsgContent = message_body
		            FROM Queue%d
	            )
	            ,TIMEOUT @RecvTimeoutMilisecs;

	            DECLARE @RowsetOut TABLE (MsgEncodedContent EncodedContent%d);
	            DECLARE @MsgTypeExpected VARCHAR(40);
	            SET @MsgTypeExpected = '%s/RequestMessage';

	            WHILE @RecvMsgDlgHandle IS NOT NULL
	            BEGIN
		            IF @RecvMsgTypeName = @MsgTypeExpected
		            BEGIN
			            INSERT INTO @RowsetOut VALUES (@RecvMsgContent);
			            END CONVERSATION @RecvMsgDlgHandle;
		            END;

		            SET @RecvMsgDlgHandle = NULL;

		            RECEIVE TOP(1)
			            @RecvMsgDlgHandle = conversation_handle
			            ,@RecvMsgTypeName = message_type_name
			            ,@RecvMsgContent = message_body
		            FROM Queue%d;
	            END;

	            SELECT MsgEncodedContent FROM @RowsetOut;
            END;
        )"
        , queueId
        , queueId
        , queueId
        , queueId
        , queueId
        , queueId
        , serviceURL
        , queueId
        , now;
    }
    catch (Poco::Data::DataException &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create broker reader queue. "
               "POCO C++ reported a data access error - " << ex.name() << ": " << ex.message();

        throw core::AppException<std::runtime_error>(oss.str());
    }
    catch (Poco::Exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Failed to create broker reader queue. "
               "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

        throw core::AppException<std::runtime_error>(oss.str());
    }
    catch (std::exception &ex)
    {
        CALL_STACK_TRACE;
        std::ostringstream oss;
        oss << "Generic failure prevented creation of broker reader queue: " << ex.what();
        throw core::AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Controls retrieval of results from asynchronous read of queue.
    /// </summary>
    /// <seealso cref="notcopiable" />
    class AsyncReadImpl : public IAsyncRead, notcopiable
    {
    private:

        Poco::AutoPtr<Poco::Data::Statement> m_stoProcExecStmt;
        Poco::AutoPtr<Poco::ActiveResult<size_t>> m_stoProcActRes;
        std::vector<string> m_messages;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="AsyncReadImpl"/> class.
        /// </summary>
        /// <param name="dbSession">The database session.</param>
        /// <param name="msgCountStepLimit">How many messages are to be retrieved
        /// at most at each asynchronous execution step.</param>
        /// <param name="queueId">The queue identifier.</param>
        AsyncReadImpl(Poco::Data::Session &dbSession,
                      uint16_t msgCountStepLimit,
                      uint8_t queueId)
        try
        {
            CALL_STACK_TRACE;

            using namespace Poco::Data::Keywords;

            m_stoProcExecStmt = new Poco::Data::Statement(
                (dbSession << "EXEC ReceiveMessagesInQueue%d 250;"
                    , queueId
                    , into(m_messages)
                    , limit(msgCountStepLimit))
            );

            m_messages.reserve(msgCountStepLimit);
            m_stoProcActRes = new Poco::ActiveResult<size_t>(m_stoProcExecStmt->executeAsync());
        }
        catch (Poco::Data::DataException &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Failed to read messages from broker queue. "
                   "POCO C++ reported a data access error - " << ex.name() << ": " << ex.message();

            throw core::AppException<std::runtime_error>(oss.str());
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
            oss << "Generic failure prevented reading broker queue: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }

        /// <summary>
        /// Evaluates whether the last asynchronous read step is finished.
        /// </summary>
        /// <returns><c>true</c> when finished, otherwise, <c>false</c>.</returns>
        virtual bool IsFinished() override
        {
            return m_stoProcActRes->available();
        }

        /// <summary>
        /// Waits for the last asynchronous read step to finish.
        /// </summary>
        virtual void Wait() override
        {
            CALL_STACK_TRACE;

            try
            {
                m_stoProcActRes->wait(5000);
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "Await for broker queue messages was aborted. "
                       "POCO C++ reported an error - " << ex.name() << ": " << ex.message();

                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Start the asynchronous execution of the next step.
        /// </summary>
        virtual void Step() override
        {
            CALL_STACK_TRACE;

            Wait();
            m_messages.clear();
            
            try
            {
                m_stoProcActRes = new Poco::ActiveResult<size_t>(m_stoProcExecStmt->executeAsync());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure prevented stepping into execution of broker queue read: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Gets the result from the last asynchronous step execution.
        /// </summary>
        /// <returns>A vector of read messages.</returns>
        virtual std::vector<string> GetStepResult() override
        {
            CALL_STACK_TRACE;

            Wait();

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
    /// at most at each asynchronous execution step.</param>
    /// <return>An object to control the result of the asynchronous operation.</return>
    std::unique_ptr<IAsyncRead> QueueReader::ReadMessages(uint16_t msgCountStepLimit)
    {
        CALL_STACK_TRACE;

        try
        {
            return std::unique_ptr<IAsyncRead>(
                new AsyncReadImpl(*m_dbSession, msgCountStepLimit, m_queueId)
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
