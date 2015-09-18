#include "stdafx.h"
#include "gc.h"
#include "gc_common.h"
#include "gc_messages.h"

#include "utils.h"
#include "logger.h"
#include "exceptions.h"
#include "callstacktracer.h"
#include "configuration.h"

#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <condition_variable>

namespace _3fd
{
	using std::string;
	using std::for_each;

	namespace memory
	{
		using core::AppConfig;
		using core::AppException;

		/// <summary>
		/// Allocates memory (aligned in 2 bytes) and registers it with the GC.
		/// </summary>
		/// <param name="size">The size of the memory block to allocated.</param>
		/// <param name="sptrObjAddr">The address of the smart pointer that will refer to the same memory.</param>
		/// <param name="freeMemCallback">The callback that must be used to free the allocated memory.</param>
		/// <returns>The address of the allocated memory.</returns>
		void *AllocMemoryAndRegisterWithGC(size_t size, 
										   void *sptrObjAddr, 
										   FreeMemProc freeMemCallback)
		{
#	ifdef _WIN32
			void *ptr = _aligned_malloc(size, 2);
#	else
			void *ptr = aligned_alloc(2, size);
#	endif
			if(ptr != nullptr)
				GarbageCollector::GetInstance()
					.RegisterNewObject(sptrObjAddr, ptr, size, freeMemCallback);
			else
				throw AppException<std::runtime_error>("Failed to allocated collectable memory");

			return ptr;
		}

		GarbageCollector * GarbageCollector::uniqueObjectPtr(nullptr);

		std::mutex GarbageCollector::singleInstanceCreationMutex;

