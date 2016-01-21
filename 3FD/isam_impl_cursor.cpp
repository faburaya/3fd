#include "stdafx.h"
#include "isam_impl.h"
#include <codecvt>
#include <cassert>

namespace _3fd
{
	namespace isam
	{
		///////////////////////////
		// TableCursor Class
		///////////////////////////

		/// <summary>
		/// Finalizes an instance of the <see cref="TableCursorImpl"/> class.
		/// </summary>
		TableCursorImpl::~TableCursorImpl()
		{
			if(m_jetTable != NULL)
			{
				auto rcode = JetCloseTable(m_jetSession, m_jetTable);

				ErrorHelper::LogError(NULL, m_jetSession, rcode, [this] ()
				{
					std::ostringstream oss;
					oss << "Failed to close cursor for table \'" << m_table.GetName() << "\' in ISAM database";
					return oss.str();
				}, core::Logger::PRIO_ERROR);
			}
		}
		
		/// <summary>
		/// Finalizes an instance of the <see cref="TableCursor"/> class.
		/// </summary>
		TableCursor::~TableCursor()
		{
			delete m_pimplTableCursor;
		}

		/// <summary>
		/// Sets for the current index to search with the table cursor.
		/// </summary>
		/// <param name="idxCode">The index numeric code.</param>
		void TableCursorImpl::SetCurrentIndex(int idxCode)
		{
			CALL_STACK_TRACE;

			try
			{
				const auto &idxMetadata = m_table.GetIndexMetadata(idxCode);

				auto rcode = JetSetCurrentIndex4W(m_jetSession, m_jetTable, 
												  nullptr, idxMetadata.idHint.get(), 
												  JET_bitMoveFirst, 0);

				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [this, &idxMetadata] ()
				{
					std::ostringstream oss;
					oss << "Failed to set \'" << idxMetadata.name 
						<< "\' as current index for table \'" << m_table.GetName() 
						<< "\' of ISAM database.";
					return oss.str();
				});

				m_curIdxName = idxMetadata.name;
			}
			catch(core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch(std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when setting current index for cursor in ISAM table: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}

		/// <summary>
		/// Makes a key to be searched in the current set index.
		/// </summary>
		/// <param name="colKeyVals">The steps to make a key for search. Each value corresponds to
		/// a key column in the index and is subject to truncation if bigger than it is supposed to.</param>
		/// <param name="typeMatch">The type of index match to apply.</param>
		/// <param name="comparisonOp">The comparison operator.</param>
		void TableCursorImpl::MakeKey(std::vector<GenericInputParam> &colKeyVals, 
									  TableCursor::IndexKeyMatch typeMatch, 
									  TableCursor::ComparisonOperator comparisonOp)
		{
			CALL_STACK_TRACE;

			for(size_t idx = 0; idx < colKeyVals.size(); ++idx)
			{
				auto &value = colKeyVals[idx];

				JET_GRBIT grbit(0);

				if(idx == 0)
					grbit |= JET_bitNewKey;
				
				if(idx == colKeyVals.size() - 1
				   && typeMatch != TableCursor::IndexKeyMatch::Regular)
				{
					// Wildcard match is not compatible with equality operator:
					_ASSERTE(comparisonOp != TableCursor::ComparisonOperator::EqualTo);
					
					// Choose the type of wildcard key according to the specified comparison operator and type of match:
					switch (comparisonOp)
					{
					case TableCursor::ComparisonOperator::GreaterThanOrEqualTo:
					case TableCursor::ComparisonOperator::LessThan:
						grbit |= 
							(typeMatch == TableCursor::IndexKeyMatch::PrefixWildcard) 
							? JET_bitPartialColumnStartLimit 
							: JET_bitFullColumnStartLimit;
						break;

					case TableCursor::ComparisonOperator::GreaterThan:
					case TableCursor::ComparisonOperator::LessThanOrEqualTo:
						grbit |= 
							(typeMatch == TableCursor::IndexKeyMatch::PrefixWildcard) 
							? JET_bitPartialColumnEndLimit 
							: JET_bitFullColumnEndLimit;
						break;

					default:
						break;
					}
				}

				if(value.qtBytes == 0 && value.data != nullptr)
					grbit |= JET_bitKeyDataZeroLength;

				auto rcode = JetMakeKey(m_jetSession, m_jetTable, 
										value.data, value.qtBytes, 
										grbit);

				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [this] ()
				{
					std::ostringstream oss;
					oss << "Failed to make key for index \'" << m_curIdxName 
						<< "\' in table \'" << m_table.GetName() 
						<< "\' of ISAM database";
					return oss.str();
				});
			}
		}

