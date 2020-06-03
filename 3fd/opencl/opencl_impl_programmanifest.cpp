//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include "opencl_impl.h"
#include <3fd/core/callstacktracer.h>
#include <3fd/utils/xml.h>

namespace _3fd
{
namespace opencl
{
    ///////////////////////////
    //  ProgramManifest Class
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
                "    <program name=\"" << m_programName << "\">\n";

            // Add information for all device programs:
            for (auto &devProgInfo : m_devicesInfo)
            {
                ofs <<
                "        <device name=\"" << devProgInfo.deviceInfo.deviceName << "\">\n"
                "            <vendor id=\"" << devProgInfo.deviceInfo.vendorId << "\">" << devProgInfo.deviceInfo.vendorName << "</vendor>\n"
                "            <driver>" << devProgInfo.deviceInfo.driverVersion << "</driver>\n"
                "            <file>" << devProgInfo.fileName << "</file>\n"
                "        </device>\n";
            }

            ofs <<
                "    </program>\n"
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
            std::vector<char> buffer;
            rapidxml::xml_document<char> xmlDocObj;

            auto rootNode = xml::ParseXmlFromFile(filePath, buffer, xmlDocObj, "manifest");
            if (rootNode == nullptr)
            {
                throw core::AppException<std::runtime_error>("Manifest file has unexpected format",
                                                             "Root node 'manifest' is missing");
            }

            ProgramManifest manifest;

            const rapidxml::xml_node<char> *elementDevice(nullptr);

            auto query = xml::QueryElement("manifest", xml::Required, {
                xml::QueryElement("program", xml::Required, {
                    xml::QueryAttribute("name", xml::Required, xml::parse_into(manifest.m_programName)),
                    xml::QueryElement("device", xml::Required, {}, &elementDevice)
                })
            });

            if (!query->Execute(rootNode, xml::QueryStrategy::TestsOnlyGivenElement))
            {
                std::ostringstream oss;
                oss << "Could not match XML query looking for" _newLine_ _newLine_;
                query->SerializeTo(2, oss);
                throw core::AppException<std::runtime_error>("XML manifest of OpenCL program is not compliant", oss.str());
            }

            // Iterate through the nodes 'device':
            do
            {
                manifest.m_devicesInfo.emplace_back();
                auto &info = manifest.m_devicesInfo.back();

                query = xml::QueryElement("device", xml::Required, {
                    xml::QueryAttribute("name", xml::Required, xml::parse_into(info.deviceInfo.deviceName)),
                    xml::QueryElement("driver", xml::Required, xml::parse_into(info.deviceInfo.driverVersion)),
                    xml::QueryElement("file", xml::Required, xml::parse_into(info.fileName)),
                    xml::QueryElement("vendor", xml::Required, xml::parse_into(info.deviceInfo.vendorName), xml::NoFormat(), {
                        xml::QueryAttribute("id", xml::Required, xml::parse_into(info.deviceInfo.vendorId))
                    })
                });

                if (!query->Execute(elementDevice, xml::QueryStrategy::TestsOnlyGivenElement))
                {
                    std::ostringstream oss;
                    oss << "Could not load invalid definition of OpenCL device for program '" << manifest.m_programName
                        << "' from manifest." _newLine_
                           "Failed to match XML query looking for" _newLine_ _newLine_;

                    query->SerializeTo(2, oss);
                    throw core::AppException<std::runtime_error>("XML manifest for OpenCL proram has unexpected format", oss.str());
                }

                info.deviceInfo.UpdateHashCode();

                elementDevice = xml::GetNextSiblingOf(elementDevice, xml::xstr("device"));

            } while (elementDevice != nullptr);

            return std::move(manifest);
        }
        catch (core::IAppException &)
        {
            throw; // just forward exceptions known to have been previously handled
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
