#include "stdafx.h"
#include "sqlite.h"
#include "logger.h"
#include "exceptions.h"
#include <cassert>
#include <sstream>
#include <algorithm>

#undef min

namespace _3fd
{
	using std::ostringstream;

	namespace sqlite
	{
		///////////////////////////////
		// Transaction Class
		///////////////////////////////

		/// <summary>
		/// Initializes a new instance of the <see cref="Transaction" /> class.
		/// </summary>
		/// <param name="connWrapper">The database connection wrapper.</param>
		Transaction::Transaction(DbConnWrapper &connWrapper) : 
			m_committed(false), 
			m_conn(connWrapper) 
		{
			CALL_STACK_TRACE;
			Begin();
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="Transaction"/> class.
		/// </summary>
		Transaction::~Transaction()
		{
			if(m_committed == false)
				RollBack();
		}

		/// <summary>
		/// Begins the transaction.
		/// </summary>
		void Transaction::Begin()
		{
			CALL_STACK_TRACE;
			m_conn.Get().CreateStatement("BEGIN TRANSACTION;").Step();
		}

		/// <summary>
		/// Commit the transaction.
		/// </summary>
		void Transaction::Commit()
		{
			CALL_STACK_TRACE;

			auto commit = m_conn.Get().CreateStatement("COMMIT TRANSACTION;");
			int status, attempts(1);

			while((status = commit.TryStep(false)) != SQLITE_DONE) // It does not throw exceptions because the status must first be analysed
			{
				unsigned char primErrCode(status & 255);

				// When the commit phase of a transaction fails because it cannot get an exclusive lock:
				if (primErrCode == SQLITE_BUSY || primErrCode == SQLITE_LOCKED)
				{
					if(attempts == 1)
						srand(static_cast<uint32_t> (time(nullptr)));
					
					// Wait a little for an eventual ongoing WAL checkpoint operation:
					std::this_thread::sleep_for(
						std::chrono::milliseconds(rand() % std::min(3 * attempts, 50) + 1)
					);
				}
				else // However, when it fails for any other reason:
				{
					ostringstream oss;
					oss << "Failed to commit SQLite transaction after " << attempts << " attempt(s): " << sqlite3_errstr(status);
					core::Logger::Write(oss.str(), core::Logger::PRIO_ERROR, true);
					return; // abort
				}

				++attempts;
			}// loop: retry the commit
		
			m_committed = true;
		}

		/// <summary>
		/// Rollback the transaction.
		/// </summary>
		void Transaction::RollBack()
		{
			CALL_STACK_TRACE;

			auto rollback = m_conn.Get().CreateStatement("ROLLBACK TRANSACTION;");
			int status, attempts(1);

			while ((status = rollback.TryStep(false)) != SQLITE_DONE) // It does not throw exceptions because it might be called by the destructor:
			{
				unsigned char primErrCode(status & 255);

				// When the transaction rollback fails because it cannot get a shared lock:
				if (primErrCode == SQLITE_BUSY || primErrCode == SQLITE_LOCKED)
				{
					if (attempts == 1)
						srand(static_cast<uint32_t> (time(nullptr)));
					
					// Wait a little for an eventual pending read operation:
					std::this_thread::sleep_for(
						std::chrono::milliseconds(rand() % std::min(3*attempts, 50) + 1)
					);
				}
				else // However, when it fails for any other reason:
				{
					ostringstream oss;
					oss << "Failed to rollback SQLite transaction after " << attempts << " attempt(s): " << sqlite3_errstr(status);
					core::Logger::Write(oss.str(), core::Logger::PRIO_CRITICAL, true);
					return; // abort
				}

				++attempts;
			}// loop: retry the rollback
		}

	}// end of namespace SQLite
}// end of namespace _3fd
