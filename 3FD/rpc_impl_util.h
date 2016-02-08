#ifndef RPC_UTIL_IMPL_H // header guard
#define RPC_UTIL_IMPL_H

#include "rpc_helpers.h"
#include "exceptions.h"
#include "logger.h"

#include <string>

namespace _3fd
{
    using std::string;

    namespace rpc
    {
        const char *ToString(ProtocolSequence protSeq);

        const unsigned long UUID_VECTOR_MAX_SIZE(32);

        /// <summary>
        /// This is an improvised fix for UUID_VECTOR,
        /// that seems to be wrongly defined in RPC API.
        /// </summary>
        struct UuidVectorFix
        {
            unsigned long size;
            UUID *data[UUID_VECTOR_MAX_SIZE];
        };

        /// <summary>
        /// Simple wrapper for a vector of <see cref="UUID"/> structs.
        /// It uses RAII to guarantee deallocation upon scope end,
        /// not having to resort to smart pointers.
        /// </summary>
        class VectorOfUuids
        {
        private:

            std::vector<UUID *> m_ptrs2Uuids;

        public:

            VectorOfUuids() {}

            VectorOfUuids(VectorOfUuids &&ob)
                : m_ptrs2Uuids(std::move(ob.m_ptrs2Uuids)) {}

            VectorOfUuids &operator =(VectorOfUuids &&ob)
            {
                if(&ob != this)
                    m_ptrs2Uuids = std::move(ob.m_ptrs2Uuids);

                return *this;
            }

            ~VectorOfUuids();

            void Add(const UUID &uuid);

            UUID_VECTOR *CopyTo(UuidVectorFix &vec) noexcept;
        };

        /////////////////////////
        // Error Helpers
        /////////////////////////

        void ThrowIfError(RPC_STATUS status, const char *message);

        void ThrowIfError(
            RPC_STATUS status,
            const char *message,
            const string &details
        );

        void LogIfError(
            RPC_STATUS status,
            const char *message,
            core::Logger::Priority prio
        ) noexcept;

        void LogIfError(
            RPC_STATUS status,
            const char *message,
            const string &details,
            core::Logger::Priority prio
        ) noexcept;
    }
}

#endif // end of header guard
