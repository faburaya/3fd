#include "stdafx.h"
#include "isam_impl.h"

namespace _3fd
{
	namespace isam
	{
		///////////////////////////
		// DatabaseConn Class
		///////////////////////////
		
		/// <summary>
		/// Try opening a table from the ISAM database.
		/// </summary>
		/// <param name="tableWasFound">Tells whether the table was found in the schema and successfully opened.</param>
		/// <param name="name">The table name.</param>
		/// <returns>A <see cref="ITable" /> interface for the opened table.</returns>
		std::shared_ptr<ITable> DatabaseConn::TryOpenTable(bool &tableWasFound, const string &name)
		{
			auto table = m_pimplDatabase->OpenTable(name, false);
			tableWasFound = (table != nullptr);
			return std::shared_ptr<ITable>(table);
		}

		/// <summary>
		/// Opens a table from the ISAM database.
		/// </summary>
		/// <param name="name">The table name.</param>
		/// <returns>A <see cref="ITable" /> interface for the opened table.</returns>
		std::shared_ptr<ITable> DatabaseConn::OpenTable(const string &name)
		{
			return std::shared_ptr<ITable>(m_pimplDatabase->OpenTable(name, true));
		}
			
		/// <summary>
		/// Creates a new table in the ISAM database.
		/// </summary>
		/// <param name="name">The table name.</param>
		/// <param name="isTemplate">Whether it is a template for creation of other similar tables.</param>
		/// <param name="columns">The set of column definitions.</param>
		/// <param name="indexes">The set of index definitions.</param>
		/// <param name="sparse">Whether the table is sparse, which will tell the backend to expect a 20% density.</param>
		/// <param name="reservedPages">The amount of pages to reserve for this table.</param>
		/// <returns>A <see cref="ITable" /> interface for the newly created table.</returns>
		std::shared_ptr<ITable> 
		DatabaseConn::CreateTable(const string &name, bool isTemplate, 
								  const std::vector<Table::ColumnDefinition> &columns, 
								  const std::vector<Table::IndexDefinition> &indexes, 
								  bool sparse, unsigned long reservedPages)
		{
			return std::shared_ptr<ITable>(
				m_pimplDatabase->CreateTable(name, isTemplate, columns, indexes, sparse, reservedPages)
			);
		}

		/// <summary>
		/// Creates a new table from a template.
		/// </summary>
		/// <param name="name">The table name.</param>
		/// <param name="templateName">Name of the template.</param>
		/// <param name="sparse">Whether the table is sparse, which will tell the backend to expect a 20% density.</param>
		/// <param name="reservedPages">The amount of pages to reserve for this table.</param>
		/// <returns>A newly created <see cref="Table" /> object for the newly created table.</returns>
		std::shared_ptr<ITable> 
		DatabaseConn::CreateTable(const string &name, const string &templateName, 
								  bool sparse, unsigned long reservedPages)
		{
			return std::shared_ptr<ITable>(
				m_pimplDatabase->CreateTable(name, templateName, sparse, reservedPages)
			);
		}

		/// <summary>
		/// Deletes a table.
		/// </summary>
		/// <param name="name">The table name.</param>
		void DatabaseConn::DeleteTable(const string &name)
		{
			m_pimplDatabase->DeleteTable(name);
		}

		/// <summary>
		/// Gets a cursor for the table.
		/// </summary>
		/// <param name="table">The table for which a cursor must be retrieved.</param>
		/// <param name="prefetch">Whether the table data should be prefetched in cache for optimized sequential access.</param>
		/// <returns>A table cursor.</returns>
		TableCursor DatabaseConn::GetCursorFor(const std::shared_ptr<ITable> &table, bool prefetch)
		{
			return TableCursor(m_pimplDatabase->GetCursorFor(*table, prefetch));
		}
		
		/// <summary>
		/// Begins a transaction in the current session.
		/// </summary>
		/// <returns>A <see cref=Transaction" /> object.</returns>
		Transaction DatabaseConn::BeginTransaction()
		{
			return Transaction(m_pimplSession->CreateTransaction());
		}

		/// <summary>
		/// Flushes all transactions previously committed by the current session 
		/// and that have noy yet been flushed to the transaction log file.
		/// </summary>
		void DatabaseConn::Flush()
		{
			m_pimplSession->Flush();
		}
		
	}// end namespace isam
}// end namespace _3fd
