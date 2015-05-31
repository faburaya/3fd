#include "stdafx.h"
#include "gc_messages.h"

namespace _3fd
{
	namespace memory
	{
		void ReferenceUpdateMsg::Execute(MasterTable &masterTable)
		{
			masterTable.DoUpdateReference(m_sptrObjAddr, m_pointedAddr);
		}

		void NewObjectMsg::Execute(MasterTable &masterTable)
		{
			masterTable.DoRegisterMemAddr(m_pointedAddr, m_blockSize, m_freeMemCallback);
			masterTable.DoUpdateReference(m_sptrObjectAddr, m_pointedAddr);
		}

		void AbortedObjectMsg::Execute(MasterTable &masterTable)
		{
			masterTable.DoUpdateReference(m_sptrObjAddr, nullptr, false);
		}

		void SptrRegistrationMsg::Execute(MasterTable &masterTable)
		{
			masterTable.DoRegisterSptr(m_sptrObjAddr, m_pointedAddr);
		}

		void SptrUnregistrationMsg::Execute(MasterTable &masterTable)
		{
			masterTable.DoUnregisterSptr(m_sptrObjAddr);
		}

	}// end of namespace memory
}// end of namespace _3fd
