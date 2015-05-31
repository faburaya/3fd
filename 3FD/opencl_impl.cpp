#include "stdafx.h"
#include "opencl_impl.h"
#include "callstacktracer.h"
#include "dependencies.h"

#ifndef _WIN32
#   include <dlfcn.h>
#endif

namespace _3fd
{
	namespace opencl
	{
		/// <summary>
		/// Imports a function from the OpenCL DLL.
		/// </summary>
		/// <param name="procName">The Name of the procedure.</param>
		/// <returns>A pointer to the requested function.</returns>
		void *ImportFromOpenCL(const char *procName)
		{
			CALL_STACK_TRACE;

#ifdef _WIN32
			void *procedure = GetProcAddress(core::Dependencies::Get().OpenCLDllHandle, procName);

			if (procedure != nullptr)
				return procedure;
			else
			{
				std::ostringstream oss;
				oss << "Could not load \'" << procName << "\' from OpenCL DLL";
				auto what = oss.str();
				oss.str("");
				oss << "Windows API: GetProcAddress returned " << core::WWAPI::GetHResultLabel(GetLastError());
				auto details = oss.str();

				throw core::AppException<std::runtime_error>(what, details);
			}
#else
			void *procedure = dlsym(core::Dependencies::Get().OpenCLDllHandle, procName);

			if(procedure != nullptr)
				return procedure;
			else
			{
				std::ostringstream oss;
				oss << "Could not load \'" << procName << "\' from OpenCL shared library";
				auto what = oss.str();
				oss.str("");

				auto errdesc = dlerror();
				if(errdesc != nullptr)
					oss << errdesc << " - POSIX API: dlsym";
				else
					oss << "POSIX API: dlsym";

				auto details = oss.str();

				throw core::AppException<std::runtime_error>(what, details);
			}
#endif
		}


		/////////////////////////////
		//	OpenCLErrors Class
		/////////////////////////////

		OpenCLErrors openclErrors;

		std::mutex OpenCLErrors::loadMessagesMutex;

