#include "stdafx.h"
#include "opencl_impl.h"
#include "callstacktracer.h"
#include "logger.h"

#include <fstream>
#include "Poco/AutoPtr.h"
#include "Poco/SAX/InputSource.h"
#include "Poco/DOM/DOMParser.h"
#include "Poco/DOM/Document.h"
#include "Poco/DOM/NodeList.h"

namespace _3fd
{
	namespace opencl
	{
		///////////////////////////
		//	ProgramManifest Class
		///////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="DeviceProgramInfo" /> struct.
		/// </summary>
		/// <param name="deviceId">The device handle.</param>
		/// <param name="fileNamePrefix">The file name prefix.</param>
		/// <param name="oss">A reusable stream for string output, so as to help building the file name.</param>
		ProgramManifest::DeviceProgramInfo::DeviceProgramInfo(
			cl_device_id deviceId, 
			const string &fileNamePrefix, 
			std::ostringstream &oss) : deviceInfo(deviceId) 
		{
			oss.str("");
			oss << fileNamePrefix << '_' << deviceInfo.hashCode << ".bin";
			fileName = oss.str();
		}

		/// <summary>
		/// Creates a manifest object for an OpenCL program.
		/// </summary>
		/// <param name="programName">Name of the program.</param>
		/// <param name="devicesInfo">The devices ID's for which the program has been compiled.</param>
		/// <returns>A new instance of <see cref="ProgramManifest"/> with the information obtained from the devices.</returns>
		ProgramManifest ProgramManifest::CreateObject(const string &programName, const std::vector<cl_device_id> &devices)
		{
			CALL_STACK_TRACE;

			_ASSERTE(devices.empty() == false);

			try
			{
				ProgramManifest manifest;
				manifest.m_programName = programName;

				std::ostringstream oss;
				oss << "ocl_program_";
				for (char ch : programName)
					if (isalnum(ch))
						oss << ch;

				string prefix = oss.str();

				// For each device for which the program has been compiled:
				for (auto deviceId : devices)
					manifest.m_devicesInfo.emplace_back(deviceId, prefix, oss); // creates a new structure to gather info

				return std::move(manifest);
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure during creation of manifest content: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Saves the manifest to a XML file.
		/// </summary>
		/// <param name="directory">The directory where to save the file.</param>
		/// <returns>The program manifest file path.</returns>
		string ProgramManifest::SaveTo(const string &directory)
		{
			CALL_STACK_TRACE;

			_ASSERTE(directory.empty() == false); // must specify a directory to place the manifest file

			try
			{
				// Assemble the file path:
				std::ostringstream oss;
				oss << directory;          
#ifdef _WIN32
                const char dirSeparatorChar('\\');
#else
                const char dirSeparatorChar('/');
#endif
                if (*directory.rbegin() == dirSeparatorChar)
					oss << "ocl_manifest_";
				else
                    oss << dirSeparatorChar << "ocl_manifest_";

				for (char ch : m_programName)
					if (isalnum(ch))
						oss << ch;

				oss << ".xml";
				string fileName = oss.str();

				// Create the file:
				std::ofstream ofs(fileName, std::ios::trunc);
				if (ofs.is_open() == false)
					throw core::AppException<std::runtime_error>("Could not open or create the manifest file", fileName);

				// Begin writing XML content:
				ofs <<
					"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
					"<manifest>\n"
					"	<program name=\"" << m_programName << "\">\n";

				// Add information for all device programs:
				for (auto &devProgInfo : m_devicesInfo)
				{
					ofs <<
					"		<device name=\"" << devProgInfo.deviceInfo.deviceName << "\">\n"
					"			<vendor id=\"" << devProgInfo.deviceInfo.vendorId << "\">" << devProgInfo.deviceInfo.vendorName << "</vendor>\n"
					"			<driver>" << devProgInfo.deviceInfo.driverVersion << "</driver>\n"
					"			<file>" << devProgInfo.fileName << "</file>\n"
					"		</device>\n";
				}

				ofs <<
					"	</program>\n"
					"</manifest>" << std::flush; // flushes to disk

				if (ofs.bad())
					throw core::AppException<std::runtime_error>("Failure when writing manifest file", fileName);

				return fileName;
			}
            catch (core::IAppException &)
			{
				throw; // just forward exceptions known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when trying to save the manifest file: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Loads an OpenCL program manifest from the file.
		/// </summary>
		/// <param name="filePath">The manifest file path.</param>
		/// <returns>The program manifest.</returns>
		ProgramManifest ProgramManifest::LoadFrom(const string &filePath)
		{
			CALL_STACK_TRACE;

			try
			{
				ProgramManifest manifest;

				// Open the manifest file:
				std::ifstream ifs(filePath, std::ios::binary);
				if (ifs.is_open() == false)
					throw core::AppException<std::runtime_error>("Could not open manifest file", filePath);

                using Poco::XML::InputSource;

				// Parse the XML content:
				Poco::XML::DOMParser parser;
                std::unique_ptr<InputSource> xmlInput(new InputSource(ifs));
                Poco::AutoPtr<Poco::XML::Document> dom = parser.parse(xmlInput.get());

				auto node = dom->getNodeByPath("manifest/program");
				if (node == nullptr)
					throw core::AppException<std::runtime_error>("Manifest file has unexpected format", "Node \'program\' is missing");

				auto attr = node->getNodeByPath("[@name]");
				if (attr != nullptr)
					manifest.m_programName = attr->getNodeValue();
				else
					throw core::AppException<std::runtime_error>("Manifest file has unexpected format", "Node \'program\' is missing attribute \'name\'");

				// Iterate through the nodes 'device':
				for (auto iter = node->firstChild(); iter != nullptr; iter = iter->nextSibling())
				{
					if (iter->nodeName() != "device")
						continue;

					manifest.m_devicesInfo.emplace_back();
					auto &info = manifest.m_devicesInfo.back();

					// Device name:
					attr = iter->getNodeByPath("[@name]");
					if (attr != nullptr)
						info.deviceInfo.deviceName = attr->getNodeValue();
					else
						throw core::AppException<std::runtime_error>("Manifest file has unexpected format", "Node \'device\' is missing attribute \'name\'");

					// Vendor name:
					node = iter->getNodeByPath("/vendor");
					if (node != nullptr)
						info.deviceInfo.vendorName = node->innerText();
					else
						throw core::AppException<std::runtime_error>("Manifest file has unexpected format", "Node \'device\' is missing child \'vendor\'");

					// Vendor ID:
					attr = node->getNodeByPath("[@id]");
					if (attr == nullptr)
						throw core::AppException<std::runtime_error>("Manifest file has unexpected format", "Node \'vendor\' is missing attribute \'id\'");
					else
					{
						auto &id = attr->getNodeValue();
						info.deviceInfo.vendorId = strtoul(id.c_str(), nullptr, 10);

						if (info.deviceInfo.vendorId == 0)
							throw core::AppException<std::runtime_error>("Program manifest has invalid value", "Node \'vendor\' has invalid value in attribute \'id\'");
					}

					// Driver version:
					node = iter->getNodeByPath("/driver");
					if (node != nullptr)
						info.deviceInfo.driverVersion = node->innerText();
					else
						throw core::AppException<std::runtime_error>("Manifest file has unexpected format", "Node \'device\' is missing child \'driver\'");

					// Binary program file name:
					node = iter->getNodeByPath("/file");
					if (node != nullptr)
						info.fileName = node->innerText();
					else
						throw core::AppException<std::runtime_error>("Manifest file has unexpected format", "Node \'device\' is missing child \'file\'");

					info.deviceInfo.UpdateHashCode();
				}

				if (manifest.m_devicesInfo.empty() == false)
					return std::move(manifest);
				else
					throw core::AppException<std::runtime_error>("Manifest file has unexpected format", "Node \'program\' has no devices");
			}
            catch (core::IAppException &)
			{
				throw; // just forward exceptions known to have been previously handled
			}
			catch (Poco::Exception &ex)
			{
				std::ostringstream oss;
				oss << "POCO C++ library reported: " << ex.message();
				throw core::AppException<std::runtime_error>("Failed to parse XML content in manifest", oss.str());
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when loading manifest: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

	}// end of namespace opencl
}// end of namespace _3fd
