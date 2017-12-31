#include "stdafx.h"
#include "sqlite.h"
#include "logger.h"
#include "exceptions.h"
#include <cassert>
#include <sstream>

namespace _3fd
{
    using std::ostringstream;

    namespace sqlite
    {
        ///////////////////////////////
        // DatabaseConn Class
        ///////////////////////////////

        /// <summary>
        /// Initializes a new instance of the <see cref="DatabaseConn" /> class.
        /// </summary>
        /// <param name="dbFilePath">The database file path.</param>
        /// <param name="fullMutex">Whether a full mutex should be specified in the database connection creation.</param>
        DatabaseConn::DatabaseConn(const string &dbFilePath, bool fullMutex)
        try : 
            m_dbHandle(nullptr), 
            m_preparedStatements() 
        {
            CALL_STACK_TRACE;

            /* The database connection is set with permission to write, 
            and if it doesn't exist, create it. */
            int status = sqlite3_open_v2(dbFilePath.c_str(), 
                                         &m_dbHandle, 
                                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_SHAREDCACHE | (fullMutex ? SQLITE_OPEN_FULLMUTEX : SQLITE_OPEN_NOMUTEX), 
                                         nullptr);
            if(status != SQLITE_OK)
            {
                ostringstream oss;
                oss << "SQLite API error code " << status
                    << " - 'sqlite3_open_v2' reported: " << sqlite3_errstr(status)
                    << " - Database was " << dbFilePath;

                throw core::AppException<std::runtime_error>("Failed to open a connection to the database", oss.str());
            }

            status = sqlite3_extended_result_codes(m_dbHandle, 1);

            if(status != SQLITE_OK)
            {
                ostringstream oss;
                oss << "SQLite API error code " << status
                    << " - 'sqlite3_extended_result_codes' reported: " << sqlite3_errstr(status)
                    << " - Database was " << dbFilePath;

                throw core::AppException<std::runtime_error>("Could not enable SQLite support for extended result codes", oss.str());
            }
        }
        catch(core::IAppException &)
        {
            if(m_dbHandle != nullptr)
            {
                int status = sqlite3_close(m_dbHandle);
                _ASSERTE(status == SQLITE_OK);
            }
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="DatabaseConn"/> class using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources will be stolen.</param>
        DatabaseConn::DatabaseConn(DatabaseConn &&ob) : 
            m_dbHandle(ob.m_dbHandle), 
            m_preparedStatements(std::move(ob.m_preparedStatements)) 
        {
            ob.m_dbHandle = nullptr;
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="DatabaseConn"/> class.
        /// </summary>
        DatabaseConn::~DatabaseConn()
        {
            if(m_dbHandle != nullptr)
            {
                m_preparedStatements.clear();
                int status = sqlite3_close(m_dbHandle);
                _ASSERTE(status == SQLITE_OK);
            }
        }

        /// <summary>
        /// Creates a SQL statement for the current database.
        /// </summary>
        /// <param name="query">The query/statement.</param>
        /// <returns>A <see cref="PrepStatement"/> object for the created SQLite statement already prepared.</returns>
        PrepStatement DatabaseConn::CreateStatement(const string &query)
        {
            CALL_STACK_TRACE;
            _ASSERTE(m_dbHandle != nullptr); // Cannot create statement for a database which failed to initialize
            return PrepStatement(*this, query);
        }

        // Overload
        PrepStatement DatabaseConn::CreateStatement(const char *query)
        {
            CALL_STACK_TRACE;
            _ASSERTE(m_dbHandle != nullptr); // Cannot create statement for a database which failed to initialize
            return PrepStatement(*this, query);
        }

        /// <summary>
        /// Prepares and stores a statement in cache for recurrent use.
        /// </summary>
        /// <param name="queryId">The query unique identifier.</param>
        /// <param name="queryCode">The query code.</param>
        /// <param name="qlen">The query length.</param>
        /// <returns>A reference to the cached statement.</returns>
        PrepStatement & DatabaseConn::CachedStatement(int queryId, const char *queryCode, size_t qlen)
        {
            CALL_STACK_TRACE;

            try
            {
                auto iter = m_preparedStatements.find(queryId);

                if(m_preparedStatements.end() != iter)
                    return iter->second;
                else
                {
                    _ASSERTE(m_dbHandle != nullptr); // Cannot create statement for a database which failed to initialize
                    _ASSERTE(queryCode != nullptr); // Cannot create statement if ID is not recognized and query code argument has been omitted

                    auto ret = m_preparedStatements.emplace(
                        queryId, 
                        PrepStatement(*this, queryCode, qlen == 0 ? strlen(queryCode) : qlen)
                    );

                    _ASSERTE(ret.second); // Cannot store more than one statement with the same ID
                    return ret.first->second;
                }
            }
            catch(std::exception &ex)
            {
                ostringstream oss;
                oss << "Failed to store SQLite prepared statement in cache: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        // Overload
        PrepStatement & DatabaseConn::CachedStatement(int queryId, const string &queryCode)
        {
            return CachedStatement(queryId, queryCode.c_str(), queryCode.length());
        }

    }// end of namespace SQLite
}// end of namespace _3fd
