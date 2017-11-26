#ifndef WEB_WWS_UTILS_H // header guard
#define WEB_WWS_UTILS_H

#include "exceptions.h"
#include <WebServices.h>

#include <vector>
#include <string>

#define WS_HEAP_NEW(HEAPOBJ, TYPE, INITIALIZER) (new (HEAPOBJ.Alloc<TYPE>()) TYPE INITIALIZER)

namespace _3fd
{
using std::string;

namespace web
{
namespace wws
{
    /// <summary>
    /// A heap provides precise control over memory allocation when producing or 
    /// consuming messages and when needing to allocate various other API structures.
    /// </summary>
    class WSHeap
    {
    private:

        WS_HEAP *m_wsHeapHandle;
        bool m_allowRelease;

    public:

        WSHeap(WS_HEAP *wsHeapHandle);

        WSHeap(size_t nBytes);

        WSHeap(const WSHeap &) = delete;

        ~WSHeap();

        /// <summary>
        /// Initializes a new instance of the <see cref="WSHeap"/> class using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources will be stolen from.</param>
        WSHeap(WSHeap &&ob) :
            m_wsHeapHandle(ob.m_wsHeapHandle),
            m_allowRelease(ob.m_allowRelease)
        {
            ob.m_wsHeapHandle = nullptr;
        }

        void Reset();

        void *Alloc(size_t qtBytes);

        template <typename Type> Type *Alloc()
        {
            return static_cast<Type *> (Alloc(sizeof(Type)));
        }

        template <typename Type> Type *Alloc(size_t qtObjects)
        {
            _ASSERTE(qtObjects > 0);
            return static_cast<Type *> (Alloc(sizeof(Type) * qtObjects));
        }

        /// <summary>
        /// Gets the heap handle.
        /// </summary>
        /// <returns>The handle for the opaque handle object.</returns>
        WS_HEAP *GetHandle() { return m_wsHeapHandle; }
    };


    /// <summary>
    /// A reusable object model capable to hold rich error information.
    /// </summary>
    class WSError
    {
    private:

        WS_ERROR *m_wsErrorHandle;
        bool m_allowRelease;

        void Initialize();

    public:

        WSError();
        ~WSError();

        WSError(const WSError &) = delete;

        /// <summary>
        /// Initializes a new instance of the <see cref="WSError"/> class
        /// with a handle for an already existent rich error info object.
        /// </summary>
        /// <param name="wsErrorHandle">The handle for the already existent object.</param>
        WSError(WS_ERROR *wsErrorHandle)
            : m_wsErrorHandle(wsErrorHandle), m_allowRelease(false) {}

        /// <summary>
        /// Initializes a new instance of the <see cref="WSError"/>
        /// class using move semantics.
        /// </summary>
        /// <param name="ob">The objects whose resources will be stolen from.</param>
        WSError(WSError &&ob) :
            m_wsErrorHandle(ob.m_wsErrorHandle),
            m_allowRelease(ob.m_allowRelease)
        {
            ob.m_wsErrorHandle = nullptr;
        }

        void RaiseExceptionApiError(HRESULT hres,
                                    const char *funcName,
                                    const char *message);

        void LogApiError(HRESULT hres,
                         const char *funcName,
                         const char *message) NOEXCEPT;

        void RaiseExClientNotOK(HRESULT hres,
                                const char *message,
                                WSHeap &heap);

        WS_ERROR *GetHandle();

        void Reset() NOEXCEPT;
    };

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd

#endif // end of header guard
