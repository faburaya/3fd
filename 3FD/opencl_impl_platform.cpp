#include "stdafx.h"
#include "opencl_impl.h"
#include "callstacktracer.h"
#include "logger.h"

namespace _3fd
{
	namespace opencl
	{
		////////////////////////////
		//	Platform Class
		////////////////////////////

		/// <summary>
		/// Gets information about the platform.
		/// </summary>
		/// <param name="paramCode">The parameter code whose information about must be retrieved.</param>
		/// <param name="param">The parameter used to save the information.</param>
		void Platform::GetPlatformInfo(cl_platform_info paramCode, GenericParam &param) const
		{
			CALL_STACK_TRACE;

			OPENCL_IMPORT(clGetPlatformInfo);
			cl_int status = clGetPlatformInfo(m_platform,
				paramCode,
				param.size,
				param.value,
				&param.sizeRet);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clGetPlatformInfo");
		}

		/// <summary>
		/// A callback to be used when there is an error on the creation of a context.
		/// This callback MIGHT be called assynchronously by OpenCL implementation.
		/// </summary>
		void CL_CALLBACK
			ContextCreationErrCallback(const char *message,
			const void *privateInfo,
			size_t sizeOfPrivateInfo,
			void *userData)
		{
			CALL_STACK_TRACE;
			core::Logger::Write("The asynchronous creation of an OpenCL context has failed", message, core::Logger::PRIO_ERROR);
		}

		/// <summary>
		/// Creates a contect from a given device type.
		/// </summary>
		/// <param name="deviceType">Type of the device.</param>
		/// <returns>An OpenCL context</returns>
		Context Platform::CreateContextFromType(cl_device_type deviceType) const
		{
			CALL_STACK_TRACE;

			cl_context_properties properties[] =
			{
				CL_CONTEXT_PLATFORM,
				reinterpret_cast<cl_context_properties> (m_platform),
				0
			};

			OPENCL_IMPORT(clCreateContextFromType);
			cl_int status;
			cl_context context = clCreateContextFromType(properties,
				deviceType,
				ContextCreationErrCallback,
				nullptr,
				&status);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clCreateContextFromType");

			return Context(context);
		}

#ifdef _WIN32
		/// <summary>
		/// Creates the platform instances using SEH.
		/// </summary>
		/// <param name="rawPlatformIds">A vector of <see cref="cl_platform_id" /> objects as known by the OpenCL API.</param>
		/// <param name="qtAvailablePlatforms">The amount of retrieved available platforms.</param>
		/// <param name="status">The status reported by the OpenCL API call.</param>
		/// <param name="exCode">The exception code if the call failed.</param>
		/// <returns>
		/// STATUS_OKAY if the call to the procedure in the DLL was successfull, regardless 
		/// whether the OpenCL call itself reported success or not. Otherwise, STATUS_FAIL.
		/// </returns>
		static bool CreatePlatformInstancesSEH(std::vector<cl_platform_id> &rawPlatformIds, cl_uint &qtAvailablePlatforms, cl_int &status, DWORD &exCode)
		{
			/* Here must be used SEH in order to catch exceptions that might arise from the OS
			(such as a problem with the OpenCL DLL) and cannot be handled by the C++ exception
			handling mechanism. It has to be done here because this is the first OpenCL API call
			performed by the framework, and any weird problem should show up here. */
			__try
			{
				// ATTENTION: The return below is 'STATUS_OKAY' despite the binding error, because 'STATUS_FAIL' is reserved for a situation with a raised structured exception.
				OPENCL_IMPORT(clGetPlatformIDs);
				status = clGetPlatformIDs(rawPlatformIds.size(),
					rawPlatformIds.data(),
					&qtAvailablePlatforms);

				return STATUS_OKAY;
			}
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				exCode = GetExceptionCode();
				return STATUS_FAIL;
			}
		}
#endif

		/// <summary>
		/// Creates some platform instances.
		/// </summary>
		/// <param name="qtPlatforms">The requested amount of platforms the implementation must try to retrieve.</param>
		/// <param name="platforms">Where to the platforms that could be retrieved.</param>
		void Platform::CreatePlatformInstances(cl_uint qtPlatforms, std::vector<Platform> &platforms)
		{
			CALL_STACK_TRACE;

			platforms.clear();

			try
			{
				cl_int status;
				cl_uint qtAvailablePlatforms;
				std::vector<cl_platform_id> rawPlatformIds(qtPlatforms);

#ifdef _WIN32
				DWORD exCode;
				if (CreatePlatformInstancesSEH(rawPlatformIds, qtAvailablePlatforms, status, exCode) == STATUS_OKAY)
					openclErrors.RaiseExceptionWhen(status, "OpenCL API: clGetPlatformIDs");
				else
				{
					std::ostringstream oss;
					oss << "An structured exception was raised in a call to the OpenCL API: " << core::WWAPI::GetHResultLabel(exCode);
					throw core::AppException<std::runtime_error>(oss.str());
				}
#else
				OPENCL_IMPORT(clGetPlatformIDs);
				status = clGetPlatformIDs(rawPlatformIds.size(),
					rawPlatformIds.data(),
					&qtAvailablePlatforms);

				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clGetPlatformIDs");
#endif

				platforms.reserve(qtAvailablePlatforms);

				for (size_t index = 0; index < qtAvailablePlatforms; ++index)
				{
					platforms.push_back(
						Platform(rawPlatformIds[index])
					);
				}
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when creating OpenCL platforms: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="Platform"/> class.
		/// </summary>
		Platform::~Platform()
		{
			CALL_STACK_TRACE;

			try
			{
				_ASSERTE(m_platform != nullptr);
				OPENCL_IMPORT(clUnloadPlatformCompiler);
				cl_int status = clUnloadPlatformCompiler(m_platform);
				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clUnloadPlatformCompiler");
			}
			catch (core::IAppException &ex)
			{
				core::Logger::Write(ex, core::Logger::PRIO_ERROR);
			}
		}

	}// end of namespace opencl
}// end of namespace _3fd
