#ifndef OPENCL_H
#define OPENCL_H

#include "base.h"
#include "configuration.h"
#include "logger.h"
#include "CL/cl.h"
#include <set>
#include <map>
#include <deque>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <cstdint>
#include <mutex>

namespace _3fd
{
    using std::string;

    namespace opencl
    {
        /// <summary>
        /// Enumerates the types of use for memory resources.
        /// </summary>
        enum class MemResourceUse : uint8_t
        {
            Input, 
            Output, 
            InputAndOutput
        };

        /// <summary>
        /// Represents a parameter for a generic type.
        /// </summary>
        struct GenericParam
        {
            size_t size;
            void *value;
            size_t sizeRet;

            template <typename ValueType> 
            void Set(ValueType &p_value)
            {
                size = sizeof(p_value);
                value = static_cast<void *> (&p_value);
                sizeRet = 0;
            }

            /// <summary>
            /// Sets the specified p_value.
            /// </summary>
            /// <param name="p_value">A pointer to the parameter variable/array.</param>
            /// <param name="p_size">The size in bytes of the variable/array.</param>
            template <typename ValueType>
            void Set(ValueType *p_value, size_t p_size)
            {
                size = p_size;
                value = static_cast<void *> (p_value);
                sizeRet = 0;
            }
        };

        class Context;

        /// <summary>
        /// Represents the platform.
        /// </summary>
        class Platform
        {
        private:

            cl_platform_id m_platform;

            /// <summary>
            /// Prevents a default instance of the <see cref="Platform"/> class from being created.
            /// </summary>
            /// <param name="platform">The platform.</param>
            Platform(cl_platform_id platform)
                : m_platform(platform) {}

        public:

            ~Platform();

            static void CreatePlatformInstances(std::vector<Platform> &platforms);
 
            void GetPlatformInfo(cl_platform_info paramCode, GenericParam &param) const;
 
            Context CreateContextFromType(cl_device_type deviceType) const;
        };

        /// <summary>
        /// Holds key information regarding an OpenCL device.
        /// </summary>
        struct DeviceInfo : notcopiable
        {
            size_t  hashCode;
            cl_uint vendorId;
            string  vendorName,
                    deviceName,
                    driverVersion;

            DeviceInfo(cl_device_id deviceId);

            /// <summary>
            /// Initializes a new instance of the <see cref="DeviceInfo"/> struct.
            /// </summary>
            DeviceInfo()
                : hashCode(0), vendorId(0) {}

            /// <summary>
            /// Initializes a new instance of the <see cref="DeviceInfo"/> struct using move semantics.
            /// </summary>
            /// <param name="ob">The object whose resources will be stolen.</param>
            DeviceInfo(DeviceInfo &&ob) : 
                hashCode(ob.hashCode),
                vendorId(ob.vendorId),
                vendorName(std::move(ob.vendorName)),
                deviceName(std::move(ob.deviceName)),
                driverVersion(std::move(ob.driverVersion))
            {
                ob.hashCode = 0;
                ob.vendorId = 0;
            }

            void UpdateHashCode();
        };

        class Device;
        class Buffer;
        class Program;

        /// <summary>
        /// Represents a context, which is capable of providing devices, 
        /// memory resources and programs built from source code.
        /// </summary>
        class Context : notcopiable
        {
        private:

            cl_context m_context;
            std::vector<cl_device_id> m_devices; // Has to be a vector because the storage must be continuous

            /// <summary>
            /// Holds key information regarding an OpenCL device, plus a reference to its ID object.
            /// </summary>
            struct DeviceInfo2
            {
                cl_device_id id;
                DeviceInfo info;

                /// <summary>
                /// Initializes a new instance of the <see cref="DeviceInfo2"/> struct.
                /// </summary>
                /// <param name="deviceId">The device identifier.</param>
                DeviceInfo2(cl_device_id deviceId)
                    : id(deviceId), info(deviceId) {}

                /// <summary>
                /// Initializes a new instance of the <see cref="DeviceInfo2"/> struct using move semantics.
                /// </summary>
                /// <param name="ob">The object whose resources will be stolen.</param>
                DeviceInfo2(DeviceInfo2 &&ob)
                    : id(ob.id), info(std::move(ob.info))
                {
                    ob.id = nullptr;
                }
            };

