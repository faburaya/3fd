// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#ifndef STDAFX_H // header guard
#define STDAFX_H

#	ifdef _MSC_VER // Visual Studio:
#		include <winapifamily.h>

#		if WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP // Windows Desktop App:
#			ifdef TESTING // Test application:
#				include "targetver.h"
#				include <stdio.h>
#				include <tchar.h>
#				define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#				include <gtest/gtest.h>

#			else // Static library:
#				include "targetver.h"
#				define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#			endif

#		else // Windows Store Apps:
#			ifdef TESTING // Test application:
#				include "targetver.h"
#				include "CppUnitTest.h" // Headers for CppUnitTest
#				define MSVC_CPPUNIT
#				define TEST(TEST_CASE, TEST_NAME)	TEST_METHOD(TEST_NAME)
#				define EXPECT_EQ(EXPECTED, ACTUAL)	Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue((EXPECTED) == (ACTUAL));
#				define EXPECT_NE(EXPECTED, ACTUAL)	Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse((EXPECTED) == (ACTUAL));
#				define EXPECT_TRUE(EXPR)	Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsTrue((EXPR));
#				define EXPECT_FALSE(EXPR)	Microsoft::VisualStudio::CppUnitTestFramework::Assert::IsFalse((EXPR));

#			else // Static library:
#				include "targetver.h"
#				ifndef WIN32_LEAN_AND_MEAN
#					define WIN32_LEAN_AND_MEAN
#				endif
#				include <windows.h>
#			endif
#		endif

// TODO: reference additional headers your program requires here

#	else // QtCreator & other IDE's:

#       ifdef TESTING // Test application:
#           include <gtest/gtest.h>
#       endif

#   endif

#endif // end of header guard
