#include "stdafx.h"
#include "opencl_impl.h"
#include "callstacktracer.h"
#include "logger.h"
#include <memory>
#include <fstream>
#include <functional>

namespace _3fd
{
namespace opencl
{
	///////////////////////////
	//  DeviceInfo Struct
	///////////////////////////

	/// <summary>
	/// Initializes a new instance of the <see cref="DeviceInfo"/> struct.
	/// </summary>
	/// <param name="deviceId">The device identifier.</param>
	DeviceInfo::DeviceInfo(cl_device_id deviceId)
	{
		CALL_STACK_TRACE;

		// Gets a unique ID for the device vendor:
		GenericParam param;
		param.Set(vendorId);
		GetDeviceInfoImpl(deviceId, CL_DEVICE_VENDOR_ID, param);

		std::array<char, 128> strbuf;
		param.Set(strbuf.data(), strbuf.size());

		// ... the device vendor name:
		GetDeviceInfoImpl(deviceId, CL_DEVICE_VENDOR, param);
		vendorName = string(strbuf.data());

		// ... the device name:
		GetDeviceInfoImpl(deviceId, CL_DEVICE_NAME, param);
		deviceName = string(strbuf.data());

		// ... and the driver version:
		GetDeviceInfoImpl(deviceId, CL_DRIVER_VERSION, param);
		driverVersion = string(strbuf.data());

		UpdateHashCode();
	}

	/// <summary>
	/// Updates the hash code based on the values of the data members.
	/// </summary>
	/// <returns>The calculated hash code.</returns>
	void DeviceInfo::UpdateHashCode()
	{
		std::hash<string> hash;
		size_t x = 17;
		x = x * 23 + vendorId;
		x = x * 23 + hash(vendorName);
		x = x * 23 + hash(deviceName);
		x = x * 23 + hash(driverVersion);
		hashCode = x;
	}


	//////////////////////////
	//  Context Class
	//////////////////////////

	/// <summary>
	/// Initializes a new instance of the <see cref="Context"/> class.
	/// </summary>
	/// <param name="context">The OpenCL context object.</param>
	Context::Context(cl_context context)
	try :
		m_context(context)
	{
		CALL_STACK_TRACE;
		DiscoverDevices();
	}
	catch (core::IAppException &)
	{
		CALL_STACK_TRACE;

		OPENCL_IMPORT(clReleaseContext);
		cl_int status = clReleaseContext(m_context);
		openclErrors.LogErrorWhen(status, "OpenCL API: clReleaseContext", core::Logger::PRIO_ERROR);
	}
	catch (std::exception &ex)
	{
		CALL_STACK_TRACE;

		OPENCL_IMPORT(clReleaseContext);
		cl_int status = clReleaseContext(m_context);
		openclErrors.LogErrorWhen(status, "OpenCL API: clReleaseContext", core::Logger::PRIO_ERROR);

		std::ostringstream oss;
		oss << "Generic failure during OpenCL context creation: " << ex.what();
		throw core::AppException<std::runtime_error>(oss.str());
	}

	/// <summary>
	/// Finalizes an instance of the <see cref="Context"/> class.
	/// </summary>
	Context::~Context()
	{
		if (m_context != nullptr)
		{
			CALL_STACK_TRACE;

			try
			{
				OPENCL_IMPORT(clReleaseContext);
				cl_int status = clReleaseContext(m_context);
				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clReleaseContext");
			}
			catch (core::IAppException &ex)
			{
				core::Logger::Write(ex, core::Logger::PRIO_ERROR);
			}
		}
	}

	/// <summary>
	/// Gets information about the context.
	/// </summary>
	/// <param name="paramCode">The code of the parameter whose information will be requested.</param>
	/// <param name="param">Where to store the retrieved information.</param>
	void Context::GetContextInfo(cl_context_info paramCode, GenericParam &param) const
	{
		CALL_STACK_TRACE;

		OPENCL_IMPORT(clGetContextInfo);
		cl_int status = clGetContextInfo(m_context,
			                             paramCode,
			                             param.size,
			                             param.value,
			                             &param.sizeRet);

		openclErrors.RaiseExceptionWhen(status, "OpenCL API: clGetContextInfo");
	}

