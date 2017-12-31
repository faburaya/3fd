#ifndef ISAM_H
#define ISAM_H

#include "base.h"
#include "exceptions.h"
#include "callstacktracer.h"
#include <mutex>
#include <map>
#include <queue>
#include <array>
#include <vector>
#include <forward_list>
#include <chrono>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>
#include <esent.h>

namespace _3fd
{
using std::string;
using std::wstring;

/// <summary>
/// Contains classes that feature an ISAM storage.
/// Current implementantion relies on Microsoft ESE.
/// </summary>
namespace isam
{
    /// <summary>
    /// Enumerates the data types supported by the backend.
    /// </summary>
    enum class DataType : JET_COLTYP
    {
        Boolean = JET_coltypBit, // 1 byte long
        UByte = JET_coltypUnsignedByte, 
        Int16 = JET_coltypShort, 
        Int32 = JET_coltypLong, 
        Int64 = JET_coltypLongLong, 
        UInt16 = JET_coltypUnsignedShort,
        UInt32 = JET_coltypUnsignedLong, 
        GUID = JET_coltypGUID, // 16 bytes large
        Float32 = JET_coltypIEEESingle, 
        Float64 = JET_coltypIEEEDouble, 
        Currency = JET_coltypCurrency, // 8-byte signed integer, negative values sort before positive values
        DateTime = JET_coltypDateTime, // 8-byte floating point number that represents a date in fractional days since the year 1900
        Blob = JET_coltypBinary, // up to 255 bytes
        LargeBlob = JET_coltypLongBinary, // up to 2147483647 bytes
        Text = JET_coltypText, // up to 255 ASCII or 127 Unicode chars
        LargeText = JET_coltypLongText // up to 2147483647 ASCII or 1073741823 Unicode chars
    };

    DataType ResolveDataType(bool value);
    DataType ResolveDataType(uint8_t value);
    DataType ResolveDataType(uint16_t value);
    DataType ResolveDataType(uint32_t value);
    DataType ResolveDataType(int16_t value);
    DataType ResolveDataType(int32_t value);
    DataType ResolveDataType(int64_t value);
    DataType ResolveDataType(float value);
    DataType ResolveDataType(double value);

    /// <summary>
    /// Holds metadata & data of a generic input parameter.
    /// </summary>
    struct GenericInputParam
    {
        void *data;
        size_t qtBytes;
        DataType dataType;

        GenericInputParam(void *p_data = nullptr, size_t p_qtBytes = 0, DataType p_dataType = DataType::Blob)
            : data(p_data), qtBytes(p_qtBytes), dataType(p_dataType) {}
    };

    GenericInputParam NullParameter(DataType dataType);
    GenericInputParam AsInputParam(const char *value);
    GenericInputParam AsInputParam(const wchar_t *value);
    GenericInputParam AsInputParam(double &daysSince1900, time_t value);
    GenericInputParam AsInputParam(double &daysSince1900, const std::chrono::time_point<std::chrono::system_clock> &value);

    /// <summary>
    /// Makes a generic input parameter from a built-in value.
    /// </summary>
    /// <param name="value">The value.</param>
    /// <returns>A <see cref="GenericInputParam" /> struct referring the given value.</returns>
    template <typename ValType> 
    GenericInputParam AsInputParam(const ValType &value)
    {
        return GenericInputParam(const_cast<ValType *> (&value), sizeof value, ResolveDataType(value));
    }
        
    template <> GenericInputParam AsInputParam(const string &value);
    template <> GenericInputParam AsInputParam(const wstring &value);

    /// <summary>
    /// Makes a blob input parameter from a vector of values.
    /// A blob and a large blob are considered the same. However if the function expects a 
    /// regular blob and receives a large one instead, truncation will take place.
    /// </summary>
    /// <param name="values">The values.</param>
    /// <returns>A <see cref="GenericInputParam" /> struct referring the given values.</returns>
    template <typename ValType> 
    GenericInputParam AsInputParam(const std::vector<ValType> &values)
    {
        return GenericInputParam(const_cast<ValType *> (values.data()), 
                                 values.size() * sizeof(ValType), 
                                 DataType::Blob);
    }

