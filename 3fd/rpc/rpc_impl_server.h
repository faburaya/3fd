#ifndef RPC_IMPL_H // header guard
#define RPC_IMPL_H

#include "rpc_helpers.h"
#include "rpc_impl_util.h"
#include <map>

namespace _3fd
{
namespace rpc
{
    /// <summary>
    /// Private implementation of <see cref="RpcServer"/> class.
    /// </summary>
    class RpcServerImpl
    {
    private:

        RPC_BINDING_VECTOR *m_bindings;

        std::map<RPC_IF_HANDLE, VectorOfUuids> m_regObjsByIntfHnd;

        std::wstring m_serviceName;

        std::unique_ptr<SystemCertificateStore> m_sysCertStore;
        std::unique_ptr<SChannelCredWrapper> m_schannelCred;

        AuthenticationLevel m_cliReqAuthnLevel;
        bool m_hasAuthnSec;

        /// <summary>
        /// Enumerates the possible states for the RPC server.
        /// </summary>
        enum class State
        {
            NotInitialized, // initial state: not fully instantiated
            BindingsAcquired, // bindings acquired for dynamic endpoints & authentication service set
            IntfRegRuntimeLib, // interfaces registered with RPC runtime library
            IntfRegLocalEndptMap, // interfaces registered in the local endpoint-map database
            Listening // server is listening
        };

        State m_state;

        void OnStartFailureRollbackIntfReg() noexcept;

    public:

        RpcServerImpl(
            ProtocolSequence protSeq,
            const string &serviceName,
            bool hasAuthnSec
        );

        RpcServerImpl(
            ProtocolSequence protSeq,
            const string &serviceName,
            AuthenticationLevel authnLevel
        );

        RpcServerImpl(
            const string &serviceName,
            const CertInfo *certInfoX509,
            AuthenticationLevel authnLevel
        );

        ~RpcServerImpl();

        /// <summary>
        /// Gets the required authentication level.
        /// </summary>
        /// <returns>The required authentication level for clients,
        /// as defined upon initialization.</returns>
        AuthenticationLevel GetRequiredAuthnLevel() const { return m_cliReqAuthnLevel; }

        bool Start(const std::vector<RpcSrvObject> &objects);

        bool Stop();

        bool Resume();

        bool Wait();
    };

}// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
