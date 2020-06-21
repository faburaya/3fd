#include "pch.h"
#include "isam_impl.h"
#include <3fd/utils/algorithms.h>
#include <cassert>
#include <codecvt>
#include <cstdlib>
#include <ctime>
#include <thread>

#undef min

namespace _3fd
{
namespace isam
{
    ///////////////////////////
    // TableWriter Class
    ///////////////////////////

    /// <summary>
    /// Initializes a new instance of the <see cref="TableWriterImpl"/> class.
    /// </summary>
    /// <param name="mode">The type of update.</param>
    TableWriterImpl::TableWriterImpl(TableCursorImpl &pimplTableCursor, TableWriter::Mode mode)
        : m_pimplTableCursor(pimplTableCursor)
        , m_saved(false)
    {
        CALL_STACK_TRACE;

        auto rcode = JetPrepareUpdate(m_pimplTableCursor.GetSessionHandle(),
                                      m_pimplTableCursor.GetCursorHandle(),
                                      static_cast<unsigned long> (mode));

        ErrorHelper::HandleError(NULL,
								 m_pimplTableCursor.GetSessionHandle(),
								 rcode,
								 "Failed to prepare row update in ISAM database table");
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="TableWriterImpl"/> class.
    /// </summary>
    TableWriterImpl::~TableWriterImpl()
    {
        if(m_pimplTableCursor.GetCursorHandle() != NULL && m_saved == false)
        {
            CALL_STACK_TRACE;

            auto rcode = JetPrepareUpdate(m_pimplTableCursor.GetSessionHandle(), 
                                          m_pimplTableCursor.GetCursorHandle(), 
                                          JET_prepCancel);

            ErrorHelper::LogError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, 
                                  "Failed to cancel row update in ISAM database table", 
                                  core::Logger::PRIO_ERROR);
        }
    }

    /// <summary>
    /// Finalizes an instance of the <see cref="TableWriter"/> class.
    /// </summary>
    TableWriter::~TableWriter()
    {
        delete m_pimpl;
    }

    /// <summary>
    /// Saves the changes made in the object scope.
    /// </summary>
    void TableWriterImpl::Save()
    {
        CALL_STACK_TRACE;

        int attempts(1);
        JET_ERR rcode;

        while(
#ifndef _3FD_PLATFORM_WINRT
            (rcode = JetUpdate(m_pimplTableCursor.GetSessionHandle(),
                               m_pimplTableCursor.GetCursorHandle(),
                               nullptr, 0, nullptr)) != JET_errSuccess
#else
            (rcode = JetUpdate2(m_pimplTableCursor.GetSessionHandle(),
                                m_pimplTableCursor.GetCursorHandle(),
                                nullptr, 0, nullptr, 0)) != JET_errSuccess
#endif
        )
        {
            if(rcode == JET_errWriteConflict)
            {
                // Wait a little to acquire lock in the row:
                std::this_thread::sleep_for(
                    utils::CalcExponentialBackOff(attempts, std::chrono::milliseconds(5))
                );
            }
            else
            {
                ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, attempts] ()
                {
                    std::ostringstream oss;
                    oss << "Failed to save table update in ISAM database after "
                        << attempts << " attempt(s)";
                    return oss.str();
                });
                return;
            }

            ++attempts;
        }