		/// <summary>
		/// Loads the possible error messages for OpenCL calls.
		/// </summary>
		void OpenCLErrors::LoadMessages()
		{
			try
			{
				// Information extracted from the OpenCL 1.1 reference:
				m_errorMessages.emplace(CL_BUILD_ERROR,									"Program building for a device generated an error");
				m_errorMessages.emplace(CL_BUILD_NONE,									"No build has been performed on the specified program object for the device");
				m_errorMessages.emplace(CL_BUILD_PROGRAM_FAILURE,						"Failure when trying to build the program executable");
				m_errorMessages.emplace(CL_COMPILER_NOT_AVAILABLE,						"A compiler is not available");
				m_errorMessages.emplace(CL_DEVICE_NOT_AVAILABLE,						"A device is currently not available");
				m_errorMessages.emplace(CL_DEVICE_NOT_FOUND,							"No OpenCL devices that matched the device type specified were found");
				m_errorMessages.emplace(CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST,	"A blocking operation failed because there was an event incomplete in the event wait list");
				m_errorMessages.emplace(CL_INVALID_ARG_INDEX,							"Invalid argument index");
				m_errorMessages.emplace(CL_INVALID_ARG_SIZE,							"The specified argument size does not match what is expected for the data type");
				m_errorMessages.emplace(CL_INVALID_ARG_VALUE,							"Invalid argument value");
				m_errorMessages.emplace(CL_INVALID_BINARY,								"Invalid program binary encountered for a device");
				m_errorMessages.emplace(CL_INVALID_BUFFER_SIZE,							"The buffer size is zero or exceeds the range supported by the compute devices associated with the context");
				m_errorMessages.emplace(CL_INVALID_BUILD_OPTIONS,						"Invalid build options specified");
				m_errorMessages.emplace(CL_INVALID_COMMAND_QUEUE,						"Invalid command queue");
				m_errorMessages.emplace(CL_INVALID_CONTEXT,								"Invalid OpenCL context");
				m_errorMessages.emplace(CL_INVALID_DEVICE,								"There is an invalid device");
				m_errorMessages.emplace(CL_INVALID_DEVICE_TYPE,							"Invalid device type specified");
				m_errorMessages.emplace(CL_INVALID_EVENT_WAIT_LIST,						"The event wait list is invalid");
				m_errorMessages.emplace(CL_INVALID_GLOBAL_WORK_SIZE,					"Invalid value of global work size (might not be supported by the device)");
				m_errorMessages.emplace(CL_INVALID_GLOBAL_OFFSET,						"Invalid value of global work offset");
				m_errorMessages.emplace(CL_INVALID_HOST_PTR,							"Host memory address incorrectly specified");
				m_errorMessages.emplace(CL_INVALID_IMAGE_SIZE,							"Image dimensions are not supported by device");
				m_errorMessages.emplace(CL_INVALID_KERNEL,								"Invalid kernel object");
				m_errorMessages.emplace(CL_INVALID_KERNEL_ARGS,							"Kernel argument values have not been specified");
				m_errorMessages.emplace(CL_INVALID_KERNEL_DEFINITION,					"The function declaration for a __kernel function is not the same for all devices for which the program executable has been built");
				m_errorMessages.emplace(CL_INVALID_KERNEL_NAME,							"The kernel name was not found in the program");
				m_errorMessages.emplace(CL_INVALID_MEM_OBJECT,							"Invalid memory object specified");
				m_errorMessages.emplace(CL_INVALID_OPERATION,							"The build of a program executable for any of the devices has not completed or there are kernel objects attached to the program");
				m_errorMessages.emplace(CL_INVALID_PLATFORM,							"Invalid platform");
				m_errorMessages.emplace(CL_INVALID_PROGRAM,								"Invalid program object");
				m_errorMessages.emplace(CL_INVALID_PROGRAM_EXECUTABLE,					"There is no successfully built executable for the program");
				m_errorMessages.emplace(CL_INVALID_PROPERTY,							"Invalid properties specification");
				m_errorMessages.emplace(CL_INVALID_QUEUE_PROPERTIES,					"Property not supported by the device");
				m_errorMessages.emplace(CL_INVALID_SAMPLER,								"Invalid sampler object specified");
				m_errorMessages.emplace(CL_INVALID_VALUE,								"Invalid argument specified");
				m_errorMessages.emplace(CL_INVALID_WORK_DIMENSION,						"Invalid value of work dimensions (not supported by the device)");
				m_errorMessages.emplace(CL_INVALID_WORK_GROUP_SIZE,						"Invalid value of work group size (not supported by the device or does not match kernel souce code specification)");
				m_errorMessages.emplace(CL_INVALID_WORK_ITEM_SIZE,						"Invalid value of work items (not supported by the device)");
				m_errorMessages.emplace(CL_MAP_FAILURE,									"Failed to map the requested region into the host address space");
				m_errorMessages.emplace(CL_MEM_OBJECT_ALLOCATION_FAILURE,				"There was a failure to allocate memory to the buffer object");
				m_errorMessages.emplace(CL_MISALIGNED_SUB_BUFFER_OFFSET,				"The offset of the sub-buffer object is not aligned to the CL_DEVICE_MEM_BASE_ADDR_ALIGN value of the device");
				m_errorMessages.emplace(CL_OUT_OF_HOST_MEMORY,							"Failed to allocate resources required by the OpenCL implementation on the host");
				m_errorMessages.emplace(CL_OUT_OF_RESOURCES,							"Failed to allocate resources required by the OpenCL implementation on the device");
			}
			catch(std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Failed to load OpenCL error messages: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Ensures the error messages were correctly loaded before searching for one.
		/// </summary>
		/// <param name="status">The status returned by the OpenCL call.</param>
		/// <param name="details">The error details.</param>
		void OpenCLErrors::EnsureInitialization(cl_int status, const char *details)
		{
			if (m_errorMessages.empty())
			{
				try
				{
					std::lock_guard<std::mutex> lock(loadMessagesMutex);

					if (m_errorMessages.empty())
						LoadMessages();
				}
				catch (std::system_error &ex)
				{
					std::ostringstream oss;
					oss << "OpenCL API returned error code " << status << ". Another failure prevented a more appropriate message to be loaded";
					auto exWhat = oss.str();
					oss << details << " - Failed to acquire lock before loading error messages: " << core::StdLibExt::GetDetailsFromSystemError(ex);
					auto exDetails = oss.str();

					throw core::AppException<std::runtime_error>(exWhat, exDetails);
				}
			}
		}

		/// <summary>
		/// Check the status and logs an error according to it.
		/// </summary>
		/// <param name="status">The status returned by the OpenCL call.</param>
		/// <param name="details">The details of the call.</param>
		/// <param name="prio">The error priority.</param>
		void OpenCLErrors::LogErrorWhen(cl_int status, const char *details, core::Logger::Priority prio)
		{
			if (status == CL_SUCCESS)
				return;
			else
			{
				EnsureInitialization(status, details);

				auto iter = m_errorMessages.find(status);

				if (m_errorMessages.end() != iter)
					core::Logger::Write(iter->second, details, prio, true);
				else
				{
					std::ostringstream oss;
					oss << "Unexpected return from an OpenCL API call. Error code " << status;
					core::Logger::Write(oss.str(), details, prio, true);
				}
			}
		}

		/// <summary>
		/// Check status and raises an exception according to it.
		/// </summary>
		/// <param name="status">The status returned by the OpenCL call.</param>
		/// <param name="details">The details of the call.</param>
		void OpenCLErrors::RaiseExceptionWhen(cl_int status, const char *details)
		{
			if (status == CL_SUCCESS)
				return;
			else
			{
				EnsureInitialization(status, details);

				auto iter = m_errorMessages.find(status);

				if (m_errorMessages.end() != iter)
					throw core::AppException<std::runtime_error>(iter->second, details);
				else
				{
					std::ostringstream oss;
					oss << "Unexpected return from an OpenCL API call. Error code " << status;
					throw core::AppException<std::runtime_error>(oss.str(), details);
				}
			}
		}

		/// <summary>
		/// Check status and raises an exception according to it.
		/// </summary>
		/// <param name="status">The status returned by the OpenCL call.</param>
		/// <param name="details">The error details.</param>
		/// <param name="innerEx">The exception to be nested inside the new one.</param>
		void OpenCLErrors::RaiseExceptionWhen(cl_int status, const char *details, core::IAppException &&innerEx)
		{
			if (status == CL_SUCCESS)
				return;
			else
			{
				EnsureInitialization(status, details);

				auto iter = m_errorMessages.find(status);

				if (m_errorMessages.end() != iter)
					throw core::AppException<std::runtime_error>(iter->second, details, innerEx);
				else
				{
					std::ostringstream oss;
					oss << "Unexpected return from an OpenCL API call. Error code " << status;
					throw core::AppException<std::runtime_error>(oss.str(), details, innerEx);
				}
			}
		}


		////////////////////////
		//	CommandEvent Class
		////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="CommandEvent" /> class.
		/// </summary>
		/// <param name="context">The OpenCL context.</param>
		CommandEvent::CommandEvent(cl_context context)
			: m_event(nullptr)
		{
			CALL_STACK_TRACE;
			OPENCL_IMPORT(clCreateUserEvent);
			cl_int status;
			m_event = clCreateUserEvent(context, &status);
			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clCreateUserEvent");
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="CommandEvent" /> class.
		/// </summary>
		/// <param name="event">The event handle.</param>
		CommandEvent::CommandEvent(cl_event event)
			: m_event(nullptr)
		{
			CALL_STACK_TRACE;
			OPENCL_IMPORT(clRetainEvent);
			cl_int status = clRetainEvent(event);
			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clRetainEvent");
			m_event = event;
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="CommandEvent"/> class using copy semantics.
		/// </summary>
		/// <param name="ob">The object whose resources will be copied.</param>
		CommandEvent::CommandEvent(const CommandEvent &ob)
			: m_event(nullptr)
		{
			CALL_STACK_TRACE;
			OPENCL_IMPORT(clRetainEvent);
			cl_int status = clRetainEvent(ob.m_event);
			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clRetainEvent");
			m_event = ob.m_event;
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="CommandEvent"/> class.
		/// </summary>
		CommandEvent::~CommandEvent()
		{
			if (m_event != nullptr)
			{
				CALL_STACK_TRACE;
				OPENCL_IMPORT(clReleaseEvent);
				cl_int status = clReleaseEvent(m_event);
				openclErrors.LogErrorWhen(status, "OpenCL API: clReleaseEvent", core::Logger::PRIO_ERROR);
			}
		}

		/// <summary>
		/// Sets a callback for a command event.
		/// </summary>
		/// <param name="cmdExecStatus">
		/// The command execution status, for which the callback waits for before executing.
		/// </param>
		/// <param name="callback">The callback.</param>
		/// <param name="args">A pointer for the argument to be passed to the callback.</param>
		void CommandEvent::SetCallback(cl_int cmdExecStatus, CmdEventCallback callback, void *args)
		{
			CALL_STACK_TRACE;
			OPENCL_IMPORT(clSetEventCallback);
			cl_int status = clSetEventCallback(m_event, cmdExecStatus, callback, args);
			openclErrors.RaiseExceptionWhen(status, "clSetEventCallback");
		}

		void CommandEvent::SetStatus(cl_int evStatus)
		{
			CALL_STACK_TRACE;
			OPENCL_IMPORT(clSetUserEventStatus);
			cl_int status = clSetUserEventStatus(m_event, evStatus);
			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clSetUserEventStatus");
		}


		//////////////////////////////////////////
		//	AsyncAction Class
		//////////////////////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="AsyncAction"/> class.
		/// </summary>
		/// <param name="cmdEvent">The command event for the asynchronous action.</param>
		AsyncAction::AsyncAction(CommandEvent cmdEvent)
			: m_eventHandle(cmdEvent.GetHandle())
		{
			CALL_STACK_TRACE;
			OPENCL_IMPORT(clRetainEvent);
			cl_int status = clRetainEvent(m_eventHandle);
			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clRetainEvent");
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="AsyncAction" /> class.
		/// </summary>
		/// <param name="eventHandle">The event handle for the asynchronous action.</param>
		/// <param name="evResFreeOnCompletion">
		/// Whether the event resources will or have been set for release on a completion callback.
		/// </param>
		/// <remarks>
		/// This ctor needs to know the strategy employed for releasing the event resources. If those will be freed 
		/// by a callback invoked upon event completion, than this object is not responsible for that. Hence the ctor/dtor 
		/// must increment/decrement the event reference counting for the sake of correct management of the event object 
		/// life cycle. However, if the responsibility for releasing the event resources relies on this object, than the 
		/// ctor must not increment the event reference counting (because that has already happened upon event creation), 
		/// whereas the dtor must still decrement it.
		/// </remarks>
		AsyncAction::AsyncAction(cl_event eventHandle, bool evResFreeOnCompletion)
			: m_eventHandle(eventHandle)
		{
			if (evResFreeOnCompletion)
			{
				CALL_STACK_TRACE;
				OPENCL_IMPORT(clRetainEvent);
				cl_int status = clRetainEvent(eventHandle);
				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clRetainEvent");
			}
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="AsyncAction"/> class.
		/// </summary>
		/// <remarks>
		/// This dtor decrements the event reference count regardless the ctor retained it (strong reference) or not.
		/// That means this dtor decrements the event reference count for objects created with a weak reference.
		/// </remarks>
		AsyncAction::~AsyncAction()
		{
			if (m_eventHandle != nullptr)
			{
				CALL_STACK_TRACE;

				try
				{
					OPENCL_IMPORT(clReleaseEvent);
					cl_int status = clReleaseEvent(m_eventHandle);
					openclErrors.RaiseExceptionWhen(status, "OpenCL API: clReleaseEvent");
				}
				catch (core::IAppException &ex)
				{
					core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
				}
			}
		}

		/// <summary>
		/// Implements the awaiting for the asynchronous call to return.
		/// </summary>
		void AsyncAction::Await()
		{
			_ASSERTE(m_eventHandle != nullptr); // no event to await
			CALL_STACK_TRACE;
			OPENCL_IMPORT(clWaitForEvents);
			cl_int status = clWaitForEvents(1, &m_eventHandle);
			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clWaitForEvents");
		}

		/// <summary>
		/// Detaches the event handle from this instance.
		/// The object becomes 'hollow' and loses the ability to await for the asynchronous action.
		/// </summary>
		void AsyncAction::Detach()
		{
			_ASSERTE(m_eventHandle != nullptr); // no event handle to detach
			CALL_STACK_TRACE;
			OPENCL_IMPORT(clReleaseEvent);
			cl_int status = clReleaseEvent(m_eventHandle);
			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clReleaseEvent");
			m_eventHandle = nullptr;
		}


		////////////////////////
		//	Buffer Class
		////////////////////////

		/// <summary>
		/// Finalizes an instance of the <see cref="Buffer"/> class.
		/// </summary>
		Buffer::~Buffer()
		{
			CALL_STACK_TRACE;

			try
			{
				OPENCL_IMPORT(clReleaseMemObject);
				cl_int status = clReleaseMemObject(m_buffer);
				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clReleaseMemObject");

				if(m_mainBuffer != nullptr)
				{
					status = clReleaseMemObject(m_mainBuffer);
					openclErrors.RaiseExceptionWhen(status, "OpenCL API: clReleaseMemObject");
				}
			}
			catch(core::IAppException &ex)
			{
				core::Logger::Write(ex, core::Logger::PRIO_ERROR);
			}
		}

		/// <summary>
		/// Currently not used:
		/// Registers a callback for when the buffer is about to be destroyed.
		/// </summary>
		/// <param name="memObjectDestructorCallback">The callback.</param>
		/// <param name="userData">The memory address used on buffer initialization.</param>
		void Buffer::SetMemObjectDtorCallback(void (CL_CALLBACK *memObjectDtorCallback)(cl_mem, void *), 
											  void *userData)
		{
			CALL_STACK_TRACE;

			OPENCL_IMPORT(clSetMemObjectDestructorCallback);
			cl_int status = clSetMemObjectDestructorCallback(m_buffer, 
															 memObjectDtorCallback, 
															 userData);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clSetMemObjectDestructorCallback");
		}
 
		/// <summary>
		/// Creates a sub-buffer.
		/// </summary>
		/// <param name="flags">The flags for creation.</param>
		/// <param name="bufferCreateType">Type of buffer.</param>
		/// <param name="origin">The origin of the sub-buffer inside the main buffer (in bytes).</param>
		/// <param name="nBytes">The size in bytes.</param>
		/// <returns>A newly created sub-buffer.</returns>
		Buffer Buffer::CreateSubBuffer(cl_mem_flags flags, 
									   cl_buffer_create_type bufferCreateType, 
									   size_t origin, 
									   size_t nBytes)
		{
			CALL_STACK_TRACE;

			cl_int status;
			_cl_buffer_region bufferCreateInfo;
			bufferCreateInfo.origin = origin;
			bufferCreateInfo.size = nBytes;

			OPENCL_IMPORT(clCreateSubBuffer);
			cl_mem subBuffer = clCreateSubBuffer(m_buffer, 
												 flags, 
												 bufferCreateType, 
												 &bufferCreateInfo, 
												 &status);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clCreateSubBuffer");

			return Buffer(subBuffer, nBytes);
		}


		//////////////////////////////////////////
		//	Kernel Class
		//////////////////////////////////////////

		/// <summary>
		/// Finalizes an instance of the <see cref="Kernel"/> class.
		/// </summary>
		Kernel::~Kernel()
		{
			CALL_STACK_TRACE;

			try
			{
				OPENCL_IMPORT(clReleaseKernel);
				cl_int status = clReleaseKernel(m_kernel);
				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clReleaseKernel");
			}
			catch(core::IAppException &ex)
			{
				core::Logger::Write(ex, core::Logger::PRIO_ERROR);
			}
		}
		
		/// <summary>
		/// Gets information from the kernel.
		/// </summary>
		/// <param name="infoCode">The information code.</param>
		/// <param name="param">The parameter where the retrieved information will be stored.</param>
		void Kernel::GetKernelInfo(cl_kernel_info infoCode, GenericParam &param) const
		{
			CALL_STACK_TRACE;

			OPENCL_IMPORT(clGetKernelInfo);
			cl_int status = clGetKernelInfo(m_kernel, 
											infoCode, 
											param.size, 
											param.value, 
											&param.sizeRet);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clGetKernelInfo");
		}

		/// <summary>
		/// Gets information about the kernel work group.
		/// </summary>
		/// <param name="device">The device where the kernel runs and whose information will be queried.</param>
		/// <param name="infoCode">The information code.</param>
		/// <param name="param">The parameter where the retrieved information will be stored.</param>
		void Kernel::GetKernelWorkGroupInfo(Device &device, 
											cl_kernel_work_group_info infoCode, 
											GenericParam &param) const
		{
			CALL_STACK_TRACE;

			OPENCL_IMPORT(clGetKernelWorkGroupInfo);
			cl_int status = clGetKernelWorkGroupInfo(m_kernel, 
													 device.GetHandle(), 
													 infoCode, 
													 param.size, 
													 param.value, 
													 &param.sizeRet);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clGetKernelWorkGroupInfo");
		}

		/// <summary>
		/// Sets the kernel arguments.
		/// </summary>
		/// <param name="argIndex">The index of the argument.</param>
		/// <param name="buffer">The buffer object.</param>
		/// <param name="direction">The direction of the argument data, whose options are specified in the enumeration <see cref="MemResourceUse" />.</param>
		void Kernel::SetKernelArg(cl_uint argIndex, 
								  Buffer &buffer, 
								  MemResourceUse direction)
		{
			CALL_STACK_TRACE;

			cl_mem memoryObject = buffer.GetHandle();

			OPENCL_IMPORT(clSetKernelArg);
			cl_int status = clSetKernelArg(m_kernel, 
										   argIndex, 
										   sizeof (cl_mem), 
										   &memoryObject);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clSetKernelArg");

			try
			{
				auto iter = m_arguments.find(argIndex);

				if(m_arguments.end() != iter)
					iter->second = Argument(buffer, direction);
				else
					m_arguments.emplace(argIndex, Argument(buffer, direction));
			}
			catch(std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when setting OpenCL kernel argument: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Gets the arguments set for the kernel.
		/// </summary>
		/// <returns>A vector with all the arguments set for the kernel.</returns>
		std::vector<Kernel::Argument> Kernel::GetArguments() const
		{
			CALL_STACK_TRACE;

			try
			{
				std::vector<Argument> argumentsVec;
				argumentsVec.reserve(m_arguments.size());

				for(auto &pair : m_arguments)
					argumentsVec.push_back(pair.second);

				return std::move(argumentsVec);
			}
			catch(std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when retrieving OpenCL kernel arguments: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

	}// end of namespace opencl
}// end of namespace _3fd
