#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "preprocessing.h"
#include <memory>
#include <string>
#include <chrono>
#include <mutex>

namespace _3fd
{
	using std::string;
	
	namespace core
	{
		/// <summary>
		/// A singleton that holds the application settings.
		/// </summary>
		class AppConfig
		{
		private:

			struct Tree
			{
				// Used by both application and framework:
				struct
				{
					struct
					{
#ifdef _3FD_POCO_SUPPORT // For POCO C++ logging facilities:
						int purgeAge;
						int purgeCount;
						bool writeToConsole;
#endif
						int sizeLimit;
					} log;
				} common;

				// Required by the framework and for its exclusive use:
				struct
				{
					struct
					{
#ifdef _3FD_OPENCL_SUPPORT
						bool opencl;
#endif
					} dependencies;

					struct
					{
						int stackLogInitialCap;
					} stackTracing;

					struct
					{
						int msgLoopSleepTimeoutMilisecs;
						
						struct
						{
							int initialSize;
							float growingFactor;
						} memBlocksMemPool;
						
						struct
						{
							int initialSizeLog2;
							float loadFactorThreshold;
						} sptrObjectsHashTable;
					} gc;

#ifdef _3FD_OPENCL_SUPPORT
					struct
					{
						int maxSourceCodeLineLength;
						int maxBuildLogSize;
					} opencl;
#endif

#ifdef _3FD_ESENT_SUPPORT
					struct
					{
						bool useWindowsFileCache;
					} isam;
#endif

#ifdef _3FD_POCO_SUPPORT
                    struct
                    {
                        int maxMessageSizeBytes;
                    } broker;
#endif
				} framework;

				struct
				{
					/* HERE YOU CAN INSERT YOUR OWN STRUCTURES FOR NEW KEYS IN THE XML CONFIGURATION FILE */
				} application;
			} settings;

			string m_applicationId;

			static std::unique_ptr<AppConfig> uniqueObject;
			static std::mutex initializationMutex;

			static AppConfig &GetInstanceInitialized();

			void Initialize();

			AppConfig() {} // empty private constructor

		public:

			static const string &GetApplicationId();
			static const Tree &GetSettings();
		};
	}
}

#endif // end of header guard
