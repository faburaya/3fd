// stdafx.cpp : source file that includes just the standard includes
// *.pch will be the pre-compiled headers
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

#if (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP || WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP) && defined TESTING
	CPPUNIT_SET_STA_THREADING
#endif

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file
