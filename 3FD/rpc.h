#ifndef RPC_H // header guard
#define RPC_H

#include <Poco\UUID.h>
#include <string>
#include <vector>
#include <rpc.h>

namespace _3fd
{
    using std::string;

    namespace rpc
    {
        using RpcInterfaceHandle = RPC_IF_HANDLE;

        class RpcServerImpl;

        // Represents the RPC server that runs inside the application process
        class RpcServer
        {
        private:

            static RpcServerImpl *uniqueObject;

            RpcServer();

        public:

            enum class ProtocolSequence { Local, TCP, UDP };

            static RpcServer &GetInstance();
            
            ~RpcServer();

            void Run(
                ProtocolSequence protSeq,
                const std::vector<RpcInterfaceHandle> &interfaces,
                const string &description);
        };

    }// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