            std::map<size_t, DeviceInfo2> m_devicesInfo;

            void GetContextInfo(cl_context_info paramCode, GenericParam &param) const;

            void DiscoverDevices();

            void GetSourceCode(const string &fileName, std::deque<string> &sourceCodeLines);

            cl_program CreateProgramFromSourceCode(const string &sourceCodeFilePath);

            cl_program CreateProgramWithBinaries(const string &manifestFilePath);

            void CheckBuildStatus(cl_program program);

        public:

            Context(cl_context context);

            /// <summary>
            /// Initializes a new instance of the <see cref="Context"/> class using move semantics.
            /// </summary>
            /// <param name="ob">The object whose resources will be stolen.</param>
            Context(Context &&ob) : 
                m_context(ob.m_context), 
                m_devices(std::move(ob.m_devices))
            {
                ob.m_context = nullptr;
            }

            ~Context();

            /// <summary>
            /// Gets the context handle.
            /// </summary>
            /// <returns>The handle to this OpenCL context.</returns>
            cl_context GetHandle() { return m_context; }
 
            /// <summary>
            /// Gets the number of devices in this context.
            /// </summary>
            /// <returns>How many devices there are currently available in this context.</returns>
            cl_uint GetNumDevices() const { return m_devices.size(); }
 
            std::unique_ptr<Device> GetDevice(cl_uint index, 
                                              cl_command_queue_properties properties) const;
 
            std::unique_ptr<Program> 
            BuildProgramFromSource(const string &sourceCodeFilePath, 
                                   const string &buildOptions);

            std::unique_ptr<Program> 
            BuildProgramWithBinaries(const string &manifestFilePath, 
                                     const string &buildOptions);
 
            Buffer CreateBuffer(size_t nBytes, 
                                cl_mem_flags flags, 
                                void *hostPtr);
        };

        /// <summary>
        /// Wraps an OpenCL event, properly requesting retain/release whenever a copy/destruction takes place.
        /// </summary>
        class CommandEvent
        {
        private:
            //XXXXXXXXXXXXXXXXXXXX ONLY MEMBER XXXXXXXXXXXXXXXXXXXXX
            /* ATTENTION: As long as this is the only member in 
            this class, an array of instances of this class can 
            be safely converted to an array of 'cl_event' handles. 
            The addition of other data members here is forbidden! */
            cl_event m_event;
            //XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
        public:

            CommandEvent(cl_context context);

            CommandEvent(cl_event event);

            CommandEvent(const CommandEvent &ob);

            ~CommandEvent();

            /// <summary>
            /// Initializes a new instance of the <see cref="Event"/> class using move semantics.
            /// </summary>
            /// <param name="ob">The object whose resouces will be stolen.</param>
            CommandEvent(CommandEvent &&ob)
                : m_event(ob.m_event)
            {
                ob.m_event = nullptr;
            }

            typedef void (CL_CALLBACK *CmdEventCallback)(cl_event, cl_int, void *);

            void SetCallback(cl_int cmdExecStatus, CmdEventCallback callback, void *args);

            void SetStatus(cl_int evStatus);

            /// <summary>
            /// Gets the event handle.
            /// </summary>
            cl_event GetHandle() const { return m_event; };
        };

        /// <summary>
        /// A container to keep track of blocker commands in the device queue.
        /// It helps to keep track of what resources are disputed and what are the events which signalize their release.
        /// </summary>
        class CommandTracker : notcopiable
        {
        private:

            Device &m_device;

            /// <summary>
            /// This struct holds information about a memory resource blocked by a command and its corresponding event.
            /// </summary>
            struct Command
            {
                void *memResource; // The memory resource blocked by the command
                MemResourceUse resourceUse;
                CommandEvent event;

                Command(void *p_memResource, MemResourceUse p_resourceUse, const CommandEvent &p_event)
                    : memResource(p_memResource), resourceUse(p_resourceUse), event(p_event) {}
            };

