#ifndef RPC_HELPERS_POSIX_ONC_H // header guard
#define RPC_HELPERS_POSIX_ONC_H

#include "base.h"
#include <string>
#include <rpc/clnt.h>

namespace _3fd
{
namespace rpc
{
    using std::string;

    enum class Protocol { TCP, UDP };

    class RpcClient : notcopiable
    {
    private:

        CLIENT *m_clientHandle;

    public:

        RpcClient(const string &hostAddr,
                  unsigned long programId,
                  unsigned long intfVersion,
                  Protocol protocol);

        virtual ~RpcClient();
    };

    void ThrowExForClientCall(CLIENT *clientHandle, const char *message, const char *function)

}// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard