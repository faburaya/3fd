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
    /// <param name="msgTypeSpec">The specification for creation of message type.
    /// Such type is createad in the backend at the first time a reader or writer
    /// for this queue is instantiated. Subsequent instantiations will not effectively
    /// alter the message type by simply using different values in this parameter.</param>
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

        // Create message type, contract, queue, service and message content data type:
        m_dbSession << R"(
            if not exists ( select * from sys.service_queues where name = N'%s/v1_0_0/Queue' )
            begin
                create message type [%s/v1_0_0/Message] validation = %s;
                create contract [%s/v1_0_0/Contract] ([%s/v1_0_0/Message] sent by initiator);
                create queue [%s/v1_0_0/Queue] with poison_message_handling (status = off);
                create service [%s/v1_0_0] on queue [%s/v1_0_0/Queue] ([%s/v1_0_0/Contract]);
            end;

            if not exists (
	            select * from sys.systypes
		            where name = N'%s/v1_0_0/Message/ContentType'
            )
            begin
	            create type [%s/v1_0_0/Message/ContentType] from varchar(%u);
            end;
            )"
            , serviceURL
            , serviceURL, ToString(msgTypeSpec.contentValidation)
            , serviceURL, serviceURL
            , serviceURL
            , serviceURL, serviceURL, serviceURL

            // create type
            , serviceURL
            , serviceURL, msgTypeSpec.nBytes
            , now;

        // Create stored procedure to read messages from queue:

        Poco::Nullable<int> stoProcObjId;
        m_dbSession <<
            "select object_id(N'dbo.%s/v1_0_0/ReadMessagesProc', N'P');"
            , serviceURL
            , into(stoProcObjId)
            , now;

        if (stoProcObjId.isNull())
        {
            m_dbSession << R"(
                create procedure [%s/v1_0_0/ReadMessagesProc] (
                    @recvMsgCountLimit int
	                ,@recvTimeoutMilisecs int
                ) as
                begin try
                    begin transaction;

                        set nocount on;

                        declare @ReceivedMessages table (
                            queuing_order        bigint
                            ,conversation_handle uniqueidentifier
                            ,message_type_name   sysname
                            ,message_body        [%s/v1_0_0/Message/ContentType]
                        );

	                    waitfor(
		                    receive top (@recvMsgCountLimit)
                                    queuing_order
                                    ,conversation_handle
			                        ,message_type_name
			                        ,message_body
		                        from [%s/v1_0_0/Queue]
                                into @ReceivedMessages
	                    )
	                    ,timeout @recvTimeoutMilisecs;
        
	                    declare @RowsetOut        table (content [%s/v1_0_0/Message/ContentType]);
                        declare @prevDialogHandle uniqueidentifier;
                        declare @dialogHandle     uniqueidentifier;
	                    declare @msgTypeName      sysname;
	                    declare @msgContent       [%s/v1_0_0/Message/ContentType];

                        declare cursorMsg
                            cursor forward_only read_only
                            for select conversation_handle
                                       ,message_type_name
                                       ,message_body
                                from @ReceivedMessages
                                order by queuing_order;

                        open cursorMsg;
                        fetch next from cursorMsg into @dialogHandle, @msgTypeName, @msgContent;
	    
	                    while @@fetch_status = 0
	                    begin
                            if @dialogHandle <> @prevDialogHandle and @prevDialogHandle is not null
                                end conversation @prevDialogHandle;

                            if @msgTypeName = '%s/v1_0_0/Message'
		                        insert into @RowsetOut values (@msgContent);

                            else if @msgTypeName = 'http://schemas.microsoft.com/SQL/ServiceBroker/Error'
		                        throw 50001, 'There was an error during conversation with service', 1;

                            else if @msgTypeName <> 'http://schemas.microsoft.com/SQL/ServiceBroker/EndDialog'
		                        throw 50000, 'Message received in service broker queue had unexpected type', 1;
            
                            set @prevDialogHandle = @dialogHandle;
		                    fetch next from cursorMsg into @dialogHandle, @msgTypeName, @msgContent;
	                    end;

                        close cursorMsg;
                        deallocate cursorMsg;

                        save transaction doneReceiving;

                        set @dialogHandle = newid();

		                receive top (1)
                            @dialogHandle = conversation_handle
		                    from [%s/v1_0_0/Queue];

                        rollback transaction doneReceiving;

                        if @dialogHandle <> @prevDialogHandle and @prevDialogHandle is not null
                            end conversation @prevDialogHandle;
            
	                    select content from @RowsetOut;

                    commit transaction;
                end try
                begin catch

                    rollback transaction;
                    throw;

                end catch;
                )"
                , serviceURL
                , serviceURL
                , serviceURL
                , serviceURL
                , serviceURL
                , serviceURL
                , serviceURL
                , now;
        }

        m_dbSession.setFeature("autoCommit", false);

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
    /// This implementation is NOT THREAD SAFE!
    /// </summary>
    /// <seealso cref="notcopiable" />
    class AsyncReadImpl : public IAsyncRead, notcopiable
    {
    private:

        Poco::Data::Session &m_dbSession;
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
        try :
            m_dbSession(dbSession)
        {
            CALL_STACK_TRACE;

            using namespace Poco::Data::Keywords;

            if (!dbSession.isConnected())
                dbSession.reconnect();

            m_stoProcExecStmt.reset(new Poco::Data::Statement(dbSession));

            char queryStrBuf[128];
            sprintf(queryStrBuf,
                    "exec [%s/v1_0_0/ReadMessagesProc] %d, %d;",
                    serviceURL.c_str(),
                    (int)msgCountStepLimit,
                    (int)msgRecvTimeout);
            
            *m_stoProcExecStmt << queryStrBuf, into(m_messages);

            dbSession.begin();

            m_messages.reserve(msgCountStepLimit);
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
        virtual AsyncReadImpl::~AsyncReadImpl()
        {
            CALL_STACK_TRACE;

            try
            {
                if (m_stoProcActRes && !TryWait(5000))
                {
                    core::Logger::Write("Await for end of asynchronous read from broker queue has timed "
                                        "out (5 secs) before releasing resources of running statement",
                                        core::Logger::PRIO_CRITICAL, true);
                }

                if (m_dbSession.isTransaction())
                    m_dbSession.rollback();
            }
            catch (core::IAppException &ex)
            {
                core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
            }
            catch (Poco::Data::DataException &ex)
            {
                std::ostringstream oss;
                oss << "Failed to end transaction reading messages from broker queue. "
                       "POCO C++ reported a data access error: " << ex.name();

                core::Logger::Write(oss.str(), ex.message(), core::Logger::PRIO_CRITICAL, true);
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "Failed to end transaction reading messages from broker queue. "
                       "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

                core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL, true);
            }
        }

        /// <summary>
        /// Evaluates whether the last asynchronous read step is finished.
        /// </summary>
        /// <returns>
        ///   <c>true</c> when finished, otherwise, <c>false</c>.
        /// </returns>
        virtual bool HasJoined() const override
        {
            CALL_STACK_TRACE;

            try
            {
                _ASSERTE(m_stoProcActRes); // cannot evaluate completion of task before starting it by stepping into execution
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
                _ASSERTE(m_stoProcActRes); // cannot evaluate completion of task before starting it by stepping into execution
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
                if (m_stoProcActRes)
                {
                    if (!m_stoProcActRes->available())
                        throw core::AppException<std::logic_error>("Could not step into "
                            "execution because the last asynchronous operation is pending!");

                    m_messages.clear();
                }

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
        /// <returns>How many messages were read from the queue after
        /// waiting for completion of asynchronous read operation.
        /// Subsequent calls or calls that time out will return zero.</returns>
        virtual uint32_t GetStepMessageCount(uint16_t timeout) override
        {
            CALL_STACK_TRACE;

            _ASSERTE(m_stoProcActRes); // cannot evaluate completion of task before starting it by stepping into execution

            if (TryWait(timeout))
                return m_stoProcExecStmt->subTotalRowCount();

            return 0;
        }

        /// <summary>
        /// Gets the result from the last asynchronous step execution.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>A vector of read messages after waiting for completion of
        /// asynchronous read operation. Subsequent calls or calls that time out
        /// will return an empty vector. The retrieved messages are not guaranteed
        /// to appear in the same order they had when inserted.</returns>
        virtual std::vector<string> GetStepResult(uint16_t timeout) override
        {
            CALL_STACK_TRACE;

            _ASSERTE(m_stoProcActRes); // cannot evaluate completion of task before starting it by stepping into execution

            if (!TryWait(timeout))
                return std::vector<string>();

            std::vector<string> result;

            if (!m_messages.empty())
            {
                auto msgVecSize = m_messages.size();
                auto msgVecCap = m_messages.capacity();

                m_messages.swap(result);

                // Last step retrieved a full rowset?
                if (msgVecCap == msgVecSize)
                    m_messages.reserve(msgVecCap);
            }

            return std::move(result);
        }

        /// <summary>
        /// Rolls back the changes accumulated in the current transaction.
        /// The messages extracted in all steps from the broker queue are
        /// put back in place.
        /// </summary>
        /// <returns>
        ///   <c>true</c> if current transaction was successfully rolled back, otherwise, <c>false</c>.
        /// </returns>
        virtual bool Rollback(uint16_t timeout) override
        {
            CALL_STACK_TRACE;

            try
            {
                _ASSERTE(m_dbSession.isTransaction()); // must be in a transaction

                if (!TryWait(timeout))
                    return false;

                m_dbSession.rollback();
                return true;
            }
            catch (Poco::Data::DataException &ex)
            {
                std::ostringstream oss;
                oss << "Failed to rollback transaction reading messages from broker queue. "
                       "POCO C++ reported a data access error: " << ex.name();

                throw core::AppException<std::runtime_error>(oss.str(), ex.message());
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "Failed to rollback transaction reading messages from broker queue. "
                       "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Commits in persistent storage all the changes accumulated in the
        /// current transaction. The messages extracted in all steps so far
        /// from the broker queue are permanently removed.
        /// </summary>
        /// <returns>
        ///   <c>true</c> if current transaction was successfully committed, otherwise, <c>false</c>.
        /// </returns>
        virtual bool Commit(uint16_t timeout) override
        {
            CALL_STACK_TRACE;

            try
            {
                _ASSERTE(m_dbSession.isTransaction()); // must be in a transaction

                if (!TryWait(timeout))
                    return false;

                m_dbSession.commit();
                return true;
            }
            catch (Poco::Data::DataException &ex)
            {
                std::ostringstream oss;
                oss << "Failed to commit transaction reading messages from broker queue. "
                       "POCO C++ reported a data access error: " << ex.name();

                throw core::AppException<std::runtime_error>(oss.str(), ex.message());
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "Failed to commit transaction reading messages from broker queue. "
                       "POCO C++ reported a generic error - " << ex.name() << ": " << ex.message();

                throw core::AppException<std::runtime_error>(oss.str());
            }
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
