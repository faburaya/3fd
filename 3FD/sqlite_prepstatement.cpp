#include "stdafx.h"
#include "sqlite.h"
#include "logger.h"
#include "exceptions.h"
#include <cassert>
#include <codecvt>
#include <locale>
#include <sstream>
#include <algorithm>

#undef min

namespace _3fd
{
    using std::ostringstream;

    namespace sqlite
    {
        ///////////////////////////////
        // PrepStatement Class
        ///////////////////////////////

        /// <summary>
        /// The implementation used by the constructors.
        /// </summary>
        /// <param name="query">The query text.</param>
        /// <param name="length">The query length.</param>
        void PrepStatement::CtorImpl(const char *query, size_t length)
        {
            CALL_STACK_TRACE;

            int attempts(0);

            while (true)
            {
                // Query preparation:
                int status = sqlite3_prepare_v2(m_database.GetHandle(),
                                                query,
                                                length + 1,
                                                &m_stmtHandle,
                                                nullptr);
                ++attempts;
                unsigned char primErrCode(status & 255);

                if (status == SQLITE_OK)
                    break;
                // Query preparation might fail if a shared lock could not be acquired:
                else if (primErrCode == SQLITE_BUSY || primErrCode == SQLITE_LOCKED)
                {
                    if (attempts == 1)
                        srand(static_cast<uint32_t> (time(nullptr)));

                    // Wait a little for an opportunity of acquiring a lock:
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(rand() % std::min(3*attempts, 50) + 1)
                    );
                }
                else // However, if it fails for any other reason:
                {
                    // Write in the log about the attempts:
                    ostringstream oss;
                    oss << "Failed to prepare SQLite statement after " << attempts
                        << " attempt(s): " << sqlite3_errstr(status);

                    core::Logger::Write(oss.str(), core::Logger::PRIO_ERROR);
                    oss.str("");

                    // Then abort throwing an exception:
                    oss << "SQLite API error code " << status
                        << " - 'sqlite3_prepare_v2' reported: " << sqlite3_errstr(status)
                        << ". Query was {" << query << '}';

                    throw core::AppException<std::runtime_error>("Failed to prepare SQLite statement", oss.str());
                }
            }// loop: if lock failure, retry

            // Get the columns count in the result set of the prepared query
            int numColumns = sqlite3_column_count(m_stmtHandle);

