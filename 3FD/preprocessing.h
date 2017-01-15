#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#define STATUS_OKAY false
#define STATUS_FAIL true

// If using a GCC compiler:
#ifdef __GNUG__
#	define	__FUNCTION__ __PRETTY_FUNCTION__
#endif

#ifdef _MSC_VER
// Microsoft Visual Studio:
#   define INTFOPT __declspec(novtable) // CHANGE THIS DEFINITION ACCORDING TO YOUR COMPILER (or leave it blank)

#   if _MSC_VER < 19000
#       define NOEXCEPT             throw()
#       define thread_local_decl    __declspec(thread)
#       define thread_local_def
#   else
#       define NOEXCEPT             noexcept
#       define thread_local_decl    thread_local
#       define thread_local_def     thread_local
#   endif
#else
// Other Compilers:
#   define INTFOPT
#   define _ASSERTE             assert
#   include <cassert>
#   define NOEXCEPT             noexcept
#   define thread_local_decl    thread_local
#   define thread_local_def     thread_local
#endif

// Platform support for particular modules/features/resources:
#ifdef _WIN32
#	include <winapifamily.h>
#   if WINAPI_FAMILY == WINAPI_FAMILY_DESKTOP_APP // Windows Desktop Apps only:
#       define _3FD_PLATFORM_WIN32API
#   	define _3FD_POCO_SUPPORT
#       define _3FD_ESENT_SUPPORT
#   	define _3FD_OPENCL_SUPPORT
#       define _3FD_CONSOLE_AVAILABLE
#   elif defined WINAPI_FAMILY_SYSTEM
#       define _3FD_PLATFORM_WINRT
#       define _3FD_PLATFORM_WINRT_UWP
#       define _3FD_ESENT_SUPPORT
#   elif WINAPI_FAMILY == WINAPI_FAMILY_PC_APP // Windows Store Apps (& UWP) only:
#       define _3FD_PLATFORM_WINRT
#       define _3FD_PLATFORM_WINRT_PC
#       define _3FD_ESENT_SUPPORT
#   elif WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP // Windows Phone 8 Store Apps only:
#       define _3FD_PLATFORM_WINRT
#       define _3FD_PLATFORM_WINRT_PHONE
#   endif

#elif defined __linux__ // Linux only:
#	define _3FD_POCO_SUPPORT
#	define _3FD_OPENCL_SUPPORT
#	define _3FD_CONSOLE_AVAILABLE
#elif defined __unix__ // Unix only:
#	define _3FD_POCO_SUPPORT
#	define _3FD_CONSOLE_AVAILABLE
#endif

// These instructions have they definition depending on whether this is a release compilation:
#ifdef NDEBUG
#   if defined _MSC_VER && _MSC_VER < 19000
#       define NOEXCEPT throw()
#   else
#       define NOEXCEPT noexcept
#   endif

#   define RELEASE_DEBUG_SWITCH(STATEMENT1, STATEMENT2) STATEMENT1
#	define ONDEBUG(CODE_LINE)	;
#else
#   ifdef _3FD_PLATFORM_WIN32API
#       include <vld.h> // Visual Leak Detector

#   elif defined _3FD_PLATFORM_WINRT
#       define _CRTDBG_MAP_ALLOC
#       include <stdlib.h>
#       include <crtdbg.h>
#   endif

#	define dbgexhnd
#   define RELEASE_DEBUG_SWITCH(STATEMENT1, STATEMENT2) STATEMENT2
#	define ONDEBUG(CODE_LINE) CODE_LINE
#endif

// Some few useful "keywords":
#define const_this const_cast<const decltype(*this) &> (*this)

// These are the calls that should be used for handling errors: it uses the RuntimeManager class:
#ifdef ENABLE_3FD_CST
#   // Obligatory use if you want call stack tracing feature:
#	define CALL_STACK_TRACE _3fd::core::CallStackTracer::TrackCall(__FILE__, __LINE__, __FUNCTION__); _3fd::core::StackDeactivationTrigger _stackDeactTrigObj;
#else
#	define CALL_STACK_TRACE
#endif

#endif // header guard