    /// <summary>
    /// Makes a blob input parameter from a array of values.
    /// A blob and a large blob are considered the same. However if the function expects a 
    /// regular blob and receives a large one instead, truncation will take place.
    /// </summary>
    /// <param name="values">The values.</param>
    /// <returns>A <see cref="GenericInputParam" /> struct referring the given values.</returns>
    template <typename ValType, size_t N> 
    GenericInputParam AsInputParam(const std::array<ValType, N> &values)
    {
        return GenericInputParam(const_cast<ValType *> (values.data()), 
                                 N * sizeof(ValType), 
                                 DataType::Blob);
    }

    class InstanceImpl;
    class SessionImpl;
    class DatabaseImpl;
    class TransactionImpl;
    class TableCursorImpl;
    class TableWriterImpl;
    class DatabaseConn;
    class RecordReader;
    class TableWriter;

    /// <summary>
    /// Wraps an ISAM instance along with a pool of preallocated sessions and attached databases.
    /// </summary>
    class Instance : notcopiable
    {
    private:

        /// <summary>
        /// The ISAM instance.
        /// </summary>
        InstanceImpl *m_pimplInstance;

        /// <summary>
        /// Hold information and resources about an attached database.
        /// </summary>
        struct AttachedDatabase
        {
            wstring fileName;
            unsigned short handlesCount;

            AttachedDatabase(wstring &&p_fileName)
                : fileName(std::move(p_fileName)), handlesCount(0) {}
        };

        /// <summary>
        /// A binary tree that holds available resources for database connections.
        /// </summary>
        std::map<int, AttachedDatabase> m_attachedDbs;

        /// <summary>
        /// A queue that holds available sessions (without an attached database).
        /// </summary>
        std::queue<SessionImpl *> m_availableSessions;

        /// <summary>
        /// A mutex to control access to the store of database connections.
        /// </summary>
        std::mutex m_accessToResources;

        bool OpenDatabaseImpl(
            int dbCode, 
            const string &dbFileName, 
            DatabaseImpl **database, 
            SessionImpl **session, 
            bool createIfNotFound
        );

        static const unsigned int defaultParamMinCachedPages = 4;
        static const unsigned int defaultParamMaxVerStorePages = 64;
        static const unsigned int defaultParamLogBufferSizeInSectors = 126;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="Instance" /> class.
        /// </summary>
        /// <param name="name">The instance name.</param>
        /// <param name="transactionLogsPath">The directory where to create the transaction logs.</param>
        /// <param name="minCachedPages">The minimum amount of pages to keep in cache. (The more cache you guarantee, the faster is the IO.)</param>
        /// <param name="maxVerStorePages">The maximum amount of pages to reserve for the version store. (Affects how big a transaction can become and how many concurrent isolated sessions the engine can afford.)</param>
        /// <param name="logBufferSizeInSectors">The size (in volume sectors) of the transaction log write buffer. (A bigger buffer will render less frequent flushes to disk.)</param>
        Instance(const string &name, 
                 const string &transactionLogsPath, 
                 unsigned int minCachedPages = defaultParamMinCachedPages,
                 unsigned int maxVerStorePages = defaultParamMaxVerStorePages,
                 unsigned int logBufferSizeInSectors = defaultParamLogBufferSizeInSectors);

        /// <summary>
        /// Finalizes an instance of the <see cref="Instance"/> class.
        /// </summary>
        ~Instance();

        void ReleaseResource(int dbCode, DatabaseImpl *database, SessionImpl *session);

        /// <summary>
        /// Opens a database. Throws an exception if the database file does not exist.
        /// </summary>
        /// <param name="dbCode">The numeric code to associate with the database.</param>
        /// <param name="dbFileName">The name of the database file to open in case a connection is not yet available for it.</param>
        /// <returns>A connection to the specified database.</returns>
        DatabaseConn OpenDatabase(int dbCode, const string &dbFileName);

        /// <summary>
        /// Opens a database. If the database file does not exist, create it.
        /// </summary>
        /// <param name="dbCode">The numeric code to associate with the database.</param>
        /// <param name="dbFileName">The name of the database file to open in case a connection is not yet available for it.</param>
        /// <param name="newDb">Whether a new database was created because the database file did not already exist.</param>
        /// <returns>A connection to the specified database.</returns>
        DatabaseConn OpenDatabase(int dbCode, const string &dbFileName, bool &newDb);
    };

