#include "stdafx.h"
#include "configuration.h"
#include "exceptions.h"

#ifdef _3FD_PLATFORM_WINRT
#	include "utils_winrt.h"
#endif

#ifdef _3FD_POCO_SUPPORT
#	include "Poco/AutoPtr.h"
#	include "Poco/Util/XMLConfiguration.h"
#endif

#include <array>
#include <sstream>
#include <codecvt>
#include <system_error>

#if defined __linux__ || defined __unix__ // POSIX:
#   include <unistd.h>
#endif

namespace _3fd
{
	using std::ostringstream;

	namespace core
	{
		std::unique_ptr<AppConfig> AppConfig::uniqueObject;

		std::mutex AppConfig::initializationMutex;

		/// <summary>
		/// Gets the instance already initialized.
		/// </summary>
		/// <returns>The unique instance of <see cref="AppConfig"/> already initialized.</returns>
		AppConfig & AppConfig::GetInstanceInitialized()
		{
			static bool initialized(false);

			if(initialized)
				return *uniqueObject;
			else
			{
				try
				{
					std::lock_guard<std::mutex> lock(initializationMutex);

					if(initialized == false)
					{
						if (!uniqueObject)
							uniqueObject.reset(new AppConfig());

						uniqueObject->Initialize();
						initialized = true;
					}
				
					return *uniqueObject;
				}
				catch(core::IAppException &)
				{
					throw; // just forward the exceptions regarding error that are known to have been previously handled
				}
				catch(std::system_error &ex)
				{
					std::ostringstream oss;
					oss << "3FD function is compromised by a critical error! "
                           "Failed to acquire lock before loading framework configurations: "
                        << core::StdLibExt::GetDetailsFromSystemError(ex);

					throw AppException<std::runtime_error>(oss.str());
				}
				catch (std::exception &ex)
				{
					std::ostringstream oss;
					oss << "3FD function is compromised by a critical error! "
                           "Generic failure when initializing framework configurations: " << ex.what();

					throw AppException<std::runtime_error>(oss.str());
				}
			}
		}

		/// <summary>
		/// Gets the application identifier.
		/// </summary>
		/// <returns>The application ID, which is the name of the current executable.</returns>
		const string & AppConfig::GetApplicationId()
		{
			return GetInstanceInitialized().m_applicationId;
		}

		/// <summary>
		/// Gets the settings.
		/// </summary>
		/// <returns>A reference to the hierarchy of settings loaded from the XML configuration file.</returns>
		const AppConfig::Tree & AppConfig::GetSettings()
		{
			return GetInstanceInitialized().settings;
		}

#if _WIN32 // Microsoft Windows:

#	ifdef _3FD_PLATFORM_WIN32API // Windows Desktop Apps only:

		/// <summary>
		/// Gets an ID for the running application, invoking a system call.
		/// </summary>
		/// <param name="appFilePath">Where to store the retrieved file path for the running application.</param>
		/// <returns>A text ID (UTF-8 encoded) for the running application.</returns>
		static string CallSysForApplicationId(string &appFilePath)
		{
			std::array<wchar_t, 300> chBuffer;

			if (GetModuleFileNameW(nullptr, chBuffer.data(), chBuffer.size()) == 0)
				throw AppException<std::runtime_error>("It was not possible to get the full file name of the executable.", "Windows API: GetModuleFileName");

			try
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
				appFilePath = converter.to_bytes(chBuffer.data());
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failures when determining the file name of the framework configuration file: " << ex.what();
				throw AppException<std::runtime_error>(oss.str());
			}

			// The application ID is the name of the executable without the extension:
			return string(appFilePath.begin() + appFilePath.rfind('\\') + 1,
				appFilePath.begin() + appFilePath.rfind('.'));
		}

#	elif defined _3FD_PLATFORM_WINRT // Store Apps only:

		/// <summary>
		/// Gets an ID for the running application, invoking a system call.
		/// </summary>
		/// <param name="appCfgFileName">Where to store the configuration file name for the running application.</param>
		/// <returns>A text ID (UTF-8 encoded) for the running application.</returns>
		static string CallSysForApplicationId(Platform::String ^&appCfgFileName)
		{
			auto curPackageId = Windows::ApplicationModel::Package::Current->Id->Name;

			try
			{
                appCfgFileName = curPackageId + Platform::StringReference(L".3fd.config");
				std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
				return converter.to_bytes(curPackageId->Data());
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "Generic failure when determining the file name of the framework configuration file: " << ex.what();
				throw AppException<std::runtime_error>(oss.str());
			}
		}

#	endif

#elif defined __linux__ || defined __unix__ // POSIX only:

