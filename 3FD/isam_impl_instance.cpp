#include "stdafx.h"
#include "isam_impl.h"
#include "configuration.h"
#include <codecvt>
#include <cassert>

namespace _3fd
{
	namespace isam
	{
		///////////////////////////
		// Instance Class
		///////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="InstanceImpl" /> class.
		/// </summary>
		/// <param name="name">The name of the instance.</param>
		/// <param name="transactionLogsPath">The transaction logs directory path.</param>
		/// <param name="minCachedPages">The minimum amount of pages to keep in cache. (The more cache you guarantee, the faster is the IO.)</param>
		/// <param name="maxVerStorePages">The maximum amount of pages to reserve for the version store. (Affects how big a transaction can become and how many concurrent isolated sessions the engine can afford.)</param>
		/// <param name="logBufferSizeInSectors">The size (in volume sectors) of the transaction log write buffer. (A bigger buffer will render less frequent flushes to disk.)</param>
		InstanceImpl::InstanceImpl(
			const string &name,
			const string &transactionLogsPath,
			unsigned int minCachedPages,
			unsigned int maxVerStorePages,
			unsigned int logBufferSizeInSectors)
		try :
			m_name(name),
			m_jetInstance(NULL),
			m_numMaxSessions(0)
		{
			CALL_STACK_TRACE;

			if (core::AppConfig::GetSettings().framework.isam.useWindowsFileCache)
			{
				// Set instance to use Windows file cache (see http://msdn.microsoft.com/en-us/library/gg269260):
				auto rcode = JetSetSystemParameterW(&m_jetInstance, NULL, JET_paramEnableFileCache, true, nullptr);
				ErrorHelper::HandleError(NULL, NULL, rcode, [&name]()
				{
					std::ostringstream oss;
					oss << "Failed to turn on Windows file cache for ISAM storage";
					return oss.str();
				});

				// Set instance to access Windows file cache directly (see http://msdn.microsoft.com/en-us/library/gg269293)
				rcode = JetSetSystemParameterW(&m_jetInstance, NULL, JET_paramEnableViewCache, true, nullptr);
				ErrorHelper::HandleError(NULL, NULL, rcode, [&name]()
				{
					std::ostringstream oss;
					oss << "Failed to set ISAM storage to access Windows file cache directly";
					return oss.str();
				});
			}

			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;

			// Create instance:
			auto ucs2InstName = transcoder.from_bytes(name);

#ifndef _3FD_PLATFORM_WINRT
			auto rcode = JetCreateInstanceW(&m_jetInstance, ucs2InstName.c_str());
#else
			auto rcode = JetCreateInstance2W(&m_jetInstance, ucs2InstName.c_str(), ucs2InstName.c_str(), 0);
#endif
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to create ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			// Set instance for index checking (see http://msdn.microsoft.com/en-us/library/gg269337):
			rcode = JetSetSystemParameterW(&m_jetInstance, NULL, JET_paramEnableIndexChecking, true, nullptr);
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to enable index checking for ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			// Set instance minimum cache size (see http://msdn.microsoft.com/en-us/library/gg269293):
			rcode = JetSetSystemParameterW(&m_jetInstance, NULL, JET_paramCacheSizeMin, minCachedPages, nullptr);
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to set minimum cache size for ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			// Set instance maximum amount of reserved pages for the version store (see http://msdn.microsoft.com/en-us/library/gg269201):
			rcode = JetSetSystemParameterW(&m_jetInstance, NULL, JET_paramMaxVerPages, maxVerStorePages, nullptr);
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to set maximum amount of pages for version store in ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			// Set location for transaction logs:
			auto ucs2tlogsPath = transcoder.from_bytes(transactionLogsPath);
			rcode = JetSetSystemParameterW(&m_jetInstance, NULL, JET_paramLogFilePath, reinterpret_cast<JET_API_PTR> (ucs2tlogsPath.c_str()), 0);
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to set directory for transaction log files of ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			// Set instance to use circular transaction log files (see http://msdn.microsoft.com/en-us/library/gg269235):
			rcode = JetSetSystemParameterW(&m_jetInstance, NULL, JET_paramCircularLog, true, nullptr);
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to enable circular transaction log files in ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			// Set instance to delete transaction log files when they go out of range (see http://msdn.microsoft.com/en-us/library/gg269236):
			rcode = JetSetSystemParameterW(&m_jetInstance, NULL, JET_paramDeleteOutOfRangeLogs, true, nullptr);
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to enable removal of out of range transaction log files in ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			// Set instance write buffer size for transaction log (see http://msdn.microsoft.com/en-us/library/gg269235):
			rcode = JetSetSystemParameterW(&m_jetInstance, NULL, JET_paramLogBuffers, logBufferSizeInSectors, nullptr);
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to set size of transaction log write buffer in ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			// Initialize instance:
#ifndef _3FD_PLATFORM_WINRT
			rcode = JetInit(&m_jetInstance);
#else
			rcode = JetInit3W(&m_jetInstance, nullptr, 0);
#endif
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to initialize ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			// Get number of maximum simultaneous open sessions:
			JET_API_PTR numMaxSessions;
			rcode = JetGetSystemParameterW(m_jetInstance, NULL, JET_paramMaxSessions, &numMaxSessions, nullptr, 0);
			ErrorHelper::HandleError(m_jetInstance, NULL, rcode, [&name]()
			{
				std::ostringstream oss;
				oss << "Failed to get information from ISAM storage instance \'" << name << "\'";
				return oss.str();
			});

			m_numMaxSessions = static_cast<unsigned int> (numMaxSessions);
		}
		catch (core::IAppException &)
		{
			if (m_jetInstance != NULL)
			{
				CALL_STACK_TRACE;

#ifndef _3FD_PLATFORM_WINRT
				auto rcode = JetTerm(m_jetInstance);
#else
				auto rcode = JetTerm2(m_jetInstance, 0);
#endif
				ErrorHelper::LogError(m_jetInstance, NULL, rcode, [name]()
				{
					std::ostringstream oss;
					oss << "Failed to finalize ISAM storage instance \'" << name
						<< "\' after error during instantiation";
					return oss.str();
				}, core::Logger::PRIO_ERROR);
			}

			throw;
		}
		catch (std::exception &ex)
		{
			CALL_STACK_TRACE;

			if (m_jetInstance != NULL)
			{
#ifndef _3FD_PLATFORM_WINRT
				auto rcode = JetTerm(m_jetInstance);
#else
				auto rcode = JetTerm2(m_jetInstance, 0);
#endif
				ErrorHelper::LogError(m_jetInstance, NULL, rcode, [&name]()
				{
					std::ostringstream oss;
					oss << "Failed to finalize ISAM storage instance \'" << name << "\'";
					return oss.str();
				}, core::Logger::PRIO_ERROR);
			}

			std::ostringstream oss;
			oss << "Generic failure when creating ISAM instance: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}

		Instance::Instance(
			const string &name,
			const string &transactionLogsPath,
			unsigned int minCachedPages,
			unsigned int maxVerStorePages,
			unsigned int logBufferSizeInSectors)
		try :
			m_pimplInstance(nullptr),
			m_availableSessions(),
			m_attachedDbs(),
			m_accessToResources()
		{
			CALL_STACK_TRACE;
			m_pimplInstance =
				new InstanceImpl(
					name,
					transactionLogsPath,
					minCachedPages,
					maxVerStorePages,
					logBufferSizeInSectors
				);
		}
		catch (core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch (std::exception &ex)
		{
			CALL_STACK_TRACE;
			std::ostringstream oss;
			oss << "Generic failure when creating ISAM instance: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="InstanceImpl"/> class.
		/// </summary>
		InstanceImpl::~InstanceImpl()
		{
			if (m_jetInstance != NULL) // if the object was not moved
			{
				CALL_STACK_TRACE;

#ifndef _3FD_PLATFORM_WINRT
				auto rcode = JetTerm(m_jetInstance);
#else
				auto rcode = JetTerm2(m_jetInstance, 0);
#endif
				ErrorHelper::LogError(m_jetInstance, NULL, rcode, [this]()
				{
					std::ostringstream oss;
					oss << "Failed to finalize ISAM storage instance \'" << m_name << "\'";
					return oss.str();
				}, core::Logger::PRIO_ERROR);
			}
		}

		Instance::~Instance()
		{
			CALL_STACK_TRACE;

			_ASSERTE(m_attachedDbs.empty()); // When the instance shuts down, all database connections must already be closed

			while (m_availableSessions.empty() == false)
			{
				delete m_availableSessions.front();
				m_availableSessions.pop();
			}

			delete m_pimplInstance;
		}

		/// <summary>
		/// Creates a new session.
		/// </summary>
		/// <returns>A new ISAM session object.</returns>
		SessionImpl * InstanceImpl::CreateSession()
		{
			CALL_STACK_TRACE;

			try
			{
				JET_SESID jetSession(NULL);
				auto rcode = JetBeginSessionW(m_jetInstance, &jetSession, nullptr, nullptr);

				ErrorHelper::HandleError(m_jetInstance, jetSession, rcode, [this]()
				{
					std::ostringstream oss;
					oss << "Failed to initialize ISAM storage session for instance \'" << m_name << "\'";
					return oss.str();
				});

				return new SessionImpl(jetSession);
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when creating ISAM session: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Releases the resources of a borrowed database connection.
		/// </summary>
		/// <param name="dbCode">The numeric code that was attributed to the database connection.</param>
		/// <param name="database">The database object.</param>
		/// <param name="session">The session object.</param>
		void Instance::ReleaseResource(int dbCode, DatabaseImpl *database, SessionImpl *session)
		{
			_ASSERTE(session != nullptr && database != nullptr);

			try
			{
				delete database;

				std::lock_guard<std::mutex> lock(m_accessToResources);

				auto iter = m_attachedDbs.find(dbCode);
				_ASSERTE(m_attachedDbs.end() != iter); // database code has to correspond to an already attached one

				auto &db = iter->second;
				if (--db.handlesCount == 0)
				{
					session->DetachDatabase(db.fileName);
					m_attachedDbs.erase(iter);
				}

				m_availableSessions.push(session);
			}
			catch (std::system_error &ex)
			{
				CALL_STACK_TRACE;
				std::ostringstream oss;
				oss << "Failed to release resources of a database connection: " << core::StdLibExt::GetDetailsFromSystemError(ex);
				throw core::AppException<std::runtime_error>(oss.str());
			}
			catch (std::exception &ex)
			{
				CALL_STACK_TRACE;
				std::ostringstream oss;
				oss << "Generic failure when releasing resources of a database connection: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Implements most of the work to open a database.
		/// It gets the resources needed to create wrapper <see cref="DatabaseConn" />.
		/// </summary>
		/// <param name="dbCode">The numeric code to associate with the database.</param>
		/// <param name="dbFileName">The name of the database file to open in case a connection is not yet available for it.</param>
		/// <param name="database">Where to save the the database object.</param>
		/// <param name="session">Where to save the the session object.</param>
		/// <param name="createIfNotFound">Creates the database if the file was not found.</param>
		/// <returns><c>true</c> if had to create a new database because the given file was not found, otherwise, <c>false</c>.</returns>
		bool Instance::OpenDatabaseImpl(
			int dbCode,
			const string &dbFileName,
			DatabaseImpl **database,
			SessionImpl **session,
			bool createIfNotFound)
		{
			CALL_STACK_TRACE;

			*database = nullptr;
			*session = nullptr;

			try
			{
				std::lock_guard<std::mutex> lock(m_accessToResources);

				// Try to get a cached session:
				if (m_availableSessions.empty() == false)
				{
					*session = m_availableSessions.front();
					m_availableSessions.pop();
				}
				else
					*session = m_pimplInstance->CreateSession();

				bool hasToCreateDb(false);

				// If the database has not been attached yet:
				auto iter = m_attachedDbs.find(dbCode);
				if (m_attachedDbs.end() == iter)
				{
					std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
					auto ucs2DbFileName = transcoder.from_bytes(dbFileName);

					if ((*session)->AttachDatabase(ucs2DbFileName, !createIfNotFound) == STATUS_OKAY)
						iter = m_attachedDbs.emplace(dbCode, std::move(ucs2DbFileName)).first;
					else
						hasToCreateDb = true;
				}

				// Open/Create the database:
				if (hasToCreateDb)
				{
					std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
					auto ucs2DbFileName = transcoder.from_bytes(dbFileName);
					*database = (*session)->CreateDatabase(ucs2DbFileName);
					iter = m_attachedDbs.emplace(dbCode, std::move(ucs2DbFileName)).first;
				}
				else
					*database = (*session)->OpenDatabase(iter->second.fileName);

				++(iter->second.handlesCount);
				return hasToCreateDb;
			}
			catch (core::IAppException &)
			{
				if (*session != nullptr)
					m_availableSessions.push(*session);

				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::system_error &ex)
			{
				if (*session != nullptr)
					m_availableSessions.push(*session);

				std::ostringstream oss;
				oss << "Failed to open ISAM database: " << core::StdLibExt::GetDetailsFromSystemError(ex);
				throw core::AppException<std::runtime_error>(oss.str());
			}
			catch (std::exception &ex)
			{
				if (*session != nullptr)
					m_availableSessions.push(*session);

				std::ostringstream oss;
				oss << "Generic failure when opening ISAM database: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		DatabaseConn Instance::OpenDatabase(int dbCode, const string &dbFileName)
		{
			CALL_STACK_TRACE;
			DatabaseImpl *database;
			SessionImpl *session;
			OpenDatabaseImpl(dbCode, dbFileName, &database, &session, false);
			return DatabaseConn(*this, session, database, dbCode);
		}

		DatabaseConn Instance::OpenDatabase(int dbCode, const string &dbFileName, bool &newDb)
		{
			CALL_STACK_TRACE;
			DatabaseImpl *database;
			SessionImpl *session;
			OpenDatabaseImpl(dbCode, dbFileName, &database, &session, true);
			return DatabaseConn(*this, session, database, dbCode);
		}

	}// end namespace isam
}// end namespace _3fd