            typedef std::multimap<void *, std::shared_ptr<Command>> MmapOfCmdsOBRs;
            typedef std::multimap<cl_event, std::shared_ptr<Command>> MmapOfCmdsOBEvs;

            MmapOfCmdsOBEvs    m_cmdsByEvent;
            MmapOfCmdsOBRs    m_cmdsByRdResource;
            MmapOfCmdsOBRs    m_cmdsByWrResource;

            mutable std::mutex m_cmdsAccessMutex;

        public:

            CommandTracker(Device &device);

            ~CommandTracker();

            void GetDistinct(
                void *memResource,
                MemResourceUse resourceUse,
                std::vector<CommandEvent> &blockerEvents) const;

            void Remember(
                void *memResource, 
                MemResourceUse resourceUse, 
                const CommandEvent &cmdEvent);

            void Forget(cl_event completedEvent);
        };

        /// <summary>
        /// Represents an asynchronous OpenCL command action.
        /// </summary>
        class AsyncAction : notcopiable
        {
        private:

            cl_event m_eventHandle;

        public:

            AsyncAction(CommandEvent cmdEvent);

            AsyncAction(cl_event eventHandle, bool evResFreeOnCompletion);

            ~AsyncAction();

            /// <summary>
            /// Initializes a new instance of the <see cref="AsyncAction"/> class using move semantics.
            /// </summary>
            /// <param name="ob">The object whose resources will be stolen.</param>
            AsyncAction(AsyncAction &&ob)
                : m_eventHandle(ob.m_eventHandle)
            {
                ob.m_eventHandle = nullptr;
            }

            /// <summary>
            /// Overloads assignment operation using move semantics.
            /// </summary>
            /// <param name="ob">The object whose resources will be stole from.</param>
            /// <returns>A reference to this object.</returns>
            AsyncAction &operator =(AsyncAction &&ob)
            {
                if (&ob != this)
                {
                    Detach();
                    m_eventHandle = ob.m_eventHandle;
                    ob.m_eventHandle = nullptr;
                }

                return *this;
            }

            void Detach();
            
            void Await();
        };

        class Kernel;

        /// <summary>
        /// This class represents a device, but it encapsulates both device and its respective command queue.
        /// </summary>
        /// <remarks>
        /// An OpenCL device is best used when saturated with commands by several concurrent threads, each one 
        /// making use of an exclusive command queue. That means each thread should have its own instance of 
        /// this class, rather than one instance being shared by several threads. This is why this implementation 
        /// is NOT THREAD SAFE.
        /// </remarks>
        class Device : notcopiable
        {
        private:

            const bool       m_oooExecEnabled;
            cl_device_id     m_device;
            cl_context       m_context;
            cl_command_queue m_commandQueue;

            /// <summary>
            /// Blocking operations in OpenCL do not lock access to the blocked memory addresses in
            /// question, so it must be enforced by the framework.The command tracker is used to keep
            /// track of those blocking events.
            /// </summary>
            CommandTracker m_blockerCommands;

            void FlushCommandQueue();

            AsyncAction EnqueueNDRangeKernelAsyncImpl(
                Kernel &kernel,
                cl_uint workDims, 
                const size_t globalWorkOffset[], 
                const size_t globalWorkSize[], 
                const size_t localWorkSize[]);

            /* The procedures below take core of forgetting an event (after being released) that refers 
            to a blocker device command. They are friend functions only because 'clSetEventCallback' 
            requires them to be outside a class, otherwise it does not accept the function pointer. */
            
            friend void CL_CALLBACK
            OnGenericCommandCompleted(cl_event completedEvent,
                                     cl_int eventCommandExecStatus,
                                     void *deviceObjAddr);

            /// <summary>
            /// Arguments for <see cref="OnMapCommandCompleted"/>.
            /// </summary>
            struct OnMapCommandCompletedArgs
            {
                Device &device;
                std::function<void(void *, size_t)> callback;
                void *mappedAddr;
                size_t nBytes;
                CommandEvent callbackDoneEvent;

