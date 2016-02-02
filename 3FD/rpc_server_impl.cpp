#include "stdafx.h"
#include "rpc_server_impl.h"
#include "rpc_util_impl.h"
#include "callstacktracer.h"

#include <map>

namespace _3fd
{
    namespace rpc
    {
        /////////////////////////
        // Interface Class
        /////////////////////////

        Interface::Interface(RpcInterfaceHandle handle, const string &uuid)
            : m_handle(handle)
        {
            CALL_STACK_TRACE;

            // Parse the UUID:
            if (!m_uuid.tryParse(uuid))
            {
                throw core::AppException<std::runtime_error>(
                    "Failed to parse UUID for RPC server interface: UUID string is invalid!",
                    uuid
                );
            }
        }

        // Adds a new UUID for an object implementing this interface
        void Interface::AddObjectUUID(const string &uuid)
        {
            CALL_STACK_TRACE;

            m_objectsUUIDs.emplace_back();

            // Parse the UUID:
            if (!m_objectsUUIDs.back().tryParse(uuid))
            {
                throw core::AppException<std::runtime_error>(
                    "Failed to parse UUID for object implementing interface for RPC server: "
                    "UUID string is invalid!",
                    uuid
                );
            }
        }

        /////////////////////////
        // RpcServer Class
        /////////////////////////

        void RpcServerImpl::Start(RpcServer::ProtocolSequence protSeq,
                                  const std::vector<Interface> &interfaces)
        {
            CALL_STACK_TRACE;

            std::map<string, std::vector<UUID>> objIDsByIntfID;

            for (auto &interface : interfaces)
            {
                // A list of UUID's for the objects implementing this interface must be kept aside for later use:
                string intfUuidStr = interface.GetUUID().toString();
                auto mapInsert = objIDsByIntfID.emplace(intfUuidStr, interface.GetObjectsUUIDs().size());

                // Cannot insert in the map when 2 or more interfaces have the same UUID:
                if (!mapInsert.second)
                {
                    throw core::AppException<std::runtime_error>(
                        "Could not register RPC interface because the provided UUID is not unique in the list",
                        intfUuidStr
                    );
                }

                // Register the interface:
                UUID intfUuidBin;
                interface.GetUUID().copyTo(reinterpret_cast<char *> (&intfUuidBin));
                auto status = RpcServerRegisterIf(interface.GetHandle(), &intfUuidBin, nullptr);
                ThrowIfError(status, "Failed to register RPC interface", intfUuidStr);

                // For later retrieval, the objects UUID's will be stored in a structured binary representation:
                auto &objectsUUIDs = mapInsert.first->second;
                int idx(0);
                for (auto &objUUID : interface.GetObjectsUUIDs())
                {
                    UUID *objUuidBin = &objectsUUIDs[idx++];
                    objUUID.copyTo(reinterpret_cast<char *> (objUuidBin));

                    // TO DO: RpcSetObjectType
                }
            }

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
            case RpcServer::ProtocolSequence::HTTP:
                protSeqStr = L"ncacn_http";
                break;
            default:
                protSeqStr = nullptr;
                break;
            }

            // Set the protocol sequence:
            _ASSERTE(protSeqStr != nullptr);
            auto status = RpcServerUseProtseqW(
                (unsigned short *)protSeqStr,
                RPC_C_PROTSEQ_MAX_REQS_DEFAULT,
                nullptr
            );

            ThrowIfError(status, "Failed to set protocol sequence for RPC server");

            // Inquire bindings:
            status = RpcServerInqBindings(&m_bindings);
            ThrowIfError(status, "Failed to inquire bindings for RPC server");


        }

    }// end of namespace rpc
}// end of namespace _3fd
