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

            const wchar_t *m_protSeqName;

            /// <summary>
            /// Enumerates the possible states for the RPC server.
            /// </summary>
            enum class State
            {
                Instantiated,
                Listening,
                InterfacesRegistered,
                BindingsAcquired
            };

            State m_state;

        public:

            RpcServerImpl(RpcServer::ProtocolSequence protSeq);

            ~RpcServerImpl();

            void Run(
                RpcServer::ProtocolSequence protSeq,
                const std::vector<RpcInterfaceHandle> &interfaces,
                const string &description);
        };

    }// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