		/// <summary>
		/// Makes a key to be searched in the current set index.
		/// </summary>
		/// <param name="colKeyVals">The steps to make a key for search. Each value corresponds to
		/// a key column in the index and is subject to truncation if bigger than it is supposed to.</param>
		/// <param name="typeMatch">The type of index match to apply.</param>
		/// <param name="upperLimit">Whether to choose the upper limit of the key matches.</param>
		void TableCursorImpl::MakeKey(std::vector<GenericInputParam> &colKeyVals, 
									  TableCursor::IndexKeyMatch typeMatch, 
									  bool upperLimit)
		{
			CALL_STACK_TRACE;

			for(size_t idx = 0; idx < colKeyVals.size(); ++idx)
			{
				auto &value = colKeyVals[idx];

				JET_GRBIT grbit(0);

				if(idx == 0)
					grbit |= JET_bitNewKey;
				
				if(idx == colKeyVals.size() - 1
				   && typeMatch != TableCursor::IndexKeyMatch::Regular)
				{
					// Choose the type of wildcard key according to the specified type of match:
					switch (typeMatch)
					{
					case TableCursor::IndexKeyMatch::Wildcard:
						grbit |= upperLimit ? JET_bitFullColumnEndLimit : JET_bitFullColumnStartLimit;
						break;

					case TableCursor::IndexKeyMatch::PrefixWildcard:
						grbit |= upperLimit ? JET_bitPartialColumnEndLimit : JET_bitPartialColumnStartLimit;
						break;

					default:
						break;
					}
				}

				if(value.qtBytes == 0 && value.data != nullptr)
					grbit |= JET_bitKeyDataZeroLength;

				auto rcode = JetMakeKey(m_jetSession, m_jetTable, 
										value.data, value.qtBytes, 
										grbit);

				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [this] ()
				{
					std::ostringstream oss;
					oss << "Failed to make key for index \'" << m_curIdxName 
						<< "\' in table \'" << m_table.GetName() 
						<< "\' of ISAM database";
					return oss.str();
				});
			}
		}
		
		/// <summary>
		/// Seeks the current index for an entry that satifies the condition imposed by
		/// a comparison operator and the current made key.
		/// ON SUCCESS:
		/// If a record has been prepared for update, then that update will be canceled.
		/// If an index range is in effect, that index range will be canceled.
		/// If a search key has been constructed for the cursor, then that search key will be deleted.
		/// When multiple index entries have the same value, the entry closest to the start of the index is always selected.
		/// ON FAILURE:
		/// There are no guarantees the position of the cursor will remain unchanged or in a valid position.
		/// If a record has been prepared for update, then that update will be canceled.
		/// If an index range is in effect, that index range will be canceled.
		/// If a search key has been constructed for the cursor, then that search key will be deleted.
		/// </summary>
		/// <param name="comparisonOp">The comparison operator.</param>
		/// <returns>
		/// 'STATUS_OKAY' if an exact match was found, otherwise, 'STATUS_FAIL'.
		/// </returns>
		bool TableCursorImpl::Seek(TableCursor::ComparisonOperator comparisonOp)
		{
			CALL_STACK_TRACE;

			auto rcode = JetSeek(m_jetSession, m_jetTable, static_cast<JET_GRBIT> (comparisonOp));

			if(rcode != JET_errRecordNotFound)
			{
				if (rcode != JET_wrnSeekNotEqual)
				{
					ErrorHelper::HandleError(NULL, m_jetSession, rcode, [this]()
					{
						std::ostringstream oss;
						oss << "Failed to seek cursor in index \'" << m_curIdxName
							<< "\' of table \'" << m_table.GetName()
							<< "\' from ISAM database";
						return oss.str();
					});
				}

				return STATUS_OKAY;
			}
			else
				return STATUS_FAIL;
		}

