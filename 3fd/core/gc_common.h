#ifndef GC_COMMON_H
#define GC_COMMON_H

#include <cstdlib>

namespace _3fd
{
namespace memory
{
    typedef void (*FreeMemProc)(void *addr, bool destroy);

    /// <summary>
    /// Frees memory allocated by the GC.
    /// This is compiled by the client code compiler.
    /// </summary>
    /// <param name="addr">The memory address.</param>
    template <typename X>
    void FreeMemAddr(void *addr, bool destroy = true)
    {
        auto ptr = static_cast<X *> (addr);

        if (destroy)
            ptr->X::~X();

#    ifdef _WIN32
        _aligned_free(ptr);
#    else
        free(ptr);
#    endif
    }

    void *AllocMemoryAndRegisterWithGC(
        size_t size,
        void *sptrObjAddr,
        FreeMemProc freeMemCallback
    );

}// end of namespace memory
}// end of namespace _3fd

#endif // end of header guard