	/// <summary>
	/// Discovers all the devices for the current platform.
	/// </summary>
	void Context::DiscoverDevices()
	{
		CALL_STACK_TRACE;

		cl_uint qtDevices;
		GenericParam param;
		param.Set(qtDevices);
		GetContextInfo(CL_CONTEXT_NUM_DEVICES, param);

		m_devices.resize(qtDevices);
		param.Set(m_devices[0]);
		GetContextInfo(CL_CONTEXT_DEVICES, param);

		// Retrieve and keep some key information about the devices:
		for (auto deviceId : m_devices)
		{
			auto devInfo = DeviceInfo2(deviceId);
			auto devHashCode = devInfo.info.hashCode;
			m_devicesInfo.emplace(devHashCode, std::move(devInfo));
		}
	}

	/// <summary>
	/// Gets the source code of a program from a text file.
	/// </summary>
	/// <param name="fileName">Name of the file.</param>
	/// <param name="sourceCodeLines">Where to store the source code lines.</param>
	void Context::GetSourceCode(const string &fileName,
			                    std::deque<string> &sourceCodeLines)
	{
		CALL_STACK_TRACE;

		try
		{
			std::ifstream sourceCodeStream(fileName, std::ios::binary);

			if (sourceCodeStream.is_open() == false)
			{
				std::ostringstream oss;
				oss << "Source code file was \'" << fileName << '\'';
				throw core::AppException<std::runtime_error>("Could not open the source code file of an OpenCL program", oss.str());
			}

			std::vector<char> line(core::AppConfig::GetSettings().framework.opencl.maxSourceCodeLineLength);

			while (sourceCodeStream.eof() == false)
			{
				if (sourceCodeStream.getline(line.data(), line.size()).bad())
				{
					std::ostringstream oss;
					oss << "Source code file was \'" << fileName << '\'';
					throw core::AppException<std::runtime_error>("Error reading the source code file of an OpenCL program", oss.str());
				}

				sourceCodeLines.push_back(string(line.data()));
			}

			sourceCodeStream.close();
		}
		catch (core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch (std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when reading OpenCL program source code: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Compiles the program from the source code.
	/// </summary>
	/// <param name="sourceCodeFilePath">Source code file path.</param>
	/// <returns>The OpenCL program object for the given source.</returns>
	cl_program Context::CreateProgramFromSourceCode(const string &sourceCodeFilePath)
	{
		CALL_STACK_TRACE;

		try
		{
			// Get the source code separated in lines:
			std::deque<string> sourceCodeLines;
			GetSourceCode(sourceCodeFilePath, sourceCodeLines);
			std::vector<const char *> sourceCodeLinePointers(sourceCodeLines.size());

			size_t index(0);
			std::generate(sourceCodeLinePointers.begin(), sourceCodeLinePointers.end(), [&index, &sourceCodeLines]()
			{
				return sourceCodeLines[index++].c_str();
			});

			// Create a program from the source:

			OPENCL_IMPORT(clCreateProgramWithSource);
			cl_int status;
			cl_program program = clCreateProgramWithSource(m_context,
				                                           sourceCodeLinePointers.size(),
				                                           sourceCodeLinePointers.data(),
				                                           nullptr,
				                                           &status);

			openclErrors.RaiseExceptionWhen(status, "OpenCL API: clCreateProgramWithSource");

			return program;
		}
		catch (core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch (std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when creating OpenCL program from source code: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Reads the program manifest file and load program binaries from disk.
	/// </summary>
	/// <param name="manifestFilePath">Path to the OpenCL program manifest file.</param>
	/// <returns>
	/// The OpenCL program created from the binary files, or 'nullptr' if the devices 
	/// described in the manifest did not match the ones in the current context.
	/// </returns>
	/// <remarks>
	/// The manifest has XML content describing the devices for which the program has previously been compiled, 
	/// and the names of such binary files. This method will load only the binaries for devices that exist in 
	/// the current context. If any loaded binary is no longer valid for its device for some reason (eg.: driver 
	/// update), that is reported in the log and the resulting program will lack a binary for such device. When 
	/// none of the loaded binaries is valid, that is considered and error and than the method throws an exception.
	/// </remarks>
	cl_program Context::CreateProgramWithBinaries(const string &manifestFilePath)
	{
		CALL_STACK_TRACE;

		try
		{
			// Load meta data from the program manifest:
			auto manifest = ProgramManifest::LoadFrom(manifestFilePath);
			auto &devsInfo = manifest.GetDeviceProgramsInfo();

			std::vector<cl_device_id> devicesIds; // the ID of each device whose binary will be loaded
			std::vector<size_t> devicesHashCodes; // the hash codes for such devices
			std::vector<std::unique_ptr<uint8_t[]>> binariesSmartPtrs; // automatically deallocates...
			std::vector<const uint8_t *> binariesCPtrs; // ... the buffers holding the binaries
			std::vector<size_t> binariesSizes; // and the sizes of the buffers

			// Reserve memory for the parameters:
			const auto maxQtDevices = devsInfo.size();
			binariesSizes.reserve(maxQtDevices);
			binariesCPtrs.reserve(maxQtDevices);
			binariesSmartPtrs.reserve(maxQtDevices);
			devicesHashCodes.reserve(maxQtDevices);
			devicesIds.reserve(maxQtDevices);

			// For each binary compiled for a given device:
			for (auto &info : devsInfo)
			{
				// Check whether the device that the binary has been compiled to belongs this context:
				auto iter = m_devicesInfo.find(info.deviceInfo.hashCode);
				if (m_devicesInfo.end() != iter)
				{
					devicesIds.push_back(iter->second.id);
					devicesHashCodes.push_back(iter->second.info.hashCode);
				}
				else
					continue;

				// Open binary program file:
				std::ifstream ifs(info.fileName, std::ios::binary);

				if (ifs.is_open() == false)
					throw core::AppException<std::runtime_error>("Could not open OpenCL binary program file", info.fileName);

				// Get the file size:
				auto qtBytes = static_cast<std::streamsize> (ifs.seekg(0, std::ios::end).tellg());
				ifs.seekg(0, std::ios::beg); // rewind

				// Read the file:
				auto buffer = new uint8_t[qtBytes];
				binariesSmartPtrs.emplace_back(buffer);
				binariesCPtrs.push_back(buffer);
				binariesSizes.push_back(qtBytes);
				ifs.read(reinterpret_cast<char *> (buffer), qtBytes);
				_ASSERTE(ifs.gcount() == qtBytes); // it has to read all the file at once

				if (ifs.bad())
					throw core::AppException<std::runtime_error>("Failure when reading OpenCL binary program file", info.fileName);
			}

			// this will receive status of creating a program for loaded binary:
			std::vector<cl_int> binariesStatus(devicesIds.size());

			// Finally create the program:
			OPENCL_IMPORT(clCreateProgramWithBinary);
			cl_int status;
			cl_program program = clCreateProgramWithBinary(m_context, 
				                                           devicesIds.size(), 
				                                           devicesIds.data(), 
				                                           binariesSizes.data(), 
				                                           binariesCPtrs.data(), 
				                                           binariesStatus.data(),
				                                           &status);

			// Release some significant amount of memory:
			binariesSmartPtrs.clear();
			binariesCPtrs.clear(); // this here just to avoid dangling pointers :)

			// How many and what loaded binaries were successfull for creating the program?
			if (status == CL_SUCCESS || status == CL_INVALID_BINARY)
			{
				std::ostringstream oss;

				// Check the build of each binary:
				int successCount(0);
				for (int idx = 0; idx < binariesStatus.size(); ++idx)
				{
					auto devHashCode = devicesHashCodes[idx]; // hash code for the device associated with the binary
					auto &devInfo = m_devicesInfo.find(devHashCode)->second; // information regarding such device
					auto binSta = binariesStatus[idx]; // the status of program creation for such binary

					if (binSta == CL_SUCCESS)
					{// Log success with the binary for this device:
						++successCount;

						oss << "Successfully created from binary files the OpenCL program \'" << manifest.GetProgramName()
							<< "\' for the device \"" << devInfo.info.vendorName
							<< " / " << devInfo.info.deviceName
							<< " [" << devInfo.info.driverVersion << "]\"";

						core::Logger::Write(oss.str(), core::Logger::PRIO_INFORMATION);
						oss.str("");
					}
					else
					{// Log failure with the binary for this device:
						oss << "Could not create from binary files the OpenCL program \'" << manifest.GetProgramName()
							<< "\' for the device \"" << devInfo.info.vendorName
							<< " / " << devInfo.info.deviceName
							<< " [" << devInfo.info.driverVersion << "]\": invalid binary";

						core::Logger::Write(oss.str(), core::Logger::PRIO_WARNING);
						oss.str("");
					}
				}

				// If no loaded binary could be used:
				if (successCount == 0)
				{
					oss << "Could not create OpenCL program \'" << manifest.GetProgramName()
						<< "\' from binary files because there was no match between the devices"
						   " in the current context and the ones described in the manifest file.";

					core::Logger::Write(oss.str(), core::Logger::PRIO_ERROR);
					return nullptr;
				}
			}
			else // in case of a more serious error:
				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clCreateProgramWithBinary");

			return program;
		}
		catch (core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch (std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when creating OpenCL program from binaries: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Checks the status of the compilation process.
	/// </summary>
	/// <param name="program">The compiled OpenCL program.</param>
	void Context::CheckBuildStatus(cl_program program)
	{
		CALL_STACK_TRACE;

		// Get information from the building process:
		try
		{
			cl_int status;
			cl_build_status	buildStatus;
			OPENCL_IMPORT(clGetProgramBuildInfo);

			for (cl_uint index = 0; index < m_devices.size(); ++index)
			{
				status = clGetProgramBuildInfo(program,
					                           m_devices[index],
					                           CL_PROGRAM_BUILD_STATUS,
					                           sizeof(cl_build_status),
					                           static_cast<void *> (&buildStatus),
					                           nullptr);

				openclErrors.RaiseExceptionWhen(status, "OpenCL API: clGetProgramBuildInfo");

				if (buildStatus == CL_BUILD_ERROR)
				{
					std::vector<char> buildLog(core::AppConfig::GetSettings().framework.opencl.maxBuildLogSize);

					size_t qtCharacters;

					status = clGetProgramBuildInfo(program,
						                           m_devices[index],
						                           CL_PROGRAM_BUILD_LOG,
						                           sizeof(char) * buildLog.size(),
						                           static_cast<void *> (buildLog.data()),
						                           &qtCharacters);

					throw core::AppException<std::runtime_error>("Failed to build an OpenCL program", string(buildLog.data()));
				}
			}
		}
		catch (core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch (std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when retrieving OpenCL program build information: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Gets a device from the current context.
	/// </summary>
	/// <param name="index">The index of the device to get.</param>
	/// <param name="properties">The properties for the command queue of the device.</param>
	/// <returns>A new <see cref="Device" /> object.</returns>
	std::unique_ptr<Device> Context::GetDevice(cl_uint index, cl_command_queue_properties properties) const
	{
		CALL_STACK_TRACE;

		_ASSERTE(m_devices.size() > index); // The specified device index is not valid

		try
		{
			return std::unique_ptr<Device>(
				new Device(m_devices[index], m_context, properties)
			);
		}
        catch (core::IAppException &)
        {
            throw; // just forward the exceptions referring to error know to have been previously handled
        }
		catch (std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when getting an OpenCL device: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Builds an OpenCL program from its source code.
	/// </summary>
	/// <param name="sourceCodeFilePath">Name of the source code file.</param>
	/// <param name="buildOptions">The build options.</param>
	/// <returns>A compiled OpenCL program.</returns>
	std::unique_ptr<Program> Context::BuildProgramFromSource(const string &sourceCodeFilePath, const string &buildOptions)
	{
		CALL_STACK_TRACE;

		try
		{
			cl_program program = CreateProgramFromSourceCode(sourceCodeFilePath);

			// Build the program for all the devices associated to the context:
			OPENCL_IMPORT(clBuildProgram);
			cl_int status = clBuildProgram(program,
				                           0,
				                           nullptr, // build for all devices referred by the program
				                           buildOptions.c_str(), 
				                           nullptr, // no callback, because this call is synchronous
				                           nullptr);

			// Check the status of the build process:
			if (status != CL_SUCCESS)
				CheckBuildStatus(program);

			return std::unique_ptr<Program>(new Program(program));
		}
		catch (core::IAppException &)
		{
			throw; // just forward the exceptions referring to error know to have been previously handled
		}
		catch (std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when building OpenCL program from source: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Builds an OpenCL program from its binaries.
	/// </summary>
	/// <param name="manifestFilePath">The file path to the program manifest.</param>
	/// <param name="buildOptions">The build options.</param>
	/// <returns>
	/// An OpenCL program retrieved from previously compiled code, or 'nullptr' if 
	/// the devices described in the manifest did not match the ones in the current context.
	/// </returns>
	/// <remarks>
	/// The manifest has XML content describing the devices for which the program has previously been compiled, 
	/// and the names of such binary files. This method will load only the binaries for devices that exist in 
	/// the current context. If any loaded binary is no longer valid for its device for some reason (eg.: driver 
	/// update), that is reported in the log and the resulting program will lack a binary for such device. When 
	/// none of the loaded binaries is valid, that is considered and error and than the method throws an exception.
	/// </remarks>
	std::unique_ptr<Program> Context::BuildProgramWithBinaries(const string &manifestFilePath, const string &buildOptions)
	{
		CALL_STACK_TRACE;

		try
		{
			cl_program program = CreateProgramWithBinaries(manifestFilePath);

			// Build the program for all the devices associated to the context:
			OPENCL_IMPORT(clBuildProgram);
			cl_int status = clBuildProgram(program,
				                           0,
				                           nullptr, // build for all devices referred by the program
				                           buildOptions.c_str(), 
				                           nullptr, // no callback, because this call is synchronous
				                           nullptr);

			// Check the status of the build process:
			if (status != CL_SUCCESS)
				CheckBuildStatus(program);

			return std::unique_ptr<Program>(new Program(program));
		}
		catch (core::IAppException &)
		{
			throw; // just forward the exceptions referring to error know to have been previously handled
		}
		catch (std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when building OpenCL program from binary files: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Creates an OpenCL buffer.
	/// </summary>
	/// <param name="nBytes">The size in bytes of the buffer.</param>
	/// <param name="flags">The flags that define whether the buffer is input and/or output and how it will be initialized.</param>
	/// <param name="hostPtr">Address for memory in the host, where data will either be copied from or stored in.</param>
	/// <returns>An OpenCL buffer (memory object).</returns>
	Buffer Context::CreateBuffer(size_t nBytes, cl_mem_flags flags, void *hostPtr)
	{
		CALL_STACK_TRACE;

		OPENCL_IMPORT(clCreateBuffer);
		cl_int status;
		cl_mem buffer = clCreateBuffer(m_context,
			                           flags,
			                           nBytes,
			                           hostPtr,
			                           &status);

		openclErrors.RaiseExceptionWhen(status, "OpenCL API: clCreateBuffer");

		return Buffer(buffer, nBytes);
	}

}// end of namespace opencl
}// end of namespace _3fd
