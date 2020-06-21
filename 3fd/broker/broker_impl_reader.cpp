//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include "broker_impl.h"
#include <3fd/core/callstacktracer.h>
#include <3fd/core/exceptions.h>
#include <3fd/core/logger.h>
#include <3fd/utils/serialization.h>

#include <chrono>
#include <sstream>

namespace _3fd
{
namespace broker
{
    /// <summary>
    /// Initializes a new instance of the <see cref="QueueReader"/> class.
    /// </summary>
    /// <param name="svcBrokerBackend">The broker backend to use.</param>
    /// <param name="dbConnString">The backend connection string.</param>
    /// <param name="utils::to_unicode(serviceURL)">The service URL.</param>
    /// <param name="msgTypeSpec">The specification for creation of message type.
    /// Such type is createad in the backend at the first time a reader or writer
    /// for this queue is instantiated. Subsequent instantiations will not effectively
    /// alter the message type by simply using different values in this parameter.</param>
    QueueReader::QueueReader(Backend svcBrokerBackend,
                             const std::string &dbConnString,
                             const std::string &serviceURL,
                             const MessageTypeSpec &msgTypeSpec)
        : m_dbConnString(dbConnString)
        , m_serviceURL(serviceURL)
    {
        CALL_STACK_TRACE;

        _ASSERTE(svcBrokerBackend == Backend::MsSqlServer);

        try
        {
            DatabaseSession dbSession(dbConnString);

            // Create message type, contract, queue, service and message content data type:
            nanodbc::just_execute(dbSession.GetConnection(),
                Text::in(_U('%'), _U(R"(
                    if not exists ( select * from sys.service_queues where name = N'%service/v1_0_0/Queue' )
                    begin
                        create message type [%service/v1_0_0/Message] validation = %validation;
                        create contract [%service/v1_0_0/Contract] ([%service/v1_0_0/Message] sent by initiator);
                        create queue [%service/v1_0_0/Queue] with poison_message_handling (status = off);
                        create service [%service/v1_0_0] on queue [%service/v1_0_0/Queue] ([%service/v1_0_0/Contract]);
                    end;

                    if not exists (
                        select * from sys.systypes
                            where name = N'%service/v1_0_0/Message/ContentType'
                    )
                    begin
                        create type [%service/v1_0_0/Message/ContentType] from varchar(%nbytes);
                    end;
                )"))
                .Replace(_U("service"), utils::to_unicode(serviceURL))
                .Replace(_U("validation"), ToString(msgTypeSpec.contentValidation))
                .Use(_U("nbytes"), msgTypeSpec.nBytes)
                .Emit()
            );

            // Create stored procedure to read messages from queue:

            auto result = nanodbc::execute(dbSession.GetConnection(),
                Text::in(_U('%'), _U("select object_id(N'%service/v1_0_0/ReadMessagesProc', N'P');"))
                    .Replace(_U("service"), utils::to_unicode(serviceURL))
                    .Emit()
            );

            if (!result.next())
            {
                throw core::AppException<std::runtime_error>(
                    "Could not check presence of stored procedure to read from broker queue!", serviceURL);
            }
        
            if (result.is_null(0))
            {
                nanodbc::just_execute(dbSession.GetConnection(), Text::in(_U('%'), _U(R"(
                    create procedure [%service/v1_0_0/ReadMessagesProc] (
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
                                ,message_body        [%service/v1_0_0/Message/ContentType]
                            );

                            waitfor(
                                receive top (@recvMsgCountLimit)
                                        queuing_order
                                        ,conversation_handle
                                        ,message_type_name
                                        ,message_body
                                    from [%service/v1_0_0/Queue]
                                    into @ReceivedMessages
                            )
                            ,timeout @recvTimeoutMilisecs;
        
                            declare @RowsetOut        table (content [%service/v1_0_0/Message/ContentType]);
                            declare @prevDialogHandle uniqueidentifier;
                            declare @dialogHandle     uniqueidentifier;
                            declare @msgTypeName      sysname;
                            declare @msgContent       [%service/v1_0_0/Message/ContentType];

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

                                if @msgTypeName = '%service/v1_0_0/Message'
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
                                from [%service/v1_0_0/Queue];

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
                )"))
                .Replace(_U("service"), utils::to_unicode(serviceURL))
                .Emit());
            }

            std::ostringstream oss;
            oss << "Initialized successfully the reader for broker queue '"
                << serviceURL << "/v1_0_0/Queue' backed by "
                << utils::to_utf8(ToString(svcBrokerBackend)) << " via ODBC";

            core::Logger::Write(oss.str(), core::Logger::PRIO_INFORMATION);
        }
        catch_and_handle_exception("creating reader for broker queue")
    }

    /// <summary>
    /// Controls retrieval of results from asynchronous read of queue.
    /// </summary>
    class AsyncReadImpl : public IAsyncDatabaseOperation
    {
    private:

        DatabaseSession m_dbSession;
        std::string m_brokerSvcUrl;
        nanodbc::statement m_stoProcExecStmt;
        CallbackReceiveMessages callbackReceiveMessages;

    public:

        AsyncReadImpl(const AsyncReadImpl &) = delete;

        virtual ~AsyncReadImpl() = default;

        /// <summary>
        /// Initializes a new instance of the <see cref="AsyncReadImpl" /> class.
        /// </summary>
        /// <param name="dbConnString">The ODBC connection string.</param>
        /// <param name="msgCountStepLimit">How many messages are to be
        /// retrieved at most at each asynchronous execution step.</param>
        /// <param name="msgRecvTimeout">The timeout (in ms) when the backend awaits for messages.</param>
        /// <param name="serviceUrl">The service URL.</param>
        /// <param name="callbackRecvMessages">Callback for receiving messages.</param>
        AsyncReadImpl(const std::string &dbConnString,
                      const std::string &serviceUrl,
                      uint16_t msgCountStepLimit,
                      uint16_t msgRecvTimeout,
                      const CallbackReceiveMessages &callbackRecvMessages)
            : m_dbSession(dbConnString)
            , m_brokerSvcUrl(serviceUrl)
            , callbackReceiveMessages(callbackRecvMessages)
        {
            CALL_STACK_TRACE;

            try
            {
                std::array<char_t, 256> buffer;
                size_t length = utils::SerializeTo(buffer,
                    _U("exec ["), serviceUrl, _U("/v1_0_0/ReadMessagesProc] "),
                    msgCountStepLimit, _U(", "), msgRecvTimeout, _U(";"));

                m_stoProcExecStmt = nanodbc::statement(
                    m_dbSession.GetConnection(),
                    nanodbc::string(buffer.data(), length)
                );

                // make itself an asynchronous callback:
                std::future<void>::operator=(
                    std::async(std::launch::async, &AsyncReadImpl::ExtractMessages, this)
                );
            }
            catch_and_handle_exception("setting up to read messages asynchronously from broker queue")
        }

        /// <summary>
        /// Extract messages from the service broker queue.
        /// </summary>
        void ExtractMessages()
        {
            CALL_STACK_TRACE;

            try
            {
                /* access for reading messages must be exclusive per queue, otherwise
                 * the concurrent transactions will read the same message simultaneously */
                auto scopeLock = LockProvider::GetInstance().GetLockFor(m_brokerSvcUrl);

                nanodbc::transaction transaction(m_dbSession.GetConnection());

                auto result = nanodbc::execute(m_stoProcExecStmt);

                std::vector<std::string> messages;
                messages.reserve(result.rows());

                while (result.next())
                    messages.push_back(result.get<std::string>(0));

                // the callback receives the messages to save them
                callbackReceiveMessages(std::move(messages));

                transaction.commit(); // now the messages are gone from the database
            }
            catch_and_handle_exception("extracting messages from broker queue")
        }

        const char *description() const noexcept override
        {
            return "reading from broker queue";
        }

    };// end of class AsyncReadImpl

    /// <summary>
    /// Asynchronously reads the messages from the queue into a vector.
    /// </summary>
    /// <param name="msgCountStepLimit">How many messages are to be retrieved
    /// at most for each asynchronous execution step.</param>
    /// <param name="msgRecvTimeout">The timeout (in ms) when the backend awaits for messages.</param>
    /// <param name="callbackRecvMessages">
    /// Callback that receives the messages before they are deleted from service broker queue.
    /// The retrieved messages are not guaranteed to appear in the same order they had when inserted.
    /// </param>
    /// <return>An object to control the result of the asynchronous operation.</return>
    std::unique_ptr<IAsyncDatabaseOperation>
    QueueReader::ReadMessages(uint16_t msgCountStepLimit,
                              uint16_t msgRecvTimeout,
                              const CallbackReceiveMessages &callbackRecvMessages)
    {
        CALL_STACK_TRACE;

        try
        {
            return std::unique_ptr<IAsyncDatabaseOperation>(
                dbg_new AsyncReadImpl(m_dbConnString,
                                      m_serviceURL,
                                      msgCountStepLimit,
                                      msgRecvTimeout,
                                      callbackRecvMessages)
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
