//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include "logger.h"

namespace _3fd
{
namespace core
{
    /// <summary>
    /// Implements the contract of <see cref="ILogFileAccess"/> while hiding
    /// the particular implementation for IO access directly to the system.
    /// </summary>
    class StandardOutputAccess : public ILogFileAccess
    {
    public:

        std::ostream &GetStream() override
        {
            return std::cout;
        }

        bool HasError() const override
        {
            return false;
        }

        void ShiftToNewLogFile() override
        {
        }

        uint64_t GetFileSize() const override
        {
            return 4096;
        }
    };

    std::unique_ptr<ILogFileAccess> GetConsoleAccess()
    {
        return std::unique_ptr<ILogFileAccess>(dbg_new StandardOutputAccess());
    }

}// end of namespace core
}// end of namespace _3fd
