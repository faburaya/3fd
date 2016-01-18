#include "stdafx.h"
#include "isam_impl.h"
#include <codecvt>

namespace _3fd
{
	namespace isam
	{
		///////////////////////////
		// Session Class
		///////////////////////////

		/// <summary>
		/// Finalizes an instance of the <see cref="SessionImpl"/> class.
		/// </summary>
		SessionImpl::~SessionImpl()
		{
			if (m_jetSession != NULL) // if the object was not moved
			{
				CALL_STACK_TRACE;
				auto rcode = JetEndSession(m_jetSession, 0);
				ErrorHelper::LogError(NULL, m_jetSession, rcode, "Failed to finalize ISAM storage session", core::Logger::PRIO_ERROR);
			}
		}

		/// <summary>
		/// Attaches the database to the instance.
		/// </summary>
		/// <param name="dbFileName">Full name of the database file.</param>
		/// <param name="throwNotFound">if set to <c>true</c>, the method will not throw an exception when the database file was not found.</param>
		/// <returns>Whether the database file was found and successfully attached to the instance.</returns>
		bool SessionImpl::AttachDatabase(const wstring &dbFileName, bool throwNotFound)
		{
			CALL_STACK_TRACE;

#ifndef _3FD_PLATFORM_WINRT
			auto rcode = JetAttachDatabaseW(m_jetSession, dbFileName.c_str(), 0);
#else
			auto rcode = JetAttachDatabase2W(m_jetSession, dbFileName.c_str(), 0, 0);
#endif
			if (throwNotFound == false && rcode == JET_errFileNotFound)
				return STATUS_FAIL;

			ErrorHelper::HandleError(NULL, m_jetSession, rcode, [&dbFileName]()
			{
				std::ostringstream oss;
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				auto utf8DbFullFileName = transcoder.to_bytes(dbFileName);
				oss << "Failed to attach database \'" << utf8DbFullFileName << "\' to ISAM instance";
				return oss.str();
			});

			return STATUS_OKAY;
		}

		/// <summary>
		/// Detaches the database from instance.
		/// </summary>
		/// <param name="dbFileName">Full name of the database file.</param>
		void SessionImpl::DetachDatabase(const wstring &dbFileName)
		{
			CALL_STACK_TRACE;

#ifndef _3FD_PLATFORM_WINRT
			auto rcode = JetDetachDatabaseW(m_jetSession, dbFileName.data());
#else
			auto rcode = JetDetachDatabase2W(m_jetSession, dbFileName.data(), 0);
#endif
			ErrorHelper::LogError(NULL, m_jetSession, rcode, [&dbFileName]()
			{
				std::ostringstream oss;
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				oss << "Failed to detach ISAM database \'" << transcoder.to_bytes(dbFileName.data()) << "\' from session";
				return oss.str();
			}, core::Logger::PRIO_ERROR); // No exceptions are thrown, because detachment is called by destructors
		}

		/// <summary>
		/// Creates a new database.
		/// </summary>
		/// <param name="dbFileName">Name of the database file.</param>
		/// <returns>A new ISAM database object.</returns>
		DatabaseImpl * SessionImpl::CreateDatabase(const wstring &dbFileName)
		{
			CALL_STACK_TRACE;

			try
			{
				JET_DBID jetDatabaseId(NULL);
				auto rcode = JetCreateDatabase2W(m_jetSession, dbFileName.c_str(), 0, &jetDatabaseId, 0);

				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [&dbFileName]()
				{
					std::ostringstream oss;
					std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
					auto utf8DbFileName = transcoder.to_bytes(dbFileName);
					oss << "Failed to create new ISAM database \'" << utf8DbFileName << "\'";
					return oss.str();
				});

				return new DatabaseImpl(m_jetSession, jetDatabaseId);
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when creating new ISAM database: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Opens an already existent database.
		/// </summary>
		/// <param name="dbFileName">Name of the database file.</param>
		/// <returns>An ISAM database object.</returns>
		DatabaseImpl * SessionImpl::OpenDatabase(const wstring &dbFileName)
		{
			CALL_STACK_TRACE;

			try
			{
				JET_DBID jetDatabase(NULL);
				auto rcode = JetOpenDatabaseW(m_jetSession, dbFileName.c_str(), nullptr, &jetDatabase, 0);

				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [&dbFileName]()
				{
					std::ostringstream oss;
					std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
					auto utf8DbFileName = transcoder.to_bytes(dbFileName);
					oss << "Failed to open ISAM database \'" << utf8DbFileName << "\'";
					return oss.str();
				});

				return new DatabaseImpl(m_jetSession, jetDatabase);
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when opening ISAM database: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Creates a transaction.
		/// </summary>
		/// <returns>A new <see cref="TransactionImpl" /> object.</returns>
		TransactionImpl * SessionImpl::CreateTransaction()
		{
			CALL_STACK_TRACE;

			try
			{
#ifndef _3FD_PLATFORM_WINRT
				auto rcode = JetBeginTransaction(m_jetSession);
#else
				auto rcode = JetBeginTransaction3(m_jetSession, 0, 0);
#endif
				ErrorHelper::HandleError(NULL, m_jetSession, rcode, "Failed to begin ISAM transaction");
				return new TransactionImpl(m_jetSession);
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when beginning ISAM transaction: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Flushes all transactions previously committed by the current session 
		/// and that have noy yet been flushed to the transaction log file.
		/// </summary>
		void SessionImpl::Flush()
		{
			CALL_STACK_TRACE;
			auto rcode = JetCommitTransaction(m_jetSession, JET_bitWaitAllLevel0Commit);
			ErrorHelper::HandleError(NULL, m_jetSession, rcode, "Failed to flush outstanding ISAM transactions");
		}

	}// end namespace isam
}// end namespace _3fd
