#include "stdafx.h"
#include "broker_impl.h"

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

}// end of namespace broker
}// end of namespace _3fd