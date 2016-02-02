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

            enum class State { Instantiated, Listening, InterfacesRegistered, BindingsAcquired };

            State m_state;

        public:

            RpcServerImpl()
                : m_bindings(nullptr), m_state(State::Instantiated) {}

            ~RpcServerImpl();

            void Run(
                RpcServer::ProtocolSequence protSeq,
                const std::vector<RpcInterfaceHandle> &interfaces,
                const string &description);
        };

    }// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
