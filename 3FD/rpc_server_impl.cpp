#include "stdafx.h"
#include "rpc_server_impl.h"
#include "rpc_util_impl.h"
#include "callstacktracer.h"
#include "logger.h"

#include <codecvt>

namespace _3fd
{
    namespace rpc
    {
        /////////////////////////
        // RpcServer Class
        /////////////////////////

        void RpcServerImpl::Run(
            RpcServer::ProtocolSequence protSeq,
            const std::vector<RpcInterfaceHandle> &interfaces,
            const string &description)
        {
            CALL_STACK_TRACE;

            RPC_STATUS status;

            switch (m_state)
            {
                case State::Instantiated:
                {
                    // What protocol sequence?
                    const wchar_t *protSeqStr;
                    switch (protSeq)
                    {
                    case RpcServer::ProtocolSequence::Local:
                        protSeqStr = L"ncalrpc";
                        break;
                    case RpcServer::ProtocolSequence::TCP:
                        protSeqStr = L"ncacn_ip_tcp";
                        break;
                    case RpcServer::ProtocolSequence::UDP:
                        protSeqStr = L"ncadg_ip_udp";
                        break;
                    default:
                        protSeqStr = nullptr;
                        break;
                    }

                    // Set the protocol sequence:
                    _ASSERTE(protSeqStr != nullptr);
                    status = RpcServerUseProtseqW(
                        (unsigned short *)protSeqStr,
                        RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                        nullptr
                    );

                    ThrowIfError(status, "Failed to set protocol sequence for RPC server");

                    // Inquire bindings:
                    status = RpcServerInqBindings(&m_bindings);
                    ThrowIfError(status, "Failed to inquire bindings for RPC server");
                    m_state = State::BindingsAcquired;
                }

                // FALLTHROUGH:
                case State::BindingsAcquired:
                {
                    // Generate the annotation according the API doc:
                    std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                    const int annStrMaxSize(64);
                    std::wstring annotation = transcoder.from_bytes(description).substr(0, annStrMaxSize - 1);

                    // For each interface:
                    for (auto &intfHandle : interfaces)
                    {
                        // Register the interface:
                        status = RpcServerRegisterIf(intfHandle, nullptr, nullptr);
                        ThrowIfError(status, "Failed to register RPC interface");

                        // Update server address information in the local endpoint-map database:
                        status = RpcEpRegisterW(
                            intfHandle,
                            m_bindings,
                            nullptr,
                            (RPC_WSTR)annotation.c_str()
                            );

                        ThrowIfError(status, "Failed to register endpoints for RPC server");
                    }

                    m_state = State::InterfacesRegistered;
                }

            // FALLTHROUGH:
            case State::InterfacesRegistered:

                // Start listening requests (blocking call):
                status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, 0);
                ThrowIfError(status, "Failed to start RPC server listeners");
                m_state = State::Listening;
                break;

            case State::Listening:
                break; // nothing to do
            
            default:
                _ASSERTE(false); // unsupported state
                break;
            }
        }

        RpcServerImpl::~RpcServerImpl()
        {
            if (m_state == State::Instantiated)
                return;

            CALL_STACK_TRACE;

            RPC_STATUS status;

            switch (m_state)
            {
            case State::Listening:
                status = RpcMgmtStopServerListening(m_bindings);
                LogIfError(status, "Failed to stop RPC server listeners", core::Logger::PRIO_CRITICAL);

            // FALLTHROUGH:
            case State::InterfacesRegistered:
                status = RpcServerUnregisterIf(nullptr, nullptr, 1);
                LogIfError(status, "Failed to unregister interfaces from RPC server", core::Logger::PRIO_CRITICAL);

            // FALLTHROUGH:
            case State::BindingsAcquired:
                status = RpcBindingVectorFree(&m_bindings);
                LogIfError(status, "Failed to release resources for RPC server bindings", core::Logger::PRIO_CRITICAL);
                break;

            case State::Instantiated:
                break;

            default:
                _ASSERTE(false); // unsupported state
                break;
            }
        }

    }// end of namespace rpc
}// end of namespace _3fd
