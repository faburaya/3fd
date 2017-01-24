#ifndef BROKER_H // header guard
#define BROKER_H

#include "base.h"
#include <string>
#include <cinttypes>
#include <Poco/AutoPtr.h>
#include <Poco/Data/Session.h>
#include <Poco/Data/Statement.h>

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
        MsSqlServer,
        OracleDatabase
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
        string url;
        uint32_t nBytes;
        MessageContentValidation contentValidation;
    };

    /// <summary>
    /// Represents a queue in the broker, from which
    /// a service can read its incoming messages.
    /// </summary>
    class QueueReader
    {
    private:

        Poco::AutoPtr<Poco::Data::Session> m_dbSession;
        Poco::AutoPtr<Poco::Data::Statement> m_readMsgStmt;

    public:

        QueueReader(Backend svcBrokerBackend,
                    const string &connString,
                    const string &serviceURL,
                    uint8_t queueId,
                    const MessageTypeSpec &msgTypeSpec);

        ;
    };
}
}// end of namespace _3fd

#endif // end of header guard
