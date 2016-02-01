#ifndef RPC_IMPL_H // header guard
#define RPC_IMPL_H

#include "rpc.h"

namespace _3fd
{
    namespace rpc
    {
        class RpcServerImpl
        {
        public:

            RpcServerImpl();
            ~RpcServerImpl();

            void Start(RPC_IF_HANDLE interfaceHandle,
                       const string &implGUID,
                       RPC_MGR_EPV *entryPointVector);
        };

    }// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