                /// <summary>
                /// Initializes a new instance of the <see cref="OnMapCommandCompletedArgs" /> struct.
                /// </summary>
                /// <param name="p_device">The device where the map command will execute.</param>
                /// <param name="p_callback">The callback to execute when map finishes.</param>
                /// <param name="p_mappedAddr">The mapped memory address (which is a callback parameter).</param>
                /// <param name="p_nBytes">The amount of bytes in the mapped memory (which is a callback parameter).</param>
                /// <param name="p_callbackDoneEvent">The event to fire when the callback finishes executing.</param>
                OnMapCommandCompletedArgs(
                    Device *p_device,
                    const std::function<void(void *, size_t)> &p_callback,
                    void *p_mappedAddr,
                    size_t p_nBytes,
                    const CommandEvent &p_callbackDoneEvent
                ) : 
                    device(*p_device), 
                    callback(p_callback),
                    mappedAddr(p_mappedAddr),
                    nBytes(p_nBytes),
                    callbackDoneEvent(p_callbackDoneEvent)
                {}
            };

            friend void CL_CALLBACK
            OnMapCommandCompleted(cl_event completedEvent,
                                  cl_int eventCommandExecStatus,
                                  void *args);

        public:

            Device(cl_device_id device, 
                   cl_context context, 
                   cl_command_queue_properties properties);

            ~Device();

            cl_device_id GetHandle() { return m_device; }
 
            void GetDeviceInfo(cl_device_info paramCode, GenericParam &param) const;

            AsyncAction EnqueueFillBufferAsync(Buffer &buffer,
                                               size_t offset,
                                               size_t patternReps,
                                               GenericParam pattern);
 
            void EnqueueReadBuffer(Buffer &buffer, size_t offset, size_t nBytes, void *ptr);

            AsyncAction EnqueueReadBufferAsync(Buffer &buffer, size_t offset, size_t nBytes, void *ptr);
 
            void EnqueueWriteBuffer(Buffer &buffer, size_t offset, size_t nBytes, void *ptr);

            AsyncAction EnqueueWriteBufferAsync(Buffer &buffer, size_t offset, size_t nBytes, void *ptr);

            AsyncAction EnqueueCopyBufferAsync(Buffer &from, Buffer &to,
                                               size_t offsetFrom, size_t offsetTo,
                                               size_t nBytes);

            void EnqueueMapBuffer(Buffer &buffer,
                                  MemResourceUse how,
                                  size_t offset,
                                  size_t nBytes,
                                  const std::function<void (void *, size_t)> &callback);

            AsyncAction EnqueueMapBufferAsync(Buffer &buffer,
                                              MemResourceUse how,
                                              size_t offset,
                                              size_t nBytes,
                                              const std::function<void(void *, size_t)> &callback);
 
            /// <summary>
            /// Enqueues a kernel for asynchronous execution in the device.
            /// This is actually a wrapper template to ensure the arguments which are arrays will all have the same length.
            /// </summary>
            /// <param name="kernel">The kernel to execute.</param>
            /// <param name="globalWorkOffset">For each dimension, the offsets used to calculate the global ID's of the work items.</param>
            /// <param name="globalWorkSize">For each dimension, the number of work items that will execute the kernel.</param>
            /// <param name="localWorkSize">The size of the work-group for each dimension.</param>
            template <cl_uint workDims> 
            AsyncAction EnqueueNDRangeKernelAsync(
                Kernel &kernel,
                const std::array<size_t, workDims> &globalWorkOffset, 
                const std::array<size_t, workDims> &globalWorkSize, 
                const std::array<size_t, workDims> &localWorkSize)
            {
                return EnqueueNDRangeKernelAsyncImpl(
                    kernel, 
                    workDims, 
                    globalWorkOffset.data(), 
                    globalWorkSize.data(), 
                    localWorkSize.data());
            }

            void Finish();
        };

        /// <summary>
        /// A memory object which stores input and output data used by the kernels.
        /// </summary>
        class Buffer : notcopiable
        {
        private:

            size_t m_nBytes;
            cl_mem m_buffer, 
                   m_mainBuffer;

            void SetMemObjectDtorCallback(void (CL_CALLBACK *memObjectDestructorCallback)(cl_mem, void *), 
                                          void *userData);

        public:

