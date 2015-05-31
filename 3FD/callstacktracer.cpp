#include "stdafx.h"
#include "callstacktracer.h"
#include "configuration.h"
#include "exceptions.h"
#include <sstream>

namespace _3fd
{
	namespace core
	{
		///////////////////////////////////////
		//  CallStack Class
		///////////////////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="CallStack" /> class.
		/// </summary>
		/// <param name="logInitialCap">The log initial capacity.</param>
		CallStack::CallStack(size_t logInitialCap)
		{
			m_stackFrames.reserve(logInitialCap);
		}

		/// <summary>
		/// Registers the frame.
		/// </summary>
		/// <param name="file">The file name.</param>
		/// <param name="line">The line number.</param>
		/// <param name="function">The function of the frame.</param>
		void CallStack::RegisterFrame(const char *file, 
									  unsigned long line, 
									  const char *function) throw()
		{
			m_stackFrames.push_back(
				Frame(file, function, line)
			);
		}

		/// <summary>
		/// Pops the last added stack frame.
		/// </summary>
		/// <returns>Whether the stack log is empty after popping an entry from it.</returns>
		bool CallStack::PopStackFrameEntry() throw()
		{
			m_stackFrames.pop_back();
			return m_stackFrames.empty();
		}

		/// <summary>
		/// Gets the call stack trace report.
		/// </summary>
		/// <returns>A string which shows the current stack trace.</returns>
		string CallStack::GetReport()
		{
			std::ostringstream oss;

			for(int index = 0 ; index < m_stackFrames.size() ; ++index)
			{
				auto &frame = m_stackFrames[index];
				oss << frame.file << " (" << frame.line << ") @ " << frame.function;

				if(index + 1 != m_stackFrames.size())
					oss << "; ";
				else
					oss << ';';
			}

			return oss.str();
		}


		//////////////////////////////////
		//  CallStackTracer Class
		//////////////////////////////////

        thread_local_def CallStack * CallStackTracer::callStack(nullptr);

		CallStackTracer	* CallStackTracer::uniqueObject(nullptr);

		std::mutex CallStackTracer::singleInstanceCreationMutex;

		/// <summary>
		/// Creates the singleton instance.
		/// </summary>
		/// <returns>A pointer to the newly created singleton of <see cref="CallStackTracer" /></returns>
		CallStackTracer * CallStackTracer::CreateInstance()
		{
			try
			{
				std::lock_guard<std::mutex> lock(singleInstanceCreationMutex);

				if(uniqueObject == nullptr)
					uniqueObject = new CallStackTracer ();

				return uniqueObject;
			}
			catch(IAppException &)
			{
				throw; // just forward exceptions known to have been previously handled
			}
			catch(std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed to acquire lock when creating the call stack tracer: " << core::StdLibExt::GetDetailsFromSystemError(ex);
				throw AppException<std::runtime_error>(oss.str());
			}
			catch(std::bad_alloc &)
			{
				throw AppException<std::runtime_error>("Failed to allocate memory to create the call stack tracer");
			}
		}

		/// <summary>
		/// Gets the singleton instance.
		/// </summary>
		/// <returns></returns>
		CallStackTracer & CallStackTracer::GetInstance()
		{
			// If there is no object, create one and returns a reference to it. If there is an object, return its reference.
			if(uniqueObject != nullptr)
				return *uniqueObject;
			else
				return *CreateInstance();
		}

		/// <summary>
		/// Shutdowns the call stack tracer instance releasing all associated resources.
		/// </summary>
		void CallStackTracer::Shutdown()
		{
			delete uniqueObject; // Call the destructor
			uniqueObject = nullptr;
		}

		/// <summary>
		/// Registers the current thread to have its stack traced.
		/// </summary>
		/// <returns>
		void CallStackTracer::RegisterThread()
		{
			if(callStack == nullptr)
			{
				try
				{
					callStack = new CallStack (
						AppConfig::GetSettings().framework.stackTracing.stackLogInitialCap
					);
				}
				catch(IAppException &)
				{
					throw; // just forward exceptions known to have been previously handled
				}
				catch(std::exception &ex)
				{
					std::ostringstream oss;
					oss << "Generic failure when registering thread for call stack tracing: " << ex.what();
					throw AppException<std::runtime_error>(oss.str());
				}
			}
		}

		/// <summary>
		/// Unregisters the current thread for stack tracing.
		/// </summary>
		void CallStackTracer::UnregisterThread()
		{
			if(callStack != nullptr)
			{
				delete callStack;
				callStack = nullptr;
			}
		}

		/// <summary>
		/// Tracks the call.
		/// </summary>
		/// <param name="file">The file name.</param>
		/// <param name="line">The line number.</param>
		/// <param name="function">The function name.</param>
		void CallStackTracer::TrackCall(const char *file, 
										unsigned long line, 
										const char *function)
		{
			if(callStack != nullptr)
				callStack->RegisterFrame(file, line, function);
			else
			{
				RegisterThread();
				callStack->RegisterFrame(file, line, function);
			}
		}

		/// <summary>
		/// Pops the last added stack frame.
		/// If the stack become empty, this object will be destroyed.
		/// </summary>
		void CallStackTracer::PopStackFrameEntry() throw()
		{
			if(callStack->PopStackFrameEntry() == false)
				return;
			else
				UnregisterThread();
		}

		/// <summary>
		/// Gets the stack frame report.
		/// </summary>
		/// <returns>A text encoded report of the stack frame.</returns>
		string CallStackTracer::GetStackReport() const
		{
			return callStack->GetReport();
		}

	}// end of namespace core
}// end of namespace _3fd
