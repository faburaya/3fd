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
            if not exists ( select * from sys.service_queues where name = N'%s/v1_0_0/Queue' )
            begin
                create message type [%s/v1_0_0/Message] validation = %s;
                create contract [%s/v1_0_0/Contract] ([%s/v1_0_0/Message] sent by initiator);
                create queue [%s/v1_0_0/Queue] with poison_message_handling (status = off);
                create service [%s/v1_0_0] on queue [%s/v1_0_0/Queue] ([%s/v1_0_0/Contract]);
            end;

            if not exists ( select * from sys.service_queues where name = N'%s/v1_0_0/ResponseQueue' )
            begin
                create queue [%s/v1_0_0/ResponseQueue];
                create service [%s/v1_0_0/Sender] on queue [%s/v1_0_0/ResponseQueue];
            end;

            if not exists (
	            select * from sys.systypes
		            where name = N'%s/v1_0_0/Message/ContentType'
            )
            begin
	            create type [%s/v1_0_0/Message/ContentType] from varchar(%u);
            end;

            if not exists (
	            select * from sys.tables
		            where name = N'%s/v1_0_0/InputStageTable'
            )
            begin
                create table [%s/v1_0_0/InputStageTable] (content [%s/v1_0_0/Message/ContentType]);
            end;
            )"
            , serviceURL
            , serviceURL, ToString(msgTypeSpec.contentValidation)
            , serviceURL, serviceURL
            , serviceURL
            , serviceURL, serviceURL, serviceURL

            // create queue ResponseQueue
            , serviceURL
            , serviceURL
            , serviceURL, serviceURL

            // create type Message/ContentType
            , serviceURL
            , serviceURL, msgTypeSpec.nBytes

            // create table InputStageTable
            , serviceURL
            , serviceURL, serviceURL
            , now;

        // Create stored procedure to send messages to service queue:

        Poco::Nullable<int> stoProcObjId;
        m_dbSession <<
            "select object_id(N'%s/v1_0_0/SendMessagesProc', N'P');"
            , serviceURL
            , into(stoProcObjId)
            , now;

        if (stoProcObjId.isNull())
        {
            m_dbSession << R"(
                create procedure [%s/v1_0_0/SendMessagesProc] as
                begin try
                    begin transaction;

                        set nocount on;

                        declare @dialogHandle uniqueidentifier;

                        begin dialog @dialogHandle
                            from service [%s/v1_0_0/Sender]
                            to service '%s/v1_0_0'
                            on contract [%s/v1_0_0/Contract]
                            with encryption = off;

                        declare @msgContent [%s/v1_0_0/Message/ContentType];

                        declare cursorMsg cursor for (
	                        select * from [%s/v1_0_0/InputStageTable]
                        );

                        open cursorMsg;
                        fetch next from cursorMsg into @msgContent;

                        while @@fetch_status = 0
                        begin
	                        send on conversation @dialogHandle
		                        message type [%s/v1_0_0/Message] (@msgContent);

	                        fetch next from cursorMsg into @msgContent;
                        end;

                        close cursorMsg;
                        deallocate cursorMsg;
                        delete from [%s/v1_0_0/InputStageTable];

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
                , serviceURL
                , now;
        }

        // Create stored procedure to finish conversations in the initiator endpoint:

        m_dbSession <<
            "select object_id(N'%s/v1_0_0/FinishDialogsOnEndptInitProc', N'P');"
            , serviceURL
            , into(stoProcObjId)
            , now;

        if (stoProcObjId.isNull())
        {
            m_dbSession << R"(
                create procedure [%s/v1_0_0/FinishDialogsOnEndptInitProc] as
                begin try
                    begin transaction;

                        set nocount on;

                        declare @ReceivedMessages table (
                            conversation_handle  uniqueidentifier
                            ,message_type_name   sysname
                        );

	                    receive conversation_handle
                                ,message_type_name
		                    from [%s/v1_0_0/ResponseQueue]
                            into @ReceivedMessages;
        
                        declare @dialogHandle  uniqueidentifier;
	                    declare @msgTypeName   sysname;
        
                        declare cursorMsg
                            cursor forward_only read_only for
                                select conversation_handle
                                       ,message_type_name
                                    from @ReceivedMessages;

                        open cursorMsg;
                        fetch next from cursorMsg into @dialogHandle, @msgTypeName;

	                    while @@fetch_status = 0
	                    begin
                            if @msgTypeName = 'http://schemas.microsoft.com/SQL/ServiceBroker/EndDialog'
                                end conversation @dialogHandle;

		                    fetch next from cursorMsg into @dialogHandle, @msgTypeName;
	                    end;

                        close cursorMsg;
                        deallocate cursorMsg;

                    commit transaction;
                end try
                begin catch

                    rollback transaction;
                    throw;

                end catch;

                alter queue [%s/v1_0_0/ResponseQueue]
                    with activation (
                        status = on,
                        max_queue_readers = 1,
                        procedure_name = [%s/v1_0_0/FinishDialogsOnEndptInitProc],
                        execute as owner
                    );
                )"
                , serviceURL
                , serviceURL

                // ALTER QUEUE ResponseQueue
                , serviceURL
                , serviceURL
                , now;
        }

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
               "POCO C++ reported a generic error - " << ex.name();

        if (!ex.message().empty())
            oss << ": " << ex.message();

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

        Poco::Data::Session &m_dbSession;
        std::unique_ptr<std::future<void>> m_future;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="AsyncWriteImpl" /> class.
        /// </summary>
        /// <param name="dbSession">The database session.</param>
        AsyncWriteImpl(Poco::Data::Session &dbSession)
        try :
            std::promise<void>(),
            m_dbSession(dbSession)
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
        virtual ~AsyncWriteImpl()
        {
            CALL_STACK_TRACE;

            try
            {
                if (!TryWait(5000))
                {
                    core::Logger::Write("Await for end of asynchronous write into broker queue has timed "
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
                oss << "Failed to end transaction writing messages into broker queue. "
                       "POCO C++ reported a data access error: " << ex.name();

                core::Logger::Write(oss.str(), ex.message(), core::Logger::PRIO_CRITICAL, true);
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "Failed to end transaction writing messages into broker queue. "
                       "POCO C++ reported a generic error - " << ex.name();

                if (!ex.message().empty())
                    oss << ": " << ex.message();

                core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL, true);
            }
        }

        /// <summary>
        /// Evaluates whether the last asynchronous write operation is finished.
        /// </summary>
        /// <returns>
        ///   <c>true</c> when finished, otherwise, <c>false</c>.
        /// </returns>
        virtual bool IsFinished() const override
        {
            return !m_future->valid() ||
                m_future->wait_for(std::chrono::seconds(0)) == std::future_status::ready;
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
            return !m_future->valid() ||
                m_future->wait_for(std::chrono::milliseconds(timeout)) == std::future_status::ready;
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
                       "POCO C++ reported a generic error - " << ex.name();

                if (!ex.message().empty())
                    oss << ": " << ex.message();

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
        /// Rolls back the changes accumulated in the current transaction,
        /// erasing all messages written into the broker queue by the call
        /// that originated this object and began such transaction.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
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
                oss << "Failed to rollback transaction writing messages into broker queue. "
                       "POCO C++ reported a data access error: " << ex.name();

                throw core::AppException<std::runtime_error>(oss.str(), ex.message());
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "Failed to rollback transaction writing messages into broker queue. "
                       "POCO C++ reported a generic error - " << ex.name();

                if (!ex.message().empty())
                    oss << ": " << ex.message();

                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Commits in persistent storage all the changes accumulated in the current
        /// transaction, which began in the call that originated this object.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
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
                oss << "Failed to commit transaction writing messages into broker queue. "
                       "POCO C++ reported a data access error: " << ex.name();

                throw core::AppException<std::runtime_error>(oss.str(), ex.message());
            }
            catch (Poco::Exception &ex)
            {
                std::ostringstream oss;
                oss << "Failed to commit transaction writing messages into broker queue. "
                       "POCO C++ reported a generic error - " << ex.name();

                if (!ex.message().empty())
                    oss << ": " << ex.message();

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

            dbSession << "insert into [%s/v1_0_0/InputStageTable] (content) values (?);", serviceURL, useRef(messages), now;

            dbSession << "exec [%s/v1_0_0/SendMessagesProc];", serviceURL, now;

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

            std::unique_ptr<IAsyncWrite> result(new AsyncWriteImpl(m_dbSession));
            
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
                   "POCO C++ reported a generic error - " << ex.name();

            if (!ex.message().empty())
                oss << ": " << ex.message();

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