            // Get the names of the columns:
            for (int index = 0; index < numColumns; ++index)
            {
                m_columnIndexes.emplace(
                    string(sqlite3_column_name(m_stmtHandle, index)),
                    index
                );
            }
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="PrepStatement"/> class.
        /// </summary>
        /// <param name="database">The SQLite database which owns this statement.</param>
        /// <param name="query">The query/statement.</param>
        PrepStatement::PrepStatement(DatabaseConn &database, 
                                     const string &query)
        try : 
            m_stmtHandle(nullptr), 
            m_database(database), 
            m_stepping(false) 
        {
            CALL_STACK_TRACE;
            CtorImpl(query.c_str(), query.length());
        }
        catch(core::IAppException &)
        {
            throw; // just forwards the exceptions known to have been previously handled
        }
        catch(std::exception &ex)
        {
            CALL_STACK_TRACE;
            ostringstream oss;
            oss << "Generic failure when creating SQLite statement: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }

        // Overload
        PrepStatement::PrepStatement(DatabaseConn &database, 
                                     const char *query, 
                                     size_t qlen)
        try : 
            m_stmtHandle(nullptr), 
            m_database(database), 
            m_stepping(false) 
        {
            CALL_STACK_TRACE;
            CtorImpl(query, qlen == 0 ? strlen(query) : qlen);
        }
        catch(core::IAppException &)
        {
            throw; // just forwards the exceptions known to have been previously handled
        }
        catch(std::exception &ex)
        {
            CALL_STACK_TRACE;
            ostringstream oss;
            oss << "Generic failure when creating SQLite statement: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="PrepStatement"/> class.
        /// </summary>
        /// <param name="ob">The object whose resources will be moved.</param>
        PrepStatement::PrepStatement(PrepStatement &&ob) : 
            m_stmtHandle(ob.m_stmtHandle), 
            m_database(ob.m_database), 
            m_columnIndexes(std::move(ob.m_columnIndexes)), 
            m_stepping(ob.m_stepping) 
        {
            ob.m_stmtHandle = nullptr;
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="PrepStatement"/> class.
        /// </summary>
        PrepStatement::~PrepStatement()
        {
            if(m_stmtHandle != nullptr)
            {
                CALL_STACK_TRACE;
                Reset(); // releases any lock in the database
                sqlite3_finalize(m_stmtHandle);
            }
        }

        /// <summary>
        /// Get the query text.
        /// </summary>
        /// <returns>The SQL text (UTF-8 encoded).</returns>
        string PrepStatement::GetQuery() const
        {
            return sqlite3_sql(m_stmtHandle);
        }

        /// <summary>
        /// Gets the index of a parameter in a SQLite statement.
        /// </summary>
        /// <param name="stmtHandle">The statement handle.</param>
        /// <param name="paramName">Name of the parameter.</param>
        /// <returns>The parameter index.</returns>
        static int GetParamIndex(sqlite3_stmt *stmtHandle, const string &paramName)
        {
            int paramIndex = sqlite3_bind_parameter_index(stmtHandle, paramName.c_str());

    #    ifndef NDEBUG

            if(paramIndex == 0)
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite API: 'sqlite3_bind_parameter_index' - The parameter \'" << paramName 
                    << "\' was not found int the query. Please check SQLite documentation. Query was {" << sqlite3_sql(stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Could not find parameter in SQLite statement", oss.str());
            }
    #    endif

            return paramIndex;
        }

        /// <summary>
        /// Binds the specified parameter to an integer value.
        /// </summary>
        /// <param name="paramName">Name of the parameter.</param>
        /// <param name="integer">The integer value.</param>
        void PrepStatement::Bind(const string &paramName, int integer)
        {
            int status = sqlite3_bind_int(m_stmtHandle, 
                                          GetParamIndex(m_stmtHandle, paramName), 
                                          integer);
            if(status != SQLITE_OK)
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite API error code " << status
                    << " - 'sqlite3_bind_int' reported: " << sqlite3_errstr(status) 
                    << ". Parameter was \'" << paramName 
                    << "\' and the query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to bind integer value to the SQLite statement parameter", oss.str());
            }
        }

        /// <summary>
        /// Binds the specified parameter to an integer value.
        /// </summary>
        /// <param name="paramName">Name of the parameter.</param>
        /// <param name="integer">The integer value.</param>
        void PrepStatement::Bind(const string &paramName, long long integer)
        {
            int status = sqlite3_bind_int64(m_stmtHandle, 
                                            GetParamIndex(m_stmtHandle, paramName), 
                                            integer);
            if(status != SQLITE_OK)
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite API error code " << status
                    << " - 'sqlite3_bind_int64' reported: " << sqlite3_errstr(status)
                    << ". Parameter was \'" << paramName 
                    << "\' and the query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to bind integer value to the SQLite statement parameter", oss.str());
            }
        }

