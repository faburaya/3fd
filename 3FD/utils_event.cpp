#include "stdafx.h"
#include "utils.h"
#include "exceptions.h"

#include <cassert>
#include <sstream>

namespace _3fd
{
    namespace utils
    {
        ///////////////////////////////////////
        // Event Class
        ///////////////////////////////////////

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
        /// </summary>
        void Event::Signalize()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_condition.notify_all();
            m_flag = true;
        }

        /// <summary>
        /// Wait for the event to be set along with a approval from the predicate.
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
                else
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

            return m_flag || m_condition.wait_for(lock, 
                                                  std::chrono::milliseconds(millisecs), 
                                                  [this]() { return m_flag; });
        }

    } // end of namespace utils
} // end of namespace _3fd
