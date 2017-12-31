#include "stdafx.h"
#include "opencl_impl.h"
#include "callstacktracer.h"
#include "logger.h"

namespace _3fd
{
    namespace opencl
    {
        ///////////////////////////
        // Device Class
        ///////////////////////////

        /// <summary>
        /// Called when a generic device command is completed.
        /// </summary>
        /// <param name="completedEvent">The completed command event.</param>
        /// <param name="eventCommandExecStatus">The command execution status.</param>
        /// <param name="deviceObjPtr">A pointer to the device object.</param>
        /// <remarks>
        /// This will be called asynchronously by OpenCL implementation. Thus, it MUST NOT throw unhandled exceptions.
        /// </remarks>
        void CL_CALLBACK OnGenericCommandCompleted(cl_event completedEvent, cl_int eventCommandExecStatus, void *deviceObjPtr)
        {
            CALL_STACK_TRACE;

            try
            {
                static_cast<Device *> (deviceObjPtr)->m_blockerCommands.Forget(completedEvent);
                
                OPENCL_IMPORT(clReleaseEvent);
                cl_int status = clReleaseEvent(completedEvent);
                openclErrors.LogErrorWhen(status, "OpenCL API: clReleaseEvent", core::Logger::PRIO_CRITICAL);

                // There was abnormal termination:
                if (eventCommandExecStatus != CL_COMPLETE)
                    openclErrors.RaiseExceptionWhen(eventCommandExecStatus, "OpenCL API: The event of a queued device command was abnormally terminated");
            }
            catch (core::IAppException &ex)
            {
                core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when removing a tracked OpenCL event: " << ex.what();
                core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL);
            }
        }

