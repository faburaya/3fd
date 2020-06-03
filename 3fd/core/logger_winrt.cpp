#include "pch.h"
#include "logger.h"
#include "configuration.h"
#include "callstacktracer.h"

#include <3fd/utils/utils_winrt.h>
#include <winrt\Windows.Storage.h>
#include <winrt\Windows.Storage.FileProperties.h>
#include <winrt\Windows.Storage.Compression.h>
#include <array>
#include <codecvt>
#include <future>
#include <fstream>
#include <sstream>
#include <chrono>

namespace _3fd
{
namespace core
{
    /// <summary>
    /// Implements the contract of <see cref="ILogFileAccess"/> while
    /// hiding the particular implementation for WinRT platform.
    /// </summary>
    class WinRtFileAccess : public ILogFileAccess
    {
    private:

        using WinRT_StorageFile = winrt::Windows::Storage::StorageFile;

        WinRT_StorageFile m_logFile;
        std::ofstream m_fileStream;

        static void OpenStreamImpl(const WinRT_StorageFile &logFile, std::ofstream &ofs)
        {
            ofs.open(logFile.Path().c_str(), std::ios::app | std::ios::out);
            if (ofs.is_open() == false)
                throw AppException<std::runtime_error>("Could not open log file");
        }

    public:

        WinRtFileAccess(WinRT_StorageFile &logFile)
            : m_logFile(logFile)
        {
            OpenStreamImpl(m_logFile, m_fileStream);
        }

        std::ostream &GetStream() override
        {
            return m_fileStream;
        }

        bool HasError() const override
        {
            return m_fileStream.bad();
        }

        void ShiftToNewLogFile() override
        {
            using namespace std::chrono;
            using namespace winrt::Windows::Storage;
            using namespace winrt::Windows::Foundation;

            m_fileStream.close(); // first close the stream to the current log file

            // Rename the current log file:
            winrt::hstring currLogFileName = m_logFile.Name();
            m_logFile.RenameAsync(m_logFile.Path() + L".old").get();

            // Start reading log file to buffer:
            auto asyncOperReadLogToBuffer = FileIO::ReadBufferAsync(m_logFile);

            // Create a new log file:
            m_logFile = ApplicationData::Current().LocalFolder().CreateFileAsync(currLogFileName).get();

            OpenStreamImpl(m_logFile, m_fileStream);

            // Create the file which will contain the previous log (compressed):
            auto now = system_clock::to_time_t(system_clock::now());
            std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
            std::wostringstream woss;
            woss << m_logFile.DisplayName().c_str() << L'[' << transcoder.from_bytes(ctime(&now)) << L"].log.dat";

            // moves it to the app temporary data store
            StorageFile compressedLogFile =
                ApplicationData::Current().TemporaryFolder().CreateFileAsync(
                    woss.str(),
                    CreationCollisionOption::ReplaceExisting
                ).get();

            // Create a writable stream for such file:
            Streams::IRandomAccessStream outputStream =
                compressedLogFile.OpenAsync(FileAccessMode::ReadWrite).get();

            // Await for completion of reading operation:
            Streams::IBuffer readBuffer = asyncOperReadLogToBuffer.get();

            // Compress the text content of the previous log file:
            Compression::Compressor compressor(outputStream);
            compressor.WriteAsync(readBuffer).get();
            compressor.FinishAsync().get();
            compressor.FlushAsync().get();

            // Write log shift event in the new log:
            now = system_clock::to_time_t(system_clock::now());
            PrepareEventString(m_fileStream, now, Logger::PRIO_NOTICE)
                << L"The log file has been shifted. The previous file has been compressed from "
                << readBuffer.Length() / 1024 << L" to " << outputStream.Size() / 1024
                << L" KB and moved to the app temporary data store." << std::endl << std::flush;
        }

        uint64_t GetFileSize() const override
        {
            return m_logFile.GetBasicPropertiesAsync().get().Size();
        }
    };

    std::unique_ptr<ILogFileAccess> GetFileAccess(const string &loggerId)
    {
        using namespace winrt::Windows::Storage;

        // Open or create the file:
        winrt::hstring name = winrt::to_hstring(loggerId);
        StorageFile logFile = ApplicationData::Current().LocalFolder()
            .CreateFileAsync(name + L".log.txt", CreationCollisionOption::OpenIfExists).get();

        return std::unique_ptr<ILogFileAccess>(dbg_new WinRtFileAccess(logFile));
    }

}// end of namespace core
}// end of namespace _3fd
