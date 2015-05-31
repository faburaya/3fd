#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include "callstacktracer.h"
#include <string>
#include <sstream>
#include <exception>
#include <future>

namespace _3fd
{
	using std::string;

	namespace core
	{
		/// <summary>
		/// Aggregates extension functions that work on objects of the C++ Standard Library.
		/// </summary>
		class StdLibExt
		{
		public:

			static string GetDetailsFromSystemError(std::system_error &ex);

			static string GetDetailsFromFutureError(std::future_error &ex);
		};

#ifdef _WIN32
		/// <summary>
		/// Aggregates extension functions that work on types/objects of the Windows API
		/// </summary>
		class WWAPI
		{
		public:

			static string GetHResultLabel(HRESULT errCode);

#	ifdef _3FD_PLATFORM_WINRT
			static string GetDetailsFromWinRTEx(Platform::Exception ^ex);
#	endif
		};
#endif

		/// <summary>
		/// An interface that encompasses all type of exceptions dealt by this application.
		/// </summary>
		class IAppException
		{
		public:

			virtual ~IAppException() {}
			virtual IAppException *Move() = 0;
			virtual IAppException *GetInnerException() const = 0;
			virtual string GetErrorMessage() const = 0;
		};

		/// <summary>
		/// A template used to wrap different types of exceptions under the same interface.
		/// </summary>
		template <typename StdExType>
		class AppException : public IAppException, public StdExType
		{
		private:

			string m_details;
			string m_cst;
			IAppException *m_innerEx;
			
			/// <summary>
			/// Moves this instance exporting the resources to a new object dynamically allocated.
			/// </summary>
			/// <returns>A new exception object built from the resources of this object.</returns>
			virtual AppException *Move() override { return new AppException(std::move(*this)); }

			/// <summary>
			/// Gets the inner exception.
			/// </summary>
			virtual IAppException *GetInnerException() const override { return m_innerEx; }

		public:

			template <typename StrType> 
			AppException(StrType &&what) : 
				StdExType(std::forward<StrType>(what)), 
				m_innerEx(nullptr) 
			{
#			ifdef ENABLE_3FD_CST
				if(CallStackTracer::IsReady())
					m_cst = CallStackTracer::GetInstance().GetStackReport();
#			endif	
			}
			
			template <typename StrType> 
			AppException(StrType &&what, IAppException &innerEx) : 
				StdExType(std::forward<StrType>(what)), 
				m_innerEx(innerEx.Move()) 
			{
#			ifdef ENABLE_3FD_CST
				if(CallStackTracer::IsReady())
					m_cst = CallStackTracer::GetInstance().GetStackReport();
#			endif
			}

			template <typename StrType1, typename StrType2> 
			AppException(StrType1 &&what, StrType2 &&details) : 
				StdExType(std::forward<StrType1>(what)), 
				m_details(std::forward<StrType2>(details)), 
				m_innerEx(nullptr) 
			{
#			ifdef ENABLE_3FD_CST
				if(CallStackTracer::IsReady())
					m_cst = CallStackTracer::GetInstance().GetStackReport();
#			endif
			}

			template <typename StrType1, typename StrType2> 
			AppException(StrType1 &&what, StrType2 &&details, IAppException &innerEx) : 
				StdExType(std::forward<StrType1>(what)), 
				m_details(std::forward<StrType2>(details)), 
				m_innerEx(innerEx.Move()) 
			{
#			ifdef ENABLE_3FD_CST
				if(CallStackTracer::IsReady())
					m_cst = CallStackTracer::GetInstance().GetStackReport();
#			endif
			}

			/// <summary>
			/// Initializes a new instance of the <see cref="AppException{StdExType}"/> class using move semantics.
			/// </summary>
			/// <param name="ob">The object whose resources will be stolen.</param>
			AppException(AppException &&ob) : 
				StdExType(std::move(ob.what())), 
				m_details(std::move(ob.m_details)), 
				m_cst(std::move(ob.m_cst)), 
				m_innerEx(ob.m_innerEx)
			{
				ob.m_innerEx = nullptr;
			}

			/// <summary>
			/// Finalizes an instance of the <see cref="AppException{StdExType}"/> class.
			/// </summary>
			virtual ~AppException()
			{
				delete m_innerEx;
			}

			/// <summary>
			/// Gets the error message (including the details if in debug mode).
			/// </summary>
			virtual string GetErrorMessage() const override
			{
				std::ostringstream oss;
				oss << StdExType::what();

#			ifdef ENABLE_3FD_ERR_IMPL_DETAILS
				if(m_details.empty() == false)
					oss << " - " << m_details;
#			endif
#			ifdef ENABLE_3FD_CST
				if(m_cst.empty() == false)
					oss << " - Call Stack: { " << m_cst << " }";
#			endif	

				return oss.str();
			}
		};

	}// end namespace core
}// end namespace _3fd

#endif // header guard