        /// <summary>
        /// Called when a map command is completed.
        /// </summary>
        /// <param name="completedEvent">The completed command event.</param>
        /// <param name="eventCommandExecStatus">The command execution status.</param>
        /// <param name="args">The arguments as layed out by <see cref="OnMapCommandCompletedArgs"/>.</param>
        /// <remarks>
        /// This will be called asynchronously by OpenCL implementation. Thus, it MUST NOT throw unhandled exceptions.
        /// </remarks>
        void CL_CALLBACK OnMapCommandCompleted(cl_event completedEvent, cl_int eventCommandExecStatus, void *args)
        {
            CALL_STACK_TRACE;

            try
            {
                std::unique_ptr<Device::OnMapCommandCompletedArgs> typedArgs(
                    static_cast<Device::OnMapCommandCompletedArgs *> (args)
                );

                typedArgs->device.m_blockerCommands.Forget(completedEvent);

                OPENCL_IMPORT(clReleaseEvent);
                cl_int status = clReleaseEvent(completedEvent);
                openclErrors.LogErrorWhen(status, "OpenCL API: clReleaseEvent", core::Logger::PRIO_CRITICAL);

                // Invoke callback if command executed successfully:
                if (eventCommandExecStatus == CL_COMPLETE)
                {
                    typedArgs->callback(typedArgs->mappedAddr, typedArgs->nBytes);
                    typedArgs->callbackDoneEvent.SetStatus(CL_COMPLETE);
                }
                else // There was abnormal termination:
                    openclErrors.RaiseExceptionWhen(eventCommandExecStatus, "OpenCL API: The event of a queued map command was abnormally terminated");
            }
            catch (core::IAppException &ex)
            {
                core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when executing callback after OpenCL map command: " << ex.what();
                core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL);
            }
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="Device"/> class.
        /// </summary>
        /// <param name="device">The device.</param>
        /// <param name="context">The context.</param>
        /// <param name="properties">The properties for the command queue creation.</param>
        Device::Device(
            cl_device_id device,
            cl_context context,
            cl_command_queue_properties properties)
        try :
            m_device(device),
            m_context(nullptr),
            m_commandQueue(nullptr),
            m_blockerCommands(*this),
            m_oooExecEnabled(properties & CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE)
        {
            CALL_STACK_TRACE;

            OPENCL_IMPORT(clRetainContext);
            cl_int status = clRetainContext(context);
            openclErrors.RaiseExceptionWhen(status, "OpenCL API: clRetainContext");

            m_context = context; // assign context only after successfully retaining it

            OPENCL_IMPORT(clCreateCommandQueue);
            m_commandQueue = clCreateCommandQueue(
                context,
                device,
                properties,
                &status);

            openclErrors.RaiseExceptionWhen(status, "OpenCL API: clCreateCommandQueue");
        }
        catch (core::IAppException &)
        {
            if (m_context != nullptr)
            {
                CALL_STACK_TRACE;
                OPENCL_IMPORT(clReleaseContext);
                cl_int status = clReleaseContext(m_context);
                openclErrors.LogErrorWhen(status, "OpenCL API: clReleaseContext", core::Logger::PRIO_ERROR);
            }
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="Device"/> class.
        /// </summary>
        Device::~Device()
        {
            CALL_STACK_TRACE;

            try
            {
                if (m_commandQueue != nullptr)
                {
                    OPENCL_IMPORT(clReleaseCommandQueue);
                    cl_int status = clReleaseCommandQueue(m_commandQueue);
                    openclErrors.LogErrorWhen(status, "OpenCL API: clReleaseCommandQueue", core::Logger::PRIO_ERROR);
                }

                if (m_context != nullptr)
                {
                    OPENCL_IMPORT(clReleaseContext);
                    cl_int status = clReleaseContext(m_context);
                    openclErrors.LogErrorWhen(status, "OpenCL API: clReleaseContext", core::Logger::PRIO_ERROR);
                }
            }
            catch (core::IAppException &ex)
            {
                core::Logger::Write(ex, core::Logger::PRIO_ERROR);
            }
        }

        /// <summary>
        /// Gets information about the OpenCL device.
        /// </summary>
        /// <param name="paramCode">The parameter code for the requested information.</param>
        /// <param name="param">The parameter where to save the requested information.</param>
        void Device::GetDeviceInfo(cl_device_info paramCode, GenericParam &param) const
        {
            CALL_STACK_TRACE;
            GetDeviceInfoImpl(m_device, paramCode, param);
        }

        /// <summary>
        /// Helper that implements how to get information about an OpenCL device.
        /// </summary>
        /// <param name="device">The OpenCL device id.</param>
        /// <param name="paramCode">The parameter code for the requested information.</param>
        /// <param name="param">The parameter where to save the requested information.</param>
        void GetDeviceInfoImpl(cl_device_id device, cl_device_info paramCode, GenericParam &param)
        {
            OPENCL_IMPORT(clGetDeviceInfo);
            cl_int status = clGetDeviceInfo(
                device,
                paramCode,
                param.size,
                param.value,
                &param.sizeRet);

            openclErrors.RaiseExceptionWhen(status, "OpenCL API: clGetDeviceInfo");
        }

        /// <summary>
        /// Flushes the command queue sending all the queued commands to execution.
        /// </summary>
        void Device::FlushCommandQueue()
        {
            CALL_STACK_TRACE;
            OPENCL_IMPORT(clFlush);
            cl_int status = clFlush(m_commandQueue);
            openclErrors.RaiseExceptionWhen(status, "OpenCL API: clFlush");
        }

        /// <summary>
        /// Enqueues a command to asynchronously fill a buffer.
        /// </summary>
        /// <param name="buffer">The buffer object to be filled.</param>
        /// <param name="offset">The offset after which the buffer memory will be filled. It must be a multiple of the pattern size.</param>
        /// <param name="pattern">The amount of pattern repetitions to fill the buffer with.</param>
        /// <param name="pattern">The pattern content filling the buffer.</param>
        /// <returns>An <see cref="AsyncAction"/> object, that can be used to await for the command completion.</returns>
        AsyncAction Device::EnqueueFillBufferAsync(
            Buffer &buffer,
            size_t offset,
            size_t patternReps,
            GenericParam pattern)
        {
            CALL_STACK_TRACE;

            try
            {
                cl_int status;
                OPENCL_IMPORT(clEnqueueFillBuffer);

                if (m_oooExecEnabled)
                {/* If the command queue has Out-Of-Order execution enabled, there is no guarantee the memory resources will be read 
                    and written in the order specified by the client, plus the risk of unsafe concurrent access. For this reason
                    the command tracker is employed to figure out what events to await before a command makes use of a resource. */

                    cl_event eventHandle;
                    std::vector<CommandEvent> blockerEvents;

                    // get the events from previous commands that read from the buffer to be written
                    m_blockerCommands.GetDistinct(&buffer, MemResourceUse::Input, blockerEvents);

                    status = clEnqueueFillBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        pattern.value,
                        pattern.size,
                        offset,
                        patternReps * pattern.size,
                        blockerEvents.size(),
                        blockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (blockerEvents.data()),
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueFillBuffer");

                    /* Create <see cref="AsyncAction"/> object now, so its ctor will immediately retain the event object. Had 
                    the callback for event completion been assigned, if the command completes quickly enough, any attempt to 
                    retain the event has risk of failure, because the completion callback might have already released the same 
                    event object in a parallel thread, hence leading to error. */
                    AsyncAction action(eventHandle, true);

                    // keep track of the resources blocked because of this enqueued command
                    CommandEvent cmdEvent(eventHandle);
                    m_blockerCommands.Remember(&buffer, MemResourceUse::Output, cmdEvent);
                    cmdEvent.SetCallback(CL_COMPLETE, OnGenericCommandCompleted, this);

                    FlushCommandQueue();
                    return std::move(action);
                }
                else
                {
                    cl_event eventHandle;
                    status = clEnqueueFillBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        pattern.value,
                        pattern.size,
                        offset,
                        patternReps * pattern.size,
                        0, nullptr, 
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueFillBuffer");

                    FlushCommandQueue();
                    return AsyncAction(eventHandle, false);
                }
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to enqueue fill command for OpenCL buffer: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when enqueuing fill command for OpenCL buffer: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Enqueues a read operation from the buffer and to the host memory.
        /// If the memory address in question is blocked, wait for it to be released.
        /// </summary>
        /// <param name="buffer">The buffer object.</param>
        /// <param name="offset">An offset for the memory to transfer.</param>
        /// <param name="nBytes">The number of bytes to transfer.</param>
        /// <param name="ptr">The address of the host memory location.</param>
        void Device::EnqueueReadBuffer(Buffer &buffer, size_t offset, size_t nBytes, void *ptr)
        {
            CALL_STACK_TRACE;

            try
            {
                OPENCL_IMPORT(clEnqueueReadBuffer);

                if (m_oooExecEnabled)
                {/* If the command queue has Out-Of-Order execution enabled, there is no guarantee the memory resources will be read 
                    and written in the order specified by the client, plus the risk of unsafe concurrent access. For this reason
                    the command tracker is employed to figure out what events to await before a command makes use of a resource. */

                    // smart refences to events prevent the from being destroyed when outside the critical section
                    std::vector<CommandEvent> blockerEvents;

                    // get the events from previous commands that write to the buffer to be read
                    m_blockerCommands.GetDistinct(&buffer, MemResourceUse::Output, blockerEvents);

                    // get the events from previous commands that read from the host memory to be written
                    m_blockerCommands.GetDistinct(ptr, MemResourceUse::Input, blockerEvents);

                    cl_int status = clEnqueueReadBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_TRUE,
                        offset,
                        nBytes,
                        ptr,
                        blockerEvents.size(),
                        blockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (blockerEvents.data()),
                        nullptr);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueReadBuffer");
                }
                else
                {
                    cl_int status = clEnqueueReadBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_TRUE,
                        offset,
                        nBytes,
                        ptr,
                        0,
                        nullptr,
                        nullptr);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueReadBuffer");
                }
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to enqueue synchronous read command for OpenCL buffer: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when enqueuing synchronous read command for OpenCL buffer: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Enqueues an asynchronous read operation from the buffer and to the host memory.
        /// If the memory address in question is blocked, wait for it to be released.
        /// </summary>
        /// <param name="buffer">The buffer object.</param>
        /// <param name="offset">An offset for the memory to transfer.</param>
        /// <param name="nBytes">The number of bytes to transfer.</param>
        /// <param name="ptr">The address of the host memory location.</param>
        /// <returns>An <see cref="AsyncAction"/> object, that can be used to await for the command completion.</returns>
        AsyncAction Device::EnqueueReadBufferAsync(Buffer &buffer, size_t offset, size_t nBytes, void *ptr)
        {
            CALL_STACK_TRACE;

            try
            {
                OPENCL_IMPORT(clEnqueueReadBuffer);

                if (m_oooExecEnabled)
                {/* If the command queue has Out-Of-Order execution enabled, there is no guarantee the memory resources will be read 
                    and written in the order specified by the client, plus the risk of unsafe concurrent access. For this reason
                    the command tracker is employed to figure out what events to await before a command makes use of a resource. */

                    cl_event eventHandle;
                    std::vector<CommandEvent> blockerEvents;

                    // get the events from previous commands that write to the buffer to be read
                    m_blockerCommands.GetDistinct(&buffer, MemResourceUse::Output, blockerEvents);

                    // get the events from previous commands that read from the host memory to be written
                    m_blockerCommands.GetDistinct(ptr, MemResourceUse::Input, blockerEvents);

                    cl_int status = clEnqueueReadBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_FALSE,
                        offset,
                        nBytes,
                        ptr,
                        blockerEvents.size(),
                        blockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (blockerEvents.data()),
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueReadBuffer");

                    /* Create <see cref="AsyncAction"/> object now, so its ctor will immediately retain the event object. Had
                    the callback for event completion been assigned, if the command completes quickly enough, any attempt to 
                    retain the event has risk of failure, because the completion callback might have already released the same 
                    event object in a parallel thread, hence leading to error. */
                    AsyncAction action(eventHandle, true);

                    // Keep track of the resources blocked because of this enqueued command:
                    CommandEvent cmdEvent(eventHandle);
                    m_blockerCommands.Remember(&buffer, MemResourceUse::Input, cmdEvent);
                    m_blockerCommands.Remember(ptr, MemResourceUse::Output, cmdEvent);
                    cmdEvent.SetCallback(CL_COMPLETE, OnGenericCommandCompleted, this);

                    FlushCommandQueue();
                    return std::move(action);
                }
                else
                {
                    cl_event eventHandle;
                    cl_int status = clEnqueueReadBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_FALSE,
                        offset,
                        nBytes,
                        ptr,
                        0, nullptr,
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueReadBuffer");

                    FlushCommandQueue();
                    return AsyncAction(eventHandle, false);
                }
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to enqueue asynchronous read command for OpenCL buffer: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when enqueuing asynchronous read command for OpenCL buffer: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Enqueues a write operation to the buffer and from the host memory.
        /// If the memory address in question is blocked, it waits for it to be released.
        /// </summary>
        /// <param name="buffer">The buffer object.</param>
        /// <param name="offset">An offset for the memory to transfer.</param>
        /// <param name="nBytes">The number of bytes to transfer.</param>
        /// <param name="ptr">The address of the host memory location.</param>
        void Device::EnqueueWriteBuffer(Buffer &buffer, size_t offset, size_t nBytes, void *ptr)
        {
            CALL_STACK_TRACE;

            try
            {
                OPENCL_IMPORT(clEnqueueWriteBuffer);

                if (m_oooExecEnabled)
                {/* If the command queue has Out-Of-Order execution enabled, there is no guarantee the memory resources will be read 
                    and written in the order specified by the client, plus the risk of unsafe concurrent access. For this reason 
                    the command tracker is employed to figure out what events to await before a command makes use of a resource. */

                    // smart refences to events prevent them from being destroyed when outside the critical section
                    std::vector<CommandEvent> blockerEvents;

                    // get the events from previous commands that write to the host memory to be read
                    m_blockerCommands.GetDistinct(ptr, MemResourceUse::Output, blockerEvents);

                    // get the events from previous commands that read from the buffer to be written
                    m_blockerCommands.GetDistinct(&buffer, MemResourceUse::Input, blockerEvents);

                    cl_int status = clEnqueueWriteBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_TRUE,
                        offset,
                        nBytes,
                        ptr,
                        blockerEvents.size(),
                        blockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (blockerEvents.data()),
                        nullptr);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueWriteBuffer");
                }
                else
                {
                    cl_int status = clEnqueueWriteBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_TRUE,
                        offset,
                        nBytes,
                        ptr,
                        0, nullptr,
                        nullptr);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueWriteBuffer");
                }
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to enqueue synchronous write command for OpenCL buffer: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when enqueuing synchronous write command for OpenCL buffer: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Enqueues an asynchronous write operation to the buffer and from the host memory.
        /// If the memory address in question is blocked, it waits for it to be released.
        /// </summary>
        /// <param name="buffer">The buffer object.</param>
        /// <param name="offset">An offset for the memory to transfer.</param>
        /// <param name="nBytes">The number of bytes to transfer.</param>
        /// <param name="ptr">The address of the host memory location.</param>
        /// <returns>An <see cref="AsyncAction"/> object, that can be used to await for the command completion.</returns>
        AsyncAction Device::EnqueueWriteBufferAsync(Buffer &buffer, size_t offset, size_t nBytes, void *ptr)
        {
            CALL_STACK_TRACE;

            try
            {
                OPENCL_IMPORT(clEnqueueWriteBuffer);

                if (m_oooExecEnabled)
                {/* If the command queue has Out-Of-Order execution enabled, there is no guarantee the memory resources will be read 
                    and written in the order specified by the client, plus the risk of unsafe concurrent access. For this reason
                    the command tracker is employed to figure out what events to await before a command makes use of a resource. */

                    cl_event eventHandle;
                    std::vector<CommandEvent> blockerEvents;

                    // get the events from previous commands that write to the host memory to be read
                    m_blockerCommands.GetDistinct(ptr, MemResourceUse::Output, blockerEvents);

                    // get the events from previous commands that read from the buffer to be written
                    m_blockerCommands.GetDistinct(&buffer, MemResourceUse::Input, blockerEvents);

                    cl_int status = clEnqueueWriteBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_FALSE,
                        offset,
                        nBytes,
                        ptr,
                        blockerEvents.size(),
                        blockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (blockerEvents.data()),
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueWriteBuffer");

                    /* Create <see cref="AsyncAction"/> object now, so its ctor will immediately retain the event object. Had
                    the callback for event completion been assigned, if the command completes quickly enough, any attempt to 
                    retain the event has risk of failure, because the completion callback might have already released the same 
                    event object in a parallel thread, hence leading to error. */
                    AsyncAction action(eventHandle, true);

                    // Keep track of the resources blocked because of this enqueued command:
                    CommandEvent cmdEvent(eventHandle);
                    m_blockerCommands.Remember(ptr, MemResourceUse::Input, cmdEvent);
                    m_blockerCommands.Remember(&buffer, MemResourceUse::Output, cmdEvent);
                    cmdEvent.SetCallback(CL_COMPLETE, OnGenericCommandCompleted, this);

                    FlushCommandQueue();
                    return std::move(action);
                }
                else
                {
                    cl_event eventHandle;
                    cl_int status = clEnqueueWriteBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_FALSE,
                        offset,
                        nBytes,
                        ptr,
                        0, nullptr,
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueWriteBuffer");

                    FlushCommandQueue();
                    return AsyncAction(eventHandle, false);
                }
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to enqueue asynchronous write command for OpenCL buffer: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when enqueuing asynchronous write command for OpenCL buffer: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Enqueues a command to asynchronously copy data from one buffer to another.
        /// </summary>
        /// <param name="from">The buffer from which data will be read.</param>
        /// <param name="to">The buffer to which data will be written.</param>
        /// <param name="offsetFrom">The offset (in bytes) after which the source buffer will be read from.</param>
        /// <param name="offsetTo">The offset (in bytes) after which the destination buffer will be written to.</param>
        /// <param name="nBytes">The amount in bytes of data to transfer.</param>
        /// <returns>An <see cref="AsyncAction"/> object, that can be used to await for the command completion.</returns>
        AsyncAction Device::EnqueueCopyBufferAsync(Buffer &from, Buffer &to,
                                                   size_t offsetFrom, size_t offsetTo,
                                                   size_t nBytes)
        {
            CALL_STACK_TRACE;

            try
            {
                OPENCL_IMPORT(clEnqueueCopyBuffer);

                if (m_oooExecEnabled)
                {/*    If the command queue has Out-Of-Order execution enabled, there is no guarantee the memory resources will be read 
                    and written in the order specified by the client, plus the risk of unsafe concurrent access. For this reason 
                    the command tracker is employed to figure out what events to await before a command makes use of a resource. */

                    cl_event eventHandle;
                    std::vector<CommandEvent> blockerEvents;

                    // get the events from previous commands that write to the buffer to be read
                    m_blockerCommands.GetDistinct(&from, MemResourceUse::Output, blockerEvents);

                    // get the events from previous commands that read from the buffer to be written
                    m_blockerCommands.GetDistinct(&to, MemResourceUse::Input, blockerEvents);

                    cl_int status = clEnqueueCopyBuffer(
                        m_commandQueue,
                        from.GetHandle(),
                        to.GetHandle(),
                        offsetFrom,
                        offsetTo,
                        nBytes,
                        blockerEvents.size(),
                        blockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (blockerEvents.data()),
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueCopyBuffer");

                    /* Create <see cref="AsyncAction"/> object now, so its ctor will immediately retain the event object. Had
                    the callback for event completion been assigned, if the command completes quickly enough, any attempt to 
                    retain the event has risk of failure, because the completion callback might have already released the same 
                    event object in a parallel thread, hence leading to error. */
                    AsyncAction action(eventHandle, true);

                    // Keep track of the resources locked because of this enqueued command:
                    CommandEvent cmdEvent(eventHandle);
                    m_blockerCommands.Remember(&from, MemResourceUse::Input, cmdEvent);
                    m_blockerCommands.Remember(&to, MemResourceUse::Output, cmdEvent);
                    cmdEvent.SetCallback(CL_COMPLETE, OnGenericCommandCompleted, this);

                    FlushCommandQueue();
                    return std::move(action);
                }
                else
                {
                    cl_event eventHandle;
                    cl_int status = clEnqueueCopyBuffer(
                        m_commandQueue,
                        from.GetHandle(),
                        to.GetHandle(),
                        offsetFrom,
                        offsetTo,
                        nBytes,
                        0, nullptr, 
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueCopyBuffer");

                    FlushCommandQueue();
                    return AsyncAction(eventHandle, false);
                }
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to enqueue copy command for OpenCL buffer: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when enqueuing copy command for OpenCL buffer: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Enqueues a map command for the given buffer.
        /// </summary>
        /// <param name="buffer">The buffer object to map.</param>
        /// <param name="how">How to map the buffer, whether for read, write or read & write.</param>
        /// <param name="offset">An offset for the memory to transfer.</param>
        /// <param name="nBytes">The number of bytes to transfer.</param>
        /// <param name="callback">
        /// The callback to invoke, which will receive the mapped memory address and its size in bytes.
        /// </param>
        void Device::EnqueueMapBuffer(
            Buffer &buffer, 
            MemResourceUse how, 
            size_t offset, 
            size_t nBytes,
            const std::function<void(void *, size_t)> &callback)
        {
            CALL_STACK_TRACE;

            try
            {
                // Select the map mode:
                cl_map_flags mapFlags;
                switch (how)
                {
                case MemResourceUse::Input:
                    mapFlags = CL_MAP_READ;
                    break;
                case MemResourceUse::Output:
                    mapFlags = CL_MAP_WRITE_INVALIDATE_REGION;
                    break;
                case MemResourceUse::InputAndOutput:
                    mapFlags = CL_MAP_WRITE;
                    break;
                default:
                    break;
                }

                cl_int status;
                OPENCL_IMPORT(clEnqueueUnmapMemObject);
                OPENCL_IMPORT(clEnqueueMapBuffer);

                if (m_oooExecEnabled)
                {/* If the command queue has Out-Of-Order execution enabled, there is no guarantee the memory resources will be read 
                    and written in the order specified by the client, plus the risk of unsafe concurrent access. For this reason
                    the command tracker is employed to figure out what events to await before a command makes use of a resource. */

                    // smart refences to events prevent the from being destroyed when outside the critical section
                    std::vector<CommandEvent> mapBlockerEvents, unmapBlockerEvents;

                    /* If the map command is to issue a read operation, then get the events from previous commands that
                    write to the buffer being mapped, but also keep track of the resources blocked because of mapping: */
                    if (how == MemResourceUse::Input || how == MemResourceUse::InputAndOutput)
                        m_blockerCommands.GetDistinct(&buffer, MemResourceUse::Output, mapBlockerEvents);

                    // Map the buffer to CPU visible memory:
                    void *ptr = clEnqueueMapBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_TRUE,
                        mapFlags,
                        offset,
                        nBytes,
                        mapBlockerEvents.size(),
                        mapBlockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (mapBlockerEvents.data()),
                        nullptr,
                        &status);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueMapBuffer");

                    callback(ptr, nBytes); // invoke callback

                    // Unmap the buffer:
                    if (how == MemResourceUse::Output || how == MemResourceUse::InputAndOutput)
                    {/* If the unmap command is to issue a write operation, then get the events from previous commands that 
                        read from the buffer being mapped, but also keep track of the resources blocked because of the unmaping: */
                        m_blockerCommands.GetDistinct(&buffer, MemResourceUse::Input, unmapBlockerEvents);

                        cl_event cmdUnmapEvent;
                        status = clEnqueueUnmapMemObject(
                            m_commandQueue,
                            buffer.GetHandle(),
                            ptr,
                            unmapBlockerEvents.size(),
                            unmapBlockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (unmapBlockerEvents.data()),
                            &cmdUnmapEvent);

                        openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueUnmapMemObject");
                        FlushCommandQueue();

                        // Because this method is synchronous and the unmapping will issue a write operation, wait for it:
                        AsyncAction(cmdUnmapEvent, false).Await();
                    }
                    else
                    {
                        // This command will not actually write anything, so we do not have to wait for its completion:
                        status = clEnqueueUnmapMemObject(
                            m_commandQueue,
                            buffer.GetHandle(),
                            ptr, 0, nullptr, nullptr);

                        openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueUnmapMemObject");
                        FlushCommandQueue();
                    }
                }
                else
                {
                    // Map the buffer to CPU visible memory:
                    void *ptr = clEnqueueMapBuffer(
                        m_commandQueue,
                        buffer.GetHandle(),
                        CL_TRUE,
                        mapFlags,
                        offset, nBytes,
                        0, nullptr, 
                        nullptr,
                        &status);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueMapBuffer");

                    callback(ptr, nBytes); // invoke callback

                    // Unmap the buffer:
                    if (how == MemResourceUse::Output || how == MemResourceUse::InputAndOutput)
                    {
                        cl_event cmdUnmapEvent;
                        status = clEnqueueUnmapMemObject(
                            m_commandQueue,
                            buffer.GetHandle(),
                            ptr, 
                            0, nullptr, 
                            &cmdUnmapEvent);

                        openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueUnmapMemObject");

                        // Because this method is synchronous and the unmapping will issue a write operation, wait for it:
                        FlushCommandQueue();
                        AsyncAction(cmdUnmapEvent, false).Await();
                    }
                    else
                    {
                        // This command will not actually write anything, so we do not have to wait for its completion:
                        status = clEnqueueUnmapMemObject(
                            m_commandQueue,
                            buffer.GetHandle(),
                            ptr, 
                            0, nullptr, 
                            nullptr);

                        openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueUnmapMemObject");
                        FlushCommandQueue();
                    }
                }
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to synchronously map OpenCL buffer: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when synchronously mapping OpenCL buffer: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        // Implements a safe call to 'clEnqueueMapBuffer'
        static void *safeClEnqueueMapBuffer(
            cl_command_queue cmdQueueHandle,
            Buffer &buffer,
            cl_bool isBlocking,
            cl_map_flags mapFlags,
            size_t offset,
            size_t size,
            std::vector<CommandEvent> *evWaitList,
            cl_event *eventHandle)
        {
            CALL_STACK_TRACE;

            OPENCL_IMPORT(clEnqueueMapBuffer);

            _ASSERTE(eventHandle != nullptr);
            auto evHndValBefore = *eventHandle;

            cl_int status;
            void *ptr = clEnqueueMapBuffer(
                cmdQueueHandle,
                buffer.GetHandle(),
                isBlocking,
                mapFlags,
                offset,
                size,
                evWaitList != nullptr ? evWaitList->size() : 0,
                (evWaitList != nullptr && !evWaitList->empty()) ? reinterpret_cast<cl_event *> (evWaitList->data()) : nullptr,
                eventHandle,
                &status);

            /* Intel implementation of 'clEnqueueMapBuffer' has shown to produce an uncompliant
            result for this call, where an event is not allocated and possibly nothing happens.
            The runtime check below will avoid memory access violation when trying to use the
            handle for the event hereby created: */
            auto evHndValAfter = *eventHandle;
            if (evHndValBefore == evHndValAfter)
            {
                throw core::AppException<std::runtime_error>(
                    "Failed to enqueue buffer mapping operation in command queue of OpenCL device: "
                    "uncompliant value of output parameter indicates that OpenCL implementation is "
                    "unreliable or not existent for this feature"
                );
            }

            openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueMapBuffer");

            return ptr;
        }

        /// <summary>
        /// Enqueues an asynchronous map command for the given buffer.
        /// </summary>
        /// <param name="buffer">The buffer object to map.</param>
        /// <param name="how">How to map the buffer, whether for read, write or read & write.</param>
        /// <param name="offset">An offset for the memory to transfer.</param>
        /// <param name="nBytes">The number of bytes to transfer.</param>
        /// <param name="callback">
        /// The callback to invoke, which will receive the mapped memory address and its size in bytes.
        /// </param>
        /// <returns>An <see cref="AsyncAction"/> object, that can be used to await for the command completion.</returns>
        AsyncAction Device::EnqueueMapBufferAsync(
            Buffer &buffer,
            MemResourceUse how,
            size_t offset,
            size_t nBytes,
            const std::function<void(void *, size_t)> &callback)
        {
            CALL_STACK_TRACE;

            try
            {
                // Select the map mode:
                cl_map_flags mapFlags;
                switch (how)
                {
                case MemResourceUse::Input:
                    mapFlags = CL_MAP_READ;
                    break;
                case MemResourceUse::Output:
                    mapFlags = CL_MAP_WRITE_INVALIDATE_REGION;
                    break;
                case MemResourceUse::InputAndOutput:
                    mapFlags = CL_MAP_WRITE;
                    break;
                default:
                    break;
                }

                cl_int status;
                OPENCL_IMPORT(clEnqueueUnmapMemObject);

                if (m_oooExecEnabled)
                {/* If the command queue has Out-Of-Order execution enabled, there is no guarantee the memory resources will be read 
                    and written in the order specified by the client, plus the risk of unsafe concurrent access. For this reason
                    the command tracker is employed to figure out what events to await before a command makes use of a resource. */
                    
                    cl_event cmdMapEventHandle;
                    std::vector<CommandEvent> mapBlockerEvents;

                    /* If the map command is to issue a read operation, then get the events 
                    from previous commands that write to the buffer being mapped: */
                    if (how == MemResourceUse::Input || how == MemResourceUse::InputAndOutput)
                        m_blockerCommands.GetDistinct(&buffer, MemResourceUse::Output, mapBlockerEvents);

                    // Map the buffer to CPU visible memory:
                    void *ptr = safeClEnqueueMapBuffer(
                        m_commandQueue,
                        buffer,
                        CL_FALSE,
                        mapFlags,
                        offset,
                        nBytes,
                        &mapBlockerEvents,
                        &cmdMapEventHandle);

                    // The callback provided by client code is going to be invoked when the map command completes:
                    CommandEvent callbackDoneEvent(m_context);
                    std::unique_ptr<OnMapCommandCompletedArgs> args(
                        new OnMapCommandCompletedArgs(this, callback, ptr, nBytes, callbackDoneEvent)
                    );

                    // Also keep track of the resources blocked because of mapping, if any:
                    CommandEvent cmdMapEvent(cmdMapEventHandle);
                    if (how == MemResourceUse::Input || how == MemResourceUse::InputAndOutput)
                        m_blockerCommands.Remember(&buffer, MemResourceUse::Input, cmdMapEvent);
                    
                    cmdMapEvent.SetCallback(CL_COMPLETE, OnMapCommandCompleted, args.release());
                    FlushCommandQueue();

                    if (how == MemResourceUse::Output || how == MemResourceUse::InputAndOutput)
                    {/* If the unmap command is to issue a write operation, then get the events from previous
                        commands that read from the buffer being mapped, but also keep track of the resources
                        blocked because of the unmaping: */
                        std::vector<CommandEvent> unmapBlockerEvents;
                        unmapBlockerEvents.push_back(callbackDoneEvent); // the unmap command must wait for callback completion
                        m_blockerCommands.GetDistinct(&buffer, MemResourceUse::Input, unmapBlockerEvents);

                        // Unmap the buffer:
                        cl_event cmdUnmapEventHandle;
                        status = clEnqueueUnmapMemObject(
                            m_commandQueue,
                            buffer.GetHandle(),
                            ptr,
                            unmapBlockerEvents.size(),
                            unmapBlockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (unmapBlockerEvents.data()),
                            &cmdUnmapEventHandle);

                        openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueUnmapMemObject");

                        /* Create <see cref="AsyncAction"/> object now, so its ctor will immediately retain the event object. Had
                        the callback for event completion been assigned, if the command completes quickly enough, any attempt to 
                        retain the event has risk of failure, because the completion callback might have already released the same 
                        event object in a parallel thread, hence leading to error. */
                        AsyncAction action(cmdUnmapEventHandle, true);

                        CommandEvent cmdUnmapEvent(cmdUnmapEventHandle);
                        m_blockerCommands.Remember(&buffer, MemResourceUse::Output, cmdUnmapEvent);
                        cmdUnmapEvent.SetCallback(CL_COMPLETE, OnGenericCommandCompleted, this);
                        
                        FlushCommandQueue();
                        return std::move(action);
                    }
                    else
                    {
                        // This command will not actually write anything, so we do not have to wait for its completion:
                        cl_event blockerEvHnds[] = { callbackDoneEvent.GetHandle() };
                        status = clEnqueueUnmapMemObject(
                            m_commandQueue,
                            buffer.GetHandle(),
                            ptr, 
                            1, blockerEvHnds,
                            nullptr);

                        openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueUnmapMemObject");

                        FlushCommandQueue();
                        return AsyncAction(callbackDoneEvent);
                    }
                }
                else
                {
                    // Map the buffer to CPU visible memory:
                    cl_event cmdMapEventHandle;
                    void *ptr = safeClEnqueueMapBuffer(
                        m_commandQueue,
                        buffer,
                        CL_FALSE,
                        mapFlags,
                        offset, nBytes,
                        nullptr, 
                        &cmdMapEventHandle);

                    // The callback provided by client code is going to be invoked when the map command completes:
                    CommandEvent cmdMapEvent(cmdMapEventHandle);
                    CommandEvent callbackDoneEvent(m_context);
                    std::unique_ptr<OnMapCommandCompletedArgs> args(
                        new OnMapCommandCompletedArgs(this, callback, ptr, nBytes, callbackDoneEvent)
                    );

                    cmdMapEvent.SetCallback(CL_COMPLETE, OnMapCommandCompleted, args.release());
                    FlushCommandQueue();

                    // the unmap must wait for client callback completion
                    cl_event unmapBlockerEvHnds[] = { callbackDoneEvent.GetHandle() };

                    if (how == MemResourceUse::Output || how == MemResourceUse::InputAndOutput)
                    {// If the unmap command is to issue a write operation, the caller must await for its completion:
                        cl_event cmdUnmapEventHandle;
                        status = clEnqueueUnmapMemObject(
                            m_commandQueue,
                            buffer.GetHandle(),
                            ptr,
                            1, unmapBlockerEvHnds,
                            &cmdUnmapEventHandle);

                        openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueUnmapMemObject");

                        FlushCommandQueue();
                        return AsyncAction(cmdUnmapEventHandle, false);
                    }
                    else
                    {
                        // This command will not actually write anything, so we do not have to wait for its completion:
                        status = clEnqueueUnmapMemObject(
                            m_commandQueue,
                            buffer.GetHandle(),
                            ptr,
                            1, unmapBlockerEvHnds,
                            nullptr);

                        openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueUnmapMemObject");

                        FlushCommandQueue();
                        return AsyncAction(callbackDoneEvent);
                    }
                }
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to enqueue asynchronous map command for OpenCL buffer: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when enqueuing asynchronous map command for OpenCL buffer: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Implements how to enqueue a kernel for execution in the device.
        /// </summary>
        /// <param name="kernel">The kernel to execute.</param>
        /// <param name="workDims">The number of working dimensions. Normally ranges from 1 to 3.</param>
        /// <param name="globalWorkOffset">For each dimension, the offsets used to calculate the global ID's of the work items.</param>
        /// <param name="globalWorkSize">For each dimension, the number of work items that will execute the kernel.</param>
        /// <param name="localWorkSize">The size of the work-group for each dimension.</param>
        AsyncAction Device::EnqueueNDRangeKernelAsyncImpl(
            Kernel &kernel,
            cl_uint workDims,
            const size_t globalWorkOffset[],
            const size_t globalWorkSize[],
            const size_t localWorkSize[])
        {
            CALL_STACK_TRACE;

            try
            {
                OPENCL_IMPORT(clEnqueueNDRangeKernel);

                if (m_oooExecEnabled)
                {
                    std::vector<CommandEvent> blockerEvents;

                    auto kernelArgs = kernel.GetArguments();

                    // Get the events for all commands blocking the access to the kernel parameters:
                    for (auto &arg : kernelArgs)
                    {
                        switch (arg.direction)
                        {
                        case MemResourceUse::Input:
                            m_blockerCommands.GetDistinct(arg.memObject, MemResourceUse::Output, blockerEvents);
                            break;
                        case MemResourceUse::Output:
                            m_blockerCommands.GetDistinct(arg.memObject, MemResourceUse::Input, blockerEvents);
                            break;
                        case MemResourceUse::InputAndOutput:
                            m_blockerCommands.GetDistinct(arg.memObject, MemResourceUse::InputAndOutput, blockerEvents);
                            break;
                        }
                    }

                    // Enqueue async execution of the kernel:
                    cl_event eventHandle;
                    cl_int status = clEnqueueNDRangeKernel(
                        m_commandQueue,
                        kernel.GetHandle(),
                        workDims,
                        globalWorkOffset,
                        globalWorkSize,
                        localWorkSize,
                        blockerEvents.size(),
                        blockerEvents.empty() ? nullptr : reinterpret_cast<cl_event *> (blockerEvents.data()),
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueNDRangeKernel");

                    /* Create <see cref="AsyncAction"/> object now, so its ctor will immediately retain the event object. After
                    the callback for event completion assigned, if the command completes quickly enough, any attempt to retain
                    the event has risk of failure, because the completion callback might have already released the same event
                    object in a parallel thread, hence leading to error. */
                    AsyncAction action(eventHandle, true);

                    // Keep track of the resources locked by this kernel execution:
                    CommandEvent cmdEvent(eventHandle);
                    for (auto &arg : kernelArgs)
                        m_blockerCommands.Remember(arg.memObject, arg.direction, cmdEvent);

                    cmdEvent.SetCallback(CL_COMPLETE, OnGenericCommandCompleted, this);
                    FlushCommandQueue();
                    return std::move(action);
                }
                else
                {
                    cl_event eventHandle;
                    cl_int status = clEnqueueNDRangeKernel(
                        m_commandQueue,
                        kernel.GetHandle(),
                        workDims,
                        globalWorkOffset,
                        globalWorkSize,
                        localWorkSize,
                        0, nullptr,
                        &eventHandle);

                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clEnqueueNDRangeKernel");

                    FlushCommandQueue();
                    return AsyncAction(eventHandle, false);
                }
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to enqueue asynchronous OpenCl kernel execution: " << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when enqueuing asynchronous OpenCL kernel execution: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Blocks until all previously queued commands have completed.
        /// </summary>
        void Device::Finish()
        {
            CALL_STACK_TRACE;
            OPENCL_IMPORT(clFinish);
            cl_int status = clFinish(m_commandQueue);
            openclErrors.RaiseExceptionWhen(status, "OpenCL API: clFinish");
        }

    }// end of namespace opencl
}// end of namespace _3fd