		/// <summary>
		/// Gets an ID for the running application, invoking a system call.
		/// </summary>
		/// <param name="appFilePath">Where to store the retrieved file path for the running application.</param>
		/// <returns>A text ID (UTF-8 encoded) for the running application.</returns>
		static string CallSysForApplicationId(string &appFilePath)
		{
			std::array<char, 300> chBuffer;
			auto rval = readlink("/proc/self/exe", chBuffer.data(), chBuffer.size());

			if (rval != -1)
			{
				// The application ID is the name of the executable:
				chBuffer[rval] = 0;
				appFilePath = string(chBuffer.begin(), chBuffer.begin() + rval);
				return string(appFilePath.begin() + appFilePath.rfind('/') + 1, appFilePath.end());
			}
			else
				throw std::system_error(std::make_error_code(static_cast<std::errc>(errno)), "POSIX API: readlink");
		}

#endif

#ifdef _3FD_POCO_SUPPORT

		/// <summary>
		/// Initializes this instance with data from the XML configuration file.
		/// </summary>
		void AppConfig::Initialize()
		{
			try
			{
				string appFilePath;
				m_applicationId = CallSysForApplicationId(appFilePath);

				using Poco::AutoPtr;
				using Poco::Util::XMLConfiguration;

				// Load the configurations in the file
				AutoPtr<XMLConfiguration> config(new XMLConfiguration(appFilePath + ".3fd.config"));
                
				settings.common.log.purgeAge = config->getInt("common.log.entry[@key='purgeAge'][@value]", 30);
				settings.common.log.purgeCount = config->getInt("common.log.entry[@key='purgeCount'][@value]", 16);
                settings.common.log.sizeLimit = config->getInt("common.log.entry[@key='sizeLimit'][@value]", 1024);
				settings.common.log.writeToConsole = config->getBool("common.log.entry[@key='writeToConsole'][@value]", false);

				settings.framework.dependencies.opencl = config->getBool("framework.dependencies.entry[@key='opencl'][@value]", false);

				settings.framework.stackTracing.stackLogInitialCap = config->getInt("framework.stackTracing.stackLogInitialCap", 32);

				settings.framework.gc.msgLoopSleepTimeoutMilisecs = config->getInt("framework.gc.entry[@key='msgLoopSleepTimeoutMillisecs'][@value]", 100);
				settings.framework.gc.memBlocksMemPool.initialSize = config->getInt("framework.gc.entry[@key='memoryBlocksPoolInitialSize'][@value]", 128);
				settings.framework.gc.memBlocksMemPool.growingFactor = static_cast<float> (config->getDouble("framework.gc.entry[@key='memoryBlocksPoolGrowingFactor'][@value]", 1.0));
				settings.framework.gc.sptrObjectsHashTable.initialSizeLog2 = config->getInt("framework.gc.entry[@key='sptrObjsHashTabInitSizeLog2'][@value]", 8);
				settings.framework.gc.sptrObjectsHashTable.loadFactorThreshold = static_cast<float> (config->getDouble("framework.gc.entry[@key='sptrObjsHashTabLoadFactorThreshold'][@value]", 0.7));

#	ifdef _3FD_OPENCL_SUPPORT
				settings.framework.opencl.maxSourceCodeLineLength = config->getInt("framework.opencl.entry[@key='maxSourceCodeLineLength'][@value]", 128);
				settings.framework.opencl.maxBuildLogSize = config->getInt("framework.opencl.entry[@key='maxBuildLogSize'][@value]", 5120);
#	endif
#	ifdef _3FD_ESENT_SUPPORT
				settings.framework.isam.useWindowsFileCache = config->getBool("framework.isam.entry[@key='useWindowsFileCache'][@value]", true);
#	endif
			}
			catch (std::system_error &ex)
			{
				std::ostringstream oss;
				oss << "3FD function is compromised by a critical error! "
                       "Failed to initialize the application settings: " << ex.what();

				throw AppException<std::runtime_error>(oss.str());
			}
			catch (Poco::Exception &ex)
			{
				ostringstream oss;
				oss << "POCO C++ library reported: " << ex.message();

				throw AppException<std::runtime_error>(
                    "3FD function is compromised by a critical error! "
                    "Failed to initialize the application settings",
                    oss.str()
                );
			}
		}

#elif defined _3FD_PLATFORM_WINRT // Store Apps only:

