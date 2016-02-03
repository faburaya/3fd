#ifndef RPC_UTIL_IMPL_H // header guard
#define RPC_UTIL_IMPL_H

#include "rpc.h"
#include "exceptions.h"
#include <string>

namespace _3fd
{
    using std::string;

    namespace rpc
    {
        const wchar_t *ToString(ProtocolSequence protSeq);

        void ThrowIfError(RPC_STATUS status, const char *message);

        void ThrowIfError(
            RPC_STATUS status,
            const char *message,
            const string &details
        );

        void LogIfError(
            RPC_STATUS status,
            const char *message,
            core::Logger::Priority prio
        ) noexcept;

        void LogIfError(
            RPC_STATUS status,
            const char *message,
            const string &details,
            core::Logger::Priority prio
        ) noexcept;
    }
}

#endif // end of header guard