        m_saved = true;
    }

    void TableWriter::Save()
    {
        m_pimpl->Save();
    }

    /// <summary>
    /// Sets the value of a (not large blob or text) column for update (or insertion).
    /// </summary>
    /// <param name="columnCode">The integer code which identifies the column.</param>
    /// <param name="value">The value to set into the column.</param>
    /// <param name="tagSequence">The tag sequence if it is a multi-value column.
    /// This is an index (base 1) that indicates which value must be overwritten.
    /// Use '0' (default) or an unexistent value if you wish to add a new one.</param>
    /// <param name="uniqueMV">Whether the column (if multi-valued) must forbid duplicates.</param>
    void TableWriterImpl::SetColumn(int columnCode, 
                                    const GenericInputParam &value, 
                                    unsigned long tagSequence, 
                                    bool mvUnique)
    {
        CALL_STACK_TRACE;

        auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
        _ASSERT(colMetadata.dataType == value.dataType); // Type specified in the argument must match the stored data type

        // Input value can only be null if the column is nullable:
        _ASSERTE(value.data != nullptr 
                 || (value.data == nullptr && colMetadata.notNull == false));

        // This function was not designed for large value columns:
        _ASSERTE(value.dataType != DataType::LargeBlob 
                 && value.dataType != DataType::LargeText);

#        ifndef NDEBUG
        // Check the data length:
        switch (value.dataType)
        {
        case DataType::Boolean:
        case DataType::UByte:
        case DataType::Int16:
        case DataType::Int32:
        case DataType::Int64:
        case DataType::UInt16:
        case DataType::UInt32:
        case DataType::GUID:
        case DataType::Float32:
        case DataType::Float64:
        case DataType::Currency:
        case DataType::DateTime:
            _ASSERTE(Table::GetMaxLength(value.dataType) == value.qtBytes
                    || (value.data == nullptr && value.qtBytes == 0));
            break;

        case DataType::Blob:
        case DataType::Text:
        default:
            break;
        }
#        endif

        JET_SETINFO setInfo;
        setInfo.cbStruct = sizeof setInfo;
        setInfo.ibLongValue = 0;
        setInfo.itagSequence = colMetadata.multiValued ? tagSequence : 1;

        JET_GRBIT grbit = (colMetadata.multiValued && mvUnique) ? JET_bitSetUniqueMultiValues : 0;

        if(value.qtBytes == 0 && value.data != nullptr)
            grbit |= JET_bitSetZeroLength;

        auto rcode = JetSetColumn(m_pimplTableCursor.GetSessionHandle(), 
                                  m_pimplTableCursor.GetCursorHandle(), 
                                  colMetadata.id, 
                                  value.data, value.qtBytes, 
                                  grbit, &setInfo);

        ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
        {
            std::ostringstream oss;
            oss << "Failed to set value of column \'" << colMetadata.name
                << "\' in table \'" << m_pimplTableCursor.GetSchema().GetName() 
                << "\' from ISAM database";
            return oss.str();
        });
    }

    void TableWriter::SetColumn(int columnCode, 
                                const GenericInputParam &value, 
                                unsigned long tagSequence, 
                                bool mvUnique)
    {
        m_pimpl->SetColumn(columnCode, value, tagSequence, mvUnique);
    }

    /// <summary>
    /// Sets the value for a large (blob or text) column.
    /// </summary>
    /// <param name="columnCode">The column code.</param>
    /// <param name="value">The value to set into the column.</param>
    /// <param name="compressed">Whether should attempt compression.</param>
    /// <param name="tagSequence">The tag sequence if it is a multi-value column.
    /// This is an index (base 1) that indicates which value must be overwritten.
    /// Use '0' (default) or an unexistent value if you wish to add a new one.</param>
    void TableWriterImpl::SetLargeColumn(int columnCode, 
                                            const GenericInputParam &value, 
                                            bool compressed, 
                                            unsigned long tagSequence)
    {
        CALL_STACK_TRACE;

        auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);

        /* Type specified in the argument must be blob or text and match the stored data type. 
        A blob and a large blob are interchangable, but truncation will take place to fit a 
        large blob into a blob. The same goes for text values. */
        _ASSERTE(
            ((colMetadata.dataType == DataType::Blob || colMetadata.dataType == DataType::LargeBlob) 
             && (value.dataType == DataType::Blob || value.dataType == DataType::LargeBlob)) 
            || ((colMetadata.dataType == DataType::Text || colMetadata.dataType == DataType::LargeText) 
                && (value.dataType == DataType::Text || value.dataType == DataType::LargeText))
        );

        JET_SETINFO setInfo;
        setInfo.cbStruct = sizeof setInfo;
        setInfo.ibLongValue = 0;
        setInfo.itagSequence = colMetadata.multiValued ? tagSequence : 1;

        JET_GRBIT grbit = compressed ? JET_bitSetCompressed : 0;

        if(value.qtBytes == 0 && value.data != nullptr)
            grbit |= JET_bitSetZeroLength;

        auto rcode = JetSetColumn(m_pimplTableCursor.GetSessionHandle(), 
                                  m_pimplTableCursor.GetCursorHandle(), 
                                  colMetadata.id, 
                                  value.data, value.qtBytes, 
                                  grbit, &setInfo);

        ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
        {
            std::ostringstream oss;
            oss << "Failed to set value of large column \'" << colMetadata.name
                << "\' in table \'" << m_pimplTableCursor.GetSchema().GetName() 
                << "\' from ISAM database";
            return oss.str();
        });
    }

    void TableWriter::SetLargeColumn(int columnCode, 
                                        const GenericInputParam &value, 
                                        bool compressed, 
                                        unsigned long tagSequence)
    {
        m_pimpl->SetLargeColumn(columnCode, value, compressed, tagSequence);
    }

    /// <summary>
    /// Sets the value for a large (blob or text) column overwriting the previously existent one.
    /// </summary>
    /// <param name="columnCode">The column code.</param>
    /// <param name="value">The value to set into the column.</param>
    /// <param name="offset">The offset where to start writing.</param>
    /// <param name="compressed">Whether should attempt compression.</param>
    /// <param name="tagSequence">The tag sequence if it is a multi-value column.
    /// This is an index (base 1) that indicates which value must be overwritten.
    /// Use '0' (default) or an unexistent value if you wish to add a new one.</param>
    void TableWriterImpl::SetLargeColumnOverwrite(int columnCode, 
                                                    const GenericInputParam &value, 
                                                    unsigned long offset, 
                                                    bool compressed, 
                                                    unsigned long tagSequence)
    {
        CALL_STACK_TRACE;

        auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
            
        /* Type specified in the argument must be blob or text and match the stored data type. 
        A blob and a large blob are interchangable, but truncation will take place to fit a 
        large blob into a blob. The same goes for text values. */
        _ASSERTE(
            ((colMetadata.dataType == DataType::Blob || colMetadata.dataType == DataType::LargeBlob) 
             && (value.dataType == DataType::Blob || value.dataType == DataType::LargeBlob)) 
            || ((colMetadata.dataType == DataType::Text || colMetadata.dataType == DataType::LargeText) 
                && (value.dataType == DataType::Text || value.dataType == DataType::LargeText))
        );

        JET_SETINFO setInfo;
        setInfo.cbStruct = sizeof setInfo;
        setInfo.ibLongValue = offset;
        setInfo.itagSequence = colMetadata.multiValued ? tagSequence : 1;

        JET_GRBIT grbit = JET_bitSetOverwriteLV;

        if(compressed)
            grbit |= JET_bitSetCompressed;

        if(value.qtBytes == 0 && value.data != nullptr)
            grbit |= JET_bitSetZeroLength;

        auto rcode = JetSetColumn(m_pimplTableCursor.GetSessionHandle(), 
                                  m_pimplTableCursor.GetCursorHandle(), 
                                  colMetadata.id, 
                                  value.data, value.qtBytes, 
                                  grbit, &setInfo);

        ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
        {
            std::ostringstream oss;
            oss << "Failed to overwrite value in large column \'" << colMetadata.name
                << "\' of table \'" << m_pimplTableCursor.GetSchema().GetName() 
                << "\' from ISAM database";
            return oss.str();
        });
    }

    void TableWriter::SetLargeColumnOverwrite(int columnCode, 
                                                const GenericInputParam &value, 
                                                unsigned long offset, 
                                                bool compressed, 
                                                unsigned long tagSequence)
    {
        m_pimpl->SetLargeColumnOverwrite(columnCode, value, offset, compressed, tagSequence);
    }

    /// <summary>
    /// Sets the value for a large (blob or text) column appending to the previous content.
    /// </summary>
    /// <param name="columnCode">The column code.</param>
    /// <param name="value">The value to set into the column.</param>
    /// <param name="compressed">Whether should attempt compression.</param>
    /// <param name="tagSequence">The tag sequence if it is a multi-value column.
    /// This is an index (base 1) that indicates which value must be appended.
    /// Use '0' (default) or an unexistent value if you wish to add a new one.</param>
    void TableWriterImpl::SetLargeColumnAppend(int columnCode, 
                                                const GenericInputParam &value, 
                                                bool compressed, 
                                                unsigned long tagSequence)
    {
        CALL_STACK_TRACE;

        auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
            
        /* Type specified in the argument must be blob or text and match the stored data type. 
        A blob and a large blob are interchangable, but truncation will take place to fit a 
        large blob into a blob. The same goes for text values. */
        _ASSERTE(
            ((colMetadata.dataType == DataType::Blob || colMetadata.dataType == DataType::LargeBlob) 
             && (value.dataType == DataType::Blob || value.dataType == DataType::LargeBlob)) 
            || ((colMetadata.dataType == DataType::Text || colMetadata.dataType == DataType::LargeText) 
                && (value.dataType == DataType::Text || value.dataType == DataType::LargeText))
        );

        JET_SETINFO setInfo;
        setInfo.cbStruct = sizeof setInfo;
        setInfo.ibLongValue = 0;
        setInfo.itagSequence = colMetadata.multiValued ? tagSequence : 1;

        JET_GRBIT grbit = JET_bitSetAppendLV;

        if(compressed)
            grbit |= JET_bitSetCompressed;

        if(value.qtBytes == 0 && value.data != nullptr)
            grbit |= JET_bitSetZeroLength;

        auto rcode = JetSetColumn(m_pimplTableCursor.GetSessionHandle(), 
                                  m_pimplTableCursor.GetCursorHandle(), 
                                  colMetadata.id, 
                                  value.data, value.qtBytes, 
                                  grbit, &setInfo);

        ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
        {
            std::ostringstream oss;
            oss << "Failed to append to value in large column \'" << colMetadata.name
                << "\' of table \'" << m_pimplTableCursor.GetSchema().GetName() 
                << "\' from ISAM database";
            return oss.str();
        });
    }

    void TableWriter::SetLargeColumnAppend(int columnCode, 
                                            const GenericInputParam &value, 
                                            bool compressed, 
                                            unsigned long tagSequence)
    {
        m_pimpl->SetLargeColumnAppend(columnCode, value, compressed, tagSequence);
    }

    /// <summary>
    /// Removes a value from a multi-value column.
    /// </summary>
    /// <param name="columnCode">The column code.</param>
    /// <param name="tagSequence">This is an index (base 1) that indicates which value must be removed.
    /// Must be greater than 1 and refer to an existent value.</param>
    void TableWriterImpl::RemoveValueFromMVColumn(int columnCode, unsigned long tagSequence)
    {
        CALL_STACK_TRACE;

        auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
        _ASSERTE(colMetadata.multiValued); // This function ONLY supports multi-valued columns
        _ASSERTE(tagSequence > 0); // Only values greater than 0 make sense in this case

        JET_SETINFO setInfo;
        setInfo.cbStruct = sizeof setInfo;
        setInfo.ibLongValue = 0;
        setInfo.itagSequence = tagSequence;

        const void *emptyBuf(nullptr);
            
        auto rcode = JetSetColumn(m_pimplTableCursor.GetSessionHandle(), 
                                  m_pimplTableCursor.GetCursorHandle(), 
                                  colMetadata.id, 
                                  &emptyBuf, 0, 0, 
                                  &setInfo);

        ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
        {
            std::ostringstream oss;
            oss << "Failed to remove value from multi-value column \'" << colMetadata.name
                << "\' in table \'" << m_pimplTableCursor.GetSchema().GetName() 
                << "\' of ISAM database";
            return oss.str();
        });
    }

    void TableWriter::RemoveValueFromMVColumn(int columnCode, unsigned long tagSequence)
    {
        m_pimpl->RemoveValueFromMVColumn(columnCode, tagSequence);
    }

}// end namespace isam
}// end namespace _3fd