    /// <summary>
    /// A transaction within the session that uses RAII.
    /// </summary>
    class Transaction : notcopiable
    {
    private:

        TransactionImpl *m_pimpl;

        // forbiden op:
        Transaction &operator =(Transaction &&ob) {}

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="Transaction"/> class.
        /// </summary>
        /// <param name="pimpl">The private implementation.</param>
        Transaction(TransactionImpl *pimpl)
            : m_pimpl(pimpl) {}

        /// <summary>
        /// Initializes a new instance of the <see cref="Transaction"/> class using move semantics.
        /// </summary>
        /// <param name="ob">The ob.</param>
        Transaction(Transaction &&ob) : 
            m_pimpl(ob.m_pimpl) 
        {
            if(this != &ob)
                ob.m_pimpl = nullptr;
        }

        ~Transaction();

        void Commit(bool blockingOp = true);
    };

    /// <summary>
    /// Provides the means to read columns from the record currently pointed by a table cursor.
    /// </summary>
    class RecordReader : notcopiable
    {
    private:
            
        TableCursorImpl    &m_pimplTableCursor;
        std::vector<char> m_buffer;

        unsigned long GetMVColumnQtEntries(int columnCode);

        bool ReadFixedSizeValueImpl(int columnCode, DataType dataType, void *to);

        void ReadFixedSizeValuesImpl(int columnCode, DataType dataType, unsigned long qtVals, void *to);

        static time_t ConvertToEpoch(double daysSince1900);

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="RecordReader" /> class.
        /// </summary>
        /// <param name="pimplTableCursor">The table cursor private implementation.</param>
        RecordReader(TableCursorImpl *pimplTableCursor) 
            : m_pimplTableCursor(*pimplTableCursor) {}

        /// <summary>
        /// Reads the value from a column whose data type is of fixed size.
        /// </summary>
        /// <param name="columnCode">The column numeric code.</param>
        /// <param name="to">Where the value will be written to.</param>
        /// <returns>'true' if not NULL, otherwise, 'false'.</returns>
        template <typename ValType> 
        bool ReadFixedSizeValue(int columnCode, ValType &to)
        {
            return ReadFixedSizeValueImpl(columnCode, ResolveDataType(to), &to);
        }

        /// <summary>
        /// Reads the value from a 'DateTime' column.
        /// </summary>
        /// <param name="columnCode">The column numeric code.</param>
        /// <param name="to">Where the value will be written to.</param>
        /// <returns>'true' if not NULL, otherwise, 'false'.</returns>
        template <> bool ReadFixedSizeValue(int columnCode, std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> &to)
        {
            double daysSince1900;

            if( ReadFixedSizeValueImpl(columnCode, DataType::DateTime, &daysSince1900) )
            {
                using namespace std::chrono;
                to = time_point<system_clock, seconds>(seconds(ConvertToEpoch(daysSince1900)));
                return true;
            }
            else
                return false;
        }

        /// <summary>
        /// Reads the value from a 'GUID' column.
        /// </summary>
        /// <param name="columnCode">The column numeric code.</param>
        /// <param name="to">Where the value will be written to.</param>
        /// <returns>'true' if not NULL, otherwise, 'false'.</returns>
        template <> bool ReadFixedSizeValue(int columnCode, std::array<int, 4> &to)
        {
            return ReadFixedSizeValueImpl(columnCode, DataType::GUID, to.data());
        }

