#include "stdafx.h"
#include "broker_impl.h"
#include <Poco/Data/ODBC/Connector.h>

namespace _3fd
{
namespace broker
{
    /// <summary>
    /// Converts an enumerated type of message content validation to a label.
    /// </summary>
    /// <param name="msgContentValidation">What validation to use in message content.</param>
    /// <returns>A label as expected by T-SQL.</returns>
    const char *ToString(MessageContentValidation msgContentValidation)
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
    /// Initializes a new instance of the <see cref="OdbcClient"/> class.
    /// </summary>
    OdbcClient::OdbcClient()
    {
        Poco::Data::ODBC::Connector::registerConnector();
    }

}// end of namespace broker
}// end of namespace _3fd