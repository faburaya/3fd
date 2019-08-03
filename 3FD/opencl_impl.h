#ifndef OPENCL_IMPL_H
#define OPENCL_IMPL_H

#include "opencl.h"
#include "exceptions.h"
#include <sstream>

// The following macro will be used to make easier to load and use OpenCL API functions
#define OPENCL_IMPORT(FUNCNAME)        auto FUNCNAME = reinterpret_cast<decltype(&::FUNCNAME)> (_3fd::opencl::ImportFromOpenCL(#FUNCNAME))

namespace _3fd
{
namespace opencl
{
    // Helper for importing procedures from the OpenCL DLL
    void *ImportFromOpenCL(const char *procName);

    // Helper for getting info from OpenCL device
    void GetDeviceInfoImpl(cl_device_id device, cl_device_info paramCode, GenericParam &param);

    /// <summary>
    /// Helper class to translate error codes from OpenCL calls into error messages.
    /// </summary>
    class OpenCLErrors
    {
    private:

        static std::mutex loadMessagesMutex;

        std::map<unsigned char, const char *> m_errorMessages;

        void LoadMessages();

        void EnsureInitialization(cl_int status, const char *details = nullptr);

    public:

        void LogErrorWhen(cl_int status, const char *details, core::Logger::Priority prio);

        void RaiseExceptionWhen(cl_int status, const char *details = nullptr);

        void RaiseExceptionWhen(cl_int status, const char *details, core::IAppException &&innerEx);
    };

    extern OpenCLErrors openclErrors;

    /// <summary>
    /// Represents an OpenCL program manifest.
    /// </summary>
    class ProgramManifest
    {
    public:

        /// <summary>
        /// Holds key information regarding an OpenCL device program.
        /// </summary>
        struct DeviceProgramInfo
        {
            string fileName;
            DeviceInfo deviceInfo;

            DeviceProgramInfo(
                cl_device_id deviceId, 
                const string &fileNamePrefix, 
                std::ostringstream &oss);

            /// <summary>
            /// Initializes a new instance of the <see cref="DeviceProgramInfo"/> struct.
            /// </summary>
            DeviceProgramInfo() {}

            /// <summary>
            /// Initializes a new instance of the <see cref="DeviceProgramInfo"/> struct using move semantics.
            /// </summary>
            /// <param name="ob">The object whose resources will be stolen.</param>
            DeviceProgramInfo(DeviceProgramInfo &&ob) noexcept
                : fileName(std::move(ob.fileName))
                , deviceInfo(std::move(ob.deviceInfo)) 
            {}
        };

    private:

        string m_programName;

        std::vector<DeviceProgramInfo> m_devicesInfo;

        /// <summary>
        /// Prevents a default instance of the <see cref="ProgramManifest"/> class from being created.
        /// </summary>
        ProgramManifest() {}

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="ProgramManifest"/> class using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources will be stolen.</param>
        ProgramManifest(ProgramManifest &&ob) noexcept
            : m_programName(std::move(ob.m_programName))
            , m_devicesInfo(std::move(ob.m_devicesInfo))
        {}

        ProgramManifest(const ProgramManifest &) = delete;

        static ProgramManifest CreateObject(const string &programName, const std::vector<cl_device_id> &devices);

        static ProgramManifest LoadFrom(const string &filePath);

        /// <summary>
        /// Gets the name of the OpenCL program.
        /// </summary>
        /// <returns>The program name.</returns>
        const string &GetProgramName() const { return m_programName; }

        /// <summary>
        /// Provides (read-only) information regarding the device programs.
        /// </summary>
        /// <returns>A reference to a collection of information regarding the device programs.</returns>
        const std::vector<DeviceProgramInfo> &GetDeviceProgramsInfo() const { return m_devicesInfo; }

        string SaveTo(const string &directory);
    };

}// end of namespace opencl
}// end of namespace _3fd

#endif // header guard
