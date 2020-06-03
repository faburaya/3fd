#ifndef ISAM_IMPL_H
#define ISAM_IMPL_H

#include "isam.h"
#include <3fd/core/logger.h>
#include <ctime>

namespace _3fd
{
namespace isam
{
    time_t GetEpoch1900();

    class Session;

    /// <summary>
    /// A helper that handles error from the ISAM engine.
    /// </summary>
    class ErrorHelper
    {
    public:

        static void HandleError(JET_INSTANCE jetInstance, 
                                JET_SESID jetSession, 
                                JET_ERR errorCode, 
                                const char *what);

        static void HandleError(JET_INSTANCE jetInstance, 
                                JET_SESID jetSession, 
                                JET_ERR errorCode, 
                                const std::function<string (void)> &what);

        static void LogError(JET_INSTANCE jetInstance, 
                                JET_SESID jetSession, 
                                JET_ERR errorCode, 
                                const char *what, 
                                core::Logger::Priority prio);

        static void LogError(JET_INSTANCE jetInstance, 
                                JET_SESID jetSession, 
                                JET_ERR errorCode, 
                                const std::function<string (void)> &what, 
                                core::Logger::Priority prio);

        static core::AppException<std::exception> 
        MakeException(JET_INSTANCE jetInstance, 
                      JET_SESID jetSession, 
                      JET_ERR errorCode, 
                      const char *what);

        static core::AppException<std::exception> 
        MakeException(JET_INSTANCE jetInstance, 
                      JET_SESID jetSession, 
                      JET_ERR errorCode, 
                      const std::function<string (void)> &what);
    };

    /// <summary>
    /// A wrapper class for the ISAM instance.
    /// </summary>
    class InstanceImpl
    {
    private:

        string            m_name;
        JET_INSTANCE    m_jetInstance;
        unsigned int    m_numMaxSessions;

    public:

        InstanceImpl(const string &name, 
                     const string &transactionLogsPath, 
                     unsigned int minCachedPages,
                     unsigned int maxVerStorePages,
                     unsigned int logBufferSizeInSectors);

        InstanceImpl(const InstanceImpl &) = delete;

        ~InstanceImpl();

        /// <summary>
        /// Gets the maximum number of sessions the instance can provide.
        /// </summary>
        /// <returns>The maximum number of sessions the instance can keep available simultaneously.</returns>
        unsigned int GetNumMaxConcurrentSessions() const { return m_numMaxSessions; }

        SessionImpl *CreateSession();
    };

    /// <summary>
    /// Databases and transactions operate in the context of a session.
    /// Each thread must have its own session, or more than one.
    /// </summary>
    class SessionImpl
    {
    private:

        JET_SESID m_jetSession;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="SessionImpl"/> class.
        /// </summary>
        /// <param name="jetSession">The session handle.</param>
        SessionImpl(JET_SESID jetSession)
            : m_jetSession(jetSession) {}

        ~SessionImpl();

        bool AttachDatabase(const wstring &dbFileName, bool throwNotFound = true);

        void DetachDatabase(const wstring &dbFileName);

        DatabaseImpl *CreateDatabase(const wstring &dbFileName);

        DatabaseImpl *OpenDatabase(const wstring &dbFileName);

        TransactionImpl *CreateTransaction();

        void Flush();
    };

    /// <summary>
    /// Private implementation for <see cref="Transaction" /> class.
    /// </summary>
    class TransactionImpl
    {
    private:

        const JET_SESID m_jetSession;
        bool m_committed;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="TransactionImpl"/> class.
        /// Begins a transaction.
        /// </summary>
        /// <param name="jetSession">The ISAM storage session handle.</param>
        TransactionImpl(JET_SESID jetSession)
            : m_jetSession(jetSession), m_committed(false) {}

        ~TransactionImpl();

        void Commit(bool blockingOp);
    };

    /// <summary>
    /// Wrapper for an ISAM database.
    /// </summary>
    class DatabaseImpl
    {
    private:

        const JET_SESID m_jetSession;
        const JET_DBID m_jetDatabase;

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="DatabaseImpl"/> class.
        /// </summary>
        /// <param name="jetDatabaseId">The ISAM database identifier.</param>
        DatabaseImpl(JET_SESID jetSession, JET_DBID jetDatabaseId) 
            : m_jetSession(jetSession), m_jetDatabase(jetDatabaseId) {}

        ~DatabaseImpl();

        /// <summary>
        /// Gets the session handle.
        /// </summary>
        /// <returns>The session ID.</returns>
        JET_SESID GetSessionHandle() { return m_jetSession; }