            /// <summary>
            /// Initializes a new instance of the <see cref="Buffer"/> class.
            /// </summary>
            /// <param name="buffer">The buffer handle.</param>
            /// <param name="nBytes">The amount of bytes.</param>
            Buffer(cl_mem buffer, size_t nBytes) : 
                m_buffer(buffer), m_nBytes(nBytes), m_mainBuffer(nullptr) {}

            /// <summary>
            /// Initializes a new instance of the <see cref="Buffer"/> class using move semantics.
            /// </summary>
            /// <param name="ob">The object whose resources will be stolen.</param>
            Buffer(Buffer &&ob) : 
                m_nBytes(ob.m_nBytes), 
                m_buffer(ob.m_buffer), 
                m_mainBuffer(ob.m_mainBuffer) 
            {
                ob.m_nBytes = 0;
                ob.m_buffer = nullptr;
                ob.m_mainBuffer = nullptr;
            }

            ~Buffer();

            /// <summary>
            /// Gets the buffer handle.
            /// </summary>
            /// <returns>The OpenCL buffer handle.</returns>
            cl_mem GetHandle() { return m_buffer; }
 
            /// <summary>
            /// Gets the buffer size.
            /// </summary>
            /// <returns>How many bytes this buffer can store.</returns>
            size_t Size() const { return m_nBytes; }

            Buffer CreateSubBuffer(cl_mem_flags flags, 
                                   cl_buffer_create_type bufferCreateType, 
                                   size_t origin, 
                                   size_t nBytes);
        };

        /// <summary>
        /// Represents a compiled OpenCL program.
        /// </summary>
        class Program : notcopiable
        {
        private:

            cl_program m_program;

            void GetProgramInfo(cl_program_info programInfo, GenericParam &param) const;

        public:

            /// <summary>
            /// Initializes a new instance of the <see cref="Program"/> class.
            /// </summary>
            /// <param name="program">The OpenCL program handle.</param>
            Program(cl_program program)
                : m_program(program) {}

            ~Program();

            string SaveAs(const string &programName, const string &directory);

            Kernel CreateKernel(const string &kernelName);

            void CreateKernelsInProgram(std::vector<Kernel> &kernels);
        };

        /// <summary>
        /// Represents an executable OpenCL kernel.
        /// </summary>
        class Kernel : notcopiable
        {
        public:

            /// <summary>
            /// Wraps a memory object along with the "direction" 
            /// so as to better describe a kernel argument.
            /// </summary>
            struct Argument
            {
                Buffer *memObject;
                MemResourceUse direction;

                Argument(Buffer &p_memObject, MemResourceUse p_direction) : 
                    memObject(&p_memObject), direction(p_direction) {}
            };

        private:

            typedef std::map<cl_uint, Argument> ArgsMap;

            cl_kernel m_kernel;
            ArgsMap   m_arguments;

        public:

            /// <summary>
            /// Initializes a new instance of the <see cref="Kernel"/> class.
            /// </summary>
            /// <param name="kernel">The OpenCL kernel handle.</param>
            Kernel(cl_kernel kernel)
                : m_kernel(kernel) {}

            /// <summary>
            /// Initializes a new instance of the <see cref="Kernel"/> class using move semantics.
            /// </summary>
            /// <param name="ob">The object whose resources will be stolen.</param>
            Kernel(Kernel &&ob) : 
                m_kernel(ob.m_kernel), 
                m_arguments(std::move(ob.m_arguments)) 
            {
                ob.m_kernel = nullptr;
            }

            ~Kernel();

            /// <summary>
            /// Gets the kernel handle.
            /// </summary>
            /// <returns>The OpenCL kernel handle.</returns>
            cl_kernel GetHandle() { return m_kernel; }

            void GetKernelInfo(cl_kernel_info infoCode, GenericParam &param) const;

            void GetKernelWorkGroupInfo(Device &device, 
                                        cl_kernel_work_group_info infoCode, 
                                        GenericParam &param) const;

            void SetKernelArg(cl_uint argIndex, 
                              Buffer &buffer, 
                              MemResourceUse direction);

            std::vector<Argument> GetArguments() const;
        };

    }// end of namespace opencl
}// end of namespace _3fd

#endif // header guard
