#include "stdafx.h"
#include "rpc.h"
#include "rpc_impl_util.h"
#include "callstacktracer.h"
#include "logger.h"

#include <codecvt>
#include <sstream>

namespace _3fd
{
    namespace rpc
    {
        /////////////////////////
        // RpcClient Class
        /////////////////////////
        
        /// <summary>
        /// Initializes a new instance of the <see cref="RpcClient"/> class.
        /// </summary>
        /// <param name="protSeq">The transport to use for RPC.</param>
        /// <param name="objUUID">The UUID of the object in the RPC server. An empty string
        /// is equivalent to a nil UUID, which is valid as long as the endpoint is specified.</param>
        /// <param name="destination">The destination: local RPC requires the machine name,
        /// while for TCP or UDP this is the network address (IP or host name).</param>
        /// <param name="endpoint">The endpoint: for local RPC is the application or service
        /// name, while for TCP or UDP this is the port number. Specifying the endpoint is
        /// optional if the server has registered its bindings with the endpoint mapper.</param>
        RpcClient::RpcClient(
            ProtocolSequence protSeq,
            const string &objUUID,
            const string &destination,
            const string &endpoint)
        try
        {
            CALL_STACK_TRACE;

            // Prepare text parameters encoded in UCS-2:
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

            std::wstring ucs2objUuid;
            RPC_WSTR paramObjUuid;

            if (objUUID.empty() || objUUID == "")
                paramObjUuid = nullptr;
            else
            {
                ucs2objUuid = transcoder.from_bytes(objUUID);
                paramObjUuid = (RPC_WSTR)ucs2objUuid.c_str();
            }

            std::wstring protSeqName = transcoder.from_bytes(ToString(protSeq));
            auto paramProtSeq = (RPC_WSTR)protSeqName.c_str();

            auto ucs2Destination = transcoder.from_bytes(destination);
            auto paramDestination = (RPC_WSTR)ucs2Destination.c_str();

            std::wstring ucs2Endpoint;
            RPC_WSTR paramEndpoint;

            if (endpoint.empty() || endpoint == "")
                paramEndpoint = nullptr;
            else
            {
                ucs2Endpoint = transcoder.from_bytes(endpoint);
                paramEndpoint = (RPC_WSTR)ucs2Endpoint.c_str();
            }
            
            // Compose the binding string:
            RPC_WSTR bindingString;
            auto status = RpcStringBindingComposeW(
                paramObjUuid,
                paramProtSeq,
                paramDestination,
                paramEndpoint,
                nullptr,
                &bindingString
            );

            ThrowIfError(status, "Failed to compose binding string for RPC client");

            // Create a binding handle from the composed string:
            status = RpcBindingFromStringBindingW(bindingString, &m_bindingHandle);
            ThrowIfError(status, "Failed to create binding handle for RPC client");

            // Release the memory allocated for the binding string:
            status = RpcStringFreeW(&bindingString);
            ThrowIfError(status, "Failed to compose binding string for RPC client");
        }
        catch (core::IAppException &)
        {
            throw; // just forward an exception regarding an error known to have been already handled
        }
        catch (std::exception &ex)
        {
            CALL_STACK_TRACE;
            std::ostringstream oss;
            oss << "Generic failure when creating RPC client: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="RpcClient"/> class.
        /// </summary>
        RpcClient::~RpcClient()
        {
            CALL_STACK_TRACE;
            auto status = RpcBindingFree(&m_bindingHandle);
            LogIfError(status,
                "Failed to release resources from binding handle of RPC client",
                core::Logger::Priority::PRIO_CRITICAL);
        }

        /// <summary>
        /// Removes the endpoint portion of the server address in the binding handle.
        /// The host remains unchanged in the binding handle. The result is a partially-bound
        /// server binding handle. When the client makes the next remote procedure call using
        /// the reset (partially-bound) binding, the client's run-time library automatically
        /// communicates with the endpoint-mapping service on the specified remote host to obtain
        /// the endpoint of a compatible server from the endpoint-map database. If a compatible
        /// server is located, the RPC run-time library updates the binding with a new endpoint.
        /// If a compatible server is not found, the remote procedure call fails.
        /// </summary>
        void RpcClient::ResetBindings()
        {
            CALL_STACK_TRACE;
            auto status = RpcBindingReset(m_bindingHandle);
            ThrowIfError(status, "Failed to reset binding handle of RPC client");
        }

    }// end of namespace rpc
}// end of namespace _3fd
