#ifndef RPC_UTIL_IMPL_H // header guard
#define RPC_UTIL_IMPL_H

#include "rpc_helpers.h"

#include <string>
#include <NtDsAPI.h>

namespace _3fd
{
    using std::string;

    namespace rpc
    {
        const char *ToString(ProtocolSequence protSeq);

        const char *ToString(DS_NAME_ERROR error);

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

        /// <summary>
        /// RAII for name result from 'DsCrackNames'.
        /// </summary>
        struct NameResult
        {
            DS_NAME_RESULTW *data;

            NameResult()
                : data(nullptr) {}

            ~NameResult()
            {
                if (data != nullptr)
                    DsFreeNameResultW(data);
            }
        };

        /// <summary>
        /// RAII for Directory Service binding handle.
        /// </summary>
        struct DirSvcBinding
        {
            HANDLE handle;

            DirSvcBinding()
                : handle(nullptr) {}

            ~DirSvcBinding()
            {
                if (handle != nullptr)
                    DsUnBindW(&handle);
            }
        };

        /// <summary>
        /// RAII for array of SPN's.
        /// </summary>
        struct ArrayOfSpn
        {
            DWORD size;
            LPWSTR *data;

            ArrayOfSpn()
                : size(0), data(nullptr) {}

            ~ArrayOfSpn()
            {
                if (data != nullptr)
                    DsFreeSpnArrayW(size, data);
            }
        };

    }// end of namespace rpc
}// end of namespace _3fd

#endif // end of header guard
