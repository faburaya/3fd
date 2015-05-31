#ifndef CALLSTACKTRACER_H
#define CALLSTACKTRACER_H

#include "base.h"
#include "preprocessing.h"
#include <string>
#include <vector>
#include <mutex>

namespace _3fd
{
	using std::string;

	namespace core
	{
		/////////////////////////////////////////
		//  Early class declarations
		/////////////////////////////////////////

		class StackTracer;
		class StackDeactivationTrigger;


		/// <summary>
		/// Stores an history of procedure call events
		/// </summary>
		class CallStack : notcopiable
		{
		private:

			/// <summary>
			/// Represents a stack frame traced event
			/// </summary>
			struct Frame
			{
				const char *file;
				const char *function;
				unsigned long line;

				Frame(const char *p_file, 
					  const char *p_function, 
					  unsigned long p_line)
					: file(p_file), function(p_function), line(p_line) 
				{}
			};

			std::vector<Frame> m_stackFrames;

		public:

			CallStack(size_t logInitialCap);

			void RegisterFrame(const char *file, 
							   unsigned long line, 
							   const char *function) throw();

			bool PopStackFrameEntry() throw();

			string GetReport();
		};

		
		/// <summary>
		/// A class for the call stack tracer, which is operated through macros defined on 'preprocessing.h'
		/// </summary>
		class CallStackTracer : notcopiable
		{
		private:

            thread_local_decl static CallStack *callStack;

			static std::mutex			singleInstanceCreationMutex;
			static CallStackTracer		*uniqueObject;
			static CallStackTracer		*CreateInstance();

			/// <summary>
			/// Prevents a default instance of the <see cref="CallStackTracer"/> class from being created.
			/// </summary>
			CallStackTracer() {}

			void RegisterThread();
			void UnregisterThread();

		public:

			static CallStackTracer &GetInstance();

			static void Shutdown();
		
			/// <summary>
			/// Determines whether call stack tracing is ready for the calling thread.
			/// </summary>
			/// <returns>'true' if available, otherwise, 'false'.</returns>
			static bool IsReady()
			{
				/* This method  is necessary to prevent things such as an exception attempting to access CST when it is not started yet, 
				or any other piece of code trying to access the framework configurations before they were loaded. These situations might 
				take place if the initialization of the frameowrk core features runs into an error, when neither CST is ready or/nor the 
				configurations are available. In these cases, an exception still has to be used in order to signalize the failure, but it 
				cannot make use of such 'services'.

				Obs.: If the exception tries to invoke the RTM to get trace information, the framework initialization is requested again recursively, 
				leading to a stack overflow. */
				return callStack != nullptr;
			}

			void TrackCall(const char *file, 
						   unsigned long line, 
						   const char *function);

			void PopStackFrameEntry() throw();

			string GetStackReport() const;
		};

		/// <summary>
		/// This class is a trick to automatize the call stack tracing using scope finalizations.
		/// </summary>
		class StackDeactivationTrigger
		{
		public:

			/// <summary>
			/// Finalizes an instance of the <see cref="StackDeactivationTrigger"/> class.
			/// </summary>
			~StackDeactivationTrigger()
			{
				CallStackTracer::GetInstance().PopStackFrameEntry();
			}
		};

	}// end of namespace core
}// end of namespace _3fd

#endif // header guard
