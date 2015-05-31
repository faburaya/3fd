#include "stdafx.h"
#include "isam_impl.h"

namespace _3fd
{
	namespace isam
	{
		///////////////////////////
		// Transaction Class
		///////////////////////////

		/// <summary>
		/// Finalizes an instance of the <see cref="TransactionImpl"/> class.
		/// </summary>
		TransactionImpl::~TransactionImpl()
		{
			// If the object was not moved and the transaction is not yet commited, rollback:
			if (m_jetSession != NULL && m_committed == false)
			{
				CALL_STACK_TRACE;
				auto rcode = JetRollback(m_jetSession, 0);
				ErrorHelper::LogError(NULL, m_jetSession, rcode, "Failed to rollback ISAM transaction", core::Logger::PRIO_CRITICAL);
			}
		}

		/// <summary>
		/// Finalizes an instance of the <see cref="Transaction"/> class.
		/// </summary>
		Transaction::~Transaction()
		{
			delete m_pimpl;
		}

		/// <summary>
		/// Commit transaction.
		/// </summary>
		/// <param name="blockingOp">
		/// Whether to wait for the transaction to be flushed to the transaction log file before returning to the caller.
		/// </param>
		void TransactionImpl::Commit(bool blockingOp)
		{
			CALL_STACK_TRACE;
			auto rcode = JetCommitTransaction(m_jetSession, blockingOp ? 0 : JET_bitCommitLazyFlush);
			ErrorHelper::HandleError(NULL, m_jetSession, rcode, "Failed to commit ISAM transaction");
			m_committed = true;
		}

		/// <summary>
		/// Commit transaction.
		/// </summary>
		/// <param name="blockingOp">
		/// Whether to wait for the transaction to be flushed to the transaction log file before returning to the caller.
		/// </param>
		void Transaction::Commit(bool blockingOp)
		{
			m_pimpl->Commit(blockingOp);
		}

	}// end namespace isam
}// end namespace _3fd
