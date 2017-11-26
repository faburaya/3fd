#include "stdafx.h"
#include "logger.h"
#include "configuration.h"
#include "callstacktracer.h"
#include "utils_winrt.h"

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
		/// Gets the unique instance of the singleton <see cref="Logger" /> class.
		/// </summary>
		/// <returns>A pointer to the singleton.</returns>
		Logger * Logger::GetInstance() noexcept
		{
			if (uniqueObjectPtr != nullptr)
				return uniqueObjectPtr;
			else
			{
				try
				{
					CreateInstance(AppConfig::GetApplicationId(), false);
				}
				catch (IAppException &)
				{/* DO NOTHING: SWALLOW EXCEPTION
					Even when the set-up of the logger fails, the application must continue
					to execute, because the logger is merely an auxiliary service. Here an 
					exception cannot be forwarded up in the stack because the current call 
					chain might have been originated in a destructor. */
				}

				return uniqueObjectPtr;
			}
		}

		/// <summary>
		/// Prepares the log event string.
		/// </summary>
		/// <param name="oss">The file stream output.</param>
		/// <param name="timestamp">The event timestamp.</param>
		/// <param name="prio">The event priority.</param>
		/// <returns>A reference to the string stream output.</returns>
		static std::ofstream &PrepareEventString(std::ofstream &ofs, time_t timestamp, Logger::Priority prio)
		{
			static const auto pid = GetCurrentProcessId();
			std::array<char, 21> buffer;
			strftime(buffer.data(), buffer.size(), "%Y-%b-%d %H:%M:%S", localtime(&timestamp));
			ofs << buffer.data() << " [process " << pid;

			switch (prio)
			{
			case Logger::PRIO_FATAL:
				ofs << "] - FATAL - ";
				break;

			case Logger::PRIO_CRITICAL:
				ofs << "] - CRITICAL - ";
				break;

			case Logger::PRIO_ERROR:
				ofs << "] - ERROR - ";
				break;

			case Logger::PRIO_WARNING:
				ofs << "] - WARNING - ";
				break;

			case Logger::PRIO_NOTICE:
				ofs << "] - NOTICE - ";
				break;

			case Logger::PRIO_INFORMATION:
				ofs << "] - INFORMATION - ";
				break;

			case Logger::PRIO_DEBUG:
				ofs << "] - DEBUG - ";
				break;

			case Logger::PRIO_TRACE:
				ofs << "] - TRACE - ";
				break;

			default:
				break;
			}

			return ofs;
		}

		/// <summary>
		/// Opens the text log file stream for writing (append).
		/// </summary>
		/// <param name="txtLogFilePath">The path for the text log file.</param>
		/// <param name="ofs">The file stream output.</param>
		static void OpenTextLogStream(Platform::String ^txtLogFilePath, std::ofstream &ofs)
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
			ofs.open(transcoder.to_bytes(txtLogFilePath->Data()), std::ios::app | std::ios::out);
			if (ofs.is_open() == false)
				throw AppException<std::runtime_error>("Could not open text log file");
		}

		/// <summary>
		/// Shifts the log to a new file, compress the content of the old, and moves it to the app temporary data store.
		/// </summary>
		static void ShiftToNewLogFile(Windows::Storage::StorageFile^ &txtLogFile, std::ofstream &ofs)
		{
			using namespace std::chrono;
			using namespace Windows::Storage;
			using namespace Windows::Foundation;

			ofs.close(); // first close the stream to the current log file

			// Rename the current log file:
			std::wostringstream woss;
			woss << txtLogFile->Path->Data() << L".old";
			auto currLogFileName = txtLogFile->Name;
			auto tempLogFileName = ref new Platform::String(woss.str().c_str());
			utils::WinRTExt::WaitForAsync(txtLogFile->RenameAsync(tempLogFileName));

			// Start reading log file to buffer:
			auto asyncOperReadLogToBuffer = FileIO::ReadBufferAsync(txtLogFile);

			// Create a new log file:
			txtLogFile = utils::WinRTExt::WaitForAsync(
				ApplicationData::Current->LocalFolder->CreateFileAsync(currLogFileName)
			);

			OpenTextLogStream(txtLogFile->Path, ofs);

			// Create the file which will contain the previous log compressed:
			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
			auto now = system_clock::to_time_t(system_clock::now());
			woss << txtLogFile->DisplayName->Data() << L'[' << transcoder.from_bytes(ctime(&now)) << L"].log.dat";

			auto compressedLogFile = utils::WinRTExt::WaitForAsync(
				ApplicationData::Current->TemporaryFolder->CreateFileAsync(
					ref new Platform::String(woss.str().c_str()),
					CreationCollisionOption::ReplaceExisting
				)
			);

			// Create a writable stream for such file:
			auto outputStream = utils::WinRTExt::WaitForAsync(
				compressedLogFile->OpenAsync(FileAccessMode::ReadWrite)
			);

            // Await for completion of reading operation:
            auto readBuffer = utils::WinRTExt::WaitForAsync(asyncOperReadLogToBuffer);

			// Compress the text content of the previous log file:
			auto compressor = ref new Compression::Compressor(outputStream);
			utils::WinRTExt::WaitForAsync(compressor->WriteAsync(readBuffer));
			utils::WinRTExt::WaitForAsync(compressor->FinishAsync());
			utils::WinRTExt::WaitForAsync(compressor->FlushAsync());

			// Write log shift event in the new log:
			now = system_clock::to_time_t(system_clock::now());
			PrepareEventString(ofs, now, Logger::PRIO_NOTICE)
				<< L"The log text file has been shifted. The previous file has been compressed from "
				<< readBuffer->Length / 1024 << L" to " << outputStream->Size / 1024
				<< L" KB and moved to the app temporary data store."
				<< std::endl << std::flush;
		}

		/// <summary>
		/// Estimates the room left in the log file for more events.
		/// </summary>
		/// <param name="txtLogFile">The text log file.</param>
		/// <returns>The estimate, which is positive when there is room left, otherwise, negative.</returns>
		static long EstimateRoomForLogEvents(Windows::Storage::StorageFile ^txtLogFile)
		{
			// Get the file size:
			auto fileSize = utils::WinRTExt::WaitForAsync(
				txtLogFile->GetBasicPropertiesAsync()
			)->Size;

#	if defined ENABLE_3FD_CST && defined ENABLE_3FD_ERR_IMPL_DETAILS
			const uint32_t avgLineSize(300);
#	elif defined ENABLE_3FD_CST
			const uint32_t avgLineSize(250);
#	elif defined ENABLE_3FD_ERR_IMPL_DETAILS
			const uint32_t avgLineSize(150);
#	else
			const uint32_t avgLineSize(100);
#	endif
			// Estimate the amount of events for which there is still room left in the log file:
			return static_cast<long> (
				(AppConfig::GetSettings().common.log.sizeLimit * 1024 - fileSize) / avgLineSize
			);
		}

		/// <summary>
		/// The procedure executed by the log writer thread.
		/// </summary>
		void Logger::LogWriterThreadProc()
		{
			try
			{
				std::ofstream ofs;
				OpenTextLogStream(m_txtLogFile->Path, ofs);
				long estimateRoomForLogEvents = EstimateRoomForLogEvents(m_txtLogFile);
				bool terminate(false);

				do
				{
					// Wait for queued messages:
					terminate = m_terminationEvent.WaitFor(100);

					// Write the queued messages in the text log file:

                    m_eventsQueue.ForEach([&estimateRoomForLogEvents, &ofs](const LogEvent &ev)
					{
						PrepareEventString(ofs, ev.time, ev.prio) << ev.what; // add the main details and message
#   ifdef ENABLE_3FD_ERR_IMPL_DETAILS
						if (ev.details.empty() == false) // add the details
							ofs << " - " << ev.details;
#   endif
#   ifdef ENABLE_3FD_CST
						if (ev.trace.empty() == false) // add the call stack trace
							ofs << "\n\n### CALL STACK ###\n" << ev.trace;
#   endif
						ofs << std::endl << std::flush; // flush the content to the file

						if (ofs.bad())
							throw AppException<std::runtime_error>("Failed to write in the text log output file stream");

						--estimateRoomForLogEvents;
					});

					// If the log file was supposed to reach its size limit now:
					if (estimateRoomForLogEvents <= 0)
					{
						// Recalculate estimate for current log file and, if there is no room left, shift to a new one:
						estimateRoomForLogEvents = EstimateRoomForLogEvents(m_txtLogFile);
						if (estimateRoomForLogEvents < 0)
						{
							ShiftToNewLogFile(m_txtLogFile, ofs);
							estimateRoomForLogEvents = EstimateRoomForLogEvents(m_txtLogFile); // recalculate estimate for new log file
						}
					}
				}
				while (!terminate);
			}
			catch (...)
			{/* DO NOTHING: SWALLOW EXCEPTION
				An eventual failure in this thread will be kept silent because logging is 
				an auxiliary service, hence it should not further affect the application 
				execution. Here an exception cannot be forwarded up in the stack because 
				this procedure is running asynchronous.*/
			}
		}

		/// <summary>
		/// Initializes a new instance of the <see cref="Logger"/> class.
		/// </summary>
		/// <param name="id">The application ID.</param>
		/// <param name="logToConsole">Whether log output should be redirected to the console. Because a store app does not have an available console, this parameter is ignored.</param>
		Logger::Logger(const string &id, bool logToConsole) : 
			m_logWriterThread(), 
			m_terminationEvent(), 
			m_eventsQueue(), 
			m_txtLogFile(nullptr) 
		{
			try
			{
				using namespace Windows::Storage;

				// Open or create the file:
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				auto loggerId = ref new Platform::String(transcoder.from_bytes(id).c_str());
				m_txtLogFile = utils::WinRTExt::WaitForAsync(
					ApplicationData::Current->LocalFolder->CreateFileAsync(loggerId + L".log.txt", CreationCollisionOption::OpenIfExists)
				);

				m_logWriterThread.swap(std::thread(&Logger::LogWriterThreadProc, this));
			}
			catch (...)
			{/* DO NOTHING: SWALLOW EXCEPTION
				Even when the set-up of the logger fails, the application must continue
				to execute, because the logger is merely an auxiliary service. */
				m_txtLogFile = nullptr;
			}
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="Logger"/> class.
		/// </summary>
		Logger::~Logger()
		{
			try
			{
				// Signalizes termination for the message loop
				m_terminationEvent.Signalize();

				if (m_logWriterThread.joinable())
					m_logWriterThread.join();

                _ASSERTE(m_eventsQueue.ForEach([](const LogEvent &) {}) == 0);
			}
			catch (...)
			{
				// DO NOTHING: SWALLOW EXCEPTION
			}
		}

		/// <summary>
		/// Writes a message and its details to the log output.
		/// </summary>
		/// <param name="what">The reason for the message.</param>
		/// <param name="details">The message details.</param>
		/// <param name="prio">The priority for the message.</param>
		/// <param name="cst">When set to <c>true</c>, append the call stack trace.</param>
		void Logger::WriteImpl(string &&what, string &&details, Priority prio, bool cst) noexcept
		{
			if (m_txtLogFile == nullptr)
				return;

			try
			{
				using namespace std::chrono;
				auto now = system_clock::to_time_t(system_clock::now());

                LogEvent logEvent(now, prio, std::move(what));

#	ifdef ENABLE_3FD_ERR_IMPL_DETAILS
				if (details.empty() == false)
					logEvent.details = std::move(details);
#	endif
#	ifdef ENABLE_3FD_CST
				if (cst && CallStackTracer::IsReady())
					logEvent.trace = CallStackTracer::GetStackReport();
#	endif
				m_eventsQueue.Push(std::move(logEvent)); // enqueue the request to write this event to the log file
			}
			catch (std::exception &)
			{
				// DO NOTHING: SWALLOW EXCEPTION
			}
		}

	}// end of namespace core
}// end of namespace _3fd