		/// <summary>
		/// Temporarily limits the set of index entries that the cursor can walk to those starting from the current index entry 
		/// and ending at the index entry that matches the search criteria specified by the search key in that cursor and the 
		/// specified bound criteria (whether inclusive or in the upper limit). 
		/// A search key must have been previously constructed.
		/// </summary>
		/// <param name="upperLimit">A bound criteria. Whether in the upper limit or not.</param>
		/// <param name="inclusive">A bound criteria. Whether inclusive or not.</param>
		/// <returns>
		/// 'STATUS_OKAY' if an exact match was found, otherwise, 'STATUS_FAIL'.
		/// </returns>
		bool TableCursorImpl::SetIndexRange(bool upperLimit, bool inclusive)
		{
			CALL_STACK_TRACE;

			JET_GRBIT flags(0);

			if(inclusive)
				flags |= JET_bitRangeInclusive;

			if(upperLimit)
				flags |= JET_bitRangeUpperLimit;

			auto rcode = JetSetIndexRange(m_jetSession, m_jetTable, flags);

			if(rcode != JET_errNoCurrentRecord)
			{
				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [this] ()
				{
					std::ostringstream oss;
					oss << "Failed to set cursor range in index \'" << m_curIdxName 
						<< "\' of table \'" << m_table.GetName() 
						<< "\' from ISAM database";
					return oss.str();
				});

				return STATUS_OKAY;
			}
			else
				return STATUS_FAIL;
		}
		
		/// <summary>
		/// Moves the cursor one position ahead or behind.
		/// </summary>
		/// <param name="option">How to move the cursor.</param>
		/// <returns>
		/// 'STATUS_OKAY' if there was a record to move on, otherwise, 'STATUS_FAIL'.
		/// </returns>
		bool TableCursorImpl::MoveCursor(MoveOption option)
		{
			CALL_STACK_TRACE;

			auto rcode = JetMove(m_jetSession, m_jetTable, static_cast<long> (option), 0);

			if(rcode != JET_errNoCurrentRecord) // If there was a record to move on, check if it went okay:
			{
				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [this, option] ()
				{
					std::ostringstream oss;
					oss << "Failed to move cursor ";

					switch (option)
					{
					case MoveOption::First:
						oss << "to the first position";
						break;
					case MoveOption::Previous:
						oss << "backward";
						break;
					case MoveOption::Next:
						oss << "forward";
						break;
					case MoveOption::Last:
						oss << "to the last position";
						break;
					default:
						break;
					}

					oss << " in index \'" << m_curIdxName 
						<< "\' of table \'" << m_table.GetName() 
						<< "\' from ISAM database";

					return oss.str();
				});

				return STATUS_OKAY;
			}
			else
				return STATUS_FAIL;
		}

		/// <summary>
		/// Scans the table beginning in the match found for the provided key and going forward/backward until the last/first record.
		/// </summary>
		/// <param name="idxCode">The index numeric code.</param>
		/// <param name="comparisonOp">The comparison operator to use to match the provided key.</param>
		/// <param name="colKeyVals">The key. A set of values to search in the columns covered by the given index.</param>
		/// <param name="typeMatch">The type of match to apply.</param>
		/// <param name="callback">The callback to invoke for every record the cursor visits.
		/// It must return 'true' to continue going forward, or 'false' to stop iterating over the records.</param>
		/// <param name="backward">Whether the iteration should proceed backwards.</param>
		/// <returns>How many records the callback was invoked on. Zero means it could not find a match for the provided key.</returns>
		size_t TableCursorImpl::ScanFrom(int idxCode, 
										 std::vector<GenericInputParam> &colKeyVals, 
										 TableCursor::IndexKeyMatch typeMatch, 
										 TableCursor::ComparisonOperator comparisonOp, 
										 const std::function<bool (RecordReader &)> &callback, 
										 bool backward)
		{
			CALL_STACK_TRACE;

			size_t count(0);
			SetCurrentIndex(idxCode);
			MakeKey(colKeyVals, typeMatch, comparisonOp);

			if(Seek(comparisonOp) == STATUS_OKAY)
			{
				const auto step = backward ? MoveOption::Previous : MoveOption::Next;

				RecordReader recReader(this);

				// Iterate over the records invoking the callback for each one of them:
				while(callback(recReader))
				{
					if(MoveCursor(step) == STATUS_OKAY)
						++count;
					else
						return ++count;
				}
			}
			
			return count;
		}
		
		size_t TableCursor::ScanFrom(int idxCode, 
									 std::vector<GenericInputParam> &colKeyVals, 
									 IndexKeyMatch typeMatch, 
									 ComparisonOperator comparisonOp, 
									 const std::function<bool (RecordReader &)> &callback, 
									 bool backward)
		{
			return m_pimplTableCursor->ScanFrom(idxCode, colKeyVals, typeMatch, comparisonOp, callback, backward);
		}

		/// <summary>
		/// Scans the table over the range established by the provided keys.
		/// </summary>
		/// <param name="idxCode">The index numeric code.</param>
		/// <param name="colKeyVals1">The col key vals1.</param>
		/// <param name="typeMatch1">The type of match to apply in the first key.</param>
		/// <param name="comparisonOp1">The comparison operator to use to match the first key.</param>
		/// <param name="colKeyVals2">The second key. A set of values to search in the columns covered by the given index. 
		/// For this key the comparison operator has to be an equality.
		/// It must be closer to the end of the index than the first key is.</param>
		/// <param name="typeMatch2">The type of match to apply in the second key.</param>
		/// <param name="upperLimit2">Whether the match of the second key should.</param>
		/// <param name="inclusive2">Whether the established range must include the record pointed by the second key.</param>
		/// <param name="callback">The callback to invoke for every record the cursor visits.
		/// It must return 'true' to continue going forward, or 'false' to stop iterating over the records.</param>
		/// <returns>
		/// How many records the callback was invoked on. Zero means it could not match both keys to set a range.
		/// </returns>
		size_t TableCursorImpl::ScanRange(int idxCode, 
										  std::vector<GenericInputParam> &colKeyVals1, 
										  TableCursor::IndexKeyMatch typeMatch1, 
										  TableCursor::ComparisonOperator comparisonOp1, 
										  std::vector<GenericInputParam> &colKeyVals2, 
										  TableCursor::IndexKeyMatch typeMatch2, 
										  bool upperLimit2, 
										  bool inclusive2, 
										  const std::function<bool (RecordReader &)> &callback)
		{
			CALL_STACK_TRACE;

			SetCurrentIndex(idxCode);
			MakeKey(colKeyVals1, typeMatch1, comparisonOp1);

			if(Seek(comparisonOp1) == STATUS_OKAY)
				MakeKey(colKeyVals2, typeMatch2, upperLimit2);
			else
				return 0;

			size_t count(0);

			// Set an index range ending with the second key:
			if(SetIndexRange(upperLimit2, inclusive2) == STATUS_OKAY)
			{
				RecordReader recReader(this);

				// Iterate over the records invoking the callback for each one of them:
				while(callback(recReader))
				{
					if(MoveCursor(MoveOption::Next) == STATUS_OKAY)
						++count;
					else
						return ++count;
				}
			}
			
			return count;
		}
		
		size_t TableCursor::ScanRange(int idxCode, 
									  std::vector<GenericInputParam> &colKeyVals1, 
									  IndexKeyMatch typeMatch1, 
									  ComparisonOperator comparisonOp1, 
									  std::vector<GenericInputParam> &colKeyVals2, 
									  IndexKeyMatch typeMatch2, 
									  bool upperLimit2, 
									  bool inclusive2, 
									  const std::function<bool (RecordReader &)> &callback)
		{
			return m_pimplTableCursor->ScanRange(idxCode, 
												 colKeyVals1, typeMatch1, comparisonOp1, 
												 colKeyVals2, typeMatch2, upperLimit2, inclusive2, 
												 callback);
		}

		/// <summary>
		/// Scans all the records in the table.
		/// </summary>
		/// <param name="idxCode">The index numeric code.</param>
		/// <param name="callback">The callback to invoke for every record the cursor visits.
		/// It must return 'true' to continue going forward, or 'false' to stop iterating over the records.</param>
		/// <param name="backward">Whether the iteration should proceed backwards.</param>
		/// <returns>
		/// How many records the callback was invoked on. Zero means it could not match both keys to set a range.
		/// </returns>
		size_t TableCursorImpl::ScanAll(int idxCode, 
										const std::function<bool (RecordReader &)> &callback, 
										bool backward)
		{
			CALL_STACK_TRACE;

			size_t count(0);
			SetCurrentIndex(idxCode);

			if(MoveCursor(backward ? MoveOption::Last : MoveOption::First) == STATUS_OKAY)
			{
				/* Notify the database engine that the application is scanning the 
				entire current index, so it can optimize the calls accordingly: */
				auto rcode = JetSetTableSequential(m_jetSession, m_jetTable, 
												   backward ? JET_bitPrereadBackward : JET_bitPrereadForward);

				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [this] ()
				{
					std::ostringstream oss;
					oss << "Failed to optimize for thorough scan in index \'" << m_curIdxName 
						<< "\' of table \'" << m_table.GetName() 
						<< "\' from ISAM database";
					return oss.str();
				});

				const auto step = backward ? MoveOption::Previous : MoveOption::Next;

				RecordReader recReader(this);

				// Iterate over the records invoking the callback for each one of them:
				while(callback(recReader))
				{
					if(MoveCursor(step) == STATUS_OKAY)
						++count;
					else
						return ++count;
				}

				rcode = JetResetTableSequential(m_jetSession, m_jetTable, 0);

				ErrorHelper::HandleError(NULL, m_jetSession, rcode, [this] ()
				{
					std::ostringstream oss;
					oss << "Failed to turn off thorough scan optimization in index \'" << m_curIdxName 
						<< "\' of table \'" << m_table.GetName() 
						<< "\' from ISAM database";
					return oss.str();
				});
			}
			
			return count;
		}

		size_t TableCursor::ScanAll(int idxCode, 
									const std::function<bool (RecordReader &)> &callback, 
									bool backward)
		{
			return m_pimplTableCursor->ScanAll(idxCode, callback, backward);
		}

		/// <summary>
		/// Starts an update process in the current scope.
		/// </summary>
		/// <param name="mode">The update mode.</param>
		/// <returns>An <see cref="TableWriterImpl" /> object to control the process using RAII.</returns>
		TableWriterImpl * TableCursorImpl::StartUpdate(TableWriter::Mode mode)
		{
			CALL_STACK_TRACE;

			try
			{
				return new TableWriterImpl(*this, mode);
			}
			catch(core::IAppException &)
			{
				throw; // just forward exceptions regarding errors known to have been previously handled
			}
			catch(std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when creating a table writer in ISAM database: " << ex.what();
				throw core::AppException<std::runtime_error>(oss.str());
			}
		}
		
		TableWriter TableCursor::StartUpdate(TableWriter::Mode mode)
		{
			return TableWriter(m_pimplTableCursor->StartUpdate(mode));
		}

		/// <summary>
		/// Deletes the current record where the cursor is at.
		/// </summary>
		void TableCursorImpl::DeleteCurrentRecord()
		{
			CALL_STACK_TRACE;

			auto rcode = JetDelete(m_jetSession, m_jetTable);

			ErrorHelper::HandleError(NULL, m_jetSession, rcode, [this] ()
			{
				std::ostringstream oss;
				oss << "Failed to delete record from table \'" << m_table.GetName() << "\' in ISAM database";
				return oss.str();
			});
		}
		
		void TableCursor::DeleteCurrentRecord()
		{
			m_pimplTableCursor->DeleteCurrentRecord();
		}
		
	}// end namespace isam
}// end namespace _3fd
