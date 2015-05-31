#include "stdafx.h"
#include "runtime.h"
#include "exceptions.h"
#include <map>
#include <list>
#include <array>
#include <chrono>
#include <thread>
#include <future>
#include <random>
#include <iostream>

namespace _3fd
{
	namespace integration_tests
	{
		using std::generate;
		using std::for_each;

		using namespace _3fd::core;

		void HandleException();

		/// <summary>
		/// Tests basic logging functionality.
		/// </summary>
		TEST(Framework_CoreRuntime_TestCase, LogOutput_Test)
		{
			// Ensures proper initialization/finalization of the framework
			core::FrameworkInstance _framework;

			/* This macro tells the framework to trace this call. If you do not use it,
			this frame will not be visible in the stack trace report. */
			CALL_STACK_TRACE;

			try
			{
				// Create exceptions and log them:
				auto innerEx = AppException<std::runtime_error>("Inner exception.", "Inner exception details.");
				auto ex = AppException<std::runtime_error>("Log entry built from exception message.", "Exception details.", innerEx);
				Logger::Write(ex, core::Logger::PRIO_DEBUG);

				// Log some text directly:
				Logger::Write("Log entry built from a single message.", core::Logger::PRIO_DEBUG);
				Logger::Write("Log entry built from message & details.", "Useless details.", core::Logger::PRIO_DEBUG);
			}
			catch (...)
			{
				HandleException();
			}
		}

		/// <summary>
		/// Third level call
		/// </summary>
		static void func3()
		{
			/* This macro tells the framework to trace this call. If you do not use it,
			this frame will not be visible in the stack trace report. */
			CALL_STACK_TRACE;
			throw AppException<std::runtime_error>("Test exception.", "Exception details.");
		}

		/// <summary>
		/// Second level call
		/// </summary>
		static void func2()
		{
			/* This macro tells the framework to trace this call. If you do not use it,
			this frame will not be visible in the stack trace report. */
			CALL_STACK_TRACE;

			try
			{
				func3();
			}
			catch (IAppException &ex)
			{
				throw AppException<std::runtime_error>("Wrapping exception.", "Wrapping details.", ex);
			}
		}

		/// <summary>
		/// First level call
		/// </summary>
		static void func1()
		{
			/* This macro tells the framework to trace this call. If you do not use it,
			this frame will not be visible in the stack trace report. */
			CALL_STACK_TRACE;

			try
			{
				func2();
			}
			catch (IAppException &ex)
			{
				throw AppException<std::runtime_error>("Extra wrapping exception.", "Extra wrapping details.", ex);
			}
		}

		/// <summary>
		/// Tests exception forwarding through the stack.
		/// </summary>
		TEST(Framework_CoreRuntime_TestCase, StackTrace_Test)
		{
			// Ensures proper initialization/finalization of the framework
			core::FrameworkInstance _framework;

			/* This macro tells the framework to trace this call. If you do not use it,
			this frame will not be visible in the stack trace report. */
			CALL_STACK_TRACE;

			try
			{
				try
				{
					func1();
				}
				catch (IAppException &ex)
				{
					Logger::Write(ex, core::Logger::PRIO_DEBUG);
				}
			}
			catch (...)
			{
				HandleException();
			}
		}

	}// end of namespace integration_tests
}// end of namespace _3fd
