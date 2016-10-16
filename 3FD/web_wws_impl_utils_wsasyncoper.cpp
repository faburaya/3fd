#include "stdafx.h"
#include "web_wws_impl_utils.h"

#include "callstacktracer.h"

namespace _3fd
{
namespace web
{
namespace wws
{
    using namespace _3fd::core;

    /// <summary>
    /// The callback invoked when an asynchronous operation finishes.
    /// This may be invoked synchronously.
    /// </summary>
    /// <param name="hres">The HRESULT of status returned by the asynchronous call.</param>
    /// <param name="model">The asynchronous model from WWS API for the current call.</param>
    /// <param name="state">The state, which packs parameters for this callback.</param>
    void CALLBACK AsyncDoneCallback(HRESULT hres, WS_CALLBACK_MODEL model, void *state)
    {
        CALL_STACK_TRACE;

        auto &promise = *static_cast<std::promise<HRESULT> *> (state);

        try
        {
            promise.set_value(hres);
        }
        catch (std::future_error &ex)
        {
            std::ostringstream oss;
            oss << "Failed to retrieve result from asynchronous operation: "
                << StdLibExt::GetDetailsFromFutureError(ex);

            AppException<std::runtime_error> appEx(oss.str());
            promise.set_exception(std::make_exception_ptr(appEx));
        }
    }

    /// <summary>
    /// Initializes a new instance of the <see cref="WSAsyncOper" /> class.
    /// </summary>
    /// <param name="heapSize">
    /// Size of the heap (in bytes) that provides memory for the call in the web service proxy.
    /// </param>
    /// <param name="promise">
    /// A <see cref="std::promise"/> object for the asynchronous call in the web service.
    /// Such an object must be allocated in a way it is allowed o outlive this object in construction,
    /// which is why it will actually reside inside the web service proxy.
    /// </param>
    WSAsyncOper::WSAsyncOper(size_t heapSize, std::promise<HRESULT> *promise)
        try :
        m_promise(promise),
        m_heap(heapSize),
        m_callReturn(WS_S_ASYNC)
    {
        m_future = m_promise->get_future();
    }
    catch (std::future_error &ex)
    {
        std::ostringstream oss;
        oss << "Failed when preparing for asynchronous operation: "
            << StdLibExt::GetDetailsFromFutureError(ex);

        throw AppException<std::runtime_error>(oss.str());
    }
    catch (std::exception &ex)
    {
        std::ostringstream oss;
        oss << "Generic failure when preparing for asynchronous operation: " << ex.what();
        throw AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Gets the context structure used by WWS API calls in asynchronous execution.
    /// </summary>
    /// <returns>
    /// A copy of the context structure to pass as parameter for the asynchronous call.
    /// </returns>
    WS_ASYNC_CONTEXT WSAsyncOper::GetContext()
    {
        WS_ASYNC_CONTEXT ctx;
        ctx.callback = AsyncDoneCallback;
        ctx.callbackState = m_promise;
        return ctx;
    }

    /// <summary>
    /// The HRESULT immediately returned by the call meant to be asynchronous.
    /// </summary>
    /// <param name="hres">The immediately returned HRESULT.</param>
    void WSAsyncOper::SetCallReturn(HRESULT hres)
    {
        m_callReturn = hres;
    }

    /// <summary>
    /// Waits for the result of the asynchronous operation.
    /// </summary>
    void WSAsyncOper::Wait() const
    {
        if (m_callReturn == WS_S_ASYNC)
            m_future.wait();
    }

    /// <summary>
    /// Gets the result of the asynchronous operation.
    /// </summary>
    /// <returns>
    /// The HRESULT of status returned at the end of the asynchronous operation.
    /// </returns>
    HRESULT WSAsyncOper::GetResult()
    {
        if (m_callReturn == WS_S_ASYNC)
        {
            try
            {
                return m_callReturn = m_future.get();
            }
            catch (IAppException &)
            {
                throw; /* just forward transported exceptions which
                        are known to have been already handled */
            }
            catch (std::future_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to retrieve result from asynchronous operation: "
                    << StdLibExt::GetDetailsFromFutureError(ex);

                throw AppException<std::runtime_error>(oss.str());
            }
        }
        else
            return m_callReturn;
    }

}// end of namespace wws
}// end of namespace web
}// end of namespace _3fd
