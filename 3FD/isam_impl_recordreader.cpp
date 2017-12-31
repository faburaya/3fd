#include "stdafx.h"
#include "isam_impl.h"
#include "logger.h"
#include <cassert>
#include <cmath>

namespace _3fd
{
namespace isam
{
    ///////////////////////////
    // RecordReader Class
    ///////////////////////////

    /// <summary>
    /// Converts to epoch time.
    /// </summary>
    /// <param name="daysSince1900">Fractional days since 1900, which is how the ISAM stores and retrieves DateTime values.</param>
    /// <returns>Seconds since epoch.</returns>
    time_t RecordReader::ConvertToEpoch(double daysSince1900)
    {
        static const time_t epoch1900 = GetEpoch1900();
        return static_cast<time_t> (floor(86400 * daysSince1900 + epoch1900 + 0.5));
    }
        
    /// <summary>
    /// Gets the number of values in a multi-value column.
    /// </summary>
    /// <param name="columnCode">The column numeric code.</param>
    /// <returns>
    /// How many values there are in the given multi-value column.
    /// </returns>
    unsigned long RecordReader::GetMVColumnQtEntries(int columnCode)
    {
        CALL_STACK_TRACE;
            
        auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
        _ASSERTE(colMetadata.multiValued); // Column must be multi-value

        JET_RETRIEVECOLUMN jetCols = {};
        jetCols.columnid = colMetadata.id;

        auto rcode = JetRetrieveColumns(m_pimplTableCursor.GetSessionHandle(), 
                                        m_pimplTableCursor.GetCursorHandle(), 
                                        &jetCols, 1);

        ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
        {
            std::ostringstream oss;
            oss << "Failed to get number of values in multi-value column \'" << colMetadata.name 
                << "\' of table \'" << m_pimplTableCursor.GetSchema().GetName() 
                << "\' from ISAM database";
            return oss.str();
        });

        return jetCols.itagSequence;
    }

    /// <summary>
    /// Implements most of the work of <see cref="RecordReader::ReadFixedSizeValue" />.
    /// </summary>
    /// <param name="columnCode">The column numeric code.</param>
    /// <param name="dataType">The type of data in the column.</param>
    /// <param name="to">The memory address of the variable where the value must be copied to.</param>
    /// <returns>'true' if not NULL, otherwise, 'false'.</returns>
    bool RecordReader::ReadFixedSizeValueImpl(int columnCode, DataType dataType, void *to)
    {
        CALL_STACK_TRACE;

        auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
        _ASSERTE(colMetadata.dataType == dataType); // Column data type must match the data type of the provided buffer
        _ASSERTE(dataType != DataType::Text 
                && dataType != DataType::LargeText 
                && dataType != DataType::Blob 
                && dataType != DataType::LargeBlob); // UNSUPPORTED DATA TYPES: This method can only retrieve data which is not text or blob

        const auto valSize = Table::GetMaxLength(dataType);

        auto rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(), 
                                       m_pimplTableCursor.GetCursorHandle(), 
                                       colMetadata.id, 
                                       to, valSize, 
                                       nullptr, 0, nullptr);

        if(rcode != JET_wrnColumnNull)
        {
            ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
            {
                std::ostringstream oss;
                oss << "Failed to retrieve value from column \'" << colMetadata.name 
                    << "\' of table \'" << m_pimplTableCursor.GetSchema().GetName() 
                    << "\' in ISAM database";
                return oss.str();
            });

