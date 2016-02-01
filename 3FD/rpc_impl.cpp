#include "stdafx.h"
#include "rpc_impl.h"
#include "callstacktracer.h"
#include "exceptions.h"

#include <Poco\UUID.h>
#include <sstream>
#include <codecvt>

namespace _3fd
{
    namespace rpc
    {
        core::AppException<std::runtime_error>
        CreateException(
            RPC_STATUS errCode,
            const string &message,
            const string &details)
        {
            unsigned short apiMsgUCS2[DCE_C_ERROR_STRING_LEN];

            // Get error message from API:
            auto status = DceErrorInqTextW(errCode, apiMsgUCS2);
            _ASSERTE(status == RPC_S_OK);

            // Transcode from UCS2 to UTF8:
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            string apiMsgUTF8 = transcoder.to_bytes(reinterpret_cast<wchar_t *> (apiMsgUCS2));

            // Assemble the message:
            std::ostringstream oss;
            oss << message << " - System RPC API reported an error: " << apiMsgUTF8;

            // Create the exception
            return core::AppException<std::runtime_error>(oss.str(), details);
        }

        void RpcServerImpl::Start(RPC_IF_HANDLE interfaceHandle,
                                  const string &implGUID,
                                  RPC_MGR_EPV *entryPointVector)
        {
            CALL_STACK_TRACE;

            // Validate the interface GUID:
            Poco::UUID guidPOCO;
            if (!guidPOCO.tryParse(implGUID))
            {
                throw core::AppException<std::runtime_error>(
                    "Failed to parse UUID for RPC server interface: UUID string is invalid!",
                    implGUID
                );
            }

            // Parse the GUID:
            UUID guidWinAPI;
            guidPOCO.copyTo(reinterpret_cast<char *> (&guidWinAPI));

            auto status = RpcServerRegisterIf(interfaceHandle, &guidWinAPI, entryPointVector);
        }

    }// end of namespace rpc
}// end of namespace _3fd
