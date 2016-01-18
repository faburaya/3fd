#include "stdafx.h"
#include "isam_impl.h"
#include <codecvt>
#include <cassert>

namespace _3fd
{
	namespace isam
	{
		///////////////////////////
		// Database Class
		///////////////////////////

		/// <summary>
		/// Finalizes an instance of the <see cref="DatabaseImpl"/> class.
		/// </summary>
		DatabaseImpl::~DatabaseImpl()
		{
			CALL_STACK_TRACE;
			auto rcode = JetCloseDatabase(m_jetSession, m_jetDatabase, 0);
			ErrorHelper::LogError(NULL, m_jetSession, rcode, "Failed to close ISAM database", core::Logger::PRIO_ERROR);
		}

		/// <summary>
		/// Opens a table from the ISAM database.
		/// </summary>
		/// <param name="name">The table name.</param>
		/// <param name="throwTableNotFound">Whether an exception must be thrown in case the table is not found in the schema.</param>
		/// <returns>
		/// A <see cref="ITable" /> interface for the opened table.
		/// If 'throwTableNotFound' is set to 'false' and the table is not found, returns a null pointer.
		/// </returns>
		ITable * DatabaseImpl::OpenTable(const string &name, bool throwTableNotFound)
		{
			CALL_STACK_TRACE;

			try
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				const auto ucs2tableName = transcoder.from_bytes(name);

				JET_TABLEID jetTable;

				auto rcode = JetOpenTableW(m_jetSession, m_jetDatabase,
					ucs2tableName.data(),
					nullptr, 0, 0,
					&jetTable);

				if (rcode == JET_errObjectNotFound && throwTableNotFound == false)
					return nullptr;
				else
				{
					ErrorHelper::HandleError(NULL, m_jetSession, rcode, [&name]()
					{
						std::ostringstream oss;
						oss << "Failed to open table \'" << name << "\' from ISAM database";
						return oss.str();
					});

					return new Table(this, jetTable, name);
				}
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when opening table in ISAM database: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
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
		ITable * DatabaseImpl::CreateTable(
			const string &name,
			bool isTemplate,
			const std::vector<ITable::ColumnDefinition> &columns,
			const std::vector<ITable::IndexDefinition> &indexes,
			bool sparse,
			unsigned long reservedPages)
		{
			CALL_STACK_TRACE;

			try
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				const auto ucs2tableName = transcoder.from_bytes(name);

#ifndef _3FD_PLATFORM_WINRT
				typedef JET_TABLECREATE2_W JET_TABLECREATE_X;
#else
				typedef JET_TABLECREATE4_W JET_TABLECREATE_X;
#endif
				// Set the table specs:
				JET_TABLECREATE_X jetTable = {};
				jetTable.cbStruct = sizeof jetTable;
				jetTable.szTableName = const_cast<wchar_t *> (ucs2tableName.c_str());
				jetTable.szTemplateTableName = nullptr;
				jetTable.ulPages = reservedPages;
				jetTable.ulDensity = sparse ? 20 : 0; // 0 forces the default value which is normally 80%
				jetTable.grbit = isTemplate ? JET_bitTableCreateTemplateTable : 0;

				std::vector<JET_COLUMNCREATE_W> jetColumns;
				jetColumns.reserve(columns.size());

				// Translated the columns definitions into the ESENT API expected structures:
				for (auto &col : columns)
				{
					/* Default value type must match the column data type. Take into consideration, however,
					that auxiliary functions such as 'AsInputParam' will consider a blob and a large blob as
					the same, so a default parameter which is a large blob might be truncated to fit into a
					regular one. The same goes for text and large text. */
					_ASSERTE(
						col.dataType == col.default.dataType
						|| ((col.dataType == DataType::Blob || col.dataType == DataType::LargeBlob)
						&& (col.default.dataType == DataType::Blob || col.default.dataType == DataType::LargeBlob))
						|| ((col.dataType == DataType::Text || col.dataType == DataType::LargeText)
						&& (col.default.dataType == DataType::Text || col.default.dataType == DataType::LargeText))
						);

					/* Multi-valued columns not null are currently not supported because they are not well handled
					by Microsoft ESE. Everything works, except for the removal of a value, which must happen setting
					it to NULL, which is not allowed. */
					_ASSERTE((col.multiValued && col.notNull) == false);

					JET_COLUMNCREATE_W jetCol = {};
					jetCol.cbStruct = sizeof jetCol;
					jetCol.szColumnName = const_cast<wchar_t *> (col.name.c_str());
					jetCol.coltyp = static_cast<decltype(jetCol.coltyp)> (col.dataType);
					jetCol.cbMax = Table::GetMaxLength(col.dataType);
					jetCol.pvDefault = col.default.data;
					jetCol.cbDefault = col.default.qtBytes;
					jetCol.cp = static_cast<decltype(jetCol.cp)> (col.codePage);
					jetCol.grbit = 0;

					// Not null?
					if (col.notNull)
						jetCol.grbit |= JET_bitColumnNotNULL;

					// Multi-valued?
					if (col.multiValued)
						jetCol.grbit |= JET_bitColumnMultiValued | JET_bitColumnTagged;

					// Sparse?
					if (col.sparse)
						jetCol.grbit |= JET_bitColumnTagged;

					// Automatic increment?
					if (col.autoIncrement)
					{
						if (col.dataType == DataType::Int32 || col.dataType == DataType::Currency)
							jetCol.grbit |= JET_bitColumnAutoincrement;
						else
						{
							std::ostringstream oss;
							oss << "Failed to create table \'" << name
								<< "\' in ISAM database: column type can only be 'Int32' or 'Currency' in order to use automatic increment";
							throw core::AppException<std::invalid_argument>(oss.str());
						}
					}

					// Can use escrow update optimization?
					if (col.dataType == DataType::Int32
						&& col.default.qtBytes > 0
						&& col.sparse == false
						&& col.autoIncrement == false)
					{
						jetCol.grbit |= JET_bitColumnEscrowUpdate;
					}

					jetColumns.emplace_back(jetCol);
				}

				jetTable.rgcolumncreate = jetColumns.data();
				jetTable.cColumns = jetColumns.size();

				// Index definitions:
				std::vector<JET_INDEXCREATE_X> jetIndexes;
				TranslateStructures(indexes, jetIndexes);
				jetTable.rgindexcreate = jetIndexes.data();
				jetTable.cIndexes = jetIndexes.size();

				// Finally invoke API to create the table:
#ifndef _3FD_PLATFORM_WINRT
				auto rcode = JetCreateTableColumnIndex2W(m_jetSession, m_jetDatabase, &jetTable);
#else
				auto rcode = JetCreateTableColumnIndex4W(m_jetSession, m_jetDatabase, &jetTable);
#endif
				if (rcode != JET_errSuccess) // Failed to create the table:
				{
					// Log an error for each column that might have caused the error:
					for (auto &jetCol : jetColumns)
					{
						if (jetCol.err != JET_errSuccess)
						{
							ErrorHelper::LogError(NULL, m_jetSession, jetCol.err, [&transcoder, &name, &jetCol]()
							{
								std::ostringstream oss;
								oss << "Failed to create column \'" << transcoder.to_bytes(jetCol.szColumnName)
									<< "\' in table \'" << name
									<< "\' of ISAM database";
								return oss.str();
							}, core::Logger::PRIO_ERROR);
						}
					}

					// Log an error for each index that might have caused the error:
					for (auto &jetIdx : jetIndexes)
					{
						if (jetIdx.err != JET_errSuccess)
						{
							ErrorHelper::LogError(NULL, m_jetSession, jetIdx.err, [&transcoder, &name, &jetIdx]()
							{
								std::ostringstream oss;
								oss << "Failed to create index \'" << transcoder.to_bytes(jetIdx.szIndexName)
									<< "\' in table \'" << name
									<< "\' of ISAM database";
								return oss.str();
							}, core::Logger::PRIO_ERROR);
						}
					}

					// Throw an exception for the error:
					ErrorHelper::HandleError(NULL, m_jetSession, rcode, [&name]()
					{
						std::ostringstream oss;
						oss << "Failed to create table \'" << name << "\' in ISAM database";
						return oss.str();
					});
				}

				return new Table(this, jetTable.tableid, name);
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when creating table in ISAM database: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Creates a new table from a template.
		/// </summary>
		/// <param name="name">The table name.</param>
		/// <param name="templateName">Name of the template.</param>
		/// <param name="sparse">Whether the table is sparse, which will tell the backend to expect a 20% density.</param>
		/// <param name="reservedPages">The amount of pages to reserve for this table.</param>
		/// <returns>A <see cref="ITable" /> interface for the newly created table.</returns>
		ITable * DatabaseImpl::CreateTable(
			const string &name,
			const string &templateName,
			bool sparse,
			unsigned long reservedPages)
		{
			CALL_STACK_TRACE;

			try
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				const auto ucs2tableName = transcoder.from_bytes(name);
				const auto ucs2templateName = transcoder.from_bytes(templateName);

#ifndef _3FD_PLATFORM_WINRT
				typedef JET_TABLECREATE_W JET_TABLECREATE_X;
#else
				typedef JET_TABLECREATE4_W JET_TABLECREATE_X;
#endif
				// Set the table specs:
				JET_TABLECREATE_X jetTable = {};
				jetTable.cbStruct = sizeof jetTable;
				jetTable.szTableName = const_cast<wchar_t *> (ucs2tableName.c_str());
				jetTable.szTemplateTableName = const_cast<wchar_t *> (ucs2templateName.c_str());
				jetTable.ulPages = reservedPages;
				jetTable.ulDensity = sparse ? 20 : 0; // 0 forces the default value which is normally 80%
				jetTable.rgcolumncreate = nullptr;
				jetTable.cColumns = 0;
				jetTable.rgindexcreate = nullptr;
				jetTable.cIndexes = 0;
				jetTable.grbit = 0;

				// Finally invoke API to create the table:
#ifndef _3FD_PLATFORM_WINRT
				auto rcode = JetCreateTableColumnIndexW(m_jetSession, m_jetDatabase, &jetTable);
#else
				auto rcode = JetCreateTableColumnIndex4W(m_jetSession, m_jetDatabase, &jetTable);
#endif
				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [&name]()
				{
					std::ostringstream oss;
					oss << "Failed to create table \'" << name << "\' from template in ISAM database";
					return oss.str();
				});

				return new Table(this, jetTable.tableid, name);
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when creating table from template in ISAM database: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Deletes a table.
		/// </summary>
		/// <param name="name">The table name.</param>
		void DatabaseImpl::DeleteTable(const string &name)
		{
			CALL_STACK_TRACE;

			try
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				const auto ucs2tableName = transcoder.from_bytes(name);

				auto rcode = JetDeleteTableW(m_jetSession, m_jetDatabase, ucs2tableName.c_str());

				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [&name]()
				{
					std::ostringstream oss;
					oss << "Failed to delete table \'" << name << "\' in ISAM database";
					return oss.str();
				});
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when deleting table from ISAM database: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Gets a cursor for the table.
		/// </summary>
		/// <param name="table">The table for which a cursor must be retrieved.</param>
		/// <param name="prefetch">Whether the table data should be prefetched in cache for optimized sequential access.</param>
		/// <returns>
		/// A private implementation for table cursor.
		/// </returns>
		TableCursorImpl * DatabaseImpl::GetCursorFor(const std::shared_ptr<ITable> &table, bool prefetch)
		{
			CALL_STACK_TRACE;

			try
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				const auto ucs2tableName = transcoder.from_bytes(table->GetName());

				JET_TABLEID jetTable;
				auto rcode = JetOpenTableW(m_jetSession, m_jetDatabase,
					ucs2tableName.data(),
					nullptr, 0,
					prefetch ? JET_bitTableSequential : 0,
					&jetTable);

				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [&table]()
				{
					std::ostringstream oss;
					oss << "Failed to get cursor for table \'" << table->GetName() << "\' from ISAM database";
					return oss.str();
				});

				return new TableCursorImpl(table, jetTable, m_jetSession);
			}
			catch (core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when creating cursor for table in ISAM database: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

	}// end namespace isam
}// end namespace _3fd
