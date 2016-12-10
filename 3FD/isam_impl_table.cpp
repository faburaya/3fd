#include "stdafx.h"
#include "isam_impl.h"
#include <codecvt>
#include <cassert>

namespace _3fd
{
namespace isam
{
	///////////////////////////
	// Table Class
	///////////////////////////
		
	std::vector<unsigned long> Table::maxLength;

	std::once_flag Table::staticInitFlag;

	/// <summary>
	/// Ensures unique initialization for the class static members.
	/// </summary>
	void Table::EnsureStaticInitialization()
	{
		if(maxLength.empty())
		{
			CALL_STACK_TRACE;

			try
			{
				// Invoke the code below only once despite concurrent access
				std::call_once(staticInitFlag, [] () 
				{
					maxLength.resize(JET_coltypMax);
					maxLength[(size_t) DataType::Boolean] = 1;
					maxLength[(size_t) DataType::UByte] = 1;
					maxLength[(size_t) DataType::Int16] = 2;
					maxLength[(size_t) DataType::Int32] = 4;
					maxLength[(size_t) DataType::Int64] = 8;
					maxLength[(size_t) DataType::UInt16] = 2;
					maxLength[(size_t) DataType::UInt32] = 4;
					maxLength[(size_t) DataType::GUID] = 16;
					maxLength[(size_t) DataType::Float32] = 4;
					maxLength[(size_t) DataType::Float64] = 8;
					maxLength[(size_t) DataType::Currency] = 8;
					maxLength[(size_t) DataType::DateTime] = 8;
					maxLength[(size_t) DataType::Blob] = 255;
					maxLength[(size_t) DataType::LargeBlob] = 2147483647;
					maxLength[(size_t) DataType::Text] = 255;
					maxLength[(size_t) DataType::LargeText] = 2147483647;
				});
			}
			catch(std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "Failed to initialize static resources in the ISAM module: " << core::StdLibExt::GetDetailsFromSystemError(ex);
				throw core::AppException<std::runtime_error>(oss.str(), "std::call_once");
			}
		}
	}
		
	/// <summary>
	/// Initializes a new instance of the <see cref="ColumnDefinition" /> struct.
	/// </summary>
	/// <param name="p_name">Name of the column.</param>
	/// <param name="p_dataType">Type of data.</param>
	/// <param name="p_codePage">The code page if a text column.</param>
	/// <param name="p_notNull">Whether can be null.</param>
	/// <param name="p_multiValued">Whether is multi-valued.</param>
	/// <param name="p_autoIncrement">Whether must automatically filled by increment.</param>
	/// <param name="p_sparse">Whether is sparse.</param>
	Table::ColumnDefinition::ColumnDefinition(const string &p_name, 
                                              DataType p_dataType, 
                                              bool p_notNull, 
                                              bool p_multiValued, 
                                              bool p_sparse)
	try : 
		dataType(p_dataType), 
		notNull(p_notNull), 
		multiValued(p_multiValued), 
		sparse(p_sparse), 
		autoIncrement(false), 
		codePage(CodePage::English) 
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
		name = transcoder.from_bytes(p_name);
		default.dataType = p_dataType;
	}
	catch(std::exception &ex)
	{
		CALL_STACK_TRACE;
		std::ostringstream oss;
		oss << "Generic failure when creating column definition for table in ISAM database: " << ex.what();
		throw core::AppException<std::runtime_error>(oss.str());
	}

	/// <summary>
	/// Initializes a new instance of the <see cref="IndexDefinition"/> struct.
	/// </summary>
	/// <param name="name">The index name.</param>
	/// <param name="keys">The name of the columns along with the order to sort them.</param>
	/// <param name="primary">Whether a primary index.</param>
	/// <param name="unique">Whether the column value must be unique.</param>
	Table::IndexDefinition::IndexDefinition(const string &p_name, 
											const std::vector<std::pair<string, Order>> &p_keys, 
											bool p_primary, 
											bool p_unique)
	try : 
		primary(p_primary), 
		unique(p_unique) 
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
		name = transcoder.from_bytes(p_name);

		_ASSERTE(p_keys.empty() == false); // Must specify at least one key column
		keys.reserve(p_keys.size() * 12); // Estimate some room to reserve