        /// <summary>
        /// Gets the database handle.
        /// </summary>
        /// <returns>The session ID.</returns>
        JET_DBID GetDatabaseHandle() { return m_jetDatabase; }

        ITable *OpenTable(const string &name, bool throwTableNotFound);

        ITable *CreateTable(const string &name, 
                            bool isTemplate, 
                            const std::vector<ITable::ColumnDefinition> &columns, 
                            const std::vector<ITable::IndexDefinition> &indexes, 
                            bool sparse, 
                            unsigned long reservedPages);

        ITable *CreateTable(const string &name, 
                            const string &templateName, 
                            bool sparse, 
                            unsigned long reservedPages);

        void DeleteTable(const string &name);

        TableCursorImpl *GetCursorFor(const ITable &table, bool prefetch);
    };

    /// <summary>
    /// Holds metadata for a table and allows the edition of its schema.
    /// </summary>
    class Table : public ITable
    {
    public:

        /// <summary>
        /// Holds some important column metadata.
        /// </summary>
        struct ColumnMetadata
        {
            JET_COLUMNID    id;
            DataType        dataType;
            string          name;
            bool            notNull, 
                            escrow, 
                            multiValued;
        };

        /// <summary>
        /// Holds some important index metadata.
        /// </summary>
        struct IndexMetadata
        {
            string name;
            std::unique_ptr<JET_INDEXID> idHint;

            IndexMetadata(const string &p_name, std::unique_ptr<JET_INDEXID> &&p_idHint) 
                : name(p_name), idHint(std::move(p_idHint)) {}

            IndexMetadata(IndexMetadata &&ob) noexcept
                : name(std::move(ob.name)), idHint(std::move(ob.idHint)) {}
        };

    private:

        DatabaseImpl        *m_pimplDatabase;
        const JET_TABLEID    m_jetTable;
        string                m_name;
            
        /// <summary>
        /// Keeps track of the mapping between an integer code and the corresponding column metadata.
        /// </summary>
        std::map<int, ColumnMetadata> m_colsMetadataByCode;

        /// <summary>
        /// Maps column names back to their numeric codes.
        /// </summary>
        std::map<string, int> m_columnCodesByName;

        /// <summary>
        /// Keeps track of the mapping between an integer code and the corresponding reader for an index.
        /// </summary>
        std::map<int, IndexMetadata> m_idxsMetadataByCode;

        /// <summary>
        /// Maps indexes names back to their numeric codes.
        /// </summary>
        std::map<string, int> m_idxCodesByName;
            
        /// <summary>
        /// The maximum length for each data type (in bytes).
        /// </summary>
        static std::vector<unsigned long> maxLength;

        /// <summary>
        /// A flag to avoid unsafe simultaneous initialization.
        /// </summary>
        static std::once_flag staticInitFlag;

        static void EnsureStaticInitialization();

    public:
            
        /// <summary>
        /// Gets the maximum (or fixed) length for a given data type.
        /// </summary>
        /// <param name="dataType">Type of the data.</param>
        /// <returns>The maximum (or fixed) length in bytes.</returns>
        static unsigned long GetMaxLength(DataType dataType)
        {
            EnsureStaticInitialization();
            return maxLength[(size_t) dataType];
        }

        Table(DatabaseImpl *pimplDatabase, JET_TABLEID jetTable, const string &name);

        virtual ~Table();

#ifdef _3FD_PLATFORM_WINRT
        DatabaseImpl *GetDatabase() const { return m_pimplDatabase; }
#endif
        /// <summary>
        /// Gets the name of the table.
        /// </summary>
        /// <returns>The name of the table.</returns>
        virtual const string &GetName() const override { return m_name; }

        virtual void Rename(const string &newName) override;

        virtual void AddColumn(const ITable::ColumnDefinition &column) override;

        virtual void DeleteColumn(const string &name) override;

        virtual void MapInt2ColName(int code, const string &colName) override;

        const ColumnMetadata &GetColumnMetadata(int columnCode) const;

        virtual void RenameColumn(const string &colName, const string &newColName) override;

        virtual void CreateIndexes(const std::vector<ITable::IndexDefinition> &indexes) override;

        virtual void DeleteIndex(const string &name) override;

        virtual void MapInt2IdxName(int code, const string &idxName) override;

        const IndexMetadata &GetIndexMetadata(int indexCode) const;
    };

#ifndef _3FD_PLATFORM_WINRT
    typedef JET_INDEXCREATE_W JET_INDEXCREATE_X;
#else
    typedef JET_INDEXCREATE3_W JET_INDEXCREATE_X;
#endif