        /// <summary>
        /// Reads multiple values from a column whose data type is of fixed size.
        /// </summary>
        /// <param name="columnCode">The column numeric code.</param>
        /// <param name="to">The vector where the multiple values in the column will be written to.</param>
        template <typename ValType> 
        void ReadFixedSizeValues(int columnCode, std::vector<ValType> &to)
        {
            CALL_STACK_TRACE;

            try
            {
                to.clear(); // Whatever happens, guarantee it will not keep the previous value
                auto qtVals = GetMVColumnQtEntries(columnCode);

                if(qtVals > 0)
                {
                    to.resize(qtVals); // Make room for all values in the column
                    ReadFixedSizeValuesImpl(columnCode, ResolveDataType(to[0]), qtVals, to.data());
                }
            }
            catch(core::IAppException &ex)
            {
                throw; // just forward exceptions regarding errors known to have been previously handled
            }
            catch(std::exception &ex)
            {
                std::ostringstream oss;
                oss << "Generic failure when retrieving values from multi-value column of table in ISAM database: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        /// <summary>
        /// Reads multiple values from a 'DateTime' column.
        /// </summary>
        /// <param name="columnCode">The column numeric code.</param>
        /// <param name="to">The vector where the multiple values in the column will be written to.</param>
        template <> void ReadFixedSizeValues(int columnCode, std::vector<std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>> &to)
        {
            CALL_STACK_TRACE;

            try
            {
                to.clear(); // Whatever happens, guarantee it will not keep the previous value
                auto qtVals = GetMVColumnQtEntries(columnCode);

                if(qtVals > 0)
                {
                    m_buffer.resize(sizeof (double) * qtVals); // Make room for all values in the column
                    ReadFixedSizeValuesImpl(columnCode, DataType::DateTime, qtVals, m_buffer.data());
                        
                    to.reserve(qtVals);
                    auto begin = reinterpret_cast<double *> (m_buffer.data());
                    auto end = reinterpret_cast<double *> (m_buffer.data()) + qtVals;

                    std::for_each(begin, end, [&to, this] (double &daysSince1900)
                    {
                        to.emplace_back(
                            std::chrono::seconds(ConvertToEpoch(daysSince1900))
                        );
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
                oss << "Generic failure when retrieving values from \'DateTime\' multi-value column of table in ISAM database: " << ex.what();
                throw core::AppException<std::runtime_error>(oss.str());
            }
        }

        bool ReadTextValue(int columnCode, string &to);
            
        void ReadTextValues(int columnCode, std::vector<string> &to);

        bool ReadBlobValue(int columnCode, std::vector<uint8_t> &to);

        void ReadBlobValues(int columnCode, std::vector<std::vector<uint8_t>> &to);
    };
        
    /// <summary>
    /// Just like a transaction, this object controls the scope of a writing operation across several columns in a table.
    /// </summary>
    class TableWriter : notcopiable
    {
    private:

        TableWriterImpl *m_pimpl;

    public:

        /// <summary>
        /// Enumerates the types of updates.
        /// </summary>
        enum class Mode : unsigned long
        {
            InsertNew = JET_prepInsert,
            InsertCopy = JET_prepInsertCopy,
            PrimaryKeyChange = JET_prepInsertCopyDeleteOriginal,
            Replace = JET_prepReplace
        };
            
        /// <summary>
        /// Initializes a new instance of the <see cref="TableWriter"/> class.
        /// </summary>
        /// <param name="pimpl">The private implementation.</param>
        TableWriter(TableWriterImpl *pimpl)
            : m_pimpl(pimpl) {}

        /// <summary>
        /// Initializes a new instance of the <see cref="TableWriter"/> class using move semantics.
        /// </summary>
        /// <param name="ob">The objects whose resources will be stolen.</param>
        TableWriter(TableWriter &&ob) : 
            m_pimpl(ob.m_pimpl) 
        {
            if(this != &ob)
                ob.m_pimpl = nullptr;
        }

        ~TableWriter();

        /// <summary>
        /// Sets the value of a (not large blob or text) column for update (or insertion).
        /// </summary>
        /// <param name="columnCode">The integer code which identifies the column.</param>
        /// <param name="value">The value to set into the column.</param>
        /// <param name="tagSequence">The tag sequence if it is a multi-value column.
        /// This is an index (base 1) that indicates which value must be overwritten.
        /// Use '0' (default) or an unexistent value if you wish to add a new one.</param>
        /// <param name="uniqueMV">Whether the column (if multi-valued) must forbid duplicates.</param>
        void SetColumn(int columnCode, const GenericInputParam &value, 
                       unsigned long tagSequence = 0, bool mvUnique = true);

        /// <summary>
        /// Sets the value for a large (blob or text) column.
        /// </summary>
        /// <param name="columnCode">The column code.</param>
        /// <param name="value">The value to set into the column.</param>
        /// <param name="compressed">Whether should attempt compression.</param>
        /// <param name="tagSequence">The tag sequence if it is a multi-value column.
        /// This is an index (base 1) that indicates which value must be overwritten.
        /// Use '0' (default) or an unexistent value if you wish to add a new one.</param>
        void SetLargeColumn(int columnCode, const GenericInputParam &value, 
                            bool compressed = false, 
                            unsigned long tagSequence = 0);

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
        void SetLargeColumnOverwrite(int columnCode, const GenericInputParam &value, 
                                     unsigned long offset = 0, 
                                     bool compressed = false, 
                                     unsigned long tagSequence = 0);

        /// <summary>
        /// Sets the value for a large (blob or text) column appending to the previous content.
        /// </summary>
        /// <param name="columnCode">The column code.</param>
        /// <param name="value">The value to set into the column.</param>
        /// <param name="compressed">Whether should attempt compression.</param>
        /// <param name="tagSequence">The tag sequence if it is a multi-value column.
        /// This is an index (base 1) that indicates which value must be appended.
        /// Use '0' (default) or an unexistent value if you wish to add a new one.</param>
        void SetLargeColumnAppend(int columnCode, const GenericInputParam &value, 
                                  bool compressed = false, 
                                  unsigned long tagSequence = 0);

        /// <summary>
        /// Removes a value from a multi-value column.
        /// </summary>
        /// <param name="columnCode">The column code.</param>
        /// <param name="tagSequence">This is an index (base 1) that indicates which value must be removed.
        /// Must be greater than 1 and refer to an existent value.</param>
        void RemoveValueFromMVColumn(int columnCode, unsigned long tagSequence);

        /// <summary>
        /// Saves the changes made in the object scope.
        /// </summary>
        void Save();
    };
        
    /// <summary>
    /// A cursor for table in the ISAM database.
    /// </summary>
    class TableCursor : notcopiable
    {
    public:

        /// <summary>
        /// Enumerates the comparison operators that can be used when comparing index keys.
        /// </summary>
        enum class ComparisonOperator : JET_GRBIT 
        {
            EqualTo = JET_bitSeekEQ, 
            GreaterThanOrEqualTo = JET_bitSeekGE, 
            GreaterThan = JET_bitSeekGT, 
            LessThanOrEqualTo = JET_bitSeekLE, 
            LessThan = JET_bitSeekLT
        };

        /// <summary>
        /// Enumerates the possible forms to match an index key.
        /// </summary>
        enum class IndexKeyMatch 
        {
            Regular, /* Do not use wildcards:
                        All columns in the index must be present in the
                        key, and the search looks for an exact match. */

            Wildcard, /* The search use wildcards as values for the unspecified columns,
                            and an exact match is required in the specified ones. */

            PrefixWildcard /* Only allowed when the last key is text content,
                                when a prefix match is used, whereas an exact match is
                                required for the previously specified ones. The search
                                uses wildcards as values for the unspecified columns */
        };

        /// <summary>
        /// Holds the configurations for the keys that define an index range.
        /// </summary>
        struct IndexRangeDefinition
        {
            /// <summary>
            /// The numeric code that identifies an index, as set by <see cref="ITable::MapInt2IdxName"/>
            /// </summary>
            int indexCode;

            /// <summary>
            /// The key used to find the record in the beginning of the range.
            /// </summary>
            struct
            {
                /// <summary>
                /// The values to match the columns that form the index. In this vector they must occur
                /// in the same order established upon index definition. If not all expected keys are
                /// present, the type of match for this key has to use wildcards. (See <see cref="IndexKeyMatch"/>.)
                /// </summary>
                std::vector<GenericInputParam> colsVals;

                /// <summary>
                /// The type of match to use when searching for the key.
                /// The use of wildcards for undefined columns implies the possibility of
                /// several records matching this key definition, hence it is not compatible
                /// with the equality operator. In this case, the comparison operator is used
                /// to select a single match. 
                /// </summary>
                IndexKeyMatch typeMatch;

                /// <summary>
                /// The comparison operator to use when selecting a match for this key.
                /// When a record must be chosen from a set of possible matches (result of
                /// a key containing wildcards, and ordered as defined by the index), the
                /// operators &gt; and &gt;= cause a match with the record closest to the beginning
                /// of the index (lower limit), while the operators &lt; and &lt;= cause a match
                /// with the record closest to the end of the index.
                /// </summary>
                ComparisonOperator comparisonOper;
            } beginKey;

            /// <summary>
            /// The key used to find the record in the end of this range.
            /// </summary>
            struct
            {
                /// <summary>
                /// The values to match the columns that form the index. In this vector they must occur
                /// in the same order established upon index definition. If not all expected keys are
                /// present, the type of match for this key has to use wildcards. (See <see cref="IndexKeyMatch"/>.)
                /// </summary>
                std::vector<GenericInputParam> colsVals;

                /// <summary>
                /// The type of match to use when searching for the key.
                /// The use of wildcards for undefined columns implies the possibility of
                /// several records matching this key definition. Because this key has no
                /// choice but using the equality operator for match, the fields <see cref="isUpperLimit"/>
                /// and <see cref="isInclusive"/> are used as tie breakers.
                /// </summary>
                IndexKeyMatch typeMatch;

                /// <summary>
                /// Whether the this end-key matches a record closer to the end of the index than
                /// then begin-key. That means going from the begin-key to the end-key, the cursor
                /// is moving forward (towards the end of the index).
                /// </summary>
                bool isUpperLimit;

                /// <summary>
                /// Whether the range to be set must include the records that match the key.
                /// </summary>
                bool isInclusive;
            } endKey;
        };

    private:

        TableCursorImpl *m_pimplTableCursor;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="TableCursor"/> class.
        /// </summary>
        /// <param name="table">The table cursor private implementation.</param>
        TableCursor(TableCursorImpl *table) 
            : m_pimplTableCursor(table) {}

        /// <summary>
        /// Initializes a new instance of the <see cref="TableCursor"/> class using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources will be stolen.</param>
        TableCursor(TableCursor &&ob) : 
            m_pimplTableCursor(ob.m_pimplTableCursor) 
        {
            if(this != &ob)
                ob.m_pimplTableCursor = nullptr;
        }

        ~TableCursor();

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
        size_t ScanFrom(int idxCode, 
                        const std::vector<GenericInputParam> &colKeyVals, 
                        IndexKeyMatch typeMatch, 
                        ComparisonOperator comparisonOp, 
                        const std::function<bool (RecordReader &)> &callback, 
                        bool backward = false);

        /// <summary>
        /// Scans the table over the range established by the provided keys.
        /// </summary>
        /// <param name="idxRangeDef">The index range definition.</param>
        /// <param name="callback">The callback to invoke for every record the cursor visits.
        /// It must return 'true' to continue going forward, or 'false' to stop iterating over the records.</param>
        /// <returns>
        /// How many records the callback was invoked on. Zero means it could not match both keys to set a range.
        /// </returns>
        size_t ScanRange(const IndexRangeDefinition &idxRangeDef,
                         const std::function<bool (RecordReader &)> &callback);

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
        size_t ScanIntersection(const std::vector<IndexRangeDefinition> &rangeDefs,
                                const std::function<bool(RecordReader &)> &callback);

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
        size_t ScanAll(int idxCode, 
                       const std::function<bool (RecordReader &)> &callback, 
                       bool backward = false);

        /// <summary>
        /// Starts an update process in the current scope.
        /// </summary>
        /// <param name="mode">The update mode.</param>
        /// <returns>An <see cref="Update" /> object to control the process using RAII.</returns>
        TableWriter StartUpdate(TableWriter::Mode mode);

        /// <summary>
        /// Deletes the current record where the cursor is at.
        /// </summary>
        void DeleteCurrentRecord();
    };

    /// <summary>
    /// Enumerates the code page supported by a text column.
    /// </summary>
    enum class CodePage : unsigned long { English = 1252, Unicode = 1200 };

    /// <summary>
    /// Enumerates the ordering options.
    /// </summary>
    enum class Order { Ascending, Descending };

    extern const uint8_t NotNull,
                         MultiValue,
                         AutoIncrement,
                         Sparse,
                         Primary, Clustered, // synonyms
                         Unique;

    /// <summary>
    /// An interface to the table schema in the ISAM database.
    /// Attention should be paid that the resources through this interface ARE NOT THREAD SAFE.
    /// </summary>
    class ITable
    {
    public:
            
        /// <summary>
        /// Represents the definition of a column to be used as parameter for table creation.
        /// </summary>
        struct ColumnDefinition
        {
            wstring        name;
            DataType    dataType;
            CodePage    codePage;

            bool notNull, 
                 multiValued, 
                 autoIncrement, 
                 sparse;

            GenericInputParam default;

            ColumnDefinition(const string &p_name, 
                             DataType p_dataType, 
                             uint8_t colValFlags = 0);
        };

        /// <summary>
        /// Represents the definition of an index to be used as parameter for table creation.
        /// </summary>
        struct IndexDefinition
        {
            wstring name;

            /// <summary>
            /// The keys are encoded as expected by 'JetCreateTableColumnIndex'.
            /// </summary>
            std::vector<wchar_t> keys;

            bool primary, 
                 unique;

            IndexDefinition(const string &p_name, 
                            const std::vector<std::pair<string, Order>> &p_keys, 
                            uint8_t colIdxFlags = 0);
        };

    public:

        virtual ~ITable() {}

        virtual const string &GetName() const = 0;

        virtual void Rename(const string &newName) = 0;

        virtual void AddColumn(const ColumnDefinition &column) = 0;

        virtual void DeleteColumn(const string &name) = 0;

        virtual void MapInt2ColName(int code, const string &colName) = 0;

        virtual void RenameColumn(const string &colName, const string &newColName) = 0;

        virtual void CreateIndexes(const std::vector<IndexDefinition> &indexes) = 0;

        virtual void DeleteIndex(const string &name) = 0;

        virtual void MapInt2IdxName(int code, const string &idxName) = 0;
    };

    /// <summary>
    /// A ISAM database "connection".
    /// It wraps a database with an exclusive session for it.
    /// </summary>
    class DatabaseConn : notcopiable
    {
    private:

        Instance        &m_instance;
        SessionImpl        *m_pimplSession;
        DatabaseImpl    *m_pimplDatabase;
        const int        m_code;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="DatabaseConn" /> class.
        /// </summary>
        /// <param name="instance">The instance.</param>
        /// <param name="session">The private implementation for a session.</param>
        /// <param name="database">The private implementation for a database.</param>
        /// <param name="code">The numeric code attributed to this database connection.</param>
        DatabaseConn(Instance &instance, SessionImpl *session, DatabaseImpl *database, int code)
            : m_instance(instance), m_pimplSession(session), m_pimplDatabase(database), m_code(code) {}

        /// <summary>
        /// Initializes a new instance of the <see cref="DatabaseConn"/> class using move semantics.
        /// </summary>
        /// <param name="ob">The object whose resources will be stolen.</param>
        DatabaseConn(DatabaseConn &&ob) : 
            m_instance(ob.m_instance), 
            m_pimplSession(ob.m_pimplSession), 
            m_pimplDatabase(ob.m_pimplDatabase), 
            m_code(ob.m_code) 
        {
            if(this != &ob)
            {
                ob.m_pimplSession = nullptr;
                ob.m_pimplDatabase = nullptr;
            }
        }

        /// <summary>
        /// Finalizes an instance of the <see cref="DatabaseConn"/> class.
        /// </summary>
        ~DatabaseConn()
        {
            if(m_pimplSession != nullptr)
                m_instance.ReleaseResource(m_code, m_pimplDatabase, m_pimplSession);
        }

        std::shared_ptr<ITable> TryOpenTable(bool &tableWasFound, const string &name);

        std::shared_ptr<ITable> OpenTable(const string &name);
            
        std::shared_ptr<ITable> CreateTable(const string &name, 
                                            bool isTemplate, 
                                            const std::vector<ITable::ColumnDefinition> &columns, 
                                            const std::vector<ITable::IndexDefinition> &indexes, 
                                            bool sparse = false, 
                                            unsigned long reservedPages = 1);

        std::shared_ptr<ITable> CreateTable(const string &name, 
                                            const string &templateName, 
                                            bool sparse = false, 
                                            unsigned long reservedPages = 1);

        void DeleteTable(const string &name);

        TableCursor GetCursorFor(const std::shared_ptr<ITable> &table, bool prefetch = false);

        Transaction BeginTransaction();

        void Flush();
    };

}// end of namespace isam
}// end of namespace _3fd

#endif // end of header guard