            if(dataType != DataType::Boolean)
                return true;
            else
            {// Boolean values must be treated to a proper representation of 'true':
                const uint8_t bTrue = 0xff;
                if(memcmp(to, &bTrue, 1) == 0)
                    memset(to, static_cast<int> (true), 1);
                return true;
            }
        }
        else
            return false;
    }
        
    /// <summary>
    /// Implements most of the work of <see cref= "RecordReader::ReadFixedSizeValues" />.
    /// </summary>
    /// <param name="columnCode">The column numeric code.</param>
    /// <param name="dataType">The type of data in the column.</param>
    /// <param name="qtVals">How many values there are in the column.</param>
    /// <param name="to">The buffer where the multiple values in the column must be copied to. (Assumed to have room for all values to store.)</param>
    void RecordReader::ReadFixedSizeValuesImpl(int columnCode, DataType dataType, unsigned long qtVals, void *to)
    {
        CALL_STACK_TRACE;

        auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
        _ASSERTE(colMetadata.dataType == dataType); // Column data type must match the data type of the provided buffer
        _ASSERTE(dataType != DataType::Text 
                 && dataType != DataType::LargeText 
                 && dataType != DataType::Blob 
                 && dataType != DataType::LargeBlob); // UNSUPPORTED DATA TYPES: This method can only retrieve data which is not text or blob

        const auto valSize = Table::GetMaxLength(dataType);

        JET_RETINFO jetColInfo = {};
        jetColInfo.cbStruct = sizeof jetColInfo;
        auto &tagSeq = jetColInfo.itagSequence;

        // Sequence the values inside the column copying them into the output buffer:
        for(tagSeq = 1; tagSeq <= qtVals; ++tagSeq)
        {
            auto rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(), 
                                           m_pimplTableCursor.GetCursorHandle(), 
                                           colMetadata.id, 
                                           to, valSize, nullptr, 
                                           0, &jetColInfo);
                
            ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
            {
                std::ostringstream oss;
                oss << "Failed to read value from multi-value column \'" << colMetadata.name 
                    << "\' in table \'" << m_pimplTableCursor.GetSchema().GetName() 
                    << "\' of ISAM database";
                return oss.str();
            });

            // Boolean values must be treated to a proper representation of 'true':
            if(dataType == DataType::Boolean)
            {
                const uint8_t bTrue = 0xff;
                if(memcmp(to, &bTrue, 1) == 0)
                    memset(to, static_cast<int> (true), 1);
            }

            to = reinterpret_cast<void *> (reinterpret_cast<uintptr_t> (to) + valSize);
        }
    }

    /// <summary>
    /// Reads the text value of a column.
    /// </summary>
    /// <param name="columnCode">The column numeric code.</param>
    /// <param name="to">The string where the value must be copied to.</param>
    /// <returns>'true' if not NULL, otherwise, 'false'.</returns>
    bool RecordReader::ReadTextValue(int columnCode, string &to)
    {
        CALL_STACK_TRACE;

        try
        {
            to.clear(); // Whatever happens, guarantee it will not keep the previous value

            auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
            _ASSERTE(colMetadata.dataType == DataType::Text 
                    || colMetadata.dataType == DataType::LargeText); // This method can only retrieve text data

            unsigned long valQtBytes;

            // First get the size of the value:
            auto rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(), 
                                           m_pimplTableCursor.GetCursorHandle(), 
                                           colMetadata.id, 
                                           nullptr, 0, &valQtBytes, 
                                           0, nullptr);
                
            if (rcode != JET_wrnBufferTruncated)
            {
                ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata]()
                {
                    std::ostringstream oss;
                    oss << "Failed to retrieve value size in column \'" << colMetadata.name
                        << "\' from table \'" << m_pimplTableCursor.GetSchema().GetName()
                        << "\' of ISAM database";
                    return oss.str();
                });
            }
                
            // Reserve memory in the text buffer
            m_buffer.resize(valQtBytes);

            // Now get the actual value:
            rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(),
                                      m_pimplTableCursor.GetCursorHandle(),
                                      colMetadata.id,
                                      m_buffer.data(), m_buffer.size(),
                                      nullptr, 0, nullptr);

            if (rcode != JET_wrnColumnNull)
            {
                ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata]()
                {
                    std::ostringstream oss;
                    oss << "Failed to retrieve text value from column \'" << colMetadata.name
                        << "\' of table \'" << m_pimplTableCursor.GetSchema().GetName()
                        << "\' in ISAM database";
                    return oss.str();
                });

                to.reserve(m_buffer.size() + 1);
                to.append(m_buffer.begin(), m_buffer.end());
                m_buffer.clear();
                return true;
            }
            else
                return false;
        }
        catch(core::IAppException &)
        {
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch(std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when reading text value from column in table of ISAM database: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Reads multiple values from a multi-value text column.
    /// </summary>
    /// <param name="columnCode">The column numeric code.</param>
    /// <param name="to">The vector where the values must be copied to.</param>
    void RecordReader::ReadTextValues(int columnCode, std::vector<string> &to)
    {
        CALL_STACK_TRACE;

        try
        {
            /* Whatever happens guarantee the output vector will not keep the previous content.
            However, the strings inside it contain preallocated memory in their buffers, which 
            is a useful resource to keep. */
            std::vector<string> temp;
            temp.swap(to);

            auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
            _ASSERTE(colMetadata.dataType == DataType::Text 
                     || colMetadata.dataType == DataType::LargeText); // This method can only retrieve text data

            // Find out how many values there is in the column
            const auto qtVals = GetMVColumnQtEntries(columnCode);

            JET_RETINFO jetColInfo = {};
            jetColInfo.cbStruct = sizeof jetColInfo;
            auto &tagSeq = jetColInfo.itagSequence;

            // Sequences the values in the column, copying them into the output vector:
            for(tagSeq = 1; tagSeq <= qtVals; ++tagSeq)
            {
                unsigned long valQtBytes;

                // First get the size of the value:
                auto rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(), 
                                               m_pimplTableCursor.GetCursorHandle(), 
                                               colMetadata.id, 
                                               nullptr, 0, &valQtBytes, 
                                               0, &jetColInfo);

                if (rcode != JET_wrnBufferTruncated)
                {
                    ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata]()
                    {
                        std::ostringstream oss;
                        oss << "Failed to retrieve size of text value in multi-value column \'" << colMetadata.name
                            << "\' from table \'" << m_pimplTableCursor.GetSchema().GetName()
                            << "\' of ISAM database";
                        return oss.str();
                    });
                }

                // Reserve memory in the text buffer
                m_buffer.resize(valQtBytes);

                // Now get the actual value:
                rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(), 
                                          m_pimplTableCursor.GetCursorHandle(), 
                                          colMetadata.id, 
                                          m_buffer.data(), 
                                          m_buffer.size(), 
                                          nullptr, 
                                          0, &jetColInfo);

                ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
                {
                    std::ostringstream oss;
                    oss << "Failed to retrieve text value from multi-value column \'" << colMetadata.name 
                        << "\' of table \'" << m_pimplTableCursor.GetSchema().GetName() 
                        << "\' in ISAM database";
                    return oss.str();
                });

                if(temp.empty() == false) // If there are resources available to move:
                {
                    to.emplace_back(std::move(temp.back())); // move already existent resources to the output vector
                    to.back().reserve(m_buffer.size() + 1); // reserve memory in the string buffer
                    to.back().assign(m_buffer.begin(), m_buffer.end()); // copy the content to the string
                }
                else // otherwise, make a new string:
                    to.emplace_back(m_buffer.begin(), m_buffer.end());

                m_buffer.clear();
            }
        }
        catch(core::IAppException &)
        {
            to.clear();
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch(std::exception &ex)
        {
            to.clear();
            std::ostringstream oss;
            oss << "Generic failure when reading multiple values from multi-value text column in table of ISAM database: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

    /// <summary>
    /// Reads the BLOB value of a column.
    /// </summary>
    /// <param name="columnCode">The column numeric code.</param>
    /// <param name="to">Where the blob must be copied to.</param>
    /// <returns>'true' if not NULL, otherwise, 'false'.</returns>
    bool RecordReader::ReadBlobValue(int columnCode, std::vector<uint8_t> &to)
    {
        CALL_STACK_TRACE;

        try
        {
            to.clear(); // Whatever happens, guarantee it will not keep the previous value

            auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
            _ASSERTE(colMetadata.dataType == DataType::Blob 
                    || colMetadata.dataType == DataType::LargeBlob); // This method can only retrieve blob data

            unsigned long valQtBytes;

            // First get the size of the value:
            auto rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(), 
                                           m_pimplTableCursor.GetCursorHandle(), 
                                           colMetadata.id, 
                                           nullptr, 0, &valQtBytes, 
                                           0, nullptr);

            ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
            {
                std::ostringstream oss;
                oss << "Failed to retrieve value size in column \'" << colMetadata.name 
                    << "\' from table \'" << m_pimplTableCursor.GetSchema().GetName() 
                    << "\' of ISAM database";
                return oss.str();
            });

            // Reserve memory in the output vector
            to.resize(valQtBytes);

            // Now get the actual value:
            rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(), 
                                      m_pimplTableCursor.GetCursorHandle(), 
                                      colMetadata.id, 
                                      to.data(), to.size(), nullptr, 
                                      0, nullptr);

            if(rcode != JET_wrnColumnNull)
            {
                ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
                {
                    std::ostringstream oss;
                    oss << "Failed to retrieve blob from column \'" << colMetadata.name 
                        << "\' of table \'" << m_pimplTableCursor.GetSchema().GetName() 
                        << "\' in ISAM database";
                    return oss.str();
                });

                return true;
            }
            else
                return false;
        }
        catch(core::IAppException &)
        {
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch(std::exception &ex)
        {
            std::ostringstream oss;
            oss << "Generic failure when reading blob value from column in table of ISAM database: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }
        
    /// <summary>
    /// Reads multiple values from a multi-value BLOB column.
    /// </summary>
    /// <param name="columnCode">The column numeric code.</param>
    /// <param name="to">The vector where the blobs must be copied to.</param>
    void RecordReader::ReadBlobValues(int columnCode, std::vector<std::vector<uint8_t>> &to)
    {
        CALL_STACK_TRACE;

        try
        {
            /* Whatever happens guarantee the output vector will not keep the previous content. However, the 
            vectors inside it contain preallocated contiguous memory, which is a useful resource to keep. */
            std::vector<std::vector<uint8_t>> temp;
            temp.swap(to);

            auto &colMetadata = m_pimplTableCursor.GetSchema().GetColumnMetadata(columnCode);
            _ASSERTE(colMetadata.dataType == DataType::Blob 
                     || colMetadata.dataType == DataType::LargeBlob); // This method can only retrieve blob data

            // Find out how many values there is in the column
            const auto qtVals = GetMVColumnQtEntries(columnCode);

            JET_RETINFO jetColInfo = {};
            auto &tagSeq = jetColInfo.itagSequence;

            // Sequences the values in the column, copying them into the output vector:
            for(tagSeq = 1; tagSeq < qtVals; ++tagSeq)
            {
                unsigned long valQtBytes;

                // First get the size of the value:
                auto rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(), 
                                               m_pimplTableCursor.GetCursorHandle(), 
                                               colMetadata.id, 
                                               nullptr, 0, &valQtBytes, 
                                               0, &jetColInfo);

                ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
                {
                    std::ostringstream oss;
                    oss << "Failed to retrieve size of value in multi-value column \'" << colMetadata.name 
                        << "\' from table \'" << m_pimplTableCursor.GetSchema().GetName() 
                        << "\' of ISAM database";
                    return oss.str();
                });

                if(temp.empty() == false) // If there are available resources to move:
                {
                    to.emplace_back(std::move(temp.back()));
                    to.back().resize(valQtBytes); // make room for the new blob
                }
                else // otherwise, make a new vector to store the blob:
                    to.emplace_back(valQtBytes);

                // Now get the actual value:
                rcode = JetRetrieveColumn(m_pimplTableCursor.GetSessionHandle(), 
                                          m_pimplTableCursor.GetCursorHandle(), 
                                          colMetadata.id, 
                                          to.back().data(), 
                                          to.back().size(), 
                                          nullptr, 
                                          0, &jetColInfo);

                ErrorHelper::HandleError(NULL, m_pimplTableCursor.GetSessionHandle(), rcode, [this, &colMetadata] ()
                {
                    std::ostringstream oss;
                    oss << "Failed to retrieve blob value from multi-value column \'" << colMetadata.name 
                        << "\' of table \'" << m_pimplTableCursor.GetSchema().GetName() 
                        << "\' in ISAM database";
                    return oss.str();
                });
            }
        }
        catch(core::IAppException &)
        {
            to.clear();
            throw; // just forward exceptions regarding errors known to have been previously handled
        }
        catch(std::exception &ex)
        {
            to.clear();
            std::ostringstream oss;
            oss << "Generic failure when reading blob value from  multi-value column in table of ISAM database: " << ex.what();
            throw core::AppException<std::runtime_error>(oss.str());
        }
    }

}// end namespace isam
}// end namespace _3fd
