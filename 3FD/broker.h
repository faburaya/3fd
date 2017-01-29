#ifndef BROKER_H // header guard
#define BROKER_H

#include "base.h"
#include "preprocessing.h"
#include <string>
#include <vector>
#include <memory>
#include <cinttypes>
#include <Poco/AutoPtr.h>
#include <Poco/Data/Session.h>

namespace _3fd
{
namespace broker
{
    using std::string;

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
    /// Controls retrieval of results from asynchronous read of queue.
    /// </summary>
    class INTFOPT IAsyncRead
    {
    public:

        /// <summary>
        /// Evaluates whether the last asynchronous read step is finished.
        /// </summary>
        /// <returns><c>true</c> when finished, otherwise, <c>false</c>.</returns>
        virtual bool IsFinished() = 0;

        /// <summary>
        /// Waits for the last asynchronous read step to finish.
        /// </summary>
        virtual void Wait() = 0;

        virtual void Step() = 0;

        /// <summary>
        /// Gets the result from the last asynchronous step execution.
        /// </summary>
        /// <returns>A vector of read messages.</returns>
        virtual std::vector<string> GetStepResult() = 0;
    };

    /// <summary>
    /// Represents a queue in the broker, from which
    /// a service can read its incoming messages.
    /// </summary>
    /// <seealso cref="notcopiable" />
    class QueueReader : notcopiable
    {
    private:

        Poco::AutoPtr<Poco::Data::Session> m_dbSession;
        
        uint8_t m_queueId;

    public:

        QueueReader(Backend svcBrokerBackend,
                    const string &connString,
                    const string &serviceURL,
                    const MessageTypeSpec &msgTypeSpec,
                    uint8_t queueId);

        std::unique_ptr<IAsyncRead> ReadMessages(uint16_t msgCountStepLimit);
    };

    /// <summary>
    /// Represents a queue in the broker, into which
    /// a service can write messages to another.
    /// </summary>
    /// <seealso cref="notcopiable" />
    class QueueWriter : notcopiable
    {
    private:

        Poco::AutoPtr<Poco::Data::Session> m_dbSession;

        uint8_t m_queueId;

        void CreateTempTableForQueueInput();

    public:

        QueueWriter(Backend svcBrokerBackend,
                    const string &connString,
                    const string &fromServiceURL,
                    const string &toServiceURL,
                    const MessageTypeSpec &msgTypeSpec,
                    uint8_t queueId);

        std::future<void> WriteMessages(const std::vector<string> &messages);
    };

}// end of namespace broker
}// end of namespace _3fd

#endif // end of header guard
