// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#ifndef STDAFX_H // header guard
#define STDAFX_H

#    ifdef _MSC_VER // Visual Studio:

#       ifndef _USING_V110_SDK71_
#            include <winapifamily.h>
#       endif

        // Windows Desktop App:
#        if WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP || defined _USING_V110_SDK71_

#            ifdef TESTING // Test application:
#                include "targetver.h"
#                include <stdio.h>
#                include <tchar.h>
#                define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#				 define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
#                include <gtest/gtest.h>

#            else // Static library:
#                include "targetver.h"
#                define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#            endif

#        else // Windows Store Apps:

#            ifdef TESTING // Test application:
#                include <collection.h>
#                include <ppltasks.h>
#				 define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
#                include <gtest/gtest.h>
#                include "App.xaml.h"

#            else // Static library:
#                include "targetver.h"
#                ifndef WIN32_LEAN_AND_MEAN
#                    define WIN32_LEAN_AND_MEAN
#                endif
#                include <windows.h>
#            endif

#        endif

#    else // QtCreator & other IDE's:

#       ifdef TESTING // Test application:
#           include <gtest/gtest.h>
#       endif

#   endif

#endif // end of header guard
