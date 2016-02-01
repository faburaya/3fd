#ifndef RPC_H // header guard
#define RPC_H

#include <string>
#include <rpc.h>

namespace _3fd
{
    using std::string;

    namespace rpc
    {
        class RpcServerImpl;

        class RpcServer
        {
        private:

            RpcServerImpl *m_pimpl;

        public:

            RpcServer();
            ~RpcServer();

            void Start(RPC_IF_HANDLE interfaceHandle,
                       const string &implGUID,
                       RPC_MGR_EPV *entryPointVector);
        };

    }// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
