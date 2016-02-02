#ifndef RPC_IMPL_H // header guard
#define RPC_IMPL_H

#include "rpc.h"

namespace _3fd
{
    namespace rpc
    {
        class RpcServerImpl
        {
        private:

            RPC_BINDING_VECTOR *m_bindings;

        public:

            RpcServerImpl()
                : m_bindings(nullptr) {}

            ~RpcServerImpl();

            void Start(RpcServer::ProtocolSequence protSeq, const std::vector<Interface> &interfaces);
        };

    }// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