    void TranslateStructures(const std::vector<ITable::IndexDefinition> &indexes, 
                                std::vector<JET_INDEXCREATE_X> &jetIndexes);

    /// <summary>
    /// A wrapper for a table in the ISAM database.
    /// </summary>
    class TableCursorImpl
    {
    private:

        const Table    &m_table;
        const JET_SESID m_jetSession;
        const JET_TABLEID m_jetTable;
        string m_curIdxName;
            
        /// <summary>
        /// Enumerates the valid options for moving the table cursor.
        /// </summary>
        enum class MoveOption : long long
        {
            First = JET_MoveFirst, 
            Previous = -1, 
            Next = 1, 
            Last = JET_MoveLast
        };
            
        void SetCurrentIndex(int idxCode);

        void MakeKey(const std::vector<GenericInputParam> &colKeyVals, 
                        TableCursor::IndexKeyMatch typeMatch, 
                        TableCursor::ComparisonOperator comparisonOp);

        void MakeKey(const std::vector<GenericInputParam> &colKeyVals, 
                        TableCursor::IndexKeyMatch typeMatch, 
                        bool upperLimit);

        bool Seek(TableCursor::ComparisonOperator comparisonOp);

        bool SetIndexRange(bool upperLimit, bool inclusive);

        bool MoveCursor(MoveOption option);

    public:

        /// <summary>
        /// Initializes a new instance of the <see cref="TableCursorImpl" /> class.
        /// </summary>
        /// <param name="table">The table implementation.</param>
        /// <param name="jetTable">The table handle.</param>
        /// <param name="jetSession">The session handle.</param>
        TableCursorImpl(const ITable &table, JET_TABLEID jetTable, JET_SESID jetSession)
            : m_table(static_cast<const Table &> (table)), m_jetTable(jetTable), m_jetSession(jetSession) {}

        ~TableCursorImpl();

        /// <summary>
        /// Gets the table schema.
        /// </summary>
        /// <returns>A reference to the correponding <see cref="Table" /> object.</returns>
        const Table &GetSchema() const { return m_table; }

        /// <summary>
        /// Gets the session handle.
        /// </summary>
        /// <returns>The session handle.</returns>
        JET_SESID GetSessionHandle() { return m_jetSession; }

        /// <summary>
        /// Gets the cursor handle.
        /// </summary>
        /// <returns>The table cursor handle.</returns>
        JET_TABLEID GetCursorHandle() { return m_jetTable; }

        size_t ScanFrom(int idxCode, 
                        const std::vector<GenericInputParam> &colKeyVals, 
                        TableCursor::IndexKeyMatch typeMatch, 
                        TableCursor::ComparisonOperator comparisonOp, 
                        const std::function<bool (RecordReader &)> &callback, 
                        bool backward);

        size_t ScanRange(const TableCursor::IndexRangeDefinition &idxRangeDef,
                            const std::function<bool (RecordReader &)> &callback);

        size_t ScanIntersection(const std::vector<TableCursor::IndexRangeDefinition> &rangeDefs,
                                const std::function<bool(RecordReader &)> &callback);

        size_t ScanAll(int idxCode, 
                        const std::function<bool (RecordReader &)> &callback, 
                        bool backward);

        TableWriterImpl *StartUpdate(TableWriter::Mode mode);

        void DeleteCurrentRecord();
    };

    /// <summary>
    /// Private implementation of <see cref="TableWriter" /> class.
    /// </summary>
    class TableWriterImpl
    {
    private:

        TableCursorImpl &m_pimplTableCursor;
        bool m_saved;

    public:
            
        TableWriterImpl(TableCursorImpl &pimplTableCursor, TableWriter::Mode mode);

        ~TableWriterImpl();

        void SetColumn(int columnCode, const GenericInputParam &value, 
                        unsigned long tagSequence = 0, bool mvUnique = true);

        void SetLargeColumn(int columnCode, const GenericInputParam &value, 
                            bool compressed = false, 
                            unsigned long tagSequence = 0);

        void SetLargeColumnOverwrite(int columnCode, const GenericInputParam &value, 
                                        unsigned long offset = 0, 
                                        bool compressed = false, 
                                        unsigned long tagSequence = 0);

        void SetLargeColumnAppend(int columnCode, const GenericInputParam &value, 
                                    bool compressed = false, 
                                    unsigned long tagSequence = 0);

        void RemoveValueFromMVColumn(int columnCode, unsigned long tagSequence);

        void Save();
    };

}// end of namespace isam
}// end of namespace _3fd

#endif // end of header guard
