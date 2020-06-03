//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#ifndef BROKER_H // header guard
#define BROKER_H

#include <3fd/core/preprocessing.h>
#include <cinttypes>
#include <functional>
#include <future>
#include <string>
#include <vector>

namespace _3fd {
namespace broker {

    /// <summary>
    /// Enumerates options for service broker backend.
    /// </summary>
    enum class Backend : short
    {
        MsSqlServer
        //, OracleDatabase
    };

    /// <summary>
    /// Enumerates options for backend validation of message content.
    /// </summary>
    enum class MessageContentValidation : short
    {
        None,
        WellFormedXml
    };

    /// <summary>
    /// Packs the necessary info to specify a message type for creation or use.
    /// </summary>
    struct MessageTypeSpec
    {
        uint32_t nBytes;
        MessageContentValidation contentValidation;
    };

    /// <summary>
    /// Behaves just like std::future.
    /// </summary>
    class INTFOPT IAsyncDatabaseOperation : public std::future<void>
    {
    public:

        IAsyncDatabaseOperation() = default;

        IAsyncDatabaseOperation(const IAsyncDatabaseOperation &) = delete;

        virtual ~IAsyncDatabaseOperation() = default;

        virtual const char *description() const noexcept = 0;
    };

    typedef std::function<void(std::vector<std::string> &&)> CallbackReceiveMessages;

    /// <summary>
    /// Represents a queue in the broker, from which
    /// a service can read its incoming messages.
    /// </summary>
    /// <seealso cref="notcopiable" />
    class QueueReader
    {
    private:

        const std::string m_dbConnString;
        const std::string m_serviceURL;

    public:

        QueueReader(Backend svcBrokerBackend,
                    const std::string &connString,
                    const std::string &serviceURL,
                    const MessageTypeSpec &msgTypeSpec);

        QueueReader(const QueueReader &) = delete;

        std::unique_ptr<IAsyncDatabaseOperation> ReadMessages(uint16_t msgCountStepLimit,
                                                              uint16_t msgRecvTimeout,
                                                              const CallbackReceiveMessages &callbackRecvMessages);
    };

    /// <summary>
    /// Represents a queue in the broker, into which
    /// a service can write messages to another.
    /// </summary>
    /// <seealso cref="notcopiable" />
    class QueueWriter
    {
    private:

        const std::string m_dbConnString;
        const std::string m_serviceURL;

    public:

        QueueWriter(Backend svcBrokerBackend,
                    const std::string &dbConnString,
                    const std::string &serviceURL,
                    const MessageTypeSpec &msgTypeSpec);

        QueueWriter(const QueueWriter &) = delete;

        std::unique_ptr<IAsyncDatabaseOperation> WriteMessages(const std::vector<std::string> &messages);
    };

}// end of namespace broker
}// end of namespace _3fd

#endif // end of header guard
