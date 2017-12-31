#include "stdafx.h"
#include "opencl_impl.h"
#include "callstacktracer.h"
#include "logger.h"

namespace _3fd
{
    namespace opencl
    {
        //////////////////////////////
        // CommandTracker Class
        //////////////////////////////


        /// <summary>
        /// Initializes a new instance of the <see cref="CommandTracker"/> class.
        /// </summary>
        /// <param name="device">The OpenCL device whose commands are to be tracked.</param>
        CommandTracker::CommandTracker(Device &device)
        try    : 
            m_device(device), 
            m_cmdsAccessMutex() 
        {}
        catch (std::system_error &ex)
        {
            std::ostringstream oss;
            oss << "System error when creating command tracker for OpenCL device: " 
                << core::StdLibExt::GetDetailsFromSystemError(ex);
            throw core::AppException<std::runtime_error>(oss.str());
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="CommandTracker"/> class.
        /// </summary>
        CommandTracker::~CommandTracker()
        {
            CALL_STACK_TRACE;

            /* Releases only the resources allocated by this class. The resources originated from
            the OpenCL implementation should be freed somewhere else. For instance, the callback 
            used in the completion of the events is fit for such purpose. */
            try
            {
                if (m_cmdsByEvent.empty() == false)
                {
                    std::vector<cl_event> events;
                    events.reserve(m_cmdsByEvent.size());

                    for (auto &pair : m_cmdsByEvent)
                        events.push_back(pair.second->event.GetHandle());

                    OPENCL_IMPORT(clWaitForEvents);
                    cl_int status = clWaitForEvents(events.size(), events.data());

                    // If the await for the events failed for whatever reason, free the memory previously allocated now:
                    openclErrors.RaiseExceptionWhen(status, "OpenCL API: clWaitForEvents");
                }
            }
            catch (core::IAppException &ex)
            {
                core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when releasing the resources of an OpenCL device command queue: " << ex.what();
                auto what = oss.str();
                oss.str("");

                std::array<char, 32> devName;
                GenericParam param;
                param.Set(devName.data(), devName.size());
                m_device.GetDeviceInfo(CL_DEVICE_NAME, param);
                oss << "Device name is " << devName.data();
                auto details = oss.str();

                core::Logger::Write(what, details, core::Logger::PRIO_CRITICAL, true);
            }
        }

        /// <summary>
        /// Gets all distinct events of a given type that are blocking a resource.
        /// </summary>
        /// <param name="memResource">The blocked memory resource, that might be either a host memory address or a buffer object.</param>
        /// <param name="resourceUse">The use of the memory resource, which is specified in the enumeration <see cref="MemResourceUse" />.</param>
        /// <param name="blockerEvents">
        /// A vector of events provided by the caller, to which will be added all the events from commands that 
        /// are blocking the specified resource. If an event is already part of it, it will not be added twice.
        /// </param>
        void CommandTracker::GetDistinct(
            void *memResource,
            MemResourceUse resourceUse,
            std::vector<CommandEvent> &blockerEvents) const
        {
            CALL_STACK_TRACE;

            try
            {
                _ASSERTE(memResource != nullptr); // Cannot get the blocker events when no resource was specified
                std::lock_guard<std::mutex> lock(m_cmdsAccessMutex);

                // Create a set of distinct events:
                std::set<cl_event> events;
                for (auto &ev : blockerEvents)
                    events.insert(ev.GetHandle());

                auto &blockerCmds =
                    (resourceUse == MemResourceUse::Input || resourceUse == MemResourceUse::InputAndOutput)
                    ? m_cmdsByRdResource
                    : m_cmdsByWrResource;

                // Now look for other events blocking the same resource:
                auto lowerBound = blockerCmds.lower_bound(memResource);
                auto upperBound = blockerCmds.upper_bound(memResource);

                for_each(lowerBound, upperBound, [resourceUse, &events](const decltype(*lowerBound) &pair)
                {
                    if (pair.second->resourceUse == resourceUse)
                        events.insert(pair.second->event.GetHandle());
                });

                std::vector<CommandEvent> temp;
                temp.reserve(events.size());
                for (auto ev : events)
                    temp.emplace_back(ev);

                blockerEvents.swap(temp);
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "System error when getting distinct tracked OpenCL events: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when getting distinct tracked OpenCL events: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Remembers a blocked host memory address.
        /// </summary>
        /// <param name="memResource">The blocked memory resource, that might be either a host memory address or a buffer object.</param>
        /// <param name="resourceUse">The use of the memory resource, which is specified in the enumeration <see cref="MemResourceUse"/>.</param>
        /// <param name="cmdEvent">The command event to wait.</param>
        void CommandTracker::Remember(
            void *memResource, 
            MemResourceUse resourceUse, 
            const CommandEvent &cmdEvent)
        {
            CALL_STACK_TRACE;

            try
            {
                auto blockingCmd = std::make_shared<Command>(memResource, resourceUse, cmdEvent);

                std::lock_guard<std::mutex> lock(m_cmdsAccessMutex);

                // Store this data so as to keep track of blocked resources:

                if (resourceUse == MemResourceUse::Input || resourceUse == MemResourceUse::InputAndOutput)
                    m_cmdsByRdResource.emplace(memResource, blockingCmd);

                if (resourceUse == MemResourceUse::Output || resourceUse == MemResourceUse::InputAndOutput)
                    m_cmdsByWrResource.emplace(memResource, blockingCmd);

                m_cmdsByEvent.emplace(cmdEvent.GetHandle(), blockingCmd);
            }
            catch (core::IAppException &)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "System error when tracking OpenCL events: "
                    << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
            catch (std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when tracking OpenCL event: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Forgets a blocker command.
        /// </summary>
        /// <param name="completedEvent">The event of the command to be forgotten.</param>
        void CommandTracker::Forget(cl_event completedEvent)
        {
            try
            {
                CALL_STACK_TRACE;

                std::lock_guard<std::mutex> lock(m_cmdsAccessMutex);

                auto beginIter = m_cmdsByEvent.lower_bound(completedEvent);
                auto endIter = m_cmdsByEvent.upper_bound(completedEvent);

                for_each(beginIter, endIter, [this, completedEvent](decltype(*beginIter) &pair)
                {
                    auto resourceUse = pair.second->resourceUse;
                    auto memResource = pair.second->memResource;

                    if (resourceUse == MemResourceUse::Input || resourceUse == MemResourceUse::InputAndOutput)
                    {
                        auto upperBound = m_cmdsByRdResource.upper_bound(memResource);
                        auto lowerBound = m_cmdsByRdResource.lower_bound(memResource);
                        auto innerIter = lowerBound;

                        while (innerIter != upperBound)
                        {
                            if (innerIter->second->event.GetHandle() == completedEvent)
                            {
                                auto next = std::next(innerIter);
                                m_cmdsByRdResource.erase(innerIter);
                                innerIter = next;
                            }
                            else
                                ++innerIter;
                        }
                    }

                    if (resourceUse == MemResourceUse::Output || resourceUse == MemResourceUse::InputAndOutput)
                    {
                        auto upperBound = m_cmdsByWrResource.upper_bound(memResource);
                        auto lowerBound = m_cmdsByWrResource.lower_bound(memResource);
                        auto innerIter = lowerBound;

                        while (innerIter != upperBound)
                        {
                            if (innerIter->second->event.GetHandle() == completedEvent)
                            {
                                auto next = std::next(innerIter);
                                m_cmdsByWrResource.erase(innerIter);
                                innerIter = next;
                            }
                            else
                                ++innerIter;
                        }
                    }
                });

                m_cmdsByEvent.erase(beginIter, endIter);
            }
            catch (std::system_error &ex)
            {
                std::ostringstream oss;
                oss << "Failed to acquire lock before removing a tracked OpenCL event: " 
                    << core::StdLibExt::GetDetailsFromSystemError(ex);
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

    }// end of namespace opencl
}// end of namespace _3fd