		/// <summary>
		/// Creates the unique instance of the <see cref="GarbageCollector" /> class.
		/// </summary>
		/// <returns></returns>
		GarbageCollector * GarbageCollector::CreateInstance()
		{
			CALL_STACK_TRACE;

			try
			{
				std::lock_guard<std::mutex> lock(singleInstanceCreationMutex);

				if(uniqueObjectPtr == nullptr)
					uniqueObjectPtr = new GarbageCollector();

				return uniqueObjectPtr;
			}
			catch(core::IAppException &)
			{
				throw; // just forward exceptions known to have been previously handled
			}
			catch(std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed to instantiate the garbage collector engine: " << core::StdLibExt::GetDetailsFromSystemError(ex);
				throw AppException<std::runtime_error>(oss.str());
			}
			catch(std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when instantiating the garbage collector engine: " << ex.what();
				throw AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Gets the unique instance of the <see cref="GarbageCollector" /> class.
		/// </summary>
		/// <returns>A reference to the unique instance.</returns>
		GarbageCollector & GarbageCollector::GetInstance()
		{
			if(uniqueObjectPtr != nullptr)
				return *uniqueObjectPtr;
			else
				return *CreateInstance();
		}

		/// <summary>
		/// Shuts down the garbage collector releasing all associated resources.
		/// </summary>
		void GarbageCollector::Shutdown()
		{
			if(uniqueObjectPtr != nullptr)
			{
				try
				{
					std::lock_guard<std::mutex> lock(singleInstanceCreationMutex);
					delete uniqueObjectPtr;
					uniqueObjectPtr = nullptr;
				}
				catch (std::system_error &)
				{/* DO NOTHING: SWALLOW EXCEPTION
					This method cannot throw an exception because it can be invoked by a destructor.
					If an exception is thrown, memory leaks are expected. */
				}
			}
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="GarbageCollector"/> class.
		/// </summary>
		GarbageCollector::GarbageCollector() 
		try : 
			m_error(nullptr), 
			m_masterTable(), 
			m_messagesQueue(/*AppConfig::GetSettings().framework.gc.msgQueueInitCap*/), 
			m_terminationEvent()
		{
			CALL_STACK_TRACE;

			//_ASSERTE(m_messagesQueue.is_lock_free()); // Queue must be implemented lock-free

			// Create the GC dedicated thread
            std::thread temp(&GarbageCollector::GCThreadProc, this);
            m_thread.swap(temp);
		}
		catch(core::IAppException &)
		{
			throw; // just forward exceptions known to have been previously handled
		}
		catch(std::system_error &ex)
		{
			CALL_STACK_TRACE;
			std::ostringstream oss;
			oss << "Failed to create garbage collection thread: " << core::StdLibExt::GetDetailsFromSystemError(ex);
			throw AppException<std::runtime_error>(oss.str());
		}
		catch(std::exception &ex)
		{
			CALL_STACK_TRACE;
			std::ostringstream oss;
			oss << "Generic failure when creating garbage collector instance: " << ex.what();
			throw AppException<std::runtime_error>(oss.str());
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="GarbageCollector"/> class.
		/// </summary>
		GarbageCollector::~GarbageCollector()
		{
			CALL_STACK_TRACE;

			try
			{
				// Signalizes termination for the message loop
				m_terminationEvent.Signalize();

				if( m_thread.joinable() )
					m_thread.join();

				if(m_error != nullptr)
					std::rethrow_exception(m_error);
			}
			catch(core::IAppException &ex)
			{
				core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
			}
			catch(std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed when attempted to stop garbage collection thread: "
					<< core::StdLibExt::GetDetailsFromSystemError(ex);

				core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL, true);
			}
			catch(std::exception &ex)
			{
				core::Logger::Write(ex.what(), core::Logger::PRIO_CRITICAL, true);
			}
		}

		/// <summary>
		/// Executed by the GC dedicated thread.
		/// </summary>
		void GarbageCollector::GCThreadProc()
		{
			CALL_STACK_TRACE;

			try
			{
				bool terminate(false);

				// The message loop:
				do
				{
					// Wait for either the termination event or a timeout
					terminate = m_terminationEvent.WaitFor(
						AppConfig::GetSettings().framework.gc.msgLoopSleepTimeoutMilisecs
					);

					// Consume the messages in the queue:
					IMessage *message = m_messagesQueue.Remove();
					while(message != nullptr)
					{
						message->Execute(m_masterTable);
						delete message;
						message = m_messagesQueue.Remove();
					}

					// If there is still work to do, optimize the master table
					if(terminate == false)
						m_masterTable.Shrink();
				}
				while(terminate == false);
			}
			catch(core::IAppException &ex)
			{
				core::Logger::Write(ex, core::Logger::PRIO_CRITICAL);
				m_error = std::current_exception();
			}
			catch(std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "There was an error in garbage collector thread: "
					<< core::StdLibExt::GetDetailsFromSystemError(ex);

				core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL);
				m_error = std::current_exception();
			}
			catch(std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure in garbage collector thread: " << ex.what();

				core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL);
				m_error = std::current_exception();
			}
		}

		void GarbageCollector::UpdateReference(void *sptrObjAddr, void *pointedAddr)
		{
			m_messagesQueue.Add(new ReferenceUpdateMsg(sptrObjAddr, pointedAddr));
		}

		void GarbageCollector::RegisterNewObject(void *sptrObjAddr, void *pointedAddr, size_t blockSize, FreeMemProc freeMemCallback)
		{
			m_messagesQueue.Add(new NewObjectMsg(sptrObjAddr, pointedAddr, blockSize, freeMemCallback));
		}

		void GarbageCollector::UnregisterAbortedObject(void *sptrObjAddr)
		{
			m_messagesQueue.Add(new AbortedObjectMsg(sptrObjAddr));
		}

		void GarbageCollector::RegisterSptr(void *sptrObjAddr, void *pointedAddr)
		{
			m_messagesQueue.Add(new SptrRegistrationMsg(sptrObjAddr, pointedAddr));
		}

		void GarbageCollector::UnregisterSptr(void *sptrObjAddr)
		{
			m_messagesQueue.Add(new SptrUnregistrationMsg(sptrObjAddr));
		}

	}// end of namespace memory
}// end of namespace _3fd