		/// <summary>
		/// Helper function to get a text value of configuration inside a DOM node.
		/// </summary>
		/// <param name="nodeSection">The node of the section where to look for the configuration.</param>
		/// <param name="xpathNodeConfig">The xpath for the configuration, starting from the right section.</param>
		/// <param name="defaultValue">The default value.</param>
		/// <returns>The configuration stored in attribute 'value'.</returns>
		static string GetString(
			Windows::Data::Xml::Dom::IXmlNode ^nodeSection,
            const wchar_t *xpathNodeConfig,
			const string &defaultValue)
		{
			if (nodeSection == nullptr)
				return defaultValue;

			auto nodeConfig = nodeSection->SelectSingleNode(Platform::StringReference(xpathNodeConfig));
			if (nodeConfig == nullptr)
				return defaultValue;

            auto attribute = nodeConfig->Attributes->GetNamedItem(Platform::StringReference(L"value"));
            if (attribute == nullptr)
                return defaultValue;

            auto valAsText = attribute->InnerText;

			if (valAsText->IsEmpty() == false)
			{
				std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
				return converter.to_bytes(valAsText->Data());
			}
			else
				return defaultValue;
		}

        /// <summary>
        /// Helper function to get a boolean value of configuration inside a DOM node.
        /// </summary>
        /// <param name="nodeSection">The node of the section where to look for the configuration.</param>
        /// <param name="xpathNodeConfig">The xpath for the configuration, starting from the right section.</param>
        /// <param name="defaultValue">The default value.</param>
        /// <returns>The configuration stored in attribute 'value'.</returns>
        static bool GetBoolean(
            Windows::Data::Xml::Dom::IXmlNode ^nodeSection,
            const wchar_t *xpathNodeConfig,
            bool defaultValue)
        {
            if (nodeSection == nullptr)
                return defaultValue;

            auto nodeConfig = nodeSection->SelectSingleNode(Platform::StringReference(xpathNodeConfig));
            if (nodeConfig == nullptr)
                return defaultValue;

            auto attribute = nodeConfig->Attributes->GetNamedItem(Platform::StringReference(L"value"));
            if (attribute == nullptr)
                return defaultValue;

            auto valAsText = attribute->InnerText;

            if (valAsText->IsEmpty() == false)
            {
                if (wcsicmp(valAsText->Data(), L"true") == 0)
                    return true;
                else if (wcsicmp(valAsText->Data(), L"false") == 0)
                    return false;
                else
                    return defaultValue;
            }
            else
                return defaultValue;
        }

		/// <summary>
		/// Helper function to get an integer value of configuration inside a DOM node.
		/// </summary>
		/// <param name="nodeSection">The node of the section where to look for the configuration.</param>
		/// <param name="xpathNodeConfig">The xpath for the configuration, starting from the right section.</param>
		/// <param name="defaultValue">The default value.</param>
        /// <returns>The configuration stored in attribute 'value'.</returns>
		static int GetInteger(
			Windows::Data::Xml::Dom::IXmlNode ^nodeSection,
            const wchar_t *xpathNodeConfig,
			int defaultValue)
		{
			if (nodeSection == nullptr)
				return defaultValue;

			auto nodeConfig = nodeSection->SelectSingleNode(Platform::StringReference(xpathNodeConfig));
			if (nodeConfig == nullptr)
				return defaultValue;

            auto attribute = nodeConfig->Attributes->GetNamedItem(Platform::StringReference(L"value"));
            if (attribute == nullptr)
                return defaultValue;

            auto valAsText = attribute->InnerText->Data();
			long parsedVal = wcstol(valAsText, nullptr, 10);
			return static_cast<int> (parsedVal > 0 ? parsedVal : defaultValue);
		}

