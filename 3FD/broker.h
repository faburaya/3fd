#ifndef BROKER_H // header guard
#define BROKER_H

#include "base.h"
#include "preprocessing.h"
#include <cinttypes>
#include <string>
#include <vector>
#include <memory>
#include <thread>
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

        virtual ~IAsyncRead() {}

        /// <summary>
        /// Evaluates whether the last asynchronous read step is finished.
        /// </summary>
        /// <returns>
        ///   <c>true</c> when finished, otherwise, <c>false</c>.
        /// </returns>
        virtual bool HasJoined() const = 0;

        /// <summary>
        /// Waits for the last asynchronous read step to finish.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>
        ///   <c>true</c> if any message could be retrieved, otherwise, <c>false</c>.
        /// </returns>
        virtual bool TryWait(uint16_t timeout) = 0;

        /// <summary>
        /// Start the asynchronous execution of the next step.
        /// </summary>
        virtual void Step() = 0;

        /// <summary>
        /// Gets the count of retrieved messages in the last step execution.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>How many messages were read from the queue after
        /// waiting for completion of asynchronous read operation.
        /// Subsequent calls or calls that time out will return zero.</returns>
        virtual uint32_t GetStepMessageCount(uint16_t timeout) = 0;

        /// <summary>
        /// Gets the result from the last asynchronous step execution.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>A vector of read messages after waiting for completion of
        /// asynchronous read operation. Subsequent calls or calls that time out
        /// will return an empty vector. The retrieved messages are not guaranteed
        /// to appear in the same order they had when inserted.</returns>
        virtual std::vector<string> GetStepResult(uint16_t timeout) = 0;
        
        /// <summary>
        /// Rolls back the changes accumulated in the current transaction.
        /// The messages extracted in all steps from the broker queue are
        /// put back in place.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>
        ///   <c>true</c> if current transaction was successfully rolled back, otherwise, <c>false</c>.
        /// </returns>
        virtual bool Rollback(uint16_t timeout) = 0;
        
        /// <summary>
        /// Commits in persistent storage all the changes accumulated in the
        /// current transaction. The messages extracted in all steps so far
        /// from the broker queue are permanently removed.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>
        ///   <c>true</c> if current transaction was successfully committed, otherwise, <c>false</c>.
        /// </returns>
        virtual bool Commit(uint16_t timeout) = 0;
    };

    /// <summary>
    /// Implementation needs this base class upon queue reader/writer
    /// initialization before using POCO Data ODBC connector.
    /// </summary>
    class OdbcClient
    {
    public:

        OdbcClient();
    };

    /// <summary>
    /// Represents a queue in the broker, from which a service can read
    /// its incoming messages. This implementation is NOT THREAD SAFE!
    /// </summary>
    /// <seealso cref="notcopiable" />
    class QueueReader : notcopiable, OdbcClient
    {
    private:

        Poco::Data::Session m_dbSession;
        
        string m_serviceURL;

    public:

        QueueReader(Backend svcBrokerBackend,
                    const string &connString,
                    const string &serviceURL,
                    const MessageTypeSpec &msgTypeSpec);

        std::unique_ptr<IAsyncRead> ReadMessages(uint16_t msgCountStepLimit, uint16_t msgRecvTimeout);
    };

    /// <summary>
    /// Helps synchronizing with an asynchronous write to a broker queue.
    /// The underlying implementation is NOT THREAD SAFE!
    /// </summary>
    class INTFOPT IAsyncWrite
    {
    public:

        virtual ~IAsyncWrite() {}

        /// <summary>
        /// Evaluates whether the last asynchronous write operation is finished.
        /// </summary>
        /// <returns>
        ///   <c>true</c> when finished, otherwise, <c>false</c>.
        /// </returns>
        virtual bool IsFinished() const = 0;

        /// <summary>
        /// Waits for the last asynchronous write operation to finish.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>
        ///   <c>true</c> if the operation is finished, otherwise, <c>false</c>.
        /// </returns>
        virtual bool TryWait(uint16_t timeout) = 0;

        /// <summary>
        /// Rethrows any eventual exception captured in the worker thread.
        /// </summary>
        virtual void Rethrow() = 0;

        /// <summary>
        /// Rolls back the changes accumulated in the current transaction,
        /// erasing all messages written into the broker queue by the call
        /// that originated this object and began such transaction.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>
        ///   <c>true</c> if current transaction was successfully rolled back, otherwise, <c>false</c>.
        /// </returns>
        virtual bool Rollback(uint16_t timeout) = 0;

        /// <summary>
        /// Commits in persistent storage all the changes accumulated in the current
        /// transaction, which began in the call that originated this object.
        /// </summary>
        /// <param name="timeout">The timeout in milliseconds.</param>
        /// <returns>
        ///   <c>true</c> if current transaction was successfully committed, otherwise, <c>false</c>.
        /// </returns>
        virtual bool Commit(uint16_t timeout) = 0;
    };

    /// <summary>
    /// Represents a queue in the broker, into which
    /// a service can write messages to another.
    /// </summary>
    /// <seealso cref="notcopiable" />
    class QueueWriter : notcopiable, OdbcClient
    {
    private:

        Poco::Data::Session m_dbSession;

        string m_serviceURL;

        std::unique_ptr<std::thread> m_workerThread;

    public:

        QueueWriter(Backend svcBrokerBackend,
                    const string &connString,
                    const string &serviceURL,
                    const MessageTypeSpec &msgTypeSpec);

        ~QueueWriter();

        std::unique_ptr<IAsyncWrite> WriteMessages(const std::vector<string> &messages);
    };

}// end of namespace broker
}// end of namespace _3fd

#endif // end of header guard
