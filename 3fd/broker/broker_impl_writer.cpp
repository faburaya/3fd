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
#include <3fd/utils/text.h>

#include <sstream>

namespace _3fd
{
namespace broker
{

    /// <summary>
    /// Initializes a new instance of the <see cref="QueueWriter"/> class.
    /// </summary>
    /// <param name="svcBrokerBackend">The broker backend to use.</param>
    /// <param name="dbConnString">The backend connection std::string.</param>
    /// <param name="serviceURL">The URL of the service that reads the messages.</param>
    /// <param name="msgTypeSpec">The specification for creation of message type.
    /// Such type is createad in the backend at the first time a reader or writer
    /// for this queue is instantiated. Subsequent instantiations will not effectively
    /// alter the message type by simply using different values in this parameter.</param>
    QueueWriter::QueueWriter(Backend svcBrokerBackend,
                             const std::string &dbConnString,
                             const std::string &serviceURL,
                             const MessageTypeSpec &msgTypeSpec)
        : m_dbConnString(dbConnString)
        , m_serviceURL(serviceURL)
    {
        CALL_STACK_TRACE;

        try
        {
            _ASSERTE(svcBrokerBackend == Backend::MsSqlServer);

            DatabaseSession dbSession(dbConnString);

            // Create message, contract, queue, service and types:
            nanodbc::just_execute(dbSession.GetConnection(),
                Text::in(_U('%'), _U(R"(
                    if not exists ( select * from sys.service_queues where name = N'%service/v1_0_0/Queue' )
                    begin
                        create message type [%service/v1_0_0/Message] validation = %validation;
                        create contract [%service/v1_0_0/Contract] ([%service/v1_0_0/Message] sent by initiator);
                        create queue [%service/v1_0_0/Queue] with poison_message_handling (status = off);
                        create service [%service/v1_0_0] on queue [%service/v1_0_0/Queue] ([%service/v1_0_0/Contract]);
                    end;

                    if not exists ( select * from sys.service_queues where name = N'%service/v1_0_0/ResponseQueue' )
                    begin
                        create queue [%service/v1_0_0/ResponseQueue];
                        create service [%service/v1_0_0/Sender] on queue [%service/v1_0_0/ResponseQueue];
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

            // Create input stage table:
            nanodbc::just_execute(dbSession.GetConnection(),
                Text::in(_U('%'), _U(R"(
                    if not exists (
                        select * from sys.tables
                            where name = N'%service/v1_0_0/InputStageTable'
                    )
                    begin
                        create table [%service/v1_0_0/InputStageTable] (content [%service/v1_0_0/Message/ContentType]);
                    end;
                )"))
                .Replace(_U("service"), utils::to_unicode(serviceURL))
                .Emit()
            );

            // Create stored procedure to send messages to service queue:
            auto result = nanodbc::execute(dbSession.GetConnection(),
                Text::in(_U('%'), _U("select object_id(N'%service/v1_0_0/SendMessagesProc', N'P');"))
                    .Replace(_U("service"), utils::to_unicode(serviceURL))
                    .Emit()
            );

            if (!result.next())
            {
                throw core::AppException<std::runtime_error>(
                    "Could not check presence of stored procedure to write into broker queue!", serviceURL);
            }

            if (result.is_null(0))
            {
                nanodbc::just_execute(dbSession.GetConnection(), Text::in(_U('%'), _U(R"(
                    create procedure [%service/v1_0_0/SendMessagesProc] as
                    begin try
                        begin transaction;

                            set nocount on;

                            declare @dialogHandle uniqueidentifier;

                            begin dialog @dialogHandle
                                from service [%service/v1_0_0/Sender]
                                to service '%service/v1_0_0'
                                on contract [%service/v1_0_0/Contract]
                                with encryption = off;

                            declare @msgContent [%service/v1_0_0/Message/ContentType];

                            declare cursorMsg cursor for (
                                select * from [%service/v1_0_0/InputStageTable]
                            );

                            open cursorMsg;
                            fetch next from cursorMsg into @msgContent;

                            while @@fetch_status = 0
                            begin
                                send on conversation @dialogHandle
                                    message type [%service/v1_0_0/Message] (@msgContent);

                                fetch next from cursorMsg into @msgContent;
                            end;

                            close cursorMsg;
                            deallocate cursorMsg;
                            delete from [%service/v1_0_0/InputStageTable];

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

            // Create stored procedure to finish conversations in the initiator endpoint:
            result = nanodbc::execute(dbSession.GetConnection(),
                Text::in(_U('%'), _U("select object_id(N'%service/v1_0_0/FinishDialogsOnEndptInitProc', N'P');"))
                    .Replace(_U("service"), utils::to_unicode(serviceURL))
                    .Emit()
            );

            if (!result.next())
            {
                throw core::AppException<std::runtime_error>(
                    "Could not check presence of stored procedure for finishing enpoint!", serviceURL);
            }

            if (result.is_null(0))
            {
                nanodbc::just_execute(dbSession.GetConnection(), Text::in(_U('%'), _U(R"(
                    create procedure [%service/v1_0_0/FinishDialogsOnEndptInitProc] as
                    begin try
                        begin transaction;

                            set nocount on;

                            declare @ReceivedMessages table (
                                conversation_handle  uniqueidentifier
                                ,message_type_name   sysname
                            );

                            receive conversation_handle
                                    ,message_type_name
                                from [%service/v1_0_0/ResponseQueue]
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

                    alter queue [%service/v1_0_0/ResponseQueue]
                        with activation (
                            status = on,
                            max_queue_readers = 1,
                            procedure_name = [%service/v1_0_0/FinishDialogsOnEndptInitProc],
                            execute as owner
                        );
                )"))
                .Replace(_U("service"), utils::to_unicode(serviceURL))
                .Emit());
            }

            std::ostringstream oss;
            oss << "Initialized successfully the writer for broker queue '"
                << serviceURL << "/v1_0_0/Queue' backed by "
                << utils::to_utf8(ToString(svcBrokerBackend)) << " via ODBC";

            core::Logger::Write(oss.str(), core::Logger::PRIO_INFORMATION);
        }
        catch_and_handle_exception("creating writer for broker queue")
    }

    /// <summary>
    /// Helps synchronizing with an asynchronous write to a broker queue.
    /// </summary>
    class AsyncWriteImpl : public IAsyncDatabaseOperation
    {
    private:

        DatabaseSession m_dbSession;

        nanodbc::statement m_stageInsertStatement;
        nanodbc::statement m_stoProcStatement;

    public:

        AsyncWriteImpl(const AsyncWriteImpl &) = delete;

        virtual ~AsyncWriteImpl() = default;

        /// <summary>
        /// Initializes a new instance of the <see cref="AsyncWriteImpl" /> class.
        /// </summary>
        /// <param name="dbConnString">The ODBC connection string.</param>
        /// <param name="serviceURL">The service URL.</param>
        /// <param name="messages">The messages to write.</param>
        AsyncWriteImpl(const std::string &dbConnString,
                       const std::string &serviceURL,
                       const std::vector<std::string> &messages)
            : m_dbSession(dbConnString)
        {
            CALL_STACK_TRACE;

            try
            {
                const auto batchSize = static_cast<long>(messages.size());

                if (batchSize == 0)
                {
                    // make this operation a synchronous no-op:
                    std::packaged_task<void(long)> task([](long){});
                    std::future<void>::operator=(task.get_future());
                    task(0);
                    return;
                }

                std::array<char_t, 256> buffer;
                size_t length = utils::SerializeTo(buffer,
                    _U("insert into ["), serviceURL, _U("/v1_0_0/InputStageTable] (content) values (?);"));

                m_stageInsertStatement = nanodbc::statement(m_dbSession.GetConnection(),
                                                            nanodbc::string(buffer.data(), length));

                m_stageInsertStatement.bind_strings(0, messages);

                length = utils::SerializeTo(buffer,
                    _U("exec ["), serviceURL, _U("/v1_0_0/SendMessagesProc];"));

                m_stoProcStatement = nanodbc::statement(m_dbSession.GetConnection(),
                                                        nanodbc::string(buffer.data(), length));

                // make this operation an asynchronous one:
                std::future<void>::operator=(
                    std::async(std::launch::async, &AsyncWriteImpl::PutMessages, this, batchSize)
                );
            }
            catch_and_handle_exception("setting up to write messages into broker queue")
        }

        /// <summary>
        /// Implements putting messages into the broker service queue.
        /// </summary>
        /// <param name="batchSize">The number of messages. (Required by nanodbc.)</param>
        void PutMessages(long batchSize)
        {
            CALL_STACK_TRACE;

            try
            {
                nanodbc::just_execute(m_stageInsertStatement, batchSize);
                nanodbc::just_execute(m_stoProcStatement);
            }
            catch_and_handle_exception("writing messages into broker queue")
        }

        const char *description() const noexcept override
        {
            return "writing into broker queue";
        }

    };// end of AsyncWriteImpl class

    /// <summary>
    /// Asychonously writes the messages into the queue.
    /// </summary>
    /// <param name="messages">The messages to write.</param>
    /// <return>An object to help synchronizing with the asynchronous operation.</return>
    std::unique_ptr<IAsyncDatabaseOperation> QueueWriter::WriteMessages(const std::vector<std::string> &messages)
    {
        CALL_STACK_TRACE;

        try
        {
            return std::unique_ptr<IAsyncDatabaseOperation>(
                dbg_new AsyncWriteImpl(m_dbConnString, m_serviceURL, messages)
            );
        }
        catch (core::IAppException &)
        {
            throw; // just forward exceptions raised for errors known to have been already handled
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
