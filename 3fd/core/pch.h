// pch.h: Dies ist eine vorkompilierte Headerdatei.
// Die unten aufgeführten Dateien werden nur einmal kompiliert, um die Buildleistung für zukünftige Builds zu verbessern.
// Dies wirkt sich auch auf die IntelliSense-Leistung aus, Codevervollständigung und viele Features zum Durchsuchen von Code eingeschlossen.
// Die hier aufgeführten Dateien werden jedoch ALLE neu kompiliert, wenn mindestens eine davon zwischen den Builds aktualisiert wird.
// Fügen Sie hier keine Dateien hinzu, die häufig aktualisiert werden sollen, da sich so der Leistungsvorteil ins Gegenteil verkehrt.

#ifndef PCH_H
#define PCH_H

#   ifdef _MSC_VER // Visual Studio:
#       include <winapifamily.h>

#       ifndef NDEBUG // CRT support for memory leak detection:
#           define _CRTDBG_MAP_ALLOC
#           include <stdlib.h>
#           include <crtdbg.h>
#       endif

// Windows Desktop App:
#        if WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP

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
#                include "winrt/Windows.ApplicationModel.Core.h"

#				 define _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING
#                include <gtest/gtest.h>

#            else // Static library:
#                include "targetver.h"
#                ifndef WIN32_LEAN_AND_MEAN
#                    define WIN32_LEAN_AND_MEAN
#                endif
#            endif

#            include <windows.h>
#            include <winrt\base.h>

#        endif

#    else // QtCreator & other IDE's:
#       ifdef TESTING // Test application:
#           include <gtest/gtest.h>
#       endif

#   endif

#endif // end of header guard
