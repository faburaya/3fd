//
// Copyright (c) 2020 Part of 3FD project (https://github.com/faburaya/3fd)
// It is FREELY distributed by the author under the Microsoft Public License
// and the observance that it should only be used for the benefit of mankind.
//
#include "pch.h"
#include "utils_concurrency.h"
#include <3fd/core/exceptions.h>

#include <cassert>
#include <sstream>

namespace _3fd
{
namespace utils
{
    /// <summary>
    /// Initializes a new instance of the <see cref="Event"/> class.
    /// </summary>
    Event::Event()
    try:
        m_mutex(), // might throw an exception
        m_condition(), // might throw an exception
        m_flag(false)
    {
    }
    catch(std::system_error &ex)
    {
        std::ostringstream oss;
        oss << "Failed to create an event for thread synchronization: " << core::StdLibExt::GetDetailsFromSystemError(ex);
        throw core::AppException<std::runtime_error>(oss.str());
    }

    /// <summary>
    /// Sets the event.
    /// This will wake only a single listener!
    /// </summary>
    void Event::Signalize()
    {
        {// sets a flag that is later checked for signal confirmation
            std::lock_guard<std::mutex> lock(m_mutex);
            m_flag = true;
        }
        m_condition.notify_one();
    }

    /// <summary>
    /// Wait for the event to be set along with an approval from the predicate.
    /// The predicate might not approve the context if, for example, the event was set before the 
    /// callee starts to wait, and by the time the callee is made aware of the previous notification, 
    /// the context has already changed and this previous notification is no longer valid.
    /// </summary>
    /// <param name="predicate">The predicate that approves the context.</param>
    void Event::Wait(const std::function<bool()> &predicate)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
            
        m_condition.wait(lock, [this, &predicate]()
        {
            if (m_flag)
            {
                m_flag = false;
                return predicate();
            }
            else // spurius wake-up?
                return false;
        });
    }

    /// <summary>
    /// Wait for the event to be set or a timeout.
    /// </summary>
    /// <param name="millisecs">The timeout in milliseconds.</param>
    /// <returns>'true' if the event was set, 'false' if a timeout happens first.</returns>
    bool Event::WaitFor(unsigned long millisecs)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        return m_flag ||
            m_condition.wait_for(lock, 
                std::chrono::milliseconds(millisecs), 
                [this]()
                {
                    if (m_flag)
                    {
                        m_flag = false;
                        return true;
                    }

                    return false;
                });
    }

} // end of namespace utils
} // end of namespace _3fd
