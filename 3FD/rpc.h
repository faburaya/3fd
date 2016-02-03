#ifndef RPC_H // header guard
#define RPC_H

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <rpc.h>

namespace _3fd
{
    using std::string;

    namespace rpc
    {
        class RpcServerImpl;

        /// <summary>
        /// Represents the RPC server that runs inside the application process.
        /// </summary>
        class RpcServer
        {
        private:

            static std::unique_ptr<RpcServerImpl> uniqueObject;

            static std::mutex singletonAccessMutex;

            RpcServer() {}

        public:

            ~RpcServer() {}

            /// <summary>
            /// Enumerates the possible options for RPC transport.
            /// </summary>
            enum class ProtocolSequence { Local, TCP, UDP };

            static void Initialize(ProtocolSequence protSeq);

            static bool Start(const std::vector<RPC_IF_HANDLE> &interfaces,
                              const string &description);

            static bool Stop();

            static bool Resume();

            static bool Wait();

            static void Finalize() noexcept;
        };

    }// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
