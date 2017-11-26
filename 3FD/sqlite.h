#ifndef SQLITE_H // header guard
#define SQLITE_H

#include "base.h"
#include <boost/lockfree/queue.hpp>
#include <string>
#include <atomic>
#include <map>
#include "sqlite3.h"

namespace _3fd
{
using std::string;
using std::wstring;

namespace sqlite
{
	class PrepStatement; // forward class declaration

	/// <summary>
	/// Represents a SQLite "connection" to a database.
	/// </summary>
	class DatabaseConn : notcopiable
	{
	private:

		sqlite3	*m_dbHandle;

		/// <summary>
		/// Keeps in cache some prepared statements, just like stored procedures.
		/// </summary>
		std::map<int, PrepStatement> m_preparedStatements;

	public:

		DatabaseConn(const string &dbFilePath, bool fullMutex = true);

		DatabaseConn(DatabaseConn &&ob);

		~DatabaseConn();

		sqlite3 *GetHandle() const { return m_dbHandle; }

		PrepStatement CreateStatement(const string &query);

		PrepStatement CreateStatement(const char *query);
		
		PrepStatement &CachedStatement(int queryId, 
                                       const string &queryCode);

		PrepStatement &CachedStatement(int queryId, 
                                       const char *queryCode = nullptr, 
                                       size_t qlen = 0);
	};

	/// <summary>
	/// Represents a SQLite prepared query.
	/// </summary>
	class PrepStatement : notcopiable
	{
	private:

		sqlite3_stmt			*m_stmtHandle;
		DatabaseConn			&m_database;
		bool					m_stepping;

		std::map<string, int>	m_columnIndexes;

		void CtorImpl(const char *query, size_t length);

	public:

		PrepStatement(DatabaseConn &database, 
						const string &query);

		PrepStatement(DatabaseConn &database, 
						const char *query, 
						size_t qlen = 0);

		PrepStatement(PrepStatement &&ob);

		~PrepStatement();

		sqlite3_stmt *GetHandle() const { return m_stmtHandle; }

		string GetQuery() const;

		void Bind(const string &paramName, int integer);

		void Bind(const string &paramName, long long integer);

		void Bind(const string &paramName, double real);

		void Bind(const string &paramName, const string &text);

		void Bind(const string &paramName, const wstring &text);

		void Bind(const string &paramName, const void *blob, int nBytes);

		void ClearBindings();

		int Step(bool throwEx = true);

		int TryStep(bool throwEx = true);

		void Reset();

		int GetColumnValueInteger(const string &columnName);

		long long GetColumnValueInteger64(const string &columnName);

		double GetColumnValueFloat64(const string &columnName);

		string GetColumnValueText(const string &columnName);

		wstring GetColumnValueText16(const string &columnName);

		const void *GetColumnValueBlob(const string &columnName, int &nBytes);
	};

	class DbConnWrapper; // forward class declaration

	/// <summary>
	/// A pool of "connections" to SQLite databases.
	/// </summary>
	class DbConnPool
	{
	private:

		/// <summary>
		/// A lock free queue that holds the database connections.
		/// </summary>
		boost::lockfree::queue<DatabaseConn *> m_availableConnections;

        std::atomic<uint32_t> m_numConns;
		string m_dbFilePath;

	public:
		
		DbConnPool(const string &dbFilePath);

		~DbConnPool();

		/// <summary>
		/// Gets the total number of connections in the pool (which is not the number of available connections).
		/// </summary>
		unsigned int GetNumConnections() const { return m_numConns; }

		DbConnWrapper AcquireSQLiteConn();

		void ReleaseSQLiteConn(DatabaseConn *conn);

		void CloseAll();
	};

	/// <summary>
	/// A wrapper that ensures a SQLite connection will be returned to its pool.
	/// </summary>
	class DbConnWrapper : notcopiable
	{
	private:

		DbConnPool &pool;
		DatabaseConn *conn;

	public:

		/// <summary>
		/// Gets the database connection.
		/// </summary>
		/// <returns>A reference to a <see cref="DatabaseConn"/> object.</returns>
		DatabaseConn &Get() { return *conn; }

		DbConnWrapper(DbConnPool &p_pool, DatabaseConn *p_conn);

		DbConnWrapper(DbConnWrapper &&ob);

		~DbConnWrapper();
	};

	/// <summary>
	/// Helps to create and guarantees adequate finalization of 
	/// a SQLite transaction while also lock the access to it.
	/// </summary>
	class Transaction : notcopiable
	{
	private:

		bool m_committed;
		DbConnWrapper &m_conn;

		void Begin();
		void RollBack();

	public:

		Transaction(DbConnWrapper &connWrapper);
		~Transaction();

		void Commit();
	};

}// end of namespace sqlite
}// end of namespace _3fd

#endif // header guard
