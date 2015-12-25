#include "stdafx.h"
#include "opencl_impl.h"
#include "callstacktracer.h"
#include "logger.h"
#include <fstream>

namespace _3fd
{
	namespace opencl
	{
		///////////////////////////
		//	Program Class
		///////////////////////////

		/// <summary>
		/// Finalizes an instance of the <see cref="Program"/> class.
		/// </summary>
		Program::~Program()
		{
			CALL_STACK_TRACE;

			try
			{
				OPENCL_IMPORT(clReleaseProgram);
				cl_int status = clReleaseProgram(m_program);
				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clReleaseProgram");
			}
			catch (core::IAppException &ex)
			{
				core::Logger::Write(ex, core::Logger::PRIO_ERROR);
			}
		}

		/// <summary>
		/// Gets information about the OpenCL program.
		/// </summary>
		/// <param name="programInfo">The code for the information to be retrieved.</param>
		/// <param name="param">The parameter where to store the retrieved information.</param>
		void Program::GetProgramInfo(cl_program_info programInfo, GenericParam &param) const
		{
			CALL_STACK_TRACE;

			OPENCL_IMPORT(clGetProgramInfo);
			cl_int status = clGetProgramInfo(m_program,
				programInfo,
				param.size,
				param.value,
				&param.sizeRet);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clGetProgramInfo");
		}

		/// <summary>
		/// Saves the OpenCL program into disk.
		/// </summary>
		/// <param name="programName">Name of the program.</param>
		/// <param name="directory">The directory where to place the files.</param>
		/// <returns>The file path for the program manifest.</returns>
		string Program::SaveAs(const string &programName, const string &directory)
		{
			CALL_STACK_TRACE;

			_ASSERTE(programName.empty() == false); // must specify a name for the program
			_ASSERTE(directory.empty() == false); // must specify a directory to place the manifest file

			try
			{
				// How many devices the program has been compiled for?
				GenericParam param;
				cl_uint qtDevices;
				param.Set(qtDevices);
				GetProgramInfo(CL_PROGRAM_NUM_DEVICES, param);

				// What devices are they?
				std::vector<cl_device_id> devices(qtDevices);
				param.Set(devices.data(), devices.size() * sizeof(devices[0]));
				GetProgramInfo(CL_PROGRAM_DEVICES, param);
				_ASSERTE(devices.size() * sizeof(devices[0]) == param.sizeRet);

				// What is the size of the binaries compiled for each device?
				std::vector<size_t> binProgSizes(qtDevices);
				param.Set(binProgSizes.data(), binProgSizes.size() * sizeof(binProgSizes[0]));
				GetProgramInfo(CL_PROGRAM_BINARY_SIZES, param);
				_ASSERTE(binProgSizes.size() * sizeof(binProgSizes[0]) == param.sizeRet);

				// this flat structure is what OpenCL API accepts:
				std::vector<uint8_t *> progBinariesPtrs;
				progBinariesPtrs.reserve(qtDevices);

				// ... and this structure automatically deallocates everything when an exception is thrown:
				std::vector<std::unique_ptr<uint8_t[]>> progBinaries;
				progBinaries.reserve(qtDevices);

				// Allocate enough memory for each binary:
				for (auto qtBytes : binProgSizes)
				{
					auto byteArray = new uint8_t[qtBytes];
					progBinaries.emplace_back(byteArray);
					progBinariesPtrs.push_back(byteArray);
				}

				// Get the program binaries for each device:
				param.Set(progBinariesPtrs.data(), progBinariesPtrs.size() * sizeof(progBinariesPtrs[0]));
				GetProgramInfo(CL_PROGRAM_BINARIES, param);
				_ASSERTE(progBinariesPtrs.size() * sizeof(progBinariesPtrs[0]) == param.sizeRet);

                // Create a manifest object for the program, and serialize it into XML to disk:
				auto manifest = ProgramManifest::CreateObject(programName, devices);
                auto manifestFilePath = manifest.SaveTo(directory);

                // Write each one of the program binaries to a file:
				int idx(0);
				for (auto &devProgInfo : manifest.GetDeviceProgramsInfo())
				{
					std::ofstream ofs(devProgInfo.fileName, std::ios::binary | std::ios::trunc);

					if (ofs.is_open() == false)
						throw core::AppException<std::runtime_error>("Could not open or create binary program file", devProgInfo.fileName);

					ofs.write(reinterpret_cast<char *>(progBinariesPtrs[idx]), binProgSizes[idx]); // write to file

					if (ofs.bad())
						throw core::AppException<std::runtime_error>("Failure when writing binary program file", devProgInfo.fileName);

					ofs.flush();
					ofs.close();
					progBinaries[idx].reset(); // release some memory
					++idx;
				}

                return manifestFilePath;
			}
			catch (core::IAppException &ex)
			{
				throw core::AppException<std::runtime_error>("Failed to save OpenCL program", ex);
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Failed to save OpenCL program: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Creates an OpenCL kernel out of the program.
		/// </summary>
		/// <param name="kernelName">Name of the kernel.</param>
		/// <returns>A smart shared pointer referring the new created kernel object.</returns>
		Kernel Program::CreateKernel(const string &kernelName)
		{
			CALL_STACK_TRACE;

			OPENCL_IMPORT(clCreateKernel);
			cl_int status;
			cl_kernel kernel = clCreateKernel(m_program,
				kernelName.c_str(),
				&status);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clCreateKernel");

			return Kernel(kernel);
		}

		/// <summary>
		/// Creates kernel objects for all the kernels present in the program.
		/// </summary>
		/// <param name="kernels">Where to store the new kernel objects.</param>
		void Program::CreateKernelsInProgram(std::vector<Kernel> &kernels)
		{
			CALL_STACK_TRACE;

			try
			{
				kernels.clear();

				size_t qtKernels;
				GenericParam param;
				param.Set(qtKernels);
				GetProgramInfo(CL_PROGRAM_NUM_KERNELS, param);
				std::vector<cl_kernel> kernelHandles(qtKernels);

				OPENCL_IMPORT(clCreateKernelsInProgram);
				cl_int status = clCreateKernelsInProgram(m_program,
					kernelHandles.size(),
					kernelHandles.data(),
					nullptr);

				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clCreateKernelsInProgram");

				kernels.reserve(qtKernels);

				for (auto handle : kernelHandles)
					kernels.emplace_back(handle);
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when creating OpenCL kernels: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

	}// end of namespace opencl
}// end of namespace _3fd
