#include "pch.h"
#include "isam_impl.h"
#include <array>
#include <cassert>
#include <codecvt>
#include <memory>

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
    void TableCursorImpl::MakeKey(const std::vector<GenericInputParam> &colKeyVals, 
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
                    
                /* Choose the type of wildcard key according to the
                specified comparison operator and type of match: */
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

            /* If this is the case, set a flat to indicate that
            the value has lenght ZERO, but it is not null: */
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
    void TableCursorImpl::MakeKey(const std::vector<GenericInputParam> &colKeyVals, 
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

            /* If this is the case, set a flat to indicate that
            the value has lenght ZERO, but it is not null: */
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
    /// <param name="idxCode">The numeric code that identifies an index, as set by <see cref="ITable::MapInt2IdxName"/>.</param>
    /// <param name="colKeyVals">The key. A set of values to search in the columns covered by the given index.</param>
    /// <param name="typeMatch">The type of match to apply.</param>
    /// <param name="comparisonOp">The comparison operator to use to match the provided key.</param>
    /// <param name="callback">The callback to invoke for every record the cursor visits.
    /// It must return 'true' to continue going forward, or 'false' to stop iterating over the records.</param>
    /// <param name="backward">Whether the iteration should proceed backwards.</param>
    /// <returns>How many records the callback was invoked on. Zero means it could not find a match for the provided key.</returns>
    size_t TableCursorImpl::ScanFrom(int idxCode, 
                                     const std::vector<GenericInputParam> &colKeyVals, 
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
                                 const std::vector<GenericInputParam> &colKeyVals, 
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
    /// <param name="idxRangeDef">The index range definition.</param>
    /// <param name="callback">The callback to invoke for every record the cursor visits.
    /// It must return 'true' to continue going forward, or 'false' to stop iterating over the records.</param>
    /// <returns>
    /// How many records the callback was invoked on. Zero means it could not match both keys to set a range.
    /// </returns>
    size_t TableCursorImpl::ScanRange(const TableCursor::IndexRangeDefinition &idxRangeDef,
                                        const std::function<bool (RecordReader &)> &callback)
    {
        CALL_STACK_TRACE;

        SetCurrentIndex(idxRangeDef.indexCode);

        MakeKey(idxRangeDef.beginKey.colsVals,
                idxRangeDef.beginKey.typeMatch,
                idxRangeDef.beginKey.comparisonOper);

        if (Seek(idxRangeDef.beginKey.comparisonOper) == STATUS_OKAY)
        {
            MakeKey(idxRangeDef.endKey.colsVals,
                    idxRangeDef.endKey.typeMatch,
                    idxRangeDef.endKey.isUpperLimit);
        }
        else
            return 0;

        size_t count(0);

        // Set an index range ending with the second key:
        if (SetIndexRange(idxRangeDef.endKey.isUpperLimit, idxRangeDef.endKey.isInclusive) == STATUS_OKAY)
        {
            RecordReader recReader(this);

            auto direction =
                idxRangeDef.endKey.isUpperLimit ? MoveOption::Next : MoveOption::Previous;

            // Iterate over the records invoking the callback for each one of them:
            while(callback(recReader))
            {
                if (MoveCursor(direction) == STATUS_OKAY)
                    ++count;
                else
                    return ++count;
            }
        }
            
        return count;
    }
        
    size_t TableCursor::ScanRange(const IndexRangeDefinition &idxRangeDef,
                                    const std::function<bool (RecordReader &)> &callback)
    {
        return m_pimplTableCursor->ScanRange(idxRangeDef, callback);
    }

    /// <summary>
    /// Uses RAII to control resources for a temporary table.
    /// </summary>
    struct ScopeTempTable
    {
    private:

        JET_SESID jetSessionHandle;
        JET_TABLEID jetTableHandle;

    public:

        ScopeTempTable(JET_SESID p_jetSessionHandle, JET_TABLEID &p_jetTableHandle)
            : jetSessionHandle(p_jetSessionHandle), jetTableHandle(p_jetTableHandle) {}

        ~ScopeTempTable()
        {
            CALL_STACK_TRACE;
            auto rcode = JetCloseTable(jetSessionHandle, jetTableHandle);
            ErrorHelper::LogError(NULL, jetSessionHandle, rcode,
                "Failed to close temporary table", core::Logger::PRIO_ERROR);
        }
    };

    /// <summary>
    /// Scans the intersection of several index ranges in this table.
    /// </summary>
    /// <param name="rangeDefs">The definitions for the index ranges to intersect. All ranges
    /// must be of distinct SECONDARY indexes from the same table of this cursor, otherwise an
    /// error is issued. ALSO, all ranges must go in the same direction, which is from closer
    /// to the index start towards closer to the index end, otherwise the results would not make
    /// sense (but an error is issued only in debug mode as an assertion).</param>
    /// <param name="callback">The callback to invoke for every record the cursor visits.
    /// It must return 'true' to continue going forward, or 'false' to stop iterating over the records.</param>
    /// <returns>
    /// How many records the callback was invoked on.
    /// Zero means there was no intersection, or that one or more ranges were empty.
    /// </returns>
    size_t TableCursorImpl::ScanIntersection(const std::vector<TableCursor::IndexRangeDefinition> &rangeDefs,
                                                const std::function<bool(RecordReader &)> &callback)
    {
        CALL_STACK_TRACE;

        _ASSERTE(rangeDefs.size() > 1); // can only intersect if more than one index range is provided

        try
        {
            /* This cursor must be prepared to visit the records in the intersection
            by setting the clustered index as the current one: */
            auto rcode = JetSetCurrentIndex4W(m_jetSession, m_jetTable,
                                                nullptr, nullptr,
                                                JET_bitMoveFirst, 0);

            ErrorHelper::HandleError(NULL, m_jetSession, rcode,
                "Failed to set clustered index as current");

            std::vector<std::unique_ptr<TableCursorImpl>> cursors;
            cursors.reserve(rangeDefs.size());

            std::vector<JET_INDEXRANGE> idxRanges;
            idxRanges.reserve(rangeDefs.size());

            // Create an index range for each definition:
            for (auto &rangeDef : rangeDefs)
            {
                /* Although documentation for ESE does not say that, tests confirm that in order to
                intersection results be correct, all the index ranges must have the same direction,
                which is from closer to the index start to closer to the index end. Thus it is wrong
                use of the API to intersect an index range whose end-key is not the upper limit. */
                _ASSERTE(rangeDef.endKey.isUpperLimit);

                JET_INDEXRANGE idxRange { sizeof(JET_INDEXRANGE), 0, JET_bitRecordInIndex };

#ifndef _3FD_PLATFORM_WINRT
                // Duplicate cursor:
                rcode = JetDupCursor(m_jetSession, m_jetTable, &idxRange.tableid, 0);
                ErrorHelper::HandleError(NULL, m_jetSession, rcode, "Failed to duplicate cursor");

                std::unique_ptr<TableCursorImpl> dupCursor(
                    dbg_new TableCursorImpl(m_table, idxRange.tableid, m_jetSession)
                );
#else
                std::unique_ptr<TableCursorImpl> dupCursor(
                    m_table.GetDatabase()->GetCursorFor(m_table, false)
                );

                idxRange.tableid = dupCursor->GetCursorHandle();
#endif
                // With the new cursor, set the index range according the given definition:

                dupCursor->SetCurrentIndex(rangeDef.indexCode);

                dupCursor->MakeKey(rangeDef.beginKey.colsVals,
                                   rangeDef.beginKey.typeMatch,
                                   rangeDef.beginKey.comparisonOper);

                if (dupCursor->Seek(rangeDef.beginKey.comparisonOper) == STATUS_OKAY)
                {
                    dupCursor->MakeKey(rangeDef.endKey.colsVals,
                                       rangeDef.endKey.typeMatch,
                                       rangeDef.endKey.isUpperLimit);
                }
                else // no range to set because the key had no match:
                    return 0;

                // If the range can be set with this cursor, keep it:
                if (dupCursor->SetIndexRange(rangeDef.endKey.isUpperLimit,
                                                rangeDef.endKey.isInclusive) == STATUS_OKAY)
                {
                    cursors.push_back(std::move(dupCursor));
                    idxRanges.push_back(idxRange);
                }
                else
                    return 0;
            }

            // Intersect the index ranges:
            JET_RECORDLIST intersectionRowset{ sizeof(JET_RECORDLIST), NULL, 0, NULL };

            rcode = JetIntersectIndexes(m_jetSession,
                                        idxRanges.data(),
                                        idxRanges.size(),
                                        &intersectionRowset,
                                        0);

            ErrorHelper::HandleError(NULL, m_jetSession, rcode,
                "Failed to perform intersection of indexes");

            /* This object destructor guarantees proper release of
            resources of the temporary table in all scenarios of exit */
            ScopeTempTable tempTable(m_jetSession, intersectionRowset.tableid);

            // If the intersection is empty:
            if (intersectionRowset.cRecord == 0)
                return 0;

            // Release some resources no longer needed:
            idxRanges.clear();
            cursors.clear();

            /* Before scanning the temporary table, the cursor must be moved. This
            signals the end of the insertion phase, making read operations allowed: */
            rcode = JetMove(m_jetSession,
                            intersectionRowset.tableid,
                            static_cast<long> (MoveOption::First),
                            0);

            if (rcode != JET_errNoCurrentRecord)
            {
                ErrorHelper::HandleError(NULL, m_jetSession, rcode,
                    "Failed to move cursor in temporary table");
            }
            else
                return 0;

            size_t count(0);
            std::array<char, JET_cbBookmarkMost> bookmarkBuffer;

            // The result is a temporary table. Scan the records in the rowset:
            for (unsigned long idx = 0; idx < intersectionRowset.cRecord; ++idx)
            {
                unsigned long qtBytes;

                // There is a single column in the table. Retrieve the value:
                rcode = JetRetrieveColumn(m_jetSession,
                                          intersectionRowset.tableid,
                                          intersectionRowset.columnidBookmark,
                                          bookmarkBuffer.data(),
                                          bookmarkBuffer.size(),
                                          &qtBytes,
                                          0, nullptr);

                ErrorHelper::HandleError(NULL, m_jetSession, rcode,
                    "Failed to retrieve column value from temporary table");

                // The value is a bookmark. Use this bookmark to position this cursor:
                rcode = JetGotoBookmark(m_jetSession,
                                        m_jetTable,
                                        bookmarkBuffer.data(),
                                        qtBytes);

                ErrorHelper::HandleError(NULL, m_jetSession, rcode,
                    "Failed to use bookmark to move the cursor to a record");

                // Read the record:
                RecordReader recReader(this);

                if (callback(recReader))
                    ++count;
                else
                    return ++count;

                // Move to next record of intersection:
                rcode = JetMove(m_jetSession,
                                intersectionRowset.tableid,
                                static_cast<long> (MoveOption::Next),
                                0);

                /* If not expected to be the last record, the cursor move should be succesfull. In the
                last record, the only error allowed is the one signaling no more records available. */
                if (idx + 1 != intersectionRowset.cRecord
                    || rcode != JET_errNoCurrentRecord)
                {
                    ErrorHelper::HandleError(NULL, m_jetSession, rcode,
                        "Failed to move cursor forward in temporary table");
                }
            }// end of loop

            return count;
        }
        catch (core::IAppException &ex)
        {
            std::ostringstream oss;
            oss << "Failed to intersect indexes of table \'" << m_table.GetName()
                << "\' from ISAM database";

            throw core::AppException<std::runtime_error>(oss.str(), ex);
        }
        catch (std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when intersecting indexes of table \'" << m_table.GetName()
                << "\' from ISAM database:" << ex.what();

            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    size_t TableCursor::ScanIntersection(const std::vector<IndexRangeDefinition> &rangeDefs,
                                            const std::function<bool(RecordReader &)> &callback)
    {
        return m_pimplTableCursor->ScanIntersection(rangeDefs, callback);
    }

    /// <summary>
    /// Scans all the records in the table.
    /// </summary>
    /// <param name="idxCode">The numeric code that identifies an index, as set by <see cref="ITable::MapInt2IdxName"/>.</param>
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
            return dbg_new TableWriterImpl(*this, mode);
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