		/// <summary>
		/// Helper function to get a floating point value of configuration inside a DOM node.
		/// </summary>
		/// <param name="nodeSection">The node of the section where to look for the configuration.</param>
		/// <param name="xpathNodeConfig">The xpath for the configuration, starting from the right section.</param>
		/// <param name="defaultValue">The default value.</param>
        /// <returns>The configuration stored in attribute 'value'.</returns>
		static float GetFloat(
			Windows::Data::Xml::Dom::IXmlNode ^nodeSection,
			const wchar_t *xpathNodeConfig,
			float defaultValue)
		{
			if (nodeSection == nullptr)
				return defaultValue;

			auto nodeConfig = nodeSection->SelectSingleNode(Platform::StringReference(xpathNodeConfig));
			if (nodeConfig == nullptr)
				return defaultValue;

            auto attribute = nodeConfig->Attributes->GetNamedItem(Platform::StringReference(L"value"));
            if (attribute == nullptr)
                return defaultValue;
            
			auto valAsText = attribute->InnerText->Data();
			float parsedVal = wcstod(valAsText, nullptr);
			return parsedVal > 0.0 ? parsedVal : defaultValue;
		}

		/// <summary>
		/// Initializes this instance with data from the XML configuration file.
		/// </summary>
		void AppConfig::Initialize()
		{
			try
			{
				Platform::String ^cfgFileName;
				m_applicationId = CallSysForApplicationId(cfgFileName);

				auto file = utils::WinRTExt::WaitForAsync(
					Windows::ApplicationModel::Package::Current->InstalledLocation->GetFileAsync(cfgFileName)
				);
				
				auto config = utils::WinRTExt::WaitForAsync(
					Windows::Data::Xml::Dom::XmlDocument::LoadFromFileAsync(file)
				);

				// Logging options:
				auto node = config->SelectSingleNode(L"/configuration/common/log");
				settings.common.log.sizeLimit = GetInteger(node, L"./entry[@key='sizeLimit']", 2048);

				auto framework = config->SelectSingleNode(L"/configuration/framework");

                // Stack tracing options:
				settings.framework.stackTracing.stackLogInitialCap =
                    GetInteger(framework, L"./stackTracing/entry[@key='logInitialCap']", 32);

				// Garbage collector options:
				node = framework->SelectSingleNode(L"./gc");

				settings.framework.gc.msgLoopSleepTimeoutMilisecs =
                    GetInteger(node, L"./entry[@key='msgLoopSleepTimeoutMillisecs']", 100);

				settings.framework.gc.memBlocksMemPool.initialSize =
                    GetInteger(node, L"./entry[@key='memoryBlocksPoolInitialSize']", 128);

				settings.framework.gc.memBlocksMemPool.growingFactor =
                    GetFloat(node, L"./entry[@key='memoryBlocksPoolGrowingFactor']", 1.0F);

				settings.framework.gc.sptrObjectsHashTable.initialSizeLog2 =
                    GetInteger(node, L"./entry[@key='sptrObjsHashTabInitSizeLog2']", 8);

				settings.framework.gc.sptrObjectsHashTable.loadFactorThreshold =
                    GetFloat(node, L"./entry[@key='sptrObjsHashTabLoadFactorThreshold']", 0.7F);

                // ISAM options:
                node = framework->SelectSingleNode(L"./isam");
                settings.framework.isam.useWindowsFileCache =
                    GetBoolean(node, L"./entry[@key='useWindowsFileCache']", true);
			}
            catch (core::IAppException &)
            {
                throw; // forward exceptions known to have been already handled
            }
			catch (Platform::Exception ^ex)
			{
				ostringstream oss;
				oss << "Windows Runtime library reported: " << WWAPI::GetDetailsFromWinRTEx(ex);

				throw AppException<std::runtime_error>(
                    "3FD function is compromised by a critical error! "
                    "Failed to initialize the application settings",
                    oss.str()
                );
			}
			catch (std::exception &ex)
			{
				std::ostringstream oss;
				oss << "3FD function is compromised by a critical error! "
                       "Generic failure when initializing the application settings: " << ex.what();

				throw AppException<std::runtime_error>(oss.str());
			}
		}

#endif

    }// end of namespace core
}// end of namespace _3fd
