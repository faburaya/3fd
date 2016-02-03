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
        /// <param name="objUUID">The UUID of the object in the RPC server.
        /// (Optional: an empty string is equivalent to a nil UUID.)</param>
        /// <param name="protSeq">The transport to use for RPC.</param>
        /// <param name="destination">The destination: local RPC requires the machine name,
        /// while for TCP or UDP this is the network address (IP or host name).</param>
        /// <param name="endpoint">The endpoint: for local RPC is the application or service
        /// name, while for TCP or UDP this is the port number. Specifying the endpoint is
        /// optional if the server has registered its bindings with the endpoint mapper.</param>
        RpcClient::RpcClient(
            const string &objUUID,
            ProtocolSequence protSeq,
            const string &destination,
            const string &endpoint)
        try
        {
            CALL_STACK_TRACE;

            // Prepare text parameters encoded in UCS-2:
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

            std::wstring ucs2ObjUuid;
            RPC_WSTR paramObjUuid;

            if (objUUID.empty() || objUUID == "")
                paramObjUuid = nullptr;
            else
            {
                ucs2ObjUuid = transcoder.from_bytes(objUUID);
                paramObjUuid = (RPC_WSTR)ucs2ObjUuid.c_str();
            }

            auto paramProtSeq = (RPC_WSTR)ToString(protSeq);

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

    }// end of namespace rpc
}// end of namespace _3fd
