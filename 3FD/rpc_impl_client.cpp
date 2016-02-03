#include "stdafx.h"
#include "rpc.h"
#include "rpc_impl_util.h"
#include "callstacktracer.h"

#include <codecvt>

namespace _3fd
{
    namespace rpc
    {
        /////////////////////////
        // RpcClient Class
        /////////////////////////

        /// <summary>
        /// Initializes the RPC server before running it.
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

            auto status = RpcStringBindingComposeW(
            );
        }
        catch (std::exception &ex)
        {

        }

    }// end of namespace rpc
}// end of namespace _3fd