        /// <summary>
        /// Binds the specified parameter to a real number.
        /// </summary>
        /// <param name="paramName">Name of the parameter.</param>
        /// <param name="real">The real number.</param>
        void PrepStatement::Bind(const string &paramName, double real)
        {
            int status = sqlite3_bind_double(m_stmtHandle, 
                                             GetParamIndex(m_stmtHandle, paramName), 
                                             real);
            if(status != SQLITE_OK)
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite API error code " << status
                    << " - 'sqlite3_bind_double' reported: " << sqlite3_errstr(status)
                    << ". Parameter was \'" << paramName 
                    << "\' and the query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to bind floating point value to the SQLite statement parameter", oss.str());
            }
        }

        /// <summary>
        /// Binds the specified parameter to text content.
        /// </summary>
        /// <param name="paramName">Name of the parameter.</param>
        /// <param name="text">The text content.</param>
        void PrepStatement::Bind(const string &paramName, const string &text)
        {
            int status = sqlite3_bind_text(m_stmtHandle,
                                           GetParamIndex(m_stmtHandle, paramName), 
                                           text.data(), 
                                           text.size(), 
                                           SQLITE_TRANSIENT);
            if(status != SQLITE_OK)
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite API error code " << status
                    << " - 'sqlite3_bind_text' reported: " << sqlite3_errstr(status)
                    << ". Parameter was \'" << paramName 
                    << "\' and the query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to bind text content to the SQLite statement parameter", oss.str());
            }
        }

        /// <summary>
        /// Binds the specified parameter to a text value (UCS-2 encoded).
        /// </summary>
        /// <param name="paramName">Name of the parameter.</param>
        /// <param name="text">The text value (UCS-2 encoded).</param>
        void PrepStatement::Bind(const string &paramName, const wstring &text)
        {
            CALL_STACK_TRACE;

            try
            {
                // Transcode UCS-2 into UTF-8:
                std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
                auto textAsUTF8 = transcoder.to_bytes(text);

                // Always store text as UTF-8:
                Bind(paramName, textAsUTF8);
            }
            catch(core::IAppException &)
            {
                throw; // Just forward errors known to have been previously handled
            }
            catch(std::exception &ex)
            {
                ostringstream oss;
                oss << "Generic failure when binding text content to a SQLite statement parameter: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Binds the specified parameter to a blob value.
        /// </summary>
        /// <param name="paramName">Name of the parameter.</param>
        /// <param name="blob">The blob value.</param>
        void PrepStatement::Bind(const string &paramName, const void *blob, int nBytes)
        {
            int status = sqlite3_bind_blob(m_stmtHandle, 
                                           GetParamIndex(m_stmtHandle, paramName), 
                                           blob, 
                                           nBytes, 
                                           SQLITE_TRANSIENT);
            if(status != SQLITE_OK)
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite API error code " << status
                    << " - 'sqlite3_bind_blob' reported: " << sqlite3_errstr(status)
                    << ". Parameter was \'" << paramName 
                    << "\' and the query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to bind text content to the SQLite statement parameter", oss.str());
            }
        }

        /// <summary>
        /// Clears the bindings.
        /// </summary>
        void PrepStatement::ClearBindings()
        {
            int status = sqlite3_clear_bindings(m_stmtHandle);

            if(status != SQLITE_OK)
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite API error code " << status
                    << " - 'sqlite3_clear_bindings' reported: " << sqlite3_errstr(status)
                    << ". Query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to clear parameter bindings from the SQLite statement", oss.str());
            }
        }

        /// <summary>
        /// Perform one more step into execution. Uses lock conflict resolution algorithm. This is a blocking call.
        /// </summary>
        /// <param name="throwEx">if set to <c>false</c>, does not throw exception on error.</param>
        /// <returns>The return code of 'sqlite3_step'.</returns>
        int PrepStatement::Step(bool throwEx)
        {
            CALL_STACK_TRACE;

            int attempts(0);

            while (true)
            {
                int status = sqlite3_step(m_stmtHandle);

                ++attempts;
                unsigned char primErrCode(status & 255);

                // Succesfully issue a row of the result set:
                if (status == SQLITE_ROW)
                {
                    m_stepping = true;
                    return status;
                }
                // Successfully issued the last row of the result set:
                else if (status == SQLITE_DONE)
                {
                    m_stepping = true;
                    Reset();
                    return status;
                }
                // Failed because it could not get a lock:
                else if (primErrCode == SQLITE_BUSY || primErrCode == SQLITE_LOCKED)
                {
                    if (attempts == 1)
                        srand(static_cast<uint32_t> (time(nullptr)));

                    // Wait a little before retrying the lock:
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(rand() % std::min(3*attempts, 50) + 1)
                    );
                }
                else // However, when it fails for any other reason:
                {
                    try
                    {
                        // Write in the log about the attempts:
                        ostringstream oss;
                        oss << "Failed to execute step of SQLite statement after " << attempts
                            << " attempt(s): " << sqlite3_errstr(status);

                        core::Logger::Write(oss.str(), core::Logger::PRIO_ERROR, true);

                        // Abort:
                        if (throwEx)
                        {
                            oss.str("");
                            oss << "SQLite API error code " << status
                                << " - 'sqlite3_step' reported: " << sqlite3_errstr(status) 
                                << ". Query was {" << sqlite3_sql(m_stmtHandle) << '}';

                            throw core::AppException<std::runtime_error>("Failed to execute step of SQLite statement", oss.str());
                        }
                        else
                            return status;
                    }
                    catch (core::IAppException &)
                    {
                        throw; // just forwards the exceptions known to have been previously handled
                    }
                    catch (std::exception &ex)
                    {
                        ostringstream oss;
                        oss << "Generic failure when executing step of SQLite statement: " << ex.what();

                        if (throwEx)
                            throw core::AppException<std::runtime_error>(oss.str());
                        else
                        {
                            core::Logger::Write(oss.str(), core::Logger::PRIO_ERROR, true);
                            return status;
                        }
                    }
                }
            }// loop: retry the step
        }

        /// <summary>
        /// Attempts to execute another step into the statement execution, but will fail on lock conflict. This is a non-blocking call.
        /// </summary>
        /// <param name="throwEx">if set to <c>false</c>, does not throw exception on error.</param>
        /// <returns>The return code of 'sqlite3_step'.</returns>
        int PrepStatement::TryStep(bool throwEx)
        {
            CALL_STACK_TRACE;

            int status = sqlite3_step(m_stmtHandle);

            if (status == SQLITE_ROW)
            {
                m_stepping = true;
            }
            else if (status == SQLITE_DONE)
            {
                m_stepping = true;
                Reset();
            }
            else
            {
                if (throwEx)
                {
                    ostringstream oss;
                    oss << "SQLite API error code " << status
                        << " - 'sqlite3_step' reported: " << sqlite3_errstr(status) 
                        << ". Query was {" << sqlite3_sql(m_stmtHandle) << '}';

                    throw core::AppException<std::runtime_error>("Failed to execute step of SQLite statement", oss.str());
                }
            }

            return status;
        }

        /// <summary>
        /// Resets an ongoing execution.
        /// </summary>
        void PrepStatement::Reset()
        {
            sqlite3_reset(m_stmtHandle);
            m_stepping = false;
        }

        /// <summary>
        /// Gets the column value as integer.
        /// </summary>
        /// <param name="columnName">Name of the column.</param>
        /// <returns>The column value.</returns>
        int PrepStatement::GetColumnValueInteger(const string &columnName)
        {
            _ASSERTE(m_stepping == true); // Cannot retrieve a value before stepping into the query execution

            auto iter = m_columnIndexes.find(columnName);

            if(m_columnIndexes.end() != iter)
            {
                _ASSERTE(sqlite3_column_type(m_stmtHandle, iter->second) == SQLITE_INTEGER); // Fires if the specified column does not hold an integer number as value
                return sqlite3_column_int(m_stmtHandle, iter->second);
            }
            else
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite wrapper error: the column \'" << columnName
                    << "\' does not belong to the output row. Query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to get integer value from SQLite query result", oss.str());
            }
        }

        /// <summary>
        /// Gets the column value as integer 64 bits.
        /// </summary>
        /// <param name="columnName">Name of the column.</param>
        /// <returns>The column value.</returns>
        long long PrepStatement::GetColumnValueInteger64(const string &columnName)
        {
            _ASSERTE(m_stepping == true); // Cannot retrieve a value before stepping into the query execution

            auto iter = m_columnIndexes.find(columnName);

            if(m_columnIndexes.end() != iter)
            {
                _ASSERTE(sqlite3_column_type(m_stmtHandle, iter->second) == SQLITE_INTEGER); // Fires if the specified column does not hold an integer number as value
                return sqlite3_column_int64(m_stmtHandle, iter->second);
            }
            else
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite wrapper error: the column \'" << columnName
                    << "\' does not belong to the output row. Query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to get integer value from SQLite query result", oss.str());
            }
        }

        /// <summary>
        /// Gets the column value as double precision floating point.
        /// </summary>
        /// <param name="columnName">Name of the column.</param>
        /// <returns>The column value.</returns>
        double PrepStatement::GetColumnValueFloat64(const string &columnName)
        {
            _ASSERTE(m_stepping == true); // Cannot retrieve a value before stepping into the query execution

            auto iter = m_columnIndexes.find(columnName);

            if(m_columnIndexes.end() != iter)
            {
                _ASSERTE(sqlite3_column_type(m_stmtHandle, iter->second) == SQLITE_FLOAT); // Fires if the specified column does not hold a floating point number as value
                return sqlite3_column_double(m_stmtHandle, iter->second);
            }
            else
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite wrapper error: the column \'" << columnName
                    << "\' does not belong to the output row. Query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to get floating point value from SQLite query result", oss.str());
            }
        }

        /// <summary>
        /// Gets the column value as text.
        /// </summary>
        /// <param name="columnName">Name of the column.</param>
        /// <returns>The column value.</returns>
        string PrepStatement::GetColumnValueText(const string &columnName)
        {
            _ASSERTE(m_stepping == true); // Cannot retrieve a value before stepping into the query execution

            auto iter = m_columnIndexes.find(columnName);

            if(m_columnIndexes.end() != iter)
            {
                _ASSERTE(sqlite3_column_type(m_stmtHandle, iter->second) == SQLITE_TEXT); // Fires if the specified column does not hold text content
                return reinterpret_cast<const char *> (sqlite3_column_text(m_stmtHandle, iter->second));
            }
            else
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite wrapper error: the column \'" << columnName
                    << "\' does not belong to the output row. Query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to get text content from SQLite query result", oss.str());
            }
        }

        /// <summary>
        /// Gets the column value as text (UTF-16 encoded).
        /// </summary>
        /// <param name="columnName">Name of the column.</param>
        /// <returns>The column value (UTF-16 encoded).</returns>
        wstring PrepStatement::GetColumnValueText16(const string &columnName)
        {
            _ASSERTE(m_stepping == true); // Cannot retrieve a value before stepping into the query execution

            auto iter = m_columnIndexes.find(columnName);

            if(m_columnIndexes.end() != iter)
            {
                _ASSERTE(sqlite3_column_type(m_stmtHandle, iter->second) == SQLITE_TEXT); // Fires if the specified column does not hold text content
                return reinterpret_cast<const wchar_t *> (sqlite3_column_text16(m_stmtHandle, iter->second));
            }
            else
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite wrapper error: the column \'" << columnName
                    << "\' does not belong to the output row. Query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to get text content from SQLite query result", oss.str());
            }
        }

        /// <summary>
        /// Gets the column value as blob.
        /// </summary>
        /// <param name="columnName">Name of the column.</param>
        /// <returns>The column value.</returns>
        const void * PrepStatement::GetColumnValueBlob(const string &columnName, int &nBytes)
        {
            _ASSERTE(m_stepping == true); // Cannot retrieve a value before stepping into the query execution

            auto iter = m_columnIndexes.find(columnName);

            if(m_columnIndexes.end() != iter)
            {
                _ASSERTE(sqlite3_column_type(m_stmtHandle, iter->second) == SQLITE_BLOB); // Fires if the specified column does not hold blob content
                auto blob = sqlite3_column_blob(m_stmtHandle, iter->second);
                nBytes = sqlite3_column_bytes(m_stmtHandle, iter->second);
                return blob;
            }
            else
            {
                CALL_STACK_TRACE;
                ostringstream oss;
                oss << "SQLite wrapper error: the column \'" << columnName 
                    << "\' does not belong to the output row. Query was {" << sqlite3_sql(m_stmtHandle) << '}';

                throw core::AppException<std::runtime_error>("Failed to get blob content from SQLite query result", oss.str());
            }
        }

    }// end of namespace SQLite
}// end of namespace _3fd