		for(auto &pair : p_keys)
		{
			auto column = transcoder.from_bytes(pair.first);
			auto order = (pair.second == Order::Ascending) ? L'+' : L'-';
			keys.push_back(order);
			keys.insert(keys.end(), column.begin(), column.end());
			keys.push_back(0);
		}

		keys.push_back(0);
	}
	catch(std::exception &ex)
	{
		CALL_STACK_TRACE;
		std::ostringstream oss;
		oss << "Generic failure when creating index definition for table in ISAM database: " << ex.what();
		throw core::AppException<std::runtime_error>(oss.str());
	}
		
	/// <summary>
	/// Initializes a new instance of the <see cref="Table" /> class.
	/// </summary>
	/// <param name="pimplDatabase">The database private implementation.</param>
	/// <param name="jetTable">The table handle.</param>
	/// <param name="name">The table name.</param>
	Table::Table(DatabaseImpl *pimplDatabase, JET_TABLEID jetTable, const string &name) 
	try : 
		m_pimplDatabase(pimplDatabase), 
		m_jetTable(jetTable), 
		m_name(name) 
	{
	}
	catch(std::exception &ex)
	{
		CALL_STACK_TRACE;
		std::ostringstream oss;
		oss << "Generic failure when creating object for table in ISAM database: " << ex.what();
		throw core::AppException<std::runtime_error>(oss.str());
	}

	/// <summary>
	/// Finalizes an instance of the <see cref="Table"/> class.
	/// </summary>
	Table::~Table()
	{
		if(m_jetTable != NULL)
		{
			auto rcode = JetCloseTable(m_pimplDatabase->GetSessionHandle(), m_jetTable);

			ErrorHelper::LogError(NULL, m_pimplDatabase->GetSessionHandle(), rcode, [this] ()
			{
				std::ostringstream oss;
				oss << "Failed to close table \'" << m_name << "\' in ISAM database";
				return oss.str();
			}, core::Logger::PRIO_ERROR);
		}
	}

	/// <summary>
	/// Renames the table.
	/// </summary>
	/// <param name="newName">The new name.</param>
	void Table::Rename(const string &newName)
	{
		CALL_STACK_TRACE;

		try
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
			auto ucs2name = transcoder.from_bytes(m_name);
			auto ucs2newName = transcoder.from_bytes(newName);

			auto rcode = JetRenameTableW(m_pimplDatabase->GetSessionHandle(), 
											m_pimplDatabase->GetDatabaseHandle(), 
											ucs2name.c_str(), 
											ucs2newName.c_str());

			ErrorHelper::HandleError(NULL, m_pimplDatabase->GetSessionHandle(), rcode, [this, &newName] ()
			{
				std::ostringstream oss;
				oss << "Failed to rename table \'" << m_name 
					<< "\' to \'" << newName 
					<< "\' in ISAM database";
				return oss.str();
			});

			m_name = newName;
		}
		catch(core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch(std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when renaming table in ISAM database: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Adds a column to the table layout.
	/// </summary>
	/// <param name="column">The column definition.</param>
	void Table::AddColumn(const ITable::ColumnDefinition &column)
	{
		CALL_STACK_TRACE;

		try
		{
			/* Default value type must match the column data type. Take into consideration, however, 
			that auxiliary functions such as 'AsInputParam' will consider a blob and a large blob as 
			the same, so a default parameter which is a large blob might be truncated to fit into a 
			regular one. The same goes for text and large text. */
			_ASSERTE(
				column.dataType == column.default.dataType
				|| ((column.dataType == DataType::Blob || column.dataType == DataType::LargeBlob) 
					&& (column.default.dataType == DataType::Blob || column.default.dataType == DataType::LargeBlob)) 
				|| ((column.dataType == DataType::Text || column.dataType == DataType::LargeText) 
					&& (column.default.dataType == DataType::Text || column.default.dataType == DataType::LargeText))
			);

			/* Multi-valued columns not null are currently not supported because they are not well handled 
			by Microsoft ESE. Everything works, except for the removal of a value, which must happen setting 
			it to NULL, which is not allowed. */
			_ASSERTE((column.multiValued && column.notNull) == false);

			JET_COLUMNDEF jetColumn;
			jetColumn.cbStruct = sizeof jetColumn;
			jetColumn.columnid = 0;
			jetColumn.coltyp = static_cast<decltype(jetColumn.coltyp)> (column.dataType);
			jetColumn.wCountry = 0;
			jetColumn.langid = 0;
			jetColumn.cp = static_cast<decltype(jetColumn.cp)> (column.codePage);
			jetColumn.wCollate = 0;
			jetColumn.cbMax = GetMaxLength(column.dataType);
			jetColumn.grbit = 0;

			// Not null?
			if(column.notNull)
				jetColumn.grbit |= JET_bitColumnNotNULL;

			// Multi-valued?
			if(column.multiValued)
				jetColumn.grbit |= JET_bitColumnMultiValued | JET_bitColumnTagged;

			// Sparse?
			if(column.sparse)
				jetColumn.grbit |= JET_bitColumnTagged;

			// Automatic increment?
			if(column.autoIncrement)
			{
				if(column.dataType == DataType::Int32 || column.dataType == DataType::Currency)
					jetColumn.grbit |= JET_bitColumnAutoincrement;
				else
				{
					std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
					std::ostringstream oss;
					oss << "Failed to add column \'" << transcoder.to_bytes(column.name) 
						<< "\' from table \'" << m_name 
						<< "\' in ISAM database: column type can only be 'Int32' or 'Currency' in order to use automatic increment";

					throw core::AppException<std::invalid_argument>(oss.str());
				}
			}

			// Can use escrow update optimization?
			if(column.dataType == DataType::Int32 
			   && column.default.qtBytes > 0 
			   && column.sparse == false 
			   && column.autoIncrement == false)
			{
				jetColumn.grbit |= JET_bitColumnEscrowUpdate;
			}

			JET_COLUMNID jetColumnId;

			// Finally invoke the API to add the column:
			auto rcode = JetAddColumnW(m_pimplDatabase->GetSessionHandle(), 
                                       m_jetTable, 
                                       column.name.c_str(), 
                                       &jetColumn, 
                                       column.default.data, 
                                       column.default.qtBytes, 
                                       &jetColumnId);

			ErrorHelper::HandleError(NULL, m_pimplDatabase->GetSessionHandle(), rcode, [this, &column] ()
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
				std::ostringstream oss;
				oss << "Failed to add column \'" << transcoder.to_bytes(column.name) 
					<< "\' to table \'" << m_name 
					<< "\' in ISAM database";
				return oss.str();
			});
		}
		catch(core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch(std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when adding column to table in ISAM database: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Deletes a column from the table.
	/// </summary>
	/// <param name="name">The column name.</param>
	void Table::DeleteColumn(const string &name)
	{
		CALL_STACK_TRACE;

		try
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
			const auto ucs2tableName = transcoder.from_bytes(name);

			auto rcode = JetDeleteColumn2W(m_pimplDatabase->GetSessionHandle(), m_jetTable, ucs2tableName.c_str(), 0);

			ErrorHelper::HandleError(NULL, m_pimplDatabase->GetSessionHandle(), rcode, [this, &name] ()
			{
				std::ostringstream oss;
				oss << "Failed to delete column \'" << name 
					<< "\' from table \'" << m_name 
					<< "\' in ISAM database";

				return oss.str();
			});

			auto iter = m_columnCodesByName.find(name);
				
			// Erase column metadata:
			if(m_columnCodesByName.end() != iter)
			{
				m_colsMetadataByCode.erase(iter->second);
				m_columnCodesByName.erase(iter);
			}
		}
		catch(core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch(std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when deleting column from table in ISAM database: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Maps an integer code to a column name.
	/// Intended to work as an optimization to avoid API calls when reading/writing in the table.
	/// </summary>
	/// <param name="code">The code (intended to be an enumerated element).</param>
	/// <param name="name">The name of the column.</param>
	void Table::MapInt2ColName(int code, const string &colName)
	{
		CALL_STACK_TRACE;

		try
		{
			_ASSERTE(m_colsMetadataByCode.find(code) == m_colsMetadataByCode.end()); // Cannot map column to already used integer code

			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
			auto ucs2colName = transcoder.from_bytes(colName);

			JET_COLUMNDEF colInfo;
			auto rcode = JetGetTableColumnInfoW(m_pimplDatabase->GetSessionHandle(), 
												m_jetTable, 
												ucs2colName.c_str(), 
												&colInfo, 
												sizeof colInfo, 
												JET_ColInfo);

			ErrorHelper::HandleError(NULL, m_pimplDatabase->GetSessionHandle(), rcode, [this, &colName] ()
			{
				std::ostringstream oss;
				oss << "Failed to get information from column \'" << colName 
					<< "\' in table \'" << m_name 
					<< "\' of ISAM database";

				return oss.str();
			});

			// Store column metadata:
			auto &colMetadata = m_colsMetadataByCode[code];
			colMetadata.id = colInfo.columnid;
			colMetadata.dataType = static_cast<DataType> (colInfo.coltyp);
			colMetadata.name = colName;
			colMetadata.notNull = (colInfo.grbit & JET_bitColumnNotNULL) != 0;
			colMetadata.escrow = (colInfo.grbit & JET_bitColumnEscrowUpdate) != 0;
			colMetadata.multiValued = (colInfo.grbit & JET_bitColumnMultiValued) != 0;

			m_columnCodesByName[colName] = code;
		}
		catch(core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch(std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when mapping integer code to column metadata from table in ISAM database: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Gets metadata about a column.
	/// </summary>
	/// <param name="columnCode">The column numeric code.</param>
	/// <returns>A structure containing metadata regarding the specified column.</returns>
	const Table::ColumnMetadata & Table::GetColumnMetadata(int columnCode) const
	{
		auto iter = m_colsMetadataByCode.find(columnCode);
		_ASSERTE(m_colsMetadataByCode.end() != iter); // Cannot use code which has not yet been mapped to a column name
		return iter->second;
	}
		
	/// <summary>
	/// Renames a column.
	/// </summary>
	/// <param name="colName">The column to rename.</param>
	/// <param name="newColName">The new name of the column.</param>
	void Table::RenameColumn(const string &colName, const string &newColName)
	{
		CALL_STACK_TRACE;

		try
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
			auto ucs2colName = transcoder.from_bytes(colName);
			auto ucs2newColName = transcoder.from_bytes(newColName);

			auto rcode = JetRenameColumnW(m_pimplDatabase->GetSessionHandle(), 
                                          m_jetTable, 
                                          ucs2colName.c_str(), 
                                          ucs2newColName.c_str(), 
                                          0);

			ErrorHelper::HandleError(NULL, m_pimplDatabase->GetSessionHandle(), rcode, [this, &colName, &newColName] ()
			{
				std::ostringstream oss;
				oss << "Failed to rename column \'" << colName 
					<< "\' to \'" << newColName 
					<< "\' in table \'" << m_name << "\' from ISAM database";
				return oss.str();
			});

			auto iter = m_columnCodesByName.find(colName);
				
			// Update column metadata:
			if(m_columnCodesByName.end() != iter)
			{
				auto code = iter->second;
				m_colsMetadataByCode[code].name = newColName;
				m_columnCodesByName.erase(iter);
				m_columnCodesByName[newColName] = code;
			}
		}
		catch(core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch(std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when renaming a column in ISAM database: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Creates new indexes to the table.
	/// </summary>
	/// <param name="index">The indexes definitions.</param>
	void Table::CreateIndexes(const std::vector<ITable::IndexDefinition> &indexes)
	{
		CALL_STACK_TRACE;

		try
		{
			std::vector<JET_INDEXCREATE_X> jetIndexes;
			TranslateStructures(indexes, jetIndexes);

#ifndef _3FD_PLATFORM_WINRT
			auto rcode = JetCreateIndex2W(m_pimplDatabase->GetSessionHandle(), 
                                          m_jetTable, 
                                          jetIndexes.data(), 
                                          jetIndexes.size());
#else
			auto rcode = JetCreateIndex4W(m_pimplDatabase->GetSessionHandle(),
                                          m_jetTable,
                                          jetIndexes.data(),
                                          jetIndexes.size());
#endif
			if(rcode != JET_errSuccess) // Failed to create indexes:
			{
				// Log an error for each index that might have caused the error:
				for(auto &jetIdx : jetIndexes)
				{
					if(jetIdx.err != JET_errSuccess)
					{
						ErrorHelper::LogError(NULL, m_pimplDatabase->GetSessionHandle(), jetIdx.err, [this, &jetIdx] ()
						{
							std::ostringstream oss;
							oss << "Failed to create index \'" << jetIdx.szIndexName 
								<< "\' in table \'" << m_name 
								<< "\' of ISAM database";
							return oss.str();
						}, core::Logger::PRIO_ERROR);
					}
				}

				// Throws an exception for the error:
				ErrorHelper::HandleError(NULL, m_pimplDatabase->GetSessionHandle(), rcode, [this] ()
				{
					std::ostringstream oss;
					oss << "Failed to create indexes in table \'" << m_name << "\' from ISAM database";
					return oss.str();
				});
			}
		}
		catch(core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch(std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when creating indexes in table from ISAM database: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Deletes an index from the table.
	/// </summary>
	/// <param name="name">The index name.</param>
	void Table::DeleteIndex(const string &name)
	{
		CALL_STACK_TRACE;

		try
		{
			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
			const auto ucs2indexName = transcoder.from_bytes(name);

			auto rcode = JetDeleteIndexW(m_pimplDatabase->GetSessionHandle(), m_jetTable, ucs2indexName.c_str());

			ErrorHelper::HandleError(NULL, m_pimplDatabase->GetSessionHandle(), rcode, [this, &name] ()
			{
				std::ostringstream oss;
				oss << "Failed to delete index \'" << name 
					<< "\' from table \'" << m_name 
					<< "\' in ISAM database";

				return oss.str();
			});

			auto iter = m_idxCodesByName.find(name);

			// Delete index metadata:
			if(m_idxCodesByName.end() != iter)
			{
				m_idxsMetadataByCode.erase(iter->second);
				m_idxCodesByName.erase(iter);
			}
		}
		catch(core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch(std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when deleting indexes from table in ISAM database: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Maps a numeric code to an index name.
	/// Intended to work as an optimization to accelerate the retrieving of a table index.
	/// </summary>
	/// <param name="code">The numeric code.</param>
	/// <param name="idxName">Name of the index.</param>
	void Table::MapInt2IdxName(int code, const string &idxName)
	{
		CALL_STACK_TRACE;

		try
		{
			_ASSERTE(m_idxsMetadataByCode.find(code) == m_idxsMetadataByCode.end()); // Cannot map column to already used integer code

			std::wstring_convert<std::codecvt_utf8<wchar_t>> transcoder;
			const auto ucs2idxName = transcoder.from_bytes(idxName);

			auto indexHint = std::unique_ptr<JET_INDEXID> (new JET_INDEXID);

			auto rcode = JetGetTableIndexInfoW(m_pimplDatabase->GetSessionHandle(), 
                                               m_jetTable, 
                                               ucs2idxName.c_str(), 
                                               indexHint.get(), sizeof *indexHint, 
                                               JET_IdxInfoIndexId);

			ErrorHelper::HandleError(NULL, m_pimplDatabase->GetSessionHandle(), rcode, [this, &idxName] ()
			{
				std::ostringstream oss;
				oss << "Failed to get information from index \'" << idxName 
					<< "\' in table \'" << m_name 
					<< "\' of ISAM database";

				return oss.str();
			});

			// Store index metadata:
			m_idxsMetadataByCode.emplace(
				std::make_pair(code, IndexMetadata(idxName, std::move(indexHint)))
			);

			m_idxCodesByName[idxName] = code;
		}
		catch(core::IAppException &)
		{
			throw; // just forward exceptions regarding errors known to have been previously handled
		}
		catch(std::exception &ex)
		{
			std::ostringstream oss;
			oss << "Generic failure when mapping integer code to index information from table in ISAM database: " << ex.what();
			throw core::AppException<std::runtime_error>(oss.str());
		}
	}

	/// <summary>
	/// Gets metadata about an index.
	/// </summary>
	/// <param name="columnCode">The index numeric code.</param>
	/// <returns>A structure containing metadata regarding the specified column.</returns>
	const Table::IndexMetadata & Table::GetIndexMetadata(int indexCode) const
	{
		auto iter = m_idxsMetadataByCode.find(indexCode);
		_ASSERTE(m_idxsMetadataByCode.end() != iter); // Cannot use code which has not yet been mapped to a column name
		return iter->second;
	}

}// end namespace isam
}// end namespace _3fd
