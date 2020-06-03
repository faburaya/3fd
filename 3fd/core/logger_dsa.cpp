//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include "configuration.h"
#include "exceptions.h"
#include "logger.h"

#include "3fd/utils/utils_io.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace _3fd
{
namespace core
{
    /// <summary>
    /// Implements the contract of <see cref="ILogFileAccess"/> while hiding
    /// the particular implementation for IO access directly to the system.
    /// </summary>
    class DirectSystemFileAccess : public ILogFileAccess
    {
    private:

        const std::filesystem::path m_filePath;
        
        std::ofstream m_fileStream;

        void OpenStream(std::ofstream &ofs)
        {
            ofs.open(m_filePath.string(), std::ios::app | std::ios::out);
            if (ofs.is_open() == false)
                throw AppException<std::runtime_error>("Could not open text log file", m_filePath.string());
        }

    public:

        DirectSystemFileAccess(const std::string &filePath)
            : m_filePath(filePath)
        {
            OpenStream(m_fileStream);
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
            m_fileStream.close(); // first close the stream to the current log file

            using std::chrono::system_clock;
            auto now = system_clock::to_time_t(system_clock::now());

            using namespace std::filesystem;

            std::array<char, 265> pathBuffer;
            utils::SerializeTo(pathBuffer,
                m_filePath.parent_path().string(),
                path::preferred_separator,
                m_filePath.stem().string(),
                '[', ctime(&now), "].log.txt");

            path archivedLogFilePath = pathBuffer.data();

            try
            {
                rename(m_filePath, archivedLogFilePath);
            }
            catch (filesystem_error &)
            {
                std::ostringstream oss;
                oss << m_filePath.string() << " -> " << archivedLogFilePath.string();
                throw AppException<std::runtime_error>("Failed to shift log file", oss.str());
            }

            OpenStream(m_fileStream); // start new file
        }

        uint64_t GetFileSize() const override
        {
            try
            {
                return std::filesystem::file_size(m_filePath);
            }
            catch (std::filesystem::filesystem_error &ex)
            {
                throw AppException<std::runtime_error>("Failed to get size of log file",
                                                       StdLibExt::GetDetailsFromSystemError(ex.code()));
            }
        }
    };

    std::unique_ptr<ILogFileAccess> GetFileAccess(const string &loggerId)
    {
        return std::unique_ptr<ILogFileAccess>(dbg_new DirectSystemFileAccess(loggerId + ".log.txt"));
    }

}// end of namespace core
}// end of namespace _3fd